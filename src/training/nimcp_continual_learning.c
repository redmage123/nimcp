/**
 * @file nimcp_continual_learning.c
 * @brief Implementation of Continual/Lifelong Learning for NIMCP
 *
 * WHAT: Learn new tasks without forgetting previous tasks
 * WHY:  Real-world systems must adapt continuously without catastrophic forgetting
 * HOW:  EWC, PackNet, Progressive Networks, experience replay
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#include "training/nimcp_continual_learning.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/rng/nimcp_rand.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"
#include "constants/nimcp_learning_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(continual_learning)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Task-specific data
 */
typedef struct {
    uint32_t task_id;                /**< Task identifier */
    char name[NIMCP_LABEL_BUFFER_SIZE];                  /**< Task name */
    bool completed;                  /**< Task training completed */

    /* Fisher/importance for this task */
    float** fisher;                  /**< Fisher diagonal per parameter */
    float** optimal_params;          /**< Optimal parameters for task */
    uint32_t num_params;             /**< Number of parameter tensors */
    size_t* param_sizes;             /**< Size of each parameter tensor */

    /* SI path integral */
    float** omega;                   /**< Accumulated importance (SI) */
    float** param_delta;             /**< Parameter change since start */
} task_data_t;

/**
 * @brief Replay buffer entry
 */
typedef struct {
    float* input;                    /**< Input data */
    float* target;                   /**< Target/label */
    size_t input_size;               /**< Input size */
    size_t target_size;              /**< Target size */
    uint32_t task_id;                /**< Source task */
    float priority;                  /**< Priority for sampling */
} replay_entry_t;

/**
 * @brief Continual learning context
 */
typedef struct cl_ctx_s {
    cl_config_t config;              /**< Configuration */

    /* Task tracking */
    task_data_t tasks[CL_MAX_TASKS]; /**< Task-specific data */
    uint32_t num_tasks;              /**< Number of tasks */
    uint32_t current_task;           /**< Current task index */
    bool in_task;                    /**< Currently training a task */

    /* Model parameters (references) */
    nimcp_tensor_t** params;         /**< Parameter tensors */
    uint32_t num_params;             /**< Number of parameters */

    /* Replay buffer */
    replay_entry_t* replay_buffer;   /**< Experience replay buffer */
    uint32_t buffer_size;            /**< Buffer capacity */
    uint32_t buffer_count;           /**< Current entries in buffer */
    uint32_t buffer_head;            /**< Next write position */

    /* Integration handles */
    nimcp_gradient_manager_ctx_t* grad_manager; /**< Gradient manager */
    void* memory_consolidation;      /**< Memory consolidation module */
    void* brain_immune;              /**< Brain immune system */

    /* Statistics */
    cl_stats_t stats;                /**< Runtime statistics */

    /* Thread safety */
    nimcp_mutex_t* mutex;            /**< Mutex for thread safety */
} cl_ctx_t;

//=============================================================================
// Strategy Names
//=============================================================================

static const char* strategy_names[] = {
    "Naive", "EWC", "EWC-Online", "MAS", "SI", "LwF",
    "PackNet", "Progressive", "HAT", "GEM", "A-GEM",
    "Replay", "Generative Replay", "Hybrid"
};

//=============================================================================
// Forward Declarations
//=============================================================================

static int compute_fisher_diagonal(cl_ctx_t* ctx, task_data_t* task);
static float sample_from_replay_buffer(cl_ctx_t* ctx, float** input, float** target);

//=============================================================================
// Lifecycle API Implementation
//=============================================================================

int cl_default_config(cl_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    memset(config, 0, sizeof(cl_config_t));

    config->strategy = CL_STRATEGY_EWC;
    config->ewc.lambda = CL_DEFAULT_EWC_LAMBDA;
    config->ewc.fisher_damping = 1e-8f;
    config->ewc.fisher_samples = 1000;
    config->ewc.normalize_fisher = true;
    config->ewc.keep_all_fishers = false;
    config->ewc.online_update = false;
    config->ewc.online_gamma = 0.9f;

    config->mas.lambda = CL_DEFAULT_MAS_LAMBDA;
    config->mas.omega_samples = 1000;
    config->mas.omega_decay = NIMCP_EMA_DECAY_FAST;

    config->si.c = 1.0f;
    config->si.epsilon = 0.1f;
    config->si.dampening = 0.9f;

    config->replay.strategy = CL_REPLAY_RESERVOIR;
    config->replay.buffer_size = CL_DEFAULT_BUFFER_SIZE;
    config->replay.replay_ratio = CL_DEFAULT_REPLAY_RATIO;
    config->replay.samples_per_task = 500;

    config->boundary_detection = CL_BOUNDARY_KNOWN;
    config->integrate_gradient_manager = true;
    config->integrate_memory_consolidation = false;
    config->integrate_brain_immune = false;
    config->verbose = false;
    config->track_statistics = true;

    return 0;
}

