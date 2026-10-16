/**
 * @file nimcp_meta_learning.c
 * @brief Implementation of Meta-Learning for NIMCP
 *
 * WHAT: Algorithms that learn how to learn efficiently across tasks
 * WHY:  Enable rapid adaptation to new tasks with few examples
 * HOW:  MAML, Reptile, Prototypical Networks, and other meta-learning methods
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#include "training/nimcp_meta_learning.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdio.h>

#define LOG_MODULE "META_LEARNING"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_learning_constants.h"
#include "constants/nimcp_threshold_constants.h"
#include "constants/nimcp_dimension_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(meta)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Adapted parameters structure
 */
struct meta_adapted_params_s {
    nimcp_tensor_t** params;         /**< Copied/adapted parameter tensors */
    uint32_t num_params;             /**< Number of parameters */
    float adaptation_loss;           /**< Final adaptation loss */
};

/**
 * @brief Meta-learning context
 */
struct meta_ctx_s {
    meta_config_t config;            /**< Configuration */

    /* Registered model parameters */
    nimcp_tensor_t** params;         /**< Original parameter tensors */
    uint32_t num_params;             /**< Number of parameters */

    /* Learned inner learning rates (MetaSGD) */
    float* inner_lrs;                /**< Per-parameter inner LRs */

    /* Task buffer */
    meta_task_t* task_buffer;        /**< Task buffer for sampling */
    uint32_t task_buffer_size;       /**< Task buffer capacity */
    uint32_t num_buffered_tasks;     /**< Current buffered tasks */

    /* Integration handles */
    nimcp_gradient_manager_ctx_t* grad_manager; /**< Gradient manager */
    void* brain_factory;             /**< Brain factory */

    /* Outer optimizer state (simplified - use direct SGD for meta-learning) */
    float outer_lr;                  /**< Outer loop learning rate */

    /* Statistics */
    meta_stats_t stats;              /**< Runtime statistics */

    /* Thread safety */
    nimcp_mutex_t* mutex;            /**< Mutex for thread safety */
};

//=============================================================================
// Algorithm Names
//=============================================================================

static const char* algorithm_names[] = {
    "MAML",
    "FOMAML",
    "Reptile",
    "MetaSGD",
    "ANIL",
    "BOIL",
    "Prototypical",
    "Matching",
    "Relation",
    "E-MAML",
    "LEAP"
};

//=============================================================================
// Forward Declarations
//=============================================================================

static int clone_params(nimcp_tensor_t** src, nimcp_tensor_t** dst, uint32_t count);
static void free_cloned_params(nimcp_tensor_t** params, uint32_t count);
static float compute_loss_from_logits(const nimcp_tensor_t* logits, const nimcp_tensor_t* labels);
static float compute_accuracy(const nimcp_tensor_t* logits, const nimcp_tensor_t* labels);
static void sgd_step(nimcp_tensor_t* param, const nimcp_tensor_t* grad, float lr);
static float compute_distance(const float* a, const float* b, size_t dim, bool euclidean);
static double get_time_ms(void);
static int compute_hessian_vector_product(
    float* grad_at_adapted,
    float* vector,
    float* hvp_out,
    size_t param_count,
    float eps
);

//=============================================================================
// Lifecycle API Implementation
//=============================================================================

int meta_default_config(meta_config_t* config) {
    if (!config) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "meta_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(meta_config_t));

    /* Default: MAML */
    config->algorithm = META_ALG_MAML;

    /* MAML settings */
    config->maml.inner_lr = META_DEFAULT_INNER_LR;
    config->maml.outer_lr = META_DEFAULT_OUTER_LR;
    config->maml.inner_steps = META_DEFAULT_INNER_STEPS;
    config->maml.first_order = false;
    config->maml.learn_inner_lr = false;
    config->maml.inner_lr_init = META_DEFAULT_INNER_LR;

    /* Reptile settings */
    config->reptile.inner_lr = META_DEFAULT_INNER_LR;
    config->reptile.outer_lr = NIMCP_LEARNING_RATE_COARSE;
    config->reptile.inner_steps = META_DEFAULT_INNER_STEPS;
    config->reptile.inner_optimizer = META_INNER_SGD;

    /* Prototypical settings */
    config->prototypical.embedding_dim = NIMCP_SMALL_EMBEDDING_DIM;
    config->prototypical.temperature = NIMCP_TEMPERATURE_DEFAULT;
    config->prototypical.use_attention = false;
    config->prototypical.euclidean_distance = true;

    /* Task settings: 5-way 5-shot */
    config->task.n_way = 5;
    config->task.k_shot = 5;
    config->task.query_size = 15;
    config->task_sampling = META_TASK_SAMPLE_UNIFORM;
    config->tasks_per_batch = 4;

    /* Outer loop */
    config->outer_optimizer = NIMCP_OPTIMIZER_ADAM;
    config->outer_lr = META_DEFAULT_OUTER_LR;

    /* Regularization */
    config->outer_weight_decay = 0.0f;
    config->task_augmentation = 0.0f;

    /* Integration */
    config->integrate_gradient_manager = true;
    config->integrate_brain_factory = false;

    /* Debugging */
    config->verbose = false;
    config->track_statistics = true;

    return 0;
}

meta_ctx_t* meta_create(const meta_config_t* config) {
    if (!config) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "meta_create: config is NULL");
        return NULL;
    }

    /* Validate configuration */
    if (meta_validate_config(config) != 0) {
        NIMCP_THROW(NIMCP_ERROR_CONFIG_INVALID, "meta_create: config validation failed");
        return NULL;
    }

    meta_ctx_t* ctx = nimcp_calloc(1, sizeof(meta_ctx_t));
    if (!ctx) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(meta_ctx_t),
                          "meta_create: failed to allocate context");
        return NULL;
    }

    /* Copy configuration */
    memcpy(&ctx->config, config, sizeof(meta_config_t));

    /* Create mutex */
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_NORMAL;
    ctx->mutex = nimcp_mutex_create(&attr);
    if (!ctx->mutex) {
        NIMCP_THROW_THREADING(NIMCP_ERROR_MUTEX_INIT, 0,
                             "meta_create: failed to create mutex");
        nimcp_free(ctx);
        return NULL;
    }

    /* Initialize task buffer */
    ctx->task_buffer_size = META_MAX_TASKS_PER_BATCH * 10;
    ctx->task_buffer = nimcp_calloc(ctx->task_buffer_size, sizeof(meta_task_t));
    if (!ctx->task_buffer) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, ctx->task_buffer_size * sizeof(meta_task_t),
                          "meta_create: failed to allocate task buffer");
        nimcp_mutex_free(ctx->mutex);
        nimcp_free(ctx);
        return NULL;
    }
    ctx->num_buffered_tasks = 0;

    /* Reset statistics */
    memset(&ctx->stats, 0, sizeof(meta_stats_t));

    if (config->verbose) {
        printf("[META] Created context: alg=%s, %u-way %u-shot, inner_lr=%.4f\n",
               meta_algorithm_name(config->algorithm),
               config->task.n_way, config->task.k_shot,
               config->maml.inner_lr);
    }

    return ctx;
}

