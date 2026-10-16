/**
 * @file nimcp_multi_task.c
 * @brief Multi-Task Learning (MTL) Coordination Implementation
 *
 * WHAT: Train single model on multiple related tasks simultaneously
 * WHY:  Improve generalization via shared representations
 * HOW:  Task-specific heads, gradient balancing (PCGrad, GradNorm)
 *
 * FRAMEWORK COMPARISON:
 * - PyTorch LibMTL: Similar task management, gradient methods
 * - JAX: Manual implementation typical
 * - TensorFlow: Multi-output models with manual balancing
 *
 * NIMCP ADVANTAGES:
 * - Bio-inspired via cortical module specialization
 * - Deep integration with thalamic routing
 * - Native gradient conflict resolution
 *
 * BIOLOGICAL GROUNDING:
 * - Shared encoder ≈ early visual cortex (V1-V4)
 * - Task heads ≈ specialized higher areas (IT, MT)
 * - Gradient conflicts ≈ competing neural populations
 * - PCGrad ≈ lateral inhibition mechanism
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#include "training/nimcp_multi_task.h"
#include "constants/nimcp_constants.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(multi_task)

//=============================================================================
// Internal Constants
//=============================================================================

#define MTL_EPSILON           NIMCP_EPSILON_ADAM
#define MTL_MAX_GRAD_NORM     10.0f

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Task runtime state
 */
typedef struct {
    mtl_task_def_t def;              /**< Task definition */
    float current_weight;            /**< Current loss weight */
    float loss_history[32];          /**< Recent loss history */
    uint32_t history_idx;            /**< History index */
    float initial_loss;              /**< Loss at start (for DWA) */
    float* last_gradient;            /**< Last gradient for this task */
    size_t gradient_size;            /**< Gradient size */
    uint64_t step_count;             /**< Steps trained on this task */
    bool initialized;                /**< Has initial loss */
} task_state_t;

/**
 * @brief GradNorm runtime state
 */
typedef struct {
    float* weights;                  /**< Learnable task weights */
    float* initial_losses;           /**< Initial losses per task */
    float* loss_ratios;              /**< L_i(t) / L_i(0) */
    float* grad_norms;               /**< Gradient norms per task */
    float* weight_momentum;          /**< Momentum for weight updates (oscillation damping) */
    float avg_loss_ratio;            /**< Average loss ratio */
    float damping_factor;            /**< Convergence damping factor (0.9 default) */
    bool initialized;
} gradnorm_state_t;

/**
 * @brief Uncertainty weighting state
 */
typedef struct {
    float* log_vars;                 /**< Log variance per task */
    float* log_var_grads;            /**< Gradients of log vars */
} uncertainty_state_t;

/**
 * @brief MTL context implementation
 */
struct mtl_ctx_s {
    mtl_config_t config;             /**< Configuration */
    bool initialized;                /**< Context initialized */

    /* Task management */
    task_state_t* tasks;             /**< Task states */
    uint32_t num_tasks;              /**< Number of registered tasks */
    uint32_t* active_task_ids;       /**< Active task IDs */
    uint32_t num_active;             /**< Number of active tasks */

    /* Weighting state */
    gradnorm_state_t gradnorm;       /**< GradNorm state */
    uncertainty_state_t uncertainty; /**< Uncertainty state */

    /* Sampling state */
    uint32_t round_robin_idx;        /**< For round-robin sampling */
    unsigned int rng_seed;           /**< RNG state */

    /* Integration points */
    nimcp_gradient_manager_ctx_t* grad_manager;
    void* brain_factory;
    void* thalamic_router;