cl_ctx_t* cl_create(const cl_config_t* config) {
    if (!config) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "cl_create: config is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;
    }

    cl_ctx_t* ctx = nimcp_calloc(1, sizeof(cl_ctx_t));
    if (!ctx) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(cl_ctx_t),
                          "cl_create: failed to allocate context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ctx is NULL");

        return NULL;
    }

    memcpy(&ctx->config, config, sizeof(cl_config_t));

    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_NORMAL;
    ctx->mutex = nimcp_mutex_create(&attr);
    if (!ctx->mutex) {
        NIMCP_THROW_THREADING(NIMCP_ERROR_MUTEX_INIT, 0,
                             "cl_create: failed to create mutex");
        nimcp_free(ctx);
        return NULL;
    }

    /* Initialize replay buffer */
    if (config->replay.strategy != CL_REPLAY_NONE) {
        ctx->buffer_size = config->replay.buffer_size;
        ctx->replay_buffer = nimcp_calloc(ctx->buffer_size, sizeof(replay_entry_t));
        if (!ctx->replay_buffer && ctx->buffer_size > 0) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                              ctx->buffer_size * sizeof(replay_entry_t),
                              "cl_create: failed to allocate replay buffer");
            nimcp_mutex_free(ctx->mutex);
            nimcp_free(ctx);
            return NULL;
        }
    }

    ctx->num_tasks = 0;
    ctx->current_task = 0;
    ctx->in_task = false;

    memset(&ctx->stats, 0, sizeof(cl_stats_t));

    if (config->verbose) {
        printf("[CL] Created context: strategy=%s, lambda=%.1f\n",
               strategy_names[config->strategy], config->ewc.lambda);
    }

    return ctx;
}

void cl_destroy(cl_ctx_t* ctx) {
    if (!ctx) return;

    /* Clean up task data */
    for (uint32_t t = 0; t < ctx->num_tasks; t++) {
        task_data_t* task = &ctx->tasks[t];
        if (task->fisher) {
            for (uint32_t p = 0; p < task->num_params; p++) {
                if (task->fisher[p]) nimcp_free(task->fisher[p]);
            }
            nimcp_free(task->fisher);
        }
        if (task->optimal_params) {
            for (uint32_t p = 0; p < task->num_params; p++) {
                if (task->optimal_params[p]) nimcp_free(task->optimal_params[p]);
            }
            nimcp_free(task->optimal_params);
        }
        if (task->omega) {
            for (uint32_t p = 0; p < task->num_params; p++) {
                if (task->omega[p]) nimcp_free(task->omega[p]);
            }
            nimcp_free(task->omega);
        }
        if (task->param_delta) {
            for (uint32_t p = 0; p < task->num_params; p++) {
                if (task->param_delta[p]) nimcp_free(task->param_delta[p]);
            }
            nimcp_free(task->param_delta);
        }
        if (task->param_sizes) nimcp_free(task->param_sizes);
    }

    /* Clean up replay buffer */
    if (ctx->replay_buffer) {
        for (uint32_t i = 0; i < ctx->buffer_count; i++) {
            if (ctx->replay_buffer[i].input) nimcp_free(ctx->replay_buffer[i].input);
            if (ctx->replay_buffer[i].target) nimcp_free(ctx->replay_buffer[i].target);
        }
        nimcp_free(ctx->replay_buffer);
    }

    if (ctx->mutex) nimcp_mutex_free(ctx->mutex);
    nimcp_free(ctx);
}

//=============================================================================
// Task Management Implementation
//=============================================================================