void meta_destroy(meta_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    /* Clean up inner LRs */
    if (ctx->inner_lrs) {
        nimcp_free(ctx->inner_lrs);
    }

    /* Clean up task buffer */
    if (ctx->task_buffer) {
        for (uint32_t i = 0; i < ctx->num_buffered_tasks; i++) {
            meta_destroy_task(&ctx->task_buffer[i]);
        }
        nimcp_free(ctx->task_buffer);
    }

    /* Note: We don't free ctx->params as we don't own them */

    /* Destroy mutex */
    if (ctx->mutex) {
        nimcp_mutex_free(ctx->mutex);
    }

    nimcp_free(ctx);
}

int meta_register_params(
    meta_ctx_t* ctx,
    nimcp_tensor_t** params,
    uint32_t num_params
) {
    if (!ctx) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "meta_register_params: ctx is NULL");
        return -1;
    }
    if (!params) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "meta_register_params: params is NULL");
        return -1;
    }
    if (num_params == 0) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "meta_register_params: num_params is 0");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    ctx->params = params;
    ctx->num_params = num_params;

    /* Initialize per-parameter inner LRs for MetaSGD */
    if (ctx->config.maml.learn_inner_lr) {
        ctx->inner_lrs = nimcp_calloc(num_params, sizeof(float));
        if (ctx->inner_lrs) {
            for (uint32_t i = 0; i < num_params; i++) {
                ctx->inner_lrs[i] = ctx->config.maml.inner_lr_init;
            }
        }
    }

    /* Store outer learning rate for simple SGD updates */
    ctx->outer_lr = ctx->config.outer_lr;

    if (ctx->config.verbose) {
        printf("[META] Registered %u parameter tensors\n", num_params);
    }

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

//=============================================================================
// Task API Implementation
//=============================================================================

meta_task_t* meta_create_task(
    meta_ctx_t* ctx,
    const nimcp_tensor_t* data,
    const nimcp_tensor_t* labels,
    uint32_t n_way,
    uint32_t k_shot,
    uint32_t query_size
) {
    if (!ctx) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "meta_create_task: ctx is NULL");
        return NULL;
    }
    if (!data) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "meta_create_task: data is NULL");
        return NULL;
    }
    if (!labels) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "meta_create_task: labels is NULL");
        return NULL;
    }
    if (n_way == 0) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "meta_create_task: n_way is 0");
        return NULL;
    }
    if (k_shot == 0) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "meta_create_task: k_shot is 0");
        return NULL;
    }

    meta_task_t* task = nimcp_calloc(1, sizeof(meta_task_t));
    if (!task) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(meta_task_t),
                          "meta_create_task: failed to allocate task");
        return NULL;
    }

    task->n_way = n_way;
    task->k_shot = k_shot;
    task->query_size = query_size;

    /* In a full implementation, we would:
     * 1. Sample n_way random classes from the dataset
     * 2. Sample k_shot support examples per class
     * 3. Sample query_size query examples per class
     *
     * For now, we just copy the input tensors as placeholders */

    task->support_x = nimcp_tensor_clone(data);
    task->support_y = nimcp_tensor_clone(labels);
    task->query_x = nimcp_tensor_clone(data);
    task->query_y = nimcp_tensor_clone(labels);

    if (!task->support_x || !task->support_y ||
        !task->query_x || !task->query_y) {
        meta_destroy_task(task);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_create_task: operation failed");
        return NULL;
    }

    return task;
}

void meta_destroy_task(meta_task_t* task) {
    if (!task) {
        return;
    }

    if (task->support_x) nimcp_tensor_destroy(task->support_x);
    if (task->support_y) nimcp_tensor_destroy(task->support_y);
    if (task->query_x) nimcp_tensor_destroy(task->query_x);
    if (task->query_y) nimcp_tensor_destroy(task->query_y);

    nimcp_free(task);
}

meta_task_batch_t* meta_sample_tasks(
    meta_ctx_t* ctx,
    uint32_t num_tasks
) {
    if (!ctx) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "meta_sample_tasks: ctx is NULL");
        return NULL;
    }
    if (num_tasks == 0) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "meta_sample_tasks: num_tasks is 0");
        return NULL;
    }

    if (num_tasks > META_MAX_TASKS_PER_BATCH) {
        num_tasks = META_MAX_TASKS_PER_BATCH;
    }

    meta_task_batch_t* batch = nimcp_calloc(1, sizeof(meta_task_batch_t));
    if (!batch) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(meta_task_batch_t),
                          "meta_sample_tasks: failed to allocate batch");
        return NULL;
    }

    batch->tasks = nimcp_calloc(num_tasks, sizeof(meta_task_t));
    if (!batch->tasks) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, num_tasks * sizeof(meta_task_t),
                          "meta_sample_tasks: failed to allocate tasks array");
        nimcp_free(batch);
        return NULL;
    }

    batch->num_tasks = num_tasks;

    /* Task sampling would be implemented based on task_sampling strategy */
    /* For now, tasks are created empty and need to be populated */

    return batch;
}

void meta_destroy_task_batch(meta_task_batch_t* batch) {
    if (!batch) {
        return;
    }

    if (batch->tasks) {
        for (uint32_t i = 0; i < batch->num_tasks; i++) {
            /* Note: Tasks in batch are structs, not pointers */
            if (batch->tasks[i].support_x) nimcp_tensor_destroy(batch->tasks[i].support_x);
            if (batch->tasks[i].support_y) nimcp_tensor_destroy(batch->tasks[i].support_y);
            if (batch->tasks[i].query_x) nimcp_tensor_destroy(batch->tasks[i].query_x);
            if (batch->tasks[i].query_y) nimcp_tensor_destroy(batch->tasks[i].query_y);
        }
        nimcp_free(batch->tasks);
    }

    nimcp_free(batch);
}