    /* Statistics */
    mtl_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

//=============================================================================
// Forward Declarations
//=============================================================================

static float dot_product(const float* a, const float* b, size_t n);
static float vector_norm(const float* v, size_t n);
static void project_gradient(float* grad, const float* other, size_t n);
static float compute_entropy(const float* probs, uint32_t n);
static float compute_variance(const float* values, uint32_t n);

//=============================================================================
// Lifecycle API
//=============================================================================

int mtl_default_config(mtl_config_t* config) {
    if (!config) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "mtl_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    /* Default architecture and strategies */
    config->architecture = MTL_ARCH_HARD_SHARING;
    config->weighting = MTL_WEIGHT_UNCERTAINTY;
    config->gradient_method = MTL_GRAD_PCGRAD;
    config->sampling = MTL_SAMPLE_UNIFORM;

    /* GradNorm defaults */
    config->gradnorm.alpha = 1.5f;
    config->gradnorm.initial_weights = 1.0f;
    config->gradnorm.weight_lr = 0.025f;
    config->gradnorm.use_last_layer = true;

    /* PCGrad defaults */
    config->pcgrad.projection_eps = NIMCP_EPSILON_ADAM;
    config->pcgrad.normalize_gradients = true;
    config->pcgrad.use_random_order = true;

    /* CAGrad defaults */
    config->cagrad.c = 0.5f;
    config->cagrad.rescale = 1.0f;

    /* Training settings */
    config->shared_lr = NIMCP_LEARNING_RATE_FINE;
    config->task_lr_multiplier = 1.0f;
    config->auxiliary_weight = 0.5f;

    /* Task scheduling */
    config->temperature = 2.0f;
    config->warmup_steps = 100;
    config->balance_batches = true;

    /* Integration enabled by default */
    config->integrate_gradient_manager = true;
    config->integrate_brain_factory = true;
    config->integrate_thalamic_router = true;

    config->verbose = false;
    config->track_statistics = true;

    return 0;
}

mtl_ctx_t* mtl_create(const mtl_config_t* config) {
    if (!config) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "mtl_create: config is NULL");
        return NULL;
    }

    /* Validate configuration */
    if (mtl_validate_config(config) != 0) {
        NIMCP_THROW(NIMCP_ERROR_CONFIG_INVALID, "mtl_create: config validation failed");
        return NULL;
    }

    mtl_ctx_t* ctx = nimcp_calloc(1, sizeof(mtl_ctx_t));
    if (!ctx) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(mtl_ctx_t),
                          "mtl_create: failed to allocate context");
        return NULL;
    }

    /* Copy configuration */
    memcpy(&ctx->config, config, sizeof(mtl_config_t));

    /* Allocate task storage */
    ctx->tasks = nimcp_calloc(MTL_MAX_TASKS, sizeof(task_state_t));
    if (!ctx->tasks) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, MTL_MAX_TASKS * sizeof(task_state_t),
                          "mtl_create: failed to allocate tasks array");
        nimcp_free(ctx);
        return NULL;
    }

    ctx->active_task_ids = nimcp_calloc(MTL_MAX_TASKS, sizeof(uint32_t));
    if (!ctx->active_task_ids) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, MTL_MAX_TASKS * sizeof(uint32_t),
                          "mtl_create: failed to allocate active task IDs");
        nimcp_free(ctx->tasks);
        nimcp_free(ctx);
        return NULL;
    }

    /* Initialize weighting state */
    if (config->weighting == MTL_WEIGHT_GRADNORM) {
        ctx->gradnorm.weights = nimcp_calloc(MTL_MAX_TASKS, sizeof(float));
        ctx->gradnorm.initial_losses = nimcp_calloc(MTL_MAX_TASKS, sizeof(float));
        ctx->gradnorm.loss_ratios = nimcp_calloc(MTL_MAX_TASKS, sizeof(float));
        ctx->gradnorm.grad_norms = nimcp_calloc(MTL_MAX_TASKS, sizeof(float));
        ctx->gradnorm.weight_momentum = nimcp_calloc(MTL_MAX_TASKS, sizeof(float));

        if (!ctx->gradnorm.weights || !ctx->gradnorm.initial_losses ||
            !ctx->gradnorm.weight_momentum) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, MTL_MAX_TASKS * sizeof(float),
                              "mtl_create: failed to allocate GradNorm state");
            mtl_destroy(ctx);
            return NULL;
        }

        /* Initialize weights and damping */
        for (uint32_t i = 0; i < MTL_MAX_TASKS; i++) {
            ctx->gradnorm.weights[i] = config->gradnorm.initial_weights;
            ctx->gradnorm.weight_momentum[i] = 0.0f;
        }
        ctx->gradnorm.damping_factor = 0.9f;  /* Default damping for oscillation control */
    }

    if (config->weighting == MTL_WEIGHT_UNCERTAINTY) {
        ctx->uncertainty.log_vars = nimcp_calloc(MTL_MAX_TASKS, sizeof(float));
        ctx->uncertainty.log_var_grads = nimcp_calloc(MTL_MAX_TASKS, sizeof(float));

        if (!ctx->uncertainty.log_vars) {
            mtl_destroy(ctx);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mtl_create: ctx->uncertainty is NULL");
            return NULL;
        }
    }

    /* Create mutex */
    ctx->mutex = nimcp_mutex_create(NULL);
    if (!ctx->mutex) {
        mtl_destroy(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mtl_create: ctx->mutex is NULL");
        return NULL;
    }

    /* P3-MTL-1: Mix in address of ctx for better entropy than time(NULL) alone */
    ctx->rng_seed = (unsigned int)time(NULL) ^ (unsigned int)(uintptr_t)ctx;

    ctx->initialized = true;
    return ctx;
}

void mtl_destroy(mtl_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    /* Free task gradients */
    if (ctx->tasks) {
        for (uint32_t i = 0; i < ctx->num_tasks; i++) {
            if (ctx->tasks[i].last_gradient) {
                nimcp_free(ctx->tasks[i].last_gradient);
            }
        }
        nimcp_free(ctx->tasks);
    }

    nimcp_free(ctx->active_task_ids);

    /* Free weighting state */
    nimcp_free(ctx->gradnorm.weights);
    nimcp_free(ctx->gradnorm.initial_losses);
    nimcp_free(ctx->gradnorm.loss_ratios);
    nimcp_free(ctx->gradnorm.grad_norms);
    nimcp_free(ctx->gradnorm.weight_momentum);

    nimcp_free(ctx->uncertainty.log_vars);
    nimcp_free(ctx->uncertainty.log_var_grads);

    if (ctx->mutex) {
        nimcp_mutex_free(ctx->mutex);
    }

    nimcp_free(ctx);
}

//=============================================================================
// Task Management API
//=============================================================================

int mtl_register_task(mtl_ctx_t* ctx, const mtl_task_def_t* task) {
    if (!ctx || !task) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mtl_register_task: required parameter is NULL (ctx, task)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    if (ctx->num_tasks >= MTL_MAX_TASKS) {
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "mtl_register_task: capacity exceeded");
        return -1;
    }

    uint32_t idx = ctx->num_tasks;
    task_state_t* state = &ctx->tasks[idx];

    /* Copy task definition */
    memcpy(&state->def, task, sizeof(mtl_task_def_t));
    state->current_weight = task->weight > 0 ? task->weight : 1.0f;
    state->history_idx = 0;
    state->step_count = 0;
    state->initialized = false;

    /* Add to active list if active */
    if (task->active) {
        ctx->active_task_ids[ctx->num_active++] = task->task_id;
    }

    ctx->num_tasks++;

    nimcp_mutex_unlock(ctx->mutex);
    return (int)idx;
}