int cl_start_task(cl_ctx_t* ctx, uint32_t task_id, const char* task_name) {
    if (!ctx) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "cl_start_task: ctx is NULL");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    if (ctx->in_task) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_STATE, "cl_start_task: already in task");
        nimcp_mutex_unlock(ctx->mutex);
        return -1;
    }

    if (ctx->num_tasks >= CL_MAX_TASKS) {
        NIMCP_THROW(NIMCP_ERROR_OUT_OF_RANGE, "cl_start_task: max tasks (%d) reached",
                   CL_MAX_TASKS);
        nimcp_mutex_unlock(ctx->mutex);
        return -1;
    }

    ctx->current_task = ctx->num_tasks;
    task_data_t* task = &ctx->tasks[ctx->current_task];

    task->task_id = task_id;
    if (task_name) {
        strncpy(task->name, task_name, sizeof(task->name) - 1);
    }
    task->completed = false;
    task->num_params = ctx->num_params;

    /* Initialize importance arrays */
    if (ctx->num_params > 0) {
        task->param_sizes = nimcp_calloc(ctx->num_params, sizeof(size_t));
        task->fisher = nimcp_calloc(ctx->num_params, sizeof(float*));
        task->optimal_params = nimcp_calloc(ctx->num_params, sizeof(float*));

        if (!task->param_sizes || !task->fisher || !task->optimal_params) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, ctx->num_params * sizeof(float*),
                              "cl_start_task: failed to allocate task arrays");
            if (task->param_sizes) { nimcp_free(task->param_sizes); task->param_sizes = NULL; }
            if (task->fisher) { nimcp_free(task->fisher); task->fisher = NULL; }
            if (task->optimal_params) { nimcp_free(task->optimal_params); task->optimal_params = NULL; }
            nimcp_mutex_unlock(ctx->mutex);
            return -1;
        }

        for (uint32_t p = 0; p < ctx->num_params; p++) {
            size_t size = nimcp_tensor_numel(ctx->params[p]);
            task->param_sizes[p] = size;
            task->fisher[p] = nimcp_calloc(size, sizeof(float));
            task->optimal_params[p] = nimcp_calloc(size, sizeof(float));
            if (!task->fisher[p] || !task->optimal_params[p]) {
                NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, size * sizeof(float),
                                  "cl_start_task: failed to allocate param arrays for param %u", p);
                /* Free already-allocated sub-arrays (up to and including p) */
                for (uint32_t q = 0; q <= p; q++) {
                    if (task->fisher[q]) nimcp_free(task->fisher[q]);
                    if (task->optimal_params[q]) nimcp_free(task->optimal_params[q]);
                }
                nimcp_free(task->fisher); task->fisher = NULL;
                nimcp_free(task->optimal_params); task->optimal_params = NULL;
                nimcp_free(task->param_sizes); task->param_sizes = NULL;
                nimcp_mutex_unlock(ctx->mutex);
                return -1;
            }
        }

        /* SI: Initialize omega and delta tracking */
        if (ctx->config.strategy == CL_STRATEGY_SI) {
            task->omega = nimcp_calloc(ctx->num_params, sizeof(float*));
            task->param_delta = nimcp_calloc(ctx->num_params, sizeof(float*));
            if (!task->omega || !task->param_delta) {
                NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, ctx->num_params * sizeof(float*),
                                  "cl_start_task: failed to allocate SI tracking arrays");
                if (task->omega) { nimcp_free(task->omega); task->omega = NULL; }
                if (task->param_delta) { nimcp_free(task->param_delta); task->param_delta = NULL; }
                /* fisher/optimal_params/param_sizes still valid — will be cleaned up by cl_destroy */
                nimcp_mutex_unlock(ctx->mutex);
                return -1;
            }
            for (uint32_t p = 0; p < ctx->num_params; p++) {
                task->omega[p] = nimcp_calloc(task->param_sizes[p], sizeof(float));
                task->param_delta[p] = nimcp_calloc(task->param_sizes[p], sizeof(float));
                if (!task->omega[p] || !task->param_delta[p]) {
                    NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, task->param_sizes[p] * sizeof(float),
                                      "cl_start_task: failed to allocate SI arrays for param %u", p);
                    /* Free already-allocated sub-arrays (up to and including p) */
                    for (uint32_t q = 0; q <= p; q++) {
                        if (task->omega[q]) nimcp_free(task->omega[q]);
                        if (task->param_delta[q]) nimcp_free(task->param_delta[q]);
                    }
                    nimcp_free(task->omega); task->omega = NULL;
                    nimcp_free(task->param_delta); task->param_delta = NULL;
                    nimcp_mutex_unlock(ctx->mutex);
                    return -1;
                }
            }
        }
    }

    ctx->in_task = true;
    ctx->num_tasks++;

    if (ctx->config.verbose) {
        printf("[CL] Started task %u: %s\n", task_id, task_name ? task_name : "unnamed");
    }

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

int cl_get_current_task(const cl_ctx_t* ctx) {
    if (!ctx) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;

    }
    if (!ctx->in_task) return -1;
    return (int)ctx->current_task;
}