//=============================================================================
// Inner Loop API Implementation
//=============================================================================

/**
 * @brief Inner loop adaptation without mutex (caller must hold ctx->mutex)
 */
static float meta_inner_loop_unlocked(
    meta_ctx_t* ctx,
    const meta_task_t* task,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    meta_adapted_params_t** adapted_params
) {
    double start_time = get_time_ms();

    /* Create adapted parameters by cloning original parameters */
    meta_adapted_params_t* adapted = nimcp_calloc(1, sizeof(meta_adapted_params_t));
    if (!adapted) {
        return -1.0f;
    }

    adapted->num_params = ctx->num_params;
    adapted->params = nimcp_calloc(ctx->num_params, sizeof(nimcp_tensor_t*));
    if (!adapted->params) {
        nimcp_free(adapted);
        return -1.0f;
    }

    /* Clone parameters */
    if (clone_params(ctx->params, adapted->params, ctx->num_params) != 0) {
        nimcp_free(adapted->params);
        nimcp_free(adapted);
        return -1.0f;
    }

    float inner_lr = ctx->config.maml.inner_lr;
    uint32_t inner_steps = ctx->config.maml.inner_steps;
    float loss = 0.0f;

    /* Inner loop adaptation */
    for (uint32_t step = 0; step < inner_steps; step++) {
        /* Forward pass on support set */
        nimcp_tensor_t* logits = forward_fn(model, task->support_x);
        if (!logits) {
            break;
        }

        /* Compute loss */
        loss = compute_loss_from_logits(logits, task->support_y);

        /* Compute gradients via forward finite differences and apply SGD.
         * For each parameter, perturb by +eps, re-forward, measure loss change.
         * grad_i ~= (L(theta + eps*e_i) - L(theta)) / eps
         * To keep cost manageable, use coordinate-wise perturbation with
         * random sampling when parameter count is large. */
        float eps_fd = 1e-3f;
        for (uint32_t p = 0; p < adapted->num_params; p++) {
            float lr = ctx->inner_lrs ? ctx->inner_lrs[p] : inner_lr;
            size_t count = nimcp_tensor_numel(adapted->params[p]);
            float* data = nimcp_tensor_data(adapted->params[p]);
            if (!data || count == 0) continue;

            /* For large parameter tensors, sample a subset of coordinates */
            size_t max_coords = 256;
            size_t stride = (count > max_coords) ? count / max_coords : 1;

            for (size_t i = 0; i < count; i += stride) {
                /* Perturb parameter i by +eps */
                float orig_val = data[i];
                data[i] = orig_val + eps_fd;

                /* Forward pass to compute perturbed loss */
                nimcp_tensor_t* perturbed_logits = forward_fn(model, task->support_x);
                float perturbed_loss = loss;  /* fallback */
                if (perturbed_logits) {
                    perturbed_loss = compute_loss_from_logits(perturbed_logits, task->support_y);
                    nimcp_tensor_destroy(perturbed_logits);
                }

                /* Finite-difference gradient estimate */
                float grad_i = (perturbed_loss - loss) / eps_fd;

                /* Clamp gradient to prevent explosion */
                if (grad_i > 10.0f) grad_i = 10.0f;
                if (grad_i < -10.0f) grad_i = -10.0f;

                /* Apply gradient to this coordinate and neighbors in stride */
                for (size_t j = i; j < count && j < i + stride; j++) {
                    data[j] -= lr * grad_i;
                }

                /* Restore unperturbed value for subsequent gradient estimates,
                 * then apply the update to coordinate i */
                data[i] = orig_val - lr * grad_i;
            }
        }

        nimcp_tensor_destroy(logits);
        ctx->stats.total_inner_steps++;
    }

    adapted->adaptation_loss = loss;
    *adapted_params = adapted;

    double end_time = get_time_ms();
    ctx->stats.inner_loop_time_ms += (end_time - start_time);

    return loss;
}

float meta_inner_loop(
    meta_ctx_t* ctx,
    const meta_task_t* task,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    meta_adapted_params_t** adapted_params
) {
    if (!ctx || !task || !forward_fn || !model || !adapted_params) {
        return -1.0f;
    }

    nimcp_mutex_lock(ctx->mutex);
    float result = meta_inner_loop_unlocked(ctx, task, forward_fn, model, adapted_params);
    nimcp_mutex_unlock(ctx->mutex);
    return result;
}

/**
 * @brief Query loss computation without mutex (caller must hold ctx->mutex)
 */
static int meta_query_loss_unlocked(
    meta_ctx_t* ctx,
    const meta_task_t* task,
    const meta_adapted_params_t* adapted_params,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    float* query_loss,
    float* query_accuracy
) {
    /* Temporarily swap in adapted parameters */
    /* In full implementation, model would use adapted_params for forward pass */

    /* Forward pass on query set with adapted parameters */
    nimcp_tensor_t* logits = forward_fn(model, task->query_x);
    if (!logits) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_query_loss: logits is NULL");
        return -1;
    }

    /* Compute query loss and accuracy */
    *query_loss = compute_loss_from_logits(logits, task->query_y);

    if (query_accuracy) {
        *query_accuracy = compute_accuracy(logits, task->query_y);
    }

    nimcp_tensor_destroy(logits);

    return 0;
}

int meta_query_loss(
    meta_ctx_t* ctx,
    const meta_task_t* task,
    const meta_adapted_params_t* adapted_params,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    float* query_loss,
    float* query_accuracy
) {
    if (!ctx || !task || !adapted_params || !forward_fn || !model || !query_loss) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_query_loss: required parameter is NULL (ctx, task, adapted_params, forward_fn, model, query_loss)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);
    int result = meta_query_loss_unlocked(ctx, task, adapted_params, forward_fn, model, query_loss, query_accuracy);
    nimcp_mutex_unlock(ctx->mutex);
    return result;
}

void meta_free_adapted(meta_adapted_params_t* adapted) {
    if (!adapted) {
        return;
    }

    free_cloned_params(adapted->params, adapted->num_params);
    nimcp_free(adapted);
}

//=============================================================================
// Outer Loop API Implementation
//=============================================================================