int mtl_set_task_active(mtl_ctx_t* ctx, uint32_t task_id, bool active) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Find task */
    int task_idx = -1;
    for (uint32_t i = 0; i < ctx->num_tasks; i++) {
        if (ctx->tasks[i].def.task_id == task_id) {
            task_idx = (int)i;
            ctx->tasks[i].def.active = active;
            break;
        }
    }

    if (task_idx < 0) {
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mtl_set_task_active: validation failed");
        return -1;
    }

    /* Rebuild active list */
    ctx->num_active = 0;
    for (uint32_t i = 0; i < ctx->num_tasks; i++) {
        if (ctx->tasks[i].def.active) {
            ctx->active_task_ids[ctx->num_active++] = ctx->tasks[i].def.task_id;
        }
    }

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

float mtl_get_task_weight(const mtl_ctx_t* ctx, uint32_t task_id) {
    if (!ctx) {
        return 1.0f;
    }

    for (uint32_t i = 0; i < ctx->num_tasks; i++) {
        if (ctx->tasks[i].def.task_id == task_id) {
            return ctx->tasks[i].current_weight;
        }
    }

    return 1.0f;
}

int mtl_set_task_weight(mtl_ctx_t* ctx, uint32_t task_id, float weight) {
    if (!ctx || weight < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mtl_set_task_weight: ctx is NULL");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    for (uint32_t i = 0; i < ctx->num_tasks; i++) {
        if (ctx->tasks[i].def.task_id == task_id) {
            ctx->tasks[i].current_weight = weight;
            nimcp_mutex_unlock(ctx->mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(ctx->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mtl_set_task_weight: validation failed");
    return -1;
}

//=============================================================================
// Training API
//=============================================================================

int mtl_compute_loss(
    mtl_ctx_t* ctx,
    const mtl_batch_t* batch,
    nimcp_tensor_t** predictions,
    float* total_loss,
    float* task_losses
) {
    if (!ctx || !batch || !predictions || !total_loss) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mtl_compute_loss: required parameter is NULL (ctx, batch, predictions, total_loss)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    float weighted_sum = 0.0f;

    for (uint32_t i = 0; i < batch->num_active_tasks; i++) {
        uint32_t task_id = batch->task_ids[i];

        /* Find task state */
        task_state_t* task = NULL;
        for (uint32_t j = 0; j < ctx->num_tasks; j++) {
            if (ctx->tasks[j].def.task_id == task_id) {
                task = &ctx->tasks[j];
                break;
            }
        }

        if (!task) continue;

        /* Compute loss for this task */
        float task_loss = 0.0f;

        /* Simplified loss computation - actual would use loss function registry */
        nimcp_tensor_t* pred = predictions[i];
        nimcp_tensor_t* label = batch->labels[i];

        if (pred && label) {
            /* MSE loss as placeholder */
            size_t n_elements = nimcp_tensor_numel(pred);
            const float* pred_data = (const float*)nimcp_tensor_data((nimcp_tensor_t*)pred);
            const float* label_data = (const float*)nimcp_tensor_data((nimcp_tensor_t*)label);
            if (pred_data && label_data && n_elements > 0) {
                for (size_t j = 0; j < n_elements; j++) {
                    float diff = pred_data[j] - label_data[j];
                    task_loss += diff * diff;
                }
                task_loss /= (float)n_elements;
            }
        }

        /* Apply task weight */
        float weight = task->current_weight;

        /* Uncertainty weighting: loss / (2 * exp(log_var)) + 0.5 * log_var */
        if (ctx->config.weighting == MTL_WEIGHT_UNCERTAINTY) {
            /* P1-MTL-1: task_id is user-defined and may not equal internal index.
             * Look up the task's internal index instead of using task_id directly. */
            uint32_t internal_idx = 0;
            bool found_idx = false;
            for (uint32_t k = 0; k < ctx->num_tasks; k++) {
                if (ctx->tasks[k].def.task_id == task_id) {
                    internal_idx = k;
                    found_idx = true;
                    break;
                }
            }
            if (!found_idx || internal_idx >= MTL_MAX_TASKS) continue;
            float log_var = ctx->uncertainty.log_vars[internal_idx];
            float precision = expf(-log_var);
            weight = precision;
            task_loss = task_loss * precision + 0.5f * log_var;
        }

        weighted_sum += weight * task_loss;

        /* Update task state */
        task->loss_history[task->history_idx % 32] = task_loss;
        task->history_idx++;
        if (!task->initialized) {
            task->initial_loss = task_loss;
            task->initialized = true;
        }
        task->step_count++;

        /* Output individual loss */
        if (task_losses) {
            task_losses[i] = task_loss;
        }

        /* Update stats */
        if (task_id < MTL_MAX_TASKS) {
            ctx->stats.task_losses[task_id] = task_loss;
        }
    }

    *total_loss = weighted_sum;
    ctx->stats.avg_loss = batch->num_active_tasks > 0
                             ? weighted_sum / (float)batch->num_active_tasks
                             : 0.0f;
    ctx->stats.total_steps++;

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

int mtl_process_gradients(
    mtl_ctx_t* ctx,
    float** gradients,
    size_t num_params,
    float* combined_gradient
) {
    if (!ctx || !gradients || !combined_gradient || num_params == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mtl_process_gradients: required parameter is NULL (ctx, gradients, combined_gradient)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    uint32_t num_tasks = ctx->num_active;

    /* Guard: prevent division by zero when no tasks are active */
    if (num_tasks == 0) {
        memset(combined_gradient, 0, num_params * sizeof(float));
        nimcp_mutex_unlock(ctx->mutex);
        return 0;
    }

    /* Initialize combined gradient to zero */
    memset(combined_gradient, 0, num_params * sizeof(float));

    switch (ctx->config.gradient_method) {
        case MTL_GRAD_NONE: {
            /* Simple average */
            for (uint32_t t = 0; t < num_tasks; t++) {
                float weight = 1.0f / (float)num_tasks;
                for (size_t i = 0; i < num_params; i++) {
                    combined_gradient[i] += weight * gradients[t][i];
                }
            }
            break;
        }

        case MTL_GRAD_PCGRAD: {
            /* PCGrad: Project conflicting gradients */
            float** projected = nimcp_calloc(num_tasks, sizeof(float*));
            if (!projected) {
                nimcp_mutex_unlock(ctx->mutex);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mtl_process_gradients: projected allocation failed");
                return -1;
            }
            bool pcgrad_alloc_failed = false;
            for (uint32_t t = 0; t < num_tasks; t++) {
                projected[t] = nimcp_calloc(num_params, sizeof(float));
                if (!projected[t]) {
                    pcgrad_alloc_failed = true;
                    break;
                }
                memcpy(projected[t], gradients[t], num_params * sizeof(float));
            }
            if (pcgrad_alloc_failed) {
                for (uint32_t t = 0; t < num_tasks; t++) {
                    if (projected[t]) nimcp_free(projected[t]);
                }
                nimcp_free(projected);
                nimcp_mutex_unlock(ctx->mutex);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mtl_process_gradients: projected[t] allocation failed");
                return -1;
            }

            /* Project each gradient against others */
            for (uint32_t i = 0; i < num_tasks; i++) {
                for (uint32_t j = 0; j < num_tasks; j++) {
                    if (i == j) continue;

                    float sim = mtl_gradient_similarity(projected[i], gradients[j], num_params);
                    if (sim < 0) {
                        /* Conflicting: project out */
                        project_gradient(projected[i], gradients[j], num_params);
                        ctx->stats.conflict_ratio += 1.0f;
                    }
                }
            }

            /* Average projected gradients */
            for (uint32_t t = 0; t < num_tasks; t++) {
                for (size_t i = 0; i < num_params; i++) {
                    combined_gradient[i] += projected[t][i] / (float)num_tasks;
                }
                nimcp_free(projected[t]);
            }
            nimcp_free(projected);

            if (num_tasks > 1) {
                ctx->stats.conflict_ratio /= (float)(num_tasks * (num_tasks - 1));
            }
            break;
        }

        case MTL_GRAD_CAGRAD: {
            /* CAGrad: Conflict-averse gradient descent */
            mtl_cagrad_combine(ctx, gradients, num_tasks, num_params, combined_gradient);
            break;
        }

        case MTL_GRAD_GRADDROP: {
            /* GradDrop: Randomly drop conflicting components */
            for (size_t i = 0; i < num_params; i++) {
                float sum = 0.0f;
                int count = 0;

                /* Compute sign consistency */
                int positive = 0, negative = 0;
                for (uint32_t t = 0; t < num_tasks; t++) {
                    if (gradients[t][i] > 0) positive++;
                    else if (gradients[t][i] < 0) negative++;
                }

                /* Keep gradient component with probability based on consistency */
                float consistency = (float)(positive > negative ? positive : negative) / (float)num_tasks;
                float r = (float)rand_r(&ctx->rng_seed) / (float)RAND_MAX;

                if (r < consistency) {
                    for (uint32_t t = 0; t < num_tasks; t++) {
                        sum += gradients[t][i];
                        count++;
                    }
                    combined_gradient[i] = count > 0 ? sum / (float)count : 0.0f;
                }
            }
            break;
        }

        case MTL_GRAD_VACCINE: {
            /* Vaccine: Mask conflicting gradient dimensions */
            for (size_t i = 0; i < num_params; i++) {
                float sum = 0.0f;
                for (uint32_t t = 0; t < num_tasks; t++) {
                    sum += gradients[t][i];
                }
                float mean = sum / (float)num_tasks;

                /* Only keep if all tasks agree on direction */
                bool all_agree = true;
                for (uint32_t t = 0; t < num_tasks; t++) {
                    if ((gradients[t][i] > 0) != (mean > 0)) {
                        all_agree = false;
                        break;
                    }
                }

                combined_gradient[i] = all_agree ? mean : 0.0f;
            }
            break;
        }

        default:
            /* Simple average */
            for (uint32_t t = 0; t < num_tasks; t++) {
                for (size_t i = 0; i < num_params; i++) {
                    combined_gradient[i] += gradients[t][i] / (float)num_tasks;
                }
            }
            break;
    }

    /* Update gradient statistics */
    float total_sim = 0.0f;
    int sim_count = 0;
    for (uint32_t i = 0; i < num_tasks; i++) {
        ctx->stats.task_grad_norms[i] = vector_norm(gradients[i], num_params);
        for (uint32_t j = i + 1; j < num_tasks; j++) {
            total_sim += mtl_gradient_similarity(gradients[i], gradients[j], num_params);
            sim_count++;
        }
    }
    ctx->stats.gradient_similarity = sim_count > 0 ? total_sim / (float)sim_count : 0.0f;

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

int mtl_update_weights(
    mtl_ctx_t* ctx,
    const float* task_losses,
    const float* task_grad_norms
) {
    if (!ctx || !task_losses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mtl_update_weights: required parameter is NULL (ctx, task_losses)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    switch (ctx->config.weighting) {
        case MTL_WEIGHT_UNIFORM:
            /* Uniform: no update needed */
            break;

        case MTL_WEIGHT_UNCERTAINTY: {
            /* Update log variances via gradient descent */
            /* Gradient: 0.5 * (1 - loss / exp(log_var)) */
            for (uint32_t i = 0; i < ctx->num_tasks; i++) {
                float log_var = ctx->uncertainty.log_vars[i];
                float precision = expf(-log_var);
                float grad = 0.5f * (1.0f - task_losses[i] * precision);

                /* SGD update on log_var */
                ctx->uncertainty.log_vars[i] -= 0.01f * grad;

                /* Clamp log_var */
                if (ctx->uncertainty.log_vars[i] < -10.0f) {
                    ctx->uncertainty.log_vars[i] = -10.0f;
                }
                if (ctx->uncertainty.log_vars[i] > 10.0f) {
                    ctx->uncertainty.log_vars[i] = 10.0f;
                }
            }
            break;
        }

        case MTL_WEIGHT_GRADNORM: {
            if (!task_grad_norms) {
                nimcp_mutex_unlock(ctx->mutex);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mtl_update_weights: task_grad_norms is NULL");
                return -1;
            }

            /* GradNorm: balance gradient magnitudes */
            if (!ctx->gradnorm.initialized) {
                /* Store initial losses */
                memcpy(ctx->gradnorm.initial_losses, task_losses,
                       ctx->num_tasks * sizeof(float));
                ctx->gradnorm.initialized = true;
            }

            /* Compute loss ratios */
            float avg_ratio = 0.0f;
            for (uint32_t i = 0; i < ctx->num_tasks; i++) {
                ctx->gradnorm.loss_ratios[i] = task_losses[i] /
                    (ctx->gradnorm.initial_losses[i] + MTL_EPSILON);
                avg_ratio += ctx->gradnorm.loss_ratios[i];
            }
            if (ctx->num_tasks > 0) {
                avg_ratio /= (float)ctx->num_tasks;
            }
            ctx->gradnorm.avg_loss_ratio = avg_ratio;

            /* Compute relative inverse training rates */
            float* r_i = nimcp_calloc(ctx->num_tasks, sizeof(float));
            /* P2-MTL-1: NULL check on allocation */
            if (!r_i) {
                nimcp_mutex_unlock(ctx->mutex);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mtl_update_weights: r_i allocation failed");
                return -1;
            }
            for (uint32_t i = 0; i < ctx->num_tasks; i++) {
                r_i[i] = powf(ctx->gradnorm.loss_ratios[i] / (avg_ratio + MTL_EPSILON),
                              ctx->config.gradnorm.alpha);
            }

            /* Compute target gradient norms */
            float avg_gnorm = 0.0f;
            for (uint32_t i = 0; i < ctx->num_tasks; i++) {
                avg_gnorm += task_grad_norms[i] * ctx->gradnorm.weights[i];
            }
            if (ctx->num_tasks > 0) {
                avg_gnorm /= (float)ctx->num_tasks;
            }

            /* Update weights to match target gradient norms with momentum damping */
            float momentum_coef = ctx->gradnorm.damping_factor;
            for (uint32_t i = 0; i < ctx->num_tasks; i++) {
                float target = avg_gnorm * r_i[i];
                float current = task_grad_norms[i] * ctx->gradnorm.weights[i];

                /* Gradient of L1 loss between current and target */
                float raw_grad = (current - target > 0 ? 1.0f : -1.0f) * task_grad_norms[i];

                /* Apply momentum for oscillation damping:
                 * momentum = momentum_coef * prev_momentum + (1 - momentum_coef) * grad
                 * This smooths out rapid oscillations in weight updates */
                ctx->gradnorm.weight_momentum[i] = momentum_coef * ctx->gradnorm.weight_momentum[i]
                                                   + (1.0f - momentum_coef) * raw_grad;

                /* Update weight using momentum-smoothed gradient */
                ctx->gradnorm.weights[i] -= ctx->config.gradnorm.weight_lr * ctx->gradnorm.weight_momentum[i];

                /* Ensure positive weights */
                if (ctx->gradnorm.weights[i] < 0.01f) {
                    ctx->gradnorm.weights[i] = 0.01f;
                }
            }

            /* Normalize weights to sum to num_tasks */
            float sum = 0.0f;
            for (uint32_t i = 0; i < ctx->num_tasks; i++) {
                sum += ctx->gradnorm.weights[i];
            }
            for (uint32_t i = 0; i < ctx->num_tasks; i++) {
                ctx->gradnorm.weights[i] *= (float)ctx->num_tasks / sum;
                ctx->tasks[i].current_weight = ctx->gradnorm.weights[i];
            }

            nimcp_free(r_i);
            break;
        }

        case MTL_WEIGHT_DWA: {
            /* Dynamic Weight Average */
            /* w_i(t) = K * exp(r_i(t-1) / T) / sum(exp(r_j(t-1) / T)) */
            /* r_i(t) = L_i(t-1) / L_i(t-2) */

            float* softmax_weights = nimcp_calloc(ctx->num_tasks, sizeof(float));
            /* P2-MTL-2: NULL check on allocation */
            if (!softmax_weights) {
                nimcp_mutex_unlock(ctx->mutex);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mtl_update_weights: softmax_weights allocation failed");
                return -1;
            }
            float max_rate = -1e9f;

            for (uint32_t i = 0; i < ctx->num_tasks; i++) {
                task_state_t* task = &ctx->tasks[i];
                uint32_t curr_idx = (task->history_idx - 1 + 32) % 32;
                uint32_t prev_idx = (task->history_idx - 2 + 32) % 32;

                float rate = task->loss_history[prev_idx] > MTL_EPSILON ?
                    task->loss_history[curr_idx] / task->loss_history[prev_idx] : 1.0f;

                softmax_weights[i] = rate / ctx->config.temperature;
                if (softmax_weights[i] > max_rate) {
                    max_rate = softmax_weights[i];
                }
            }

            /* Softmax */
            float sum_exp = 0.0f;
            for (uint32_t i = 0; i < ctx->num_tasks; i++) {
                softmax_weights[i] = expf(softmax_weights[i] - max_rate);
                sum_exp += softmax_weights[i];
            }

            for (uint32_t i = 0; i < ctx->num_tasks; i++) {
                ctx->tasks[i].current_weight =
                    (float)ctx->num_tasks * softmax_weights[i] / sum_exp;
            }

            nimcp_free(softmax_weights);
            break;
        }

        case MTL_WEIGHT_MGDA: {
            /* Multiple Gradient Descent Algorithm (MGDA)
             * Find Pareto-optimal weights via min-norm solver
             * Reference: Sener & Koltun 2018 "Multi-Task Learning as Multi-Objective Optimization"
             *
             * Solves: min_alpha ||sum_i alpha_i * g_i||^2
             * Subject to: alpha_i >= 0, sum_i alpha_i = 1
             *
             * For 2 tasks, closed-form solution exists
             * For n tasks, use Frank-Wolfe algorithm
             */
            if (!task_grad_norms) {
                nimcp_mutex_unlock(ctx->mutex);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mtl_update_weights: task_grad_norms is NULL");
                return -1;
            }

            uint32_t n = ctx->num_tasks;

            if (n == 2) {
                /* Two-task closed-form solution */
                /* alpha_1 = (g2^T(g2-g1)) / ||g2-g1||^2 */
                /* For simplified case using norms only: */
                float g1 = task_grad_norms[0];
                float g2 = task_grad_norms[1];

                /* Compute approximate alpha based on gradient norms */
                float diff = g2 - g1;
                float alpha1 = 0.5f;
                if (fabsf(diff) > MTL_EPSILON) {
                    alpha1 = g2 / (g1 + g2 + MTL_EPSILON);
                }

                /* Clamp to valid range */
                if (alpha1 < 0.0f) alpha1 = 0.0f;
                if (alpha1 > 1.0f) alpha1 = 1.0f;

                ctx->tasks[0].current_weight = alpha1 * 2.0f;
                ctx->tasks[1].current_weight = (1.0f - alpha1) * 2.0f;
            } else {
                /* Frank-Wolfe algorithm for n > 2 tasks */
                float* alphas = nimcp_calloc(n, sizeof(float));
                if (!alphas) {
                    nimcp_mutex_unlock(ctx->mutex);
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mtl_update_weights: alphas is NULL");
                    return -1;
                }

                /* Initialize with uniform weights */
                for (uint32_t i = 0; i < n; i++) {
                    alphas[i] = 1.0f / (float)n;
                }

                /* Frank-Wolfe iterations (limited iterations for efficiency) */
                uint32_t max_iter = 20;
                for (uint32_t iter = 0; iter < max_iter; iter++) {
                    /* Compute gradient of objective: d/d_alpha ||sum alpha_i g_i||^2 */
                    /* Simplified using gradient norms as proxy */
                    float* grad_obj = nimcp_calloc(n, sizeof(float));
                    if (!grad_obj) {
                        nimcp_free(alphas);
                        nimcp_mutex_unlock(ctx->mutex);
                        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: grad_obj is NULL");
                        return -1;
                    }

                    /* Compute weighted sum of squared norms (proxy for ||sum alpha_i g_i||^2) */
                    float weighted_norm = 0.0f;
                    for (uint32_t i = 0; i < n; i++) {
                        weighted_norm += alphas[i] * task_grad_norms[i] * task_grad_norms[i];
                    }

                    /* Gradient: 2 * g_i * ||sum alpha_j g_j|| (simplified) */
                    for (uint32_t i = 0; i < n; i++) {
                        grad_obj[i] = 2.0f * task_grad_norms[i] * sqrtf(weighted_norm + MTL_EPSILON);
                    }

                    /* Find min gradient index (FW direction) */
                    uint32_t min_idx = 0;
                    float min_grad = grad_obj[0];
                    for (uint32_t i = 1; i < n; i++) {
                        if (grad_obj[i] < min_grad) {
                            min_grad = grad_obj[i];
                            min_idx = i;
                        }
                    }

                    /* Step size: 2/(iter+2) for convergence guarantee */
                    float gamma = 2.0f / (float)(iter + 2);

                    /* Update: alpha = (1-gamma)*alpha + gamma*e_min_idx */
                    for (uint32_t i = 0; i < n; i++) {
                        alphas[i] = (1.0f - gamma) * alphas[i];
                    }
                    alphas[min_idx] += gamma;

                    nimcp_free(grad_obj);

                    /* Check for convergence (small change) */
                    if (gamma < 0.01f) break;
                }

                /* Apply weights (scale to sum to num_tasks) */
                for (uint32_t i = 0; i < n; i++) {
                    ctx->tasks[i].current_weight = alphas[i] * (float)n;
                }

                nimcp_free(alphas);
            }
            break;
        }

        default:
            break;
    }

    /* Update statistics */
    for (uint32_t i = 0; i < ctx->num_tasks; i++) {
        ctx->stats.task_weights[i] = ctx->tasks[i].current_weight;
    }
    ctx->stats.weight_variance = compute_variance(ctx->stats.task_weights, ctx->num_tasks);

    /* Compute entropy */
    float* normalized = nimcp_calloc(ctx->num_tasks, sizeof(float));
    /* P2-MTL-5: NULL check on allocation */
    if (!normalized) {
        nimcp_mutex_unlock(ctx->mutex);
        return 0;
    }
    float sum = 0.0f;
    for (uint32_t i = 0; i < ctx->num_tasks; i++) {
        sum += ctx->stats.task_weights[i];
    }
    for (uint32_t i = 0; i < ctx->num_tasks; i++) {
        normalized[i] = ctx->stats.task_weights[i] / sum;
    }
    ctx->stats.weight_entropy = compute_entropy(normalized, ctx->num_tasks);
    nimcp_free(normalized);

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

uint32_t mtl_sample_task(mtl_ctx_t* ctx) {
    if (!ctx || ctx->num_active == 0) {
        return 0;
    }

    nimcp_mutex_lock(ctx->mutex);

    uint32_t task_id = 0;

    switch (ctx->config.sampling) {
        case MTL_SAMPLE_UNIFORM: {
            uint32_t idx = rand_r(&ctx->rng_seed) % ctx->num_active;
            task_id = ctx->active_task_ids[idx];
            break;
        }

        case MTL_SAMPLE_ROUND_ROBIN: {
            task_id = ctx->active_task_ids[ctx->round_robin_idx % ctx->num_active];
            ctx->round_robin_idx++;
            break;
        }

        case MTL_SAMPLE_TEMPERATURE: {
            /* Temperature-based sampling */
            float* probs = nimcp_calloc(ctx->num_active, sizeof(float));
            /* P2-MTL-3: NULL check on allocation */
            if (!probs) {
                nimcp_mutex_unlock(ctx->mutex);
                return 0;
            }
            float sum = 0.0f;

            for (uint32_t i = 0; i < ctx->num_active; i++) {
                probs[i] = expf(ctx->tasks[i].current_weight / ctx->config.temperature);
                sum += probs[i];
            }

            float r = (float)rand_r(&ctx->rng_seed) / (float)RAND_MAX * sum;
            float cumsum = 0.0f;

            for (uint32_t i = 0; i < ctx->num_active; i++) {
                cumsum += probs[i];
                if (r <= cumsum) {
                    task_id = ctx->active_task_ids[i];
                    break;
                }
            }

            nimcp_free(probs);
            break;
        }

        default: {
            uint32_t idx = rand_r(&ctx->rng_seed) % ctx->num_active;
            task_id = ctx->active_task_ids[idx];
            break;
        }
    }

    nimcp_mutex_unlock(ctx->mutex);
    return task_id;
}

uint32_t mtl_sample_tasks(
    mtl_ctx_t* ctx,
    uint32_t num_tasks,
    uint32_t* task_ids
) {
    if (!ctx || !task_ids || num_tasks == 0) {
        return 0;
    }

    nimcp_mutex_lock(ctx->mutex);

    uint32_t sampled = 0;

    if (num_tasks >= ctx->num_active) {
        /* Return all active tasks */
        memcpy(task_ids, ctx->active_task_ids, ctx->num_active * sizeof(uint32_t));
        sampled = ctx->num_active;
    } else {
        /* Sample without replacement */
        bool* selected = nimcp_calloc(ctx->num_active, sizeof(bool));
        /* P2-MTL-4: NULL check on allocation */
        if (!selected) {
            nimcp_mutex_unlock(ctx->mutex);
            return 0;
        }

        while (sampled < num_tasks) {
            uint32_t idx = rand_r(&ctx->rng_seed) % ctx->num_active;
            if (!selected[idx]) {
                selected[idx] = true;
                task_ids[sampled++] = ctx->active_task_ids[idx];
            }
        }

        nimcp_free(selected);
    }

    nimcp_mutex_unlock(ctx->mutex);
    return sampled;
}

//=============================================================================
// Gradient Conflict API
//=============================================================================

float mtl_gradient_similarity(
    const float* grad1,
    const float* grad2,
    size_t num_params
) {
    if (!grad1 || !grad2 || num_params == 0) {
        return 0.0f;
    }

    float dot = dot_product(grad1, grad2, num_params);
    float norm1 = vector_norm(grad1, num_params);
    float norm2 = vector_norm(grad2, num_params);

    if (norm1 < MTL_EPSILON || norm2 < MTL_EPSILON) {
        return 0.0f;
    }

    return dot / (norm1 * norm2);
}

bool mtl_gradients_conflict(
    const float* grad1,
    const float* grad2,
    size_t num_params
) {
    return mtl_gradient_similarity(grad1, grad2, num_params) < 0.0f;
}

int mtl_pcgrad_project(
    mtl_ctx_t* ctx,
    float* main_grad,
    float** other_grads,
    uint32_t num_other,
    size_t num_params
) {
    if (!ctx || !main_grad || !other_grads || num_params == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mtl_pcgrad_project: required parameter is NULL (ctx, main_grad, other_grads)");
        return -1;
    }

    /* Random order if configured */
    uint32_t* order = nimcp_calloc(num_other, sizeof(uint32_t));
    for (uint32_t i = 0; i < num_other; i++) {
        order[i] = i;
    }

    if (ctx->config.pcgrad.use_random_order) {
        /* Fisher-Yates shuffle */
        for (uint32_t i = num_other - 1; i > 0; i--) {
            uint32_t j = rand_r(&ctx->rng_seed) % (i + 1);
            uint32_t tmp = order[i];
            order[i] = order[j];
            order[j] = tmp;
        }
    }

    /* Project against each conflicting gradient */
    for (uint32_t k = 0; k < num_other; k++) {
        uint32_t i = order[k];
        float* other = other_grads[i];

        float sim = mtl_gradient_similarity(main_grad, other, num_params);

        if (sim < 0) {
            /* Conflicting: project out the component */
            project_gradient(main_grad, other, num_params);
        }
    }

    nimcp_free(order);
    return 0;
}

int mtl_cagrad_combine(
    mtl_ctx_t* ctx,
    float** gradients,
    uint32_t num_tasks,
    size_t num_params,
    float* combined
) {
    if (!ctx || !gradients || !combined || num_tasks == 0 || num_params == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mtl_cagrad_combine: required parameter is NULL (ctx, gradients, combined)");
        return -1;
    }

    float c = ctx->config.cagrad.c;

    /* Compute average gradient g0 */
    float* g0 = nimcp_calloc(num_params, sizeof(float));
    for (uint32_t t = 0; t < num_tasks; t++) {
        for (size_t i = 0; i < num_params; i++) {
            g0[i] += gradients[t][i];
        }
    }
    for (size_t i = 0; i < num_params; i++) {
        g0[i] /= (float)num_tasks;
    }

    /* Compute g_w = g0 + c * (gw - g0) where gw minimizes max_i <gi, gw> */
    /* Simplified: use g0 as initial, refine based on conflict */
    memcpy(combined, g0, num_params * sizeof(float));

    /* Find worst-case gradient (highest positive inner product with average) */
    float worst_sim = -1e9f;
    uint32_t worst_idx = 0;

    for (uint32_t t = 0; t < num_tasks; t++) {
        float sim = dot_product(gradients[t], g0, num_params);
        if (sim > worst_sim) {
            worst_sim = sim;
            worst_idx = t;
        }
    }

    /* Adjust towards conflict-averse direction */
    for (size_t i = 0; i < num_params; i++) {
        float conflict_dir = gradients[worst_idx][i] - g0[i];
        combined[i] = g0[i] - c * conflict_dir;
    }

    /* Rescale */
    float g0_norm = vector_norm(g0, num_params);
    float combined_norm = vector_norm(combined, num_params);

    if (combined_norm > MTL_EPSILON) {
        float scale = ctx->config.cagrad.rescale * g0_norm / combined_norm;
        for (size_t i = 0; i < num_params; i++) {
            combined[i] *= scale;
        }
    }

    nimcp_free(g0);
    return 0;
}

//=============================================================================
// Integration API
//=============================================================================

int mtl_connect_gradient_manager(
    mtl_ctx_t* ctx,
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

int mtl_connect_brain_factory(mtl_ctx_t* ctx, void* brain_factory) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);
    ctx->brain_factory = brain_factory;
    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

int mtl_connect_thalamic_router(mtl_ctx_t* ctx, void* router) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);
    ctx->thalamic_router = router;
    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

//=============================================================================
// Statistics API
//=============================================================================

int mtl_get_stats(const mtl_ctx_t* ctx, mtl_stats_t* stats) {
    if (!ctx || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mtl_get_stats: required parameter is NULL (ctx, stats)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)ctx->mutex);
    memcpy(stats, &ctx->stats, sizeof(mtl_stats_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)ctx->mutex);
    return 0;
}

void mtl_reset_stats(mtl_ctx_t* ctx) {
    if (!ctx) return;

    nimcp_mutex_lock(ctx->mutex);
    memset(&ctx->stats, 0, sizeof(mtl_stats_t));
    nimcp_mutex_unlock(ctx->mutex);
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* mtl_architecture_name(mtl_architecture_t arch) {
    static const char* names[] = {
        "Hard Sharing",
        "Soft Sharing",
        "Modular",
        "Progressive",
        "Mixture of Experts"
    };
    return arch < MTL_ARCH_COUNT ? names[arch] : "Unknown";
}

const char* mtl_weight_strategy_name(mtl_weight_strategy_t strategy) {
    static const char* names[] = {
        "Uniform",
        "Uncertainty",
        "GradNorm",
        "DWA",
        "MGDA",
        "Nash",
        "IMTL",
        "Random",
        "Learned"
    };
    return strategy < MTL_WEIGHT_COUNT ? names[strategy] : "Unknown";
}

const char* mtl_gradient_method_name(mtl_gradient_method_t method) {
    static const char* names[] = {
        "None",
        "PCGrad",
        "CAGrad",
        "GradDrop",
        "Vaccine"
    };
    return method < MTL_GRAD_COUNT ? names[method] : "Unknown";
}

int mtl_validate_config(const mtl_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    if (config->architecture >= MTL_ARCH_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "mtl_validate_config: capacity exceeded");
        return -1;
    }

    if (config->weighting >= MTL_WEIGHT_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "mtl_validate_config: capacity exceeded");
        return -1;
    }

    if (config->gradient_method >= MTL_GRAD_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "mtl_validate_config: capacity exceeded");
        return -1;
    }

    if (config->sampling >= MTL_SAMPLE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "mtl_validate_config: capacity exceeded");
        return -1;
    }

    if (config->gradnorm.alpha < 0) {
        return -1;
    }

    if (config->temperature <= 0) {
        return -1;
    }

    return 0;
}

void mtl_free_batch(mtl_batch_t* batch) {
    if (!batch) return;

    if (batch->inputs) {
        for (uint32_t i = 0; i < batch->num_active_tasks; i++) {
            nimcp_tensor_destroy(batch->inputs[i]);
        }
        nimcp_free(batch->inputs);
    }

    if (batch->labels) {
        for (uint32_t i = 0; i < batch->num_active_tasks; i++) {
            nimcp_tensor_destroy(batch->labels[i]);
        }
        nimcp_free(batch->labels);
    }

    nimcp_free(batch->batch_sizes);
    nimcp_free(batch->task_ids);
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

static float dot_product(const float* a, const float* b, size_t n) {
    float sum = 0.0f;
    for (size_t i = 0; i < n; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

static float vector_norm(const float* v, size_t n) {
    return sqrtf(dot_product(v, v, n));
}

static void project_gradient(float* grad, const float* other, size_t n) {
    /* Project grad onto plane perpendicular to other */
    /* grad = grad - (grad . other) / ||other||^2 * other */
    float dot = dot_product(grad, other, n);
    float norm_sq = dot_product(other, other, n);

    if (norm_sq < MTL_EPSILON) {
        return;
    }

    float coef = dot / norm_sq;

    for (size_t i = 0; i < n; i++) {
        grad[i] -= coef * other[i];
    }
}

static float compute_entropy(const float* probs, uint32_t n) {
    float entropy = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        if (probs[i] > MTL_EPSILON) {
            entropy -= probs[i] * logf(probs[i]);
        }
    }
    return entropy;
}

static float compute_variance(const float* values, uint32_t n) {
    if (n == 0) return 0.0f;

    float mean = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        mean += values[i];
    }
    mean /= (float)n;

    float var = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float diff = values[i] - mean;
        var += diff * diff;
    }

    return var / (float)n;
}