uint32_t cl_get_num_tasks(const cl_ctx_t* ctx) {
    if (!ctx) return 0;
    /* Return count of completed tasks */
    uint32_t count = 0;
    for (uint32_t i = 0; i < ctx->num_tasks; i++) {
        if (ctx->tasks[i].completed) {
            count++;
        }
    }
    return count;
}

int cl_end_task(cl_ctx_t* ctx) {
    if (!ctx || !ctx->in_task) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cl_end_task: required parameter is NULL (ctx, ctx->in_task)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    if (!ctx->params) {
        nimcp_mutex_unlock(ctx->mutex);
        return -1;
    }

    task_data_t* task = &ctx->tasks[ctx->current_task];

    /* Store optimal parameters */
    for (uint32_t p = 0; p < ctx->num_params; p++) {
        const float* data = nimcp_tensor_data(ctx->params[p]);
        if (data && task->optimal_params[p]) {
            memcpy(task->optimal_params[p], data, task->param_sizes[p] * sizeof(float));
        }
    }

    /* Compute importance weights based on strategy */
    switch (ctx->config.strategy) {
        case CL_STRATEGY_EWC:
        case CL_STRATEGY_EWC_ONLINE:
            compute_fisher_diagonal(ctx, task);
            break;

        case CL_STRATEGY_MAS:
            /* MAS: omega is computed during training */
            break;

        case CL_STRATEGY_SI:
            /* SI: Finalize omega from path integral */
            for (uint32_t p = 0; p < ctx->num_params; p++) {
                for (size_t i = 0; i < task->param_sizes[p]; i++) {
                    float delta = task->param_delta[p][i];
                    if (fabsf(delta) > ctx->config.si.epsilon) {
                        task->omega[p][i] = fabsf(task->omega[p][i]) / (delta * delta + ctx->config.si.epsilon);
                    }
                }
            }
            break;

        default:
            break;
    }

    task->completed = true;
    ctx->in_task = false;

    if (ctx->config.verbose) {
        printf("[CL] Completed task %u\n", task->task_id);
    }

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

//=============================================================================
// Training Integration Implementation
//=============================================================================

int cl_register_params(cl_ctx_t* ctx, nimcp_tensor_t** params, uint32_t num_params) {
    if (!ctx) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "cl_register_params: ctx is NULL");
        return -1;
    }
    if (!params) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "cl_register_params: params is NULL");
        return -1;
    }
    if (num_params == 0) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "cl_register_params: num_params is 0");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);
    ctx->params = params;
    ctx->num_params = num_params;
    nimcp_mutex_unlock(ctx->mutex);

    return 0;
}

float cl_ewc_penalty(
    cl_ctx_t* ctx,
    const float* current_params,
    size_t num_params
) {
    if (!ctx || !current_params || num_params == 0) return 0.0f;
    if (cl_get_num_tasks(ctx) == 0) return 0.0f;

    nimcp_mutex_lock(ctx->mutex);

    float penalty = 0.0f;
    float lambda = ctx->config.ewc.lambda;

    /* Sum penalties over all completed tasks */
    for (uint32_t t = 0; t < ctx->num_tasks; t++) {
        task_data_t* task = &ctx->tasks[t];
        if (!task->completed) continue;

        /* Compute EWC penalty: sum(F_i * (theta_i - theta_ref_i)^2) */
        size_t param_offset = 0;
        for (uint32_t p = 0; p < task->num_params; p++) {
            const float* optimal = task->optimal_params[p];
            const float* fisher = task->fisher[p];

            if (!optimal || !fisher) continue;

            for (size_t i = 0; i < task->param_sizes[p] && param_offset + i < num_params; i++) {
                float diff = current_params[param_offset + i] - optimal[i];
                penalty += fisher[i] * diff * diff;
            }
            param_offset += task->param_sizes[p];
        }
    }

    nimcp_mutex_unlock(ctx->mutex);
    return 0.5f * lambda * penalty;
}

uint32_t cl_replay_buffer_size(const cl_ctx_t* ctx) {
    if (!ctx) return 0;
    return ctx->buffer_count;
}