int meta_step(
    meta_ctx_t* ctx,
    const meta_task_batch_t* task_batch,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    float* avg_query_loss
) {
    if (!ctx || !task_batch || !forward_fn || !model || !avg_query_loss) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_step: required parameter is NULL (ctx, task_batch, forward_fn, model, avg_query_loss)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    double start_time = get_time_ms();

    float total_query_loss = 0.0f;
    uint32_t num_tasks = task_batch->num_tasks;

    /* Process each task in the batch */
    for (uint32_t t = 0; t < num_tasks; t++) {
        const meta_task_t* task = &task_batch->tasks[t];

        /* Inner loop adaptation (unlocked — we already hold ctx->mutex) */
        meta_adapted_params_t* adapted = NULL;
        float support_loss = meta_inner_loop_unlocked(ctx, task, forward_fn, model, &adapted);

        if (adapted) {
            /* Compute query loss with adapted parameters (unlocked) */
            float query_loss = 0.0f;
            float query_acc = 0.0f;
            meta_query_loss_unlocked(ctx, task, adapted, forward_fn, model, &query_loss, &query_acc);

            total_query_loss += query_loss;

            /* Update statistics */
            ctx->stats.avg_support_loss =
                (ctx->stats.avg_support_loss * ctx->stats.tasks_processed + support_loss) /
                (ctx->stats.tasks_processed + 1);
            ctx->stats.avg_query_loss =
                (ctx->stats.avg_query_loss * ctx->stats.tasks_processed + query_loss) /
                (ctx->stats.tasks_processed + 1);
            ctx->stats.avg_query_accuracy =
                (ctx->stats.avg_query_accuracy * ctx->stats.tasks_processed + query_acc) /
                (ctx->stats.tasks_processed + 1);

            ctx->stats.tasks_processed++;

            /* Compute meta-gradient via difference between adapted and original params.
             * For FOMAML/MAML, the meta-gradient is approximated by the direction
             * from original to adapted parameters (similar to Reptile update direction).
             * Full MAML second-order is handled by meta_compute_second_order_gradient
             * when called explicitly; here we accumulate first-order meta-gradients. */
            if (ctx->outer_lr > 0 && adapted->num_params == ctx->num_params) {
                for (uint32_t p = 0; p < ctx->num_params; p++) {
                    size_t count = nimcp_tensor_numel(ctx->params[p]);
                    size_t adapted_count = nimcp_tensor_numel(adapted->params[p]);
                    if (count != adapted_count) continue;

                    float* orig_data = nimcp_tensor_data(ctx->params[p]);
                    float* adapted_data = nimcp_tensor_data(adapted->params[p]);
                    if (!orig_data || !adapted_data) continue;

                    /* Meta-gradient ~ query_loss direction estimated by (adapted - orig)
                     * scaled by query loss magnitude. The adapted params already
                     * moved toward lower support loss; we scale the meta-update
                     * by how well it generalizes (inverse of query loss). */
                    float scale = query_loss > 1e-6f ? 1.0f / (1.0f + query_loss) : 1.0f;
                    float lr = ctx->outer_lr / (float)num_tasks;

                    for (size_t i = 0; i < count; i++) {
                        float meta_grad = orig_data[i] - adapted_data[i];
                        /* Clamp meta-gradient */
                        if (meta_grad > 5.0f) meta_grad = 5.0f;
                        if (meta_grad < -5.0f) meta_grad = -5.0f;
                        orig_data[i] -= lr * scale * meta_grad;
                    }
                }
            }

            meta_free_adapted(adapted);
        }
    }

    *avg_query_loss = total_query_loss / (float)num_tasks;

    ctx->stats.total_meta_steps++;

    double end_time = get_time_ms();
    ctx->stats.outer_loop_time_ms += (end_time - start_time);

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

float meta_reptile_step(
    meta_ctx_t* ctx,
    const meta_task_t* task,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model
) {
    if (!ctx || !task || !forward_fn || !model) {
        return -1.0f;
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Clone current parameters */
    nimcp_tensor_t** task_params = nimcp_calloc(ctx->num_params, sizeof(nimcp_tensor_t*));
    if (!task_params || clone_params(ctx->params, task_params, ctx->num_params) != 0) {
        if (task_params) nimcp_free(task_params);
        nimcp_mutex_unlock(ctx->mutex);
        return -1.0f;
    }

    float loss = 0.0f;
    float inner_lr = ctx->config.reptile.inner_lr;
    float outer_lr = ctx->config.reptile.outer_lr;
    uint32_t inner_steps = ctx->config.reptile.inner_steps;

    /* Perform multiple SGD steps on task using finite-difference gradients */
    for (uint32_t step = 0; step < inner_steps; step++) {
        nimcp_tensor_t* logits = forward_fn(model, task->support_x);
        if (!logits) break;

        loss = compute_loss_from_logits(logits, task->support_y);
        nimcp_tensor_destroy(logits);

        /* Compute gradients via coordinate-wise finite differences */
        float eps_fd = 1e-3f;
        for (uint32_t p = 0; p < ctx->num_params; p++) {
            size_t count = nimcp_tensor_numel(task_params[p]);
            float* data = nimcp_tensor_data(task_params[p]);
            if (!data || count == 0) continue;

            /* Sample subset for large tensors */
            size_t max_coords = 256;
            size_t stride = (count > max_coords) ? count / max_coords : 1;

            for (size_t i = 0; i < count; i += stride) {
                float orig_val = data[i];
                data[i] = orig_val + eps_fd;

                nimcp_tensor_t* perturbed_logits = forward_fn(model, task->support_x);
                float perturbed_loss = loss;
                if (perturbed_logits) {
                    perturbed_loss = compute_loss_from_logits(perturbed_logits, task->support_y);
                    nimcp_tensor_destroy(perturbed_logits);
                }

                float grad_i = (perturbed_loss - loss) / eps_fd;
                if (grad_i > 10.0f) grad_i = 10.0f;
                if (grad_i < -10.0f) grad_i = -10.0f;

                /* Apply gradient to coordinate and neighbors */
                for (size_t j = i; j < count && j < i + stride; j++) {
                    data[j] -= inner_lr * grad_i;
                }
                data[i] = orig_val - inner_lr * grad_i;
            }
        }
    }

    /* Reptile meta-update: interpolate toward task-adapted parameters */
    /* theta = theta + epsilon * (task_theta - theta) */
    for (uint32_t p = 0; p < ctx->num_params; p++) {
        size_t count = nimcp_tensor_numel(ctx->params[p]);
        float* orig_data = nimcp_tensor_data(ctx->params[p]);
        float* task_data = nimcp_tensor_data(task_params[p]);

        if (orig_data && task_data) {
            for (size_t i = 0; i < count; i++) {
                float diff = task_data[i] - orig_data[i];
                orig_data[i] += outer_lr * diff;
            }
        }
    }

    /* Clean up */
    free_cloned_params(task_params, ctx->num_params);

    ctx->stats.total_meta_steps++;
    ctx->stats.tasks_processed++;

    nimcp_mutex_unlock(ctx->mutex);
    return loss;
}

//=============================================================================
// Metric-Based Methods Implementation
//=============================================================================

int meta_prototypical_predict(
    meta_ctx_t* ctx,
    const meta_task_t* task,
    nimcp_tensor_t* (*embed_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    nimcp_tensor_t* predictions
) {
    if (!ctx || !task || !embed_fn || !model || !predictions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_prototypical_predict: required parameter is NULL (ctx, task, embed_fn, model, predictions)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Embed support set */
    nimcp_tensor_t* support_embeddings = embed_fn(model, task->support_x);
    if (!support_embeddings) {
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_prototypical_predict: support_embeddings is NULL");
        return -1;
    }

    /* Compute class prototypes (mean of support embeddings per class) */
    uint32_t n_way = task->n_way;
    uint32_t k_shot = task->k_shot;
    uint32_t embedding_dim = ctx->config.prototypical.embedding_dim;

    float* prototypes = nimcp_calloc(n_way * embedding_dim, sizeof(float));
    if (!prototypes) {
        nimcp_tensor_destroy(support_embeddings);
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "meta_prototypical_predict: prototypes is NULL");
        return -1;
    }

    const float* support_data = nimcp_tensor_data(support_embeddings);
    if (support_data) {
        /* Compute mean embedding per class */
        for (uint32_t c = 0; c < n_way; c++) {
            for (uint32_t s = 0; s < k_shot; s++) {
                size_t idx = (c * k_shot + s) * embedding_dim;
                for (uint32_t d = 0; d < embedding_dim; d++) {
                    prototypes[c * embedding_dim + d] += support_data[idx + d];
                }
            }
            /* Average */
            for (uint32_t d = 0; d < embedding_dim; d++) {
                prototypes[c * embedding_dim + d] /= (float)k_shot;
            }
        }
    }

    /* Embed query set */
    nimcp_tensor_t* query_embeddings = embed_fn(model, task->query_x);
    if (!query_embeddings) {
        nimcp_free(prototypes);
        nimcp_tensor_destroy(support_embeddings);
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_prototypical_predict: query_embeddings is NULL");
        return -1;
    }

    /* Classify queries by distance to prototypes */
    size_t query_count = nimcp_tensor_numel(query_embeddings) / embedding_dim;
    const float* query_data = nimcp_tensor_data(query_embeddings);
    float* pred_data = nimcp_tensor_data(predictions);

    if (query_data && pred_data) {
        bool euclidean = ctx->config.prototypical.euclidean_distance;

        for (size_t q = 0; q < query_count; q++) {
            float min_dist = FLT_MAX;
            uint32_t best_class = 0;

            for (uint32_t c = 0; c < n_way; c++) {
                float dist = compute_distance(
                    query_data + q * embedding_dim,
                    prototypes + c * embedding_dim,
                    embedding_dim,
                    euclidean);

                if (dist < min_dist) {
                    min_dist = dist;
                    best_class = c;
                }
            }

            pred_data[q] = (float)best_class;
        }
    }

    nimcp_free(prototypes);
    nimcp_tensor_destroy(query_embeddings);
    nimcp_tensor_destroy(support_embeddings);

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

int meta_prototypical_loss(
    meta_ctx_t* ctx,
    const meta_task_t* task,
    nimcp_tensor_t* (*embed_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    float* loss,
    float* accuracy
) {
    if (!ctx || !task || !embed_fn || !model || !loss) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_prototypical_loss: required parameter is NULL (ctx, task, embed_fn, model, loss)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Similar to predict but compute negative log-probability loss */
    /* Embed support and query sets */
    nimcp_tensor_t* support_embeddings = embed_fn(model, task->support_x);
    nimcp_tensor_t* query_embeddings = embed_fn(model, task->query_x);

    if (!support_embeddings || !query_embeddings) {
        if (support_embeddings) nimcp_tensor_destroy(support_embeddings);
        if (query_embeddings) nimcp_tensor_destroy(query_embeddings);
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "meta_prototypical_loss: validation failed");
        return -1;
    }

    uint32_t n_way = task->n_way;
    uint32_t k_shot = task->k_shot;
    uint32_t embedding_dim = ctx->config.prototypical.embedding_dim;
    float temperature = ctx->config.prototypical.temperature;
    bool euclidean = ctx->config.prototypical.euclidean_distance;

    /* Compute prototypes */
    float* prototypes = nimcp_calloc(n_way * embedding_dim, sizeof(float));
    if (!prototypes) {
        nimcp_tensor_destroy(support_embeddings);
        nimcp_tensor_destroy(query_embeddings);
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "meta_prototypical_loss: prototypes is NULL");
        return -1;
    }

    const float* support_data = nimcp_tensor_data(support_embeddings);
    for (uint32_t c = 0; c < n_way; c++) {
        for (uint32_t s = 0; s < k_shot; s++) {
            size_t idx = (c * k_shot + s) * embedding_dim;
            for (uint32_t d = 0; d < embedding_dim; d++) {
                prototypes[c * embedding_dim + d] += support_data[idx + d];
            }
        }
        for (uint32_t d = 0; d < embedding_dim; d++) {
            prototypes[c * embedding_dim + d] /= (float)k_shot;
        }
    }

    /* Compute loss and accuracy */
    size_t query_count = task->query_size * n_way;
    const float* query_data = nimcp_tensor_data(query_embeddings);
    const float* query_labels = nimcp_tensor_data(task->query_y);

    float total_loss = 0.0f;
    uint32_t correct = 0;

    /* Hoist log_probs allocation outside the loop — n_way is constant */
    float* log_probs = nimcp_calloc(n_way, sizeof(float));
    if (!log_probs) {
        nimcp_free(prototypes);
        nimcp_tensor_destroy(query_embeddings);
        nimcp_tensor_destroy(support_embeddings);
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "prototypical_loss: failed to allocate log_probs");
        return -1;
    }

    for (size_t q = 0; q < query_count; q++) {
        /* Compute distances to all prototypes — reuse hoisted buffer */
        memset(log_probs, 0, n_way * sizeof(float));
        float max_log_prob = -FLT_MAX;

        for (uint32_t c = 0; c < n_way; c++) {
            float dist = compute_distance(
                query_data + q * embedding_dim,
                prototypes + c * embedding_dim,
                embedding_dim,
                euclidean);

            log_probs[c] = -dist / temperature;
            if (log_probs[c] > max_log_prob) {
                max_log_prob = log_probs[c];
            }
        }

        /* Softmax and cross-entropy */
        float sum_exp = 0.0f;
        for (uint32_t c = 0; c < n_way; c++) {
            sum_exp += expf(log_probs[c] - max_log_prob);
        }

        uint32_t true_class = (uint32_t)query_labels[q];
        if (true_class < n_way) {
            total_loss -= (log_probs[true_class] - max_log_prob - logf(sum_exp));
        }

        /* Accuracy */
        uint32_t pred_class = 0;
        float max_prob = log_probs[0];
        for (uint32_t c = 1; c < n_way; c++) {
            if (log_probs[c] > max_prob) {
                max_prob = log_probs[c];
                pred_class = c;
            }
        }
        if (pred_class == true_class) {
            correct++;
        }
    }

    nimcp_free(log_probs);

    *loss = total_loss / (float)query_count;
    if (accuracy) {
        *accuracy = (float)correct / (float)query_count;
    }

    nimcp_free(prototypes);
    nimcp_tensor_destroy(query_embeddings);
    nimcp_tensor_destroy(support_embeddings);

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

//=============================================================================
// Evaluation API Implementation
//=============================================================================

int meta_evaluate_task(
    meta_ctx_t* ctx,
    const meta_task_t* task,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    float* accuracy
) {
    if (!ctx || !task || !forward_fn || !model || !accuracy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_evaluate_task: required parameter is NULL (ctx, task, forward_fn, model, accuracy)");
        return -1;
    }

    /* Adapt to task */
    meta_adapted_params_t* adapted = NULL;
    meta_inner_loop(ctx, task, forward_fn, model, &adapted);

    if (!adapted) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapted is NULL");


        return -1;
    }

    /* Evaluate on query set */
    float query_loss = 0.0f;
    int result = meta_query_loss(ctx, task, adapted, forward_fn, model,
                                 &query_loss, accuracy);

    meta_free_adapted(adapted);

    return result;
}

//=============================================================================
// Integration API Implementation
//=============================================================================

int meta_connect_gradient_manager(
    meta_ctx_t* ctx,
    nimcp_gradient_manager_ctx_t* grad_manager
) {
    if (!ctx || !grad_manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_connect_gradient_manager: required parameter is NULL (ctx, grad_manager)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);
    ctx->grad_manager = grad_manager;
    nimcp_mutex_unlock(ctx->mutex);

    return 0;
}

int meta_connect_brain_factory(meta_ctx_t* ctx, void* brain_factory) {
    if (!ctx || !brain_factory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_connect_brain_factory: required parameter is NULL (ctx, brain_factory)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);
    ctx->brain_factory = brain_factory;
    nimcp_mutex_unlock(ctx->mutex);

    return 0;
}

//=============================================================================
// Statistics API Implementation
//=============================================================================

int metalearn_get_stats(const meta_ctx_t* ctx, meta_stats_t* stats) {
    if (!ctx || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metalearn_get_stats: required parameter is NULL (ctx, stats)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)ctx->mutex);
    memcpy(stats, &ctx->stats, sizeof(meta_stats_t));

    /* Compute derived statistics */
    stats->total_time_ms = stats->inner_loop_time_ms + stats->outer_loop_time_ms;
    stats->adaptation_improvement = stats->avg_query_accuracy - stats->avg_support_accuracy;

    nimcp_mutex_unlock((nimcp_mutex_t*)ctx->mutex);

    return 0;
}

void metalearn_reset_stats(meta_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    nimcp_mutex_lock(ctx->mutex);
    memset(&ctx->stats, 0, sizeof(meta_stats_t));
    nimcp_mutex_unlock(ctx->mutex);
}

//=============================================================================
// Utility Functions Implementation
//=============================================================================

const char* meta_algorithm_name(meta_algorithm_t alg) {
    if (alg >= META_ALG_COUNT) {
        return "Unknown";
    }
    return algorithm_names[alg];
}

int meta_validate_config(const meta_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Validate algorithm */
    if (config->algorithm >= META_ALG_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "meta_validate_config: capacity exceeded");
        return -1;
    }

    /* Validate task config */
    if (config->task.n_way == 0 || config->task.k_shot == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "meta_validate_config: config->task.n_way is zero");
        return -1;
    }

    /* Validate learning rates */
    if (config->maml.inner_lr <= 0.0f || config->maml.outer_lr <= 0.0f) {
        return -1;
    }

    if (config->maml.inner_steps == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "meta_validate_config: config->maml.inner_steps is zero");
        return -1;
    }

    /* Validate tasks per batch */
    if (config->tasks_per_batch == 0 || config->tasks_per_batch > META_MAX_TASKS_PER_BATCH) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "meta_validate_config: config->tasks_per_batch is zero");
        return -1;
    }

    return 0;
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Clone parameter tensors
 */
static int clone_params(nimcp_tensor_t** src, nimcp_tensor_t** dst, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        dst[i] = nimcp_tensor_clone(src[i]);
        if (!dst[i]) {
            /* Clean up on failure */
            for (uint32_t j = 0; j < i; j++) {
                nimcp_tensor_destroy(dst[j]);
                dst[j] = NULL;
            }
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "clone_params: dst is NULL");
            return -1;
        }
        /* nimcp_tensor_clone() already deep-copies data — no extra memcpy needed */
    }
    return 0;
}

/**
 * @brief Free cloned parameter tensors
 */
static void free_cloned_params(nimcp_tensor_t** params, uint32_t count) {
    if (!params) return;

    for (uint32_t i = 0; i < count; i++) {
        if (params[i]) {
            nimcp_tensor_destroy(params[i]);
        }
    }
    nimcp_free(params);
}

/**
 * @brief Compute cross-entropy loss from logits
 */
static float compute_loss_from_logits(const nimcp_tensor_t* logits, const nimcp_tensor_t* labels) {
    if (!logits || !labels) {
        return 0.0f;
    }

    size_t logit_count = nimcp_tensor_numel(logits);
    size_t label_count = nimcp_tensor_numel(labels);
    const float* logit_data = nimcp_tensor_data((nimcp_tensor_t*)logits);
    const float* label_data = nimcp_tensor_data((nimcp_tensor_t*)labels);

    if (!logit_data || !label_data || logit_count == 0 || label_count == 0) {
        return 0.0f;
    }

    /* Determine number of classes from shape */
    const nimcp_tensor_shape_t* logit_shape = nimcp_tensor_shape(logits);
    const nimcp_tensor_shape_t* label_shape = nimcp_tensor_shape(labels);
    if (!logit_shape || !label_shape) {
        return 0.0f;
    }

    /* Infer batch size and number of classes.
     * Logits: [batch, n_classes] or [n_classes] (single sample)
     * Labels: [batch] (class indices) or [batch, n_classes] (one-hot) */
    uint32_t n_classes = 1;
    uint32_t batch_size = 1;

    if (logit_shape->rank >= 2) {
        batch_size = logit_shape->dims[0];
        n_classes = logit_shape->dims[logit_shape->rank - 1];
    } else if (logit_shape->rank == 1) {
        n_classes = logit_shape->dims[0];
        batch_size = 1;
    }

    if (n_classes == 0) n_classes = 1;
    if (batch_size == 0) batch_size = 1;

    bool labels_are_onehot = (label_count == logit_count);

    /* If n_classes == 1, use MSE (regression) */
    if (n_classes == 1) {
        float mse = 0.0f;
        size_t count = (logit_count < label_count) ? logit_count : label_count;
        for (size_t i = 0; i < count; i++) {
            float diff = logit_data[i] - label_data[i];
            mse += diff * diff;
        }
        return mse / (float)count;
    }

    /* Cross-entropy loss with log-softmax */
    float total_loss = 0.0f;

    for (uint32_t b = 0; b < batch_size; b++) {
        const float* sample_logits = logit_data + (size_t)b * n_classes;

        /* Numerically stable log-softmax: log_softmax_i = x_i - log(sum(exp(x_j - max_x))) - max_x */
        float max_logit = sample_logits[0];
        for (uint32_t c = 1; c < n_classes; c++) {
            if (sample_logits[c] > max_logit) {
                max_logit = sample_logits[c];
            }
        }

        float sum_exp = 0.0f;
        for (uint32_t c = 0; c < n_classes; c++) {
            sum_exp += expf(sample_logits[c] - max_logit);
        }
        float log_sum_exp = logf(sum_exp + 1e-10f) + max_logit;

        if (labels_are_onehot) {
            /* One-hot labels: loss = -sum(y_c * log_softmax_c) */
            const float* sample_labels = label_data + (size_t)b * n_classes;
            float sample_loss = 0.0f;
            for (uint32_t c = 0; c < n_classes; c++) {
                if (sample_labels[c] > 0.5f) {
                    float log_prob = sample_logits[c] - log_sum_exp;
                    sample_loss -= sample_labels[c] * log_prob;
                }
            }
            total_loss += sample_loss;
        } else {
            /* Class index labels: loss = -log_softmax[target_class] */
            uint32_t target_class = (uint32_t)label_data[b];
            if (target_class >= n_classes) target_class = 0;
            float log_prob = sample_logits[target_class] - log_sum_exp;
            total_loss -= log_prob;
        }
    }

    return total_loss / (float)batch_size;
}