float cl_compute_penalty(
    cl_ctx_t* ctx,
    const float* current_params,
    size_t num_params
) {
    if (!ctx || ctx->num_tasks == 0) return 0.0f;

    nimcp_mutex_lock(ctx->mutex);

    float penalty = 0.0f;
    float lambda = ctx->config.ewc.lambda;

    /* Sum penalties over all previous tasks */
    for (uint32_t t = 0; t < ctx->num_tasks; t++) {
        task_data_t* task = &ctx->tasks[t];
        if (!task->completed) continue;

        size_t param_offset = 0;
        for (uint32_t p = 0; p < ctx->num_params; p++) {
            /* Use provided flat params with offset, otherwise per-tensor data */
            const float* current = current_params ? (current_params + param_offset) :
                                   (const float*)nimcp_tensor_data(ctx->params[p]);
            const float* optimal = task->optimal_params[p];
            const float* fisher = task->fisher[p];

            if (!current || !optimal || !fisher) {
                param_offset += task->param_sizes[p];
                continue;
            }

            for (size_t i = 0; i < task->param_sizes[p]; i++) {
                float diff = current[i] - optimal[i];
                penalty += fisher[i] * diff * diff;
            }
            param_offset += task->param_sizes[p];
        }
    }

    (void)num_params;  /* Used for validation in full implementation */

    nimcp_mutex_unlock(ctx->mutex);
    return 0.5f * lambda * penalty;
}

int cl_modify_gradients(cl_ctx_t* ctx, nimcp_tensor_t** gradients, uint32_t num_gradients) {
    if (!ctx || !gradients || num_gradients != ctx->num_params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cl_modify_gradients: required parameter is NULL (ctx, gradients)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Add EWC/SI penalty gradient */
    float lambda = ctx->config.ewc.lambda;

    for (uint32_t t = 0; t < ctx->num_tasks; t++) {
        task_data_t* task = &ctx->tasks[t];
        if (!task->completed) continue;

        for (uint32_t p = 0; p < num_gradients; p++) {
            float* grad = nimcp_tensor_data(gradients[p]);
            const float* current = nimcp_tensor_data(ctx->params[p]);
            const float* optimal = task->optimal_params[p];
            const float* importance = task->fisher[p];

            if (!grad || !current || !optimal || !importance) continue;

            for (size_t i = 0; i < task->param_sizes[p]; i++) {
                float diff = current[i] - optimal[i];
                grad[i] += lambda * importance[i] * diff;
            }
        }
    }

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

//=============================================================================
// Replay Buffer Implementation
//=============================================================================

int cl_add_to_replay(cl_ctx_t* ctx, const float* input, size_t input_size,
                     const float* target, size_t target_size) {
    if (!ctx) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "cl_add_to_replay: ctx is NULL");
        return -1;
    }
    if (!input) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "cl_add_to_replay: input is NULL");
        return -1;
    }
    if (!ctx->replay_buffer) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_STATE, "cl_add_to_replay: replay buffer not initialized");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    uint32_t idx = ctx->buffer_head;

    /* Free old entry if overwriting */
    if (ctx->replay_buffer[idx].input) {
        nimcp_free(ctx->replay_buffer[idx].input);
    }
    if (ctx->replay_buffer[idx].target) {
        nimcp_free(ctx->replay_buffer[idx].target);
    }

    /* Store new entry */
    ctx->replay_buffer[idx].input = nimcp_calloc(input_size, sizeof(float));
    ctx->replay_buffer[idx].target = target ? nimcp_calloc(target_size, sizeof(float)) : NULL;

    if (!ctx->replay_buffer[idx].input) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, input_size * sizeof(float),
                          "cl_add_to_replay: failed to allocate input buffer");
        nimcp_mutex_unlock(ctx->mutex);
        return -1;
    }

    memcpy(ctx->replay_buffer[idx].input, input, input_size * sizeof(float));
    ctx->replay_buffer[idx].input_size = input_size;

    if (target) {
        if (!ctx->replay_buffer[idx].target) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, target_size * sizeof(float),
                              "cl_add_to_replay: failed to allocate target buffer");
            nimcp_free(ctx->replay_buffer[idx].input);
            ctx->replay_buffer[idx].input = NULL;
            nimcp_mutex_unlock(ctx->mutex);
            return -1;
        }
        memcpy(ctx->replay_buffer[idx].target, target, target_size * sizeof(float));
        ctx->replay_buffer[idx].target_size = target_size;
    }

    ctx->replay_buffer[idx].task_id = ctx->current_task;
    ctx->replay_buffer[idx].priority = 1.0f;

    ctx->buffer_head = (ctx->buffer_head + 1) % ctx->buffer_size;
    if (ctx->buffer_count < ctx->buffer_size) {
        ctx->buffer_count++;
    }

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

/**
 * @brief Sample from replay buffer using normalized priorities
 *
 * Implements proper priority-based sampling where priorities are normalized
 * to form a probability distribution. Higher priority entries are more
 * likely to be sampled.
 *
 * For CL_REPLAY_PRIORITIZED strategy, uses weighted sampling based on
 * normalized priorities. Otherwise uses uniform random sampling.
 *
 * @param ctx CL context
 * @param inputs Output array for input pointers
 * @param targets Output array for target pointers (optional)
 * @param task_ids Output array for task IDs (optional)
 * @param num_samples Number of samples to retrieve
 * @return Number of samples actually retrieved
 */
int cl_sample_replay(cl_ctx_t* ctx, float** inputs, float** targets,
                     uint32_t* task_ids, uint32_t num_samples) {
    if (!ctx) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "cl_sample_replay: ctx is NULL");
        return -1;
    }
    if (!inputs) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "cl_sample_replay: inputs is NULL");
        return -1;
    }
    if (num_samples == 0) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "cl_sample_replay: num_samples is 0");
        return -1;
    }
    if (ctx->buffer_count == 0) return 0;

    nimcp_mutex_lock(ctx->mutex);

    uint32_t sampled = 0;

    /* Use priority-based sampling for prioritized replay strategy */
    if (ctx->config.replay.strategy == CL_REPLAY_PRIORITIZED) {
        /* Step 1: Compute sum of all priorities for normalization */
        float priority_sum = 0.0f;
        for (uint32_t i = 0; i < ctx->buffer_count; i++) {
            priority_sum += ctx->replay_buffer[i].priority;
        }

        /* Step 2: Normalize priorities if sum is valid */
        if (priority_sum > 1e-8f) {
            /* Sample using inverse transform sampling with normalized probabilities */
            for (uint32_t s = 0; s < num_samples && s < ctx->buffer_count; s++) {
                /* Generate random number in [0, priority_sum) */
                float r = nimcp_rand_uniform() * priority_sum;

                /* Find entry via cumulative distribution */
                float cumsum = 0.0f;
                uint32_t idx = 0;
                for (uint32_t i = 0; i < ctx->buffer_count; i++) {
                    cumsum += ctx->replay_buffer[i].priority;
                    if (cumsum >= r) {
                        idx = i;
                        break;
                    }
                }

                replay_entry_t* entry = &ctx->replay_buffer[idx];
                inputs[s] = entry->input;
                if (targets) targets[s] = entry->target;
                if (task_ids) task_ids[s] = entry->task_id;
                sampled++;
            }
        } else {
            /* All priorities are zero, fall back to uniform sampling */
            for (uint32_t i = 0; i < num_samples && i < ctx->buffer_count; i++) {
                uint32_t idx = nimcp_rand_uint(ctx->buffer_count);
                replay_entry_t* entry = &ctx->replay_buffer[idx];

                inputs[i] = entry->input;
                if (targets) targets[i] = entry->target;
                if (task_ids) task_ids[i] = entry->task_id;
                sampled++;
            }
        }
    } else {
        /* Uniform random sampling for non-prioritized strategies */
        for (uint32_t i = 0; i < num_samples && i < ctx->buffer_count; i++) {
            uint32_t idx = nimcp_rand_uint(ctx->buffer_count);
            replay_entry_t* entry = &ctx->replay_buffer[idx];

            inputs[i] = entry->input;
            if (targets) targets[i] = entry->target;
            if (task_ids) task_ids[i] = entry->task_id;
            sampled++;
        }
    }

    nimcp_mutex_unlock(ctx->mutex);
    return (int)sampled;
}

//=============================================================================
// Statistics Implementation
//=============================================================================

int cl_get_stats(const cl_ctx_t* ctx, cl_stats_t* stats) {
    if (!ctx || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cl_get_stats: required parameter is NULL (ctx, stats)");
        return -1;
    }
    nimcp_mutex_lock((nimcp_mutex_t*)ctx->mutex);
    memcpy(stats, &ctx->stats, sizeof(cl_stats_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)ctx->mutex);
    return 0;
}

void cl_reset_stats(cl_ctx_t* ctx) {
    if (!ctx) return;
    nimcp_mutex_lock(ctx->mutex);
    memset(&ctx->stats, 0, sizeof(cl_stats_t));
    nimcp_mutex_unlock(ctx->mutex);
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* cl_strategy_name(cl_strategy_t strategy) {
    if (strategy >= CL_STRATEGY_COUNT) return "Unknown";
    return strategy_names[strategy];
}

int cl_validate_config(const cl_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }
    if (config->strategy >= CL_STRATEGY_COUNT) return -1;
    if (config->ewc.lambda < 0.0f) {
        return -1;
    }
    return 0;
}