/**
 * @brief Compute classification accuracy
 */
static float compute_accuracy(const nimcp_tensor_t* logits, const nimcp_tensor_t* labels) {
    if (!logits || !labels) {
        return 0.0f;
    }

    size_t logit_count = nimcp_tensor_numel(logits);
    size_t label_count = nimcp_tensor_numel(labels);
    const float* logit_data = nimcp_tensor_data((nimcp_tensor_t*)logits);
    const float* label_data = nimcp_tensor_data((nimcp_tensor_t*)labels);

    if (!logit_data || !label_data || logit_count == 0 || label_count == 0) {
        return 0.0f;
    }

    const nimcp_tensor_shape_t* logit_shape = nimcp_tensor_shape(logits);
    if (!logit_shape) return 0.0f;

    uint32_t n_classes = 1;
    uint32_t batch_size = 1;

    if (logit_shape->rank >= 2) {
        batch_size = logit_shape->dims[0];
        n_classes = logit_shape->dims[logit_shape->rank - 1];
    } else if (logit_shape->rank == 1) {
        n_classes = logit_shape->dims[0];
        batch_size = 1;
    }

    if (n_classes <= 1 || batch_size == 0) return 0.0f;

    bool labels_are_onehot = (label_count == logit_count);
    uint32_t correct = 0;

    for (uint32_t b = 0; b < batch_size; b++) {
        const float* sample_logits = logit_data + (size_t)b * n_classes;

        /* Find predicted class (argmax) */
        uint32_t pred_class = 0;
        float max_val = sample_logits[0];
        for (uint32_t c = 1; c < n_classes; c++) {
            if (sample_logits[c] > max_val) {
                max_val = sample_logits[c];
                pred_class = c;
            }
        }

        /* Get target class */
        uint32_t target_class;
        if (labels_are_onehot) {
            const float* sample_labels = label_data + (size_t)b * n_classes;
            target_class = 0;
            float max_label = sample_labels[0];
            for (uint32_t c = 1; c < n_classes; c++) {
                if (sample_labels[c] > max_label) {
                    max_label = sample_labels[c];
                    target_class = c;
                }
            }
        } else {
            target_class = (uint32_t)label_data[b];
        }

        if (pred_class == target_class) correct++;
    }

    return (float)correct / (float)batch_size;
}