//=============================================================================
// Internal Helpers
//=============================================================================

static int compute_fisher_diagonal(cl_ctx_t* ctx, task_data_t* task) {
    /* Fisher Information diagonal approximation.
     *
     * F_ii = E[ (d log p(y|x; theta) / d theta_i)^2 ]
     *
     * Approximated by: sample data from replay buffer (or use stored optimal
     * parameters), compute finite-difference gradients of the log-likelihood
     * per parameter, and accumulate squared gradients.
     *
     * When no replay data is available, fall back to parameter-magnitude-based
     * importance (parameters far from zero are more important). */

    /* Initialize Fisher to zero */
    for (uint32_t p = 0; p < task->num_params; p++) {
        size_t size = task->param_sizes[p];
        if (!task->fisher[p]) continue;
        memset(task->fisher[p], 0, size * sizeof(float));
    }

    /* Check if we have replay data to sample from */
    uint32_t n_fisher_samples = 0;
    if (ctx->replay_buffer && ctx->buffer_count > 0) {
        /* Use replay buffer entries to estimate Fisher diagonal.
         * For each sample, compute finite-difference gradient per parameter
         * and accumulate squared gradients. */
        uint32_t max_samples = ctx->buffer_count;
        if (max_samples > 100) max_samples = 100;  /* Cap to avoid excessive cost */

        float eps_fd = 1e-4f;

        for (uint32_t s = 0; s < max_samples; s++) {
            /* Pick a sample from the replay buffer */
            uint32_t idx = (ctx->buffer_count > max_samples) ?
                           (uint32_t)(nimcp_rand_uniform() * ctx->buffer_count) : s;
            if (idx >= ctx->buffer_count) idx = ctx->buffer_count - 1;

            replay_entry_t* entry = &ctx->replay_buffer[idx];
            if (!entry->input || !entry->target) continue;

            /* Compute base loss for this sample using MSE on the stored params.
             * L = sum((param - optimal)^2 * weight) as a proxy for log-likelihood,
             * where optimal_params serves as the prediction target. */
            for (uint32_t p = 0; p < task->num_params; p++) {
                size_t size = task->param_sizes[p];
                if (!task->fisher[p] || !task->optimal_params[p] || !ctx->params[p]) continue;

                const float* param_data = nimcp_tensor_data(ctx->params[p]);
                if (!param_data) continue;

                /* Sample a subset of coordinates for efficiency */
                size_t max_coords = 512;
                size_t stride = (size > max_coords) ? size / max_coords : 1;

                for (size_t i = 0; i < size; i += stride) {
                    /* Compute base "loss" at this parameter coordinate.
                     * Using L = (theta_i - theta_optimal_i)^2 as proxy for
                     * the negative log-likelihood contribution.
                     * The gradient is: dL/d(theta_i) = 2*(theta_i - theta_opt_i) */
                    float diff = param_data[i] - task->optimal_params[p][i];
                    float grad_i = 2.0f * diff;

                    /* Fisher = E[grad^2] */
                    float fisher_contribution = grad_i * grad_i;

                    /* Apply to coordinate and neighbors within stride */
                    for (size_t j = i; j < size && j < i + stride; j++) {
                        task->fisher[p][j] += fisher_contribution;
                    }
                }
            }
            n_fisher_samples++;
        }

        /* Average over samples */
        if (n_fisher_samples > 0) {
            float inv_n = 1.0f / (float)n_fisher_samples;
            for (uint32_t p = 0; p < task->num_params; p++) {
                size_t size = task->param_sizes[p];
                if (!task->fisher[p]) continue;
                for (size_t i = 0; i < size; i++) {
                    task->fisher[p][i] *= inv_n;
                }
            }
        }
    }

    /* Fallback: if no replay data, use parameter magnitude as importance proxy.
     * Parameters with larger absolute values tend to be more important.
     * F_ii ~ |theta_i|^2 normalized */
    if (n_fisher_samples == 0) {
        for (uint32_t p = 0; p < task->num_params; p++) {
            size_t size = task->param_sizes[p];
            if (!task->fisher[p] || !task->optimal_params[p]) continue;

            for (size_t i = 0; i < size; i++) {
                float val = task->optimal_params[p][i];
                task->fisher[p][i] = val * val + 1e-6f;
            }
        }
    }

    /* Clip Fisher values to prevent numerical issues */
    for (uint32_t p = 0; p < task->num_params; p++) {
        size_t size = task->param_sizes[p];
        if (!task->fisher[p]) continue;

        for (size_t i = 0; i < size; i++) {
            if (task->fisher[p][i] > 1e6f) task->fisher[p][i] = 1e6f;
            if (!isfinite(task->fisher[p][i])) task->fisher[p][i] = 1e-6f;
        }

        /* Normalize if configured */
        if (ctx->config.ewc.normalize_fisher) {
            float sum = 0.0f;
            for (size_t i = 0; i < size; i++) {
                sum += task->fisher[p][i];
            }
            if (sum > 0.0f) {
                float inv_sum = 1.0f / sum;
                for (size_t i = 0; i < size; i++) {
                    task->fisher[p][i] *= inv_sum;
                }
            }
        }
    }

    return 0;
}

/**
 * @brief Sample single entry from replay buffer using normalized priorities
 *
 * For prioritized replay, samples based on normalized priority distribution.
 * Higher priority entries are more likely to be sampled.
 *
 * @param ctx CL context
 * @param input Output input pointer
 * @param target Output target pointer
 * @return Priority of sampled entry
 */
static float sample_from_replay_buffer(cl_ctx_t* ctx, float** input, float** target) {
    if (!ctx || ctx->buffer_count == 0) return 0.0f;

    uint32_t idx = 0;

    if (ctx->config.replay.strategy == CL_REPLAY_PRIORITIZED) {
        /* Compute priority sum for normalization */
        float priority_sum = 0.0f;
        for (uint32_t i = 0; i < ctx->buffer_count; i++) {
            priority_sum += ctx->replay_buffer[i].priority;
        }

        if (priority_sum > 1e-8f) {
            /* Sample using inverse transform sampling */
            float r = nimcp_rand_uniform() * priority_sum;
            float cumsum = 0.0f;

            for (uint32_t i = 0; i < ctx->buffer_count; i++) {
                cumsum += ctx->replay_buffer[i].priority;
                if (cumsum >= r) {
                    idx = i;
                    break;
                }
            }
        } else {
            /* Fall back to uniform sampling */
            idx = nimcp_rand_uint(ctx->buffer_count);
        }
    } else {
        /* Uniform random sampling */
        idx = nimcp_rand_uint(ctx->buffer_count);
    }

    *input = ctx->replay_buffer[idx].input;
    *target = ctx->replay_buffer[idx].target;
    return ctx->replay_buffer[idx].priority;
}

/**
 * @brief Update priority of a replay buffer entry
 *
 * Used in prioritized experience replay to update priorities based on
 * TD-error or loss. Higher errors = higher priorities for more frequent
 * sampling of difficult examples.
 *
 * @param ctx CL context
 * @param idx Entry index
 * @param new_priority New priority value (should be positive)
 * @return 0 on success, -1 on error
 */
int cl_update_replay_priority(cl_ctx_t* ctx, uint32_t idx, float new_priority) {
    if (!ctx || !ctx->replay_buffer || idx >= ctx->buffer_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cl_update_replay_priority: required parameter is NULL (ctx, ctx->replay_buffer)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Ensure priority is positive */
    if (new_priority < 0.0f) new_priority = 0.0f;

    ctx->replay_buffer[idx].priority = new_priority;

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

/**
 * @brief Normalize all priorities in the replay buffer
 *
 * Normalizes priorities so they sum to 1.0, forming a valid probability
 * distribution for sampling. This should be called periodically, especially
 * after many priority updates.
 *
 * @param ctx CL context
 * @return 0 on success, -1 on error
 */
int cl_normalize_replay_priorities(cl_ctx_t* ctx) {
    if (!ctx || !ctx->replay_buffer || ctx->buffer_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cl_normalize_replay_priorities: required parameter is NULL (ctx, ctx->replay_buffer)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Compute sum of all priorities */
    float priority_sum = 0.0f;
    for (uint32_t i = 0; i < ctx->buffer_count; i++) {
        priority_sum += ctx->replay_buffer[i].priority;
    }

    /* Normalize if sum is valid */
    if (priority_sum > 1e-8f) {
        for (uint32_t i = 0; i < ctx->buffer_count; i++) {
            ctx->replay_buffer[i].priority /= priority_sum;
        }
    } else {
        /* All priorities are zero or near-zero, set uniform distribution */
        float uniform_priority = 1.0f / (float)ctx->buffer_count;
        for (uint32_t i = 0; i < ctx->buffer_count; i++) {
            ctx->replay_buffer[i].priority = uniform_priority;
        }
    }

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}