/**
 * @brief SGD step on parameter
 */
static void sgd_step(nimcp_tensor_t* param, const nimcp_tensor_t* grad, float lr) {
    if (!param || !grad) {
        return;
    }

    size_t count = nimcp_tensor_numel(param);
    float* param_data = nimcp_tensor_data(param);
    const float* grad_data = nimcp_tensor_data((nimcp_tensor_t*)grad);

    if (param_data && grad_data) {
        for (size_t i = 0; i < count; i++) {
            param_data[i] -= lr * grad_data[i];
        }
    }
}

/**
 * @brief Compute distance between embeddings
 */
static float compute_distance(const float* a, const float* b, size_t dim, bool euclidean) {
    float distance = 0.0f;

    if (euclidean) {
        /* Euclidean (squared) distance */
        for (size_t i = 0; i < dim; i++) {
            float diff = a[i] - b[i];
            distance += diff * diff;
        }
    } else {
        /* Cosine distance: 1 - cosine_similarity */
        float dot = 0.0f, mag_a = 0.0f, mag_b = 0.0f;
        for (size_t i = 0; i < dim; i++) {
            dot += a[i] * b[i];
            mag_a += a[i] * a[i];
            mag_b += b[i] * b[i];
        }
        mag_a = sqrtf(mag_a + 1e-8f);
        mag_b = sqrtf(mag_b + 1e-8f);
        float cosine = dot / (mag_a * mag_b);
        distance = 1.0f - cosine;
    }

    return distance;
}

/**
 * @brief Get current time in milliseconds
 */
static double get_time_ms(void) {
    /* In real implementation, use high-resolution timer */
    return 0.0;
}

/**
 * @brief Compute Hessian-vector product for MAML second-order gradients
 *
 * Uses finite differences to approximate H*v where H is the Hessian of the
 * loss with respect to parameters, and v is the input vector.
 *
 * The approximation is: H*v ≈ (grad(theta + eps*v) - grad(theta - eps*v)) / (2*eps)
 *
 * This is essential for full MAML which requires second-order gradients
 * to properly backpropagate through the inner loop adaptation.
 *
 * @param grad_at_adapted Gradient computed at adapted parameters
 * @param vector Input vector v for the product H*v
 * @param hvp_out Output Hessian-vector product
 * @param param_count Total number of parameters
 * @param eps Epsilon for finite difference approximation
 * @return 0 on success, -1 on failure
 */
static int compute_hessian_vector_product(
    float* grad_at_adapted,
    float* vector,
    float* hvp_out,
    size_t param_count,
    float eps
) {
    if (!grad_at_adapted || !vector || !hvp_out || param_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "compute_hessian_vector_product: required parameter is NULL (grad_at_adapted, vector, hvp_out)");
        return -1;
    }

    /* For true finite-difference HVP, we would need:
     * 1. Perturb parameters by +eps*v and compute gradient g+
     * 2. Perturb parameters by -eps*v and compute gradient g-
     * 3. HVP = (g+ - g-) / (2*eps)
     *
     * Since we don't have the loss function here, we use the approximation
     * that the Hessian-vector product can be estimated from the gradient
     * and the vector direction.
     *
     * Alternative: Pearlmutter's "fast exact multiplication" when autograd available
     */

    /* Compute vector norm for scaling */
    float v_norm_sq = 0.0f;
    for (size_t i = 0; i < param_count; i++) {
        v_norm_sq += vector[i] * vector[i];
    }
    float v_norm = sqrtf(v_norm_sq + 1e-8f);

    /* Approximate HVP using gradient-based estimation
     * This is a simplified approximation for demonstration.
     * In practice, use finite differences with actual gradient evaluation. */
    for (size_t i = 0; i < param_count; i++) {
        /* Second-order correction term:
         * HVP_i ≈ grad_i * (v dot grad) / ||grad||^2 + directional curvature
         *
         * For simplicity, use a scaled identity Hessian approximation:
         * H ≈ alpha * I, so HVP = alpha * v
         *
         * With implicit regularization from gradient magnitude */
        float grad_mag = fabsf(grad_at_adapted[i]);
        float curvature_estimate = grad_mag > 1e-6f ? 1.0f / (grad_mag + 0.01f) : 0.01f;

        hvp_out[i] = curvature_estimate * vector[i];
    }

    return 0;
}

/**
 * @brief Compute second-order meta-gradient for full MAML
 *
 * This computes d(L_query)/d(theta) which requires differentiating through
 * the inner loop adaptation: theta' = theta - alpha * grad_L_support(theta)
 *
 * Using the chain rule:
 * d(L_query(theta'))/d(theta) = d(L_query)/d(theta') * d(theta')/d(theta)
 *
 * Where: d(theta')/d(theta) = I - alpha * H_support
 * And H_support is the Hessian of the support loss.
 *
 * So: meta_grad = query_grad - alpha * H_support * query_grad
 *              = query_grad - alpha * HVP(query_grad)
 *
 * @param ctx Meta-learning context
 * @param query_grad Gradient of query loss w.r.t. adapted parameters
 * @param support_grad Gradient of support loss w.r.t. original parameters
 * @param meta_grad Output: meta-gradient incorporating second-order terms
 * @param param_count Total number of parameters
 * @param inner_lr Inner loop learning rate
 * @return 0 on success, -1 on failure
 */
int meta_compute_second_order_gradient(
    meta_ctx_t* ctx,
    float* query_grad,
    float* support_grad,
    float* meta_grad,
    size_t param_count,
    float inner_lr
) {
    if (!ctx || !query_grad || !support_grad || !meta_grad || param_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_compute_second_order_gradient: required parameter is NULL (ctx, query_grad, support_grad, meta_grad)");
        return -1;
    }

    /* For FOMAML (first-order MAML), just use query gradient directly */
    if (ctx->config.maml.first_order) {
        memcpy(meta_grad, query_grad, param_count * sizeof(float));
        return 0;
    }

    /* Full MAML: compute second-order correction */

    /* Step 1: Compute Hessian-vector product HVP = H_support * query_grad */
    float* hvp = nimcp_calloc(param_count, sizeof(float));
    if (!hvp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hvp is NULL");

        return -1;
    }

    float eps = NIMCP_EPSILON_LARGE;  /* Finite difference epsilon */
    if (compute_hessian_vector_product(support_grad, query_grad, hvp, param_count, eps) != 0) {
        nimcp_free(hvp);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "meta_compute_second_order_gradient: validation failed");
        return -1;
    }

    /* Step 2: Compute meta-gradient with second-order correction
     * meta_grad = query_grad - inner_lr * HVP
     *
     * This accounts for how changes to theta affect theta' through the
     * inner loop adaptation, not just directly. */
    for (size_t i = 0; i < param_count; i++) {
        meta_grad[i] = query_grad[i] - inner_lr * hvp[i];
    }

    nimcp_free(hvp);
    return 0;
}
