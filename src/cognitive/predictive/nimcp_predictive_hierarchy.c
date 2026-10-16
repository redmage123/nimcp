/**
 * @file nimcp_predictive_hierarchy.c
 * @brief Predictive Coding Hierarchy Implementation
 * @version 1.0.0
 * @date 2025-01-04
 */

#include "cognitive/predictive/nimcp_predictive_hierarchy.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"

BRIDGE_BOILERPLATE(predictive_hierarchy, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* Logging macros - wrap LOG_* for consistent usage */
#define NIMCP_LOG_INFO(...)  LOG_INFO(__VA_ARGS__)
#define NIMCP_LOG_WARN(...)  LOG_WARN(__VA_ARGS__)
#define NIMCP_LOG_ERROR(...) LOG_ERROR(__VA_ARGS__)

/* ============================================================================
 * Private Helpers
 * ============================================================================ */

/**
 * @brief Create a single hierarchy level
 */
static pred_level_t* create_level(const pred_level_config_t* config,
                                   uint32_t level_index) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;
    }

    pred_level_t* level = nimcp_calloc(1, sizeof(pred_level_t));
    if (!level) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate level");

        return NULL;
    }

    level->dim = config->dim;
    level->level_index = level_index;
    level->gen_hidden_dim = config->gen_hidden_dim;

    level->state = nimcp_calloc(config->dim, sizeof(float));
    level->prediction = nimcp_calloc(config->dim, sizeof(float));
    level->prediction_error = nimcp_calloc(config->dim, sizeof(float));
    level->precision = nimcp_calloc(config->dim, sizeof(float));

    if (!level->state || !level->prediction ||
        !level->prediction_error || !level->precision) {
        goto error;
    }

    for (uint32_t i = 0; i < config->dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && config->dim > 256) {
            predictive_hierarchy_heartbeat("predictive_h_loop",
                             (float)(i + 1) / (float)config->dim);
        }

        level->precision[i] = config->initial_precision;
    }

    if (config->gen_type == PRED_HIER_GEN_LINEAR && level_index > 0) {
        level->gen_weights = nimcp_calloc(config->dim * config->dim, sizeof(float));
        level->gen_bias = nimcp_calloc(config->dim, sizeof(float));
        level->grad_gen_weights = nimcp_calloc(config->dim * config->dim, sizeof(float));
        level->grad_gen_bias = nimcp_calloc(config->dim, sizeof(float));

        if (!level->gen_weights || !level->gen_bias ||
            !level->grad_gen_weights || !level->grad_gen_bias) {
            goto error;
        }

        for (uint32_t i = 0; i < config->dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && config->dim > 256) {
                predictive_hierarchy_heartbeat("predictive_h_loop",
                                 (float)(i + 1) / (float)config->dim);
            }

            level->gen_weights[i * config->dim + i] = 1.0f;
        }
    }

    level->above = NULL;
    level->below = NULL;
    level->avg_error = 0.0f;
    level->avg_precision = config->initial_precision;
    level->update_count = 0;

    return level;

error:
    if (level->state) nimcp_free(level->state);
    if (level->prediction) nimcp_free(level->prediction);
    if (level->prediction_error) nimcp_free(level->prediction_error);
    if (level->precision) nimcp_free(level->precision);
    if (level->gen_weights) nimcp_free(level->gen_weights);
    if (level->gen_bias) nimcp_free(level->gen_bias);
    if (level->grad_gen_weights) nimcp_free(level->grad_gen_weights);
    if (level->grad_gen_bias) nimcp_free(level->grad_gen_bias);
    nimcp_free(level);
    level = NULL;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_level: memory allocation failed");
    return NULL;
}

/**
 * @brief Destroy a hierarchy level
 */
static void destroy_level(pred_level_t* level) {
    if (!level) {
        return;
    }

    if (level->state) nimcp_free(level->state);
    if (level->prediction) nimcp_free(level->prediction);
    if (level->prediction_error) nimcp_free(level->prediction_error);
    if (level->precision) nimcp_free(level->precision);
    if (level->gen_weights) nimcp_free(level->gen_weights);
    if (level->gen_bias) nimcp_free(level->gen_bias);
    if (level->rec_weights) nimcp_free(level->rec_weights);
    if (level->rec_bias) nimcp_free(level->rec_bias);
    if (level->grad_gen_weights) nimcp_free(level->grad_gen_weights);
    if (level->grad_gen_bias) nimcp_free(level->grad_gen_bias);

    nimcp_free(level);
    level = NULL;
}

/**
 * @brief Apply generative model (top-down prediction)
 */
static void apply_generative_model(pred_level_t* from_level,
                                    pred_level_t* to_level) {
    if (!from_level || !to_level || !from_level->gen_weights) {
        return;
    }

    uint32_t from_dim = from_level->dim;
    uint32_t to_dim = to_level->dim;

    if (from_dim != to_dim) {
        return;
    }

    for (uint32_t i = 0; i < to_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && to_dim > 256) {
            predictive_hierarchy_heartbeat("predictive_h_loop",
                             (float)(i + 1) / (float)to_dim);
        }

        float sum = from_level->gen_bias ? from_level->gen_bias[i] : 0.0f;
        for (uint32_t j = 0; j < from_dim; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && from_dim > 256) {
                predictive_hierarchy_heartbeat("predictive_h_loop",
                                 (float)(j + 1) / (float)from_dim);
            }

            sum += from_level->gen_weights[i * from_dim + j] * from_level->state[j];
        }
        to_level->prediction[i] = sum;
    }
}

/**
 * @brief Compute prediction error
 */
static void compute_prediction_error(pred_level_t* level) {
    if (!level) {
        return;
    }

    float error_sum = 0.0f;
    for (uint32_t i = 0; i < level->dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && level->dim > 256) {
            predictive_hierarchy_heartbeat("predictive_h_loop",
                             (float)(i + 1) / (float)level->dim);
        }

        float error = level->state[i] - level->prediction[i];
        level->prediction_error[i] = error;
        error_sum += error * error;
    }

    float rms_error = (level->dim > 0) ? sqrtf(error_sum / level->dim) : 0.0f;
    if (isfinite(rms_error)) {
        level->avg_error = (level->avg_error * 0.99f) + (rms_error * 0.01f);
    }
}

/**
 * @brief Compute precision-weighted free energy for level
 */
static float compute_level_free_energy(const pred_level_t* level) {
    if (!level) {
        return 0.0f;
    }

    float fe = 0.0f;
    for (uint32_t i = 0; i < level->dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && level->dim > 256) {
            predictive_hierarchy_heartbeat("predictive_h_loop",
                             (float)(i + 1) / (float)level->dim);
        }

        float error = level->prediction_error[i];
        fe += level->precision[i] * error * error;
    }

    return fe;
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int pred_hier_default_config(pred_hier_config_t* config) {
    if (!config) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_default_co", 0.0f);


    memset(config, 0, sizeof(*config));

    config->num_levels = PRED_HIER_DEFAULT_LEVELS;
    config->level_configs = NULL;
    config->update_mode = PRED_HIER_UPDATE_SEQUENTIAL;

    config->state_update_rate = PRED_HIER_DEFAULT_UPDATE_RATE;
    config->weight_lr = 0.001f;
    config->precision_lr = 0.01f;
    config->enable_learning = true;
    config->enable_lateral = false;

    config->enable_fep = true;
    config->complexity_weight = 0.01f;

    config->gpu_mode = PRED_HIER_GPU_AUTO;

    config->enable_bio_async = false;

    return NIMCP_SUCCESS;
}

int pred_hier_simple_config(pred_hier_config_t* config,
                             uint32_t num_levels,
                             const uint32_t* dims) {
    if (!config || num_levels == 0 || !dims) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_simple_con", 0.0f);


    pred_hier_default_config(config);
    config->num_levels = num_levels;

    config->level_configs = nimcp_calloc(num_levels, sizeof(pred_level_config_t));
    if (!config->level_configs) {
        return NIMCP_ERROR_MEMORY;
    }

    for (uint32_t i = 0; i < num_levels; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_levels > 256) {
            predictive_hierarchy_heartbeat("predictive_h_loop",
                             (float)(i + 1) / (float)num_levels);
        }

        config->level_configs[i].dim = dims[i];
        config->level_configs[i].gen_hidden_dim = dims[i];
        config->level_configs[i].gen_type = PRED_HIER_GEN_LINEAR;
        config->level_configs[i].initial_precision = PRED_HIER_DEFAULT_PRECISION;
        config->level_configs[i].precision_lr = NIMCP_LEARNING_RATE_DEFAULT;
        config->level_configs[i].learnable_precision = true;
    }

    return NIMCP_SUCCESS;
}

void pred_hier_free_config(pred_hier_config_t* config) {
    if (!config) {
        return;
    }
    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_free_confi", 0.0f);


    if (config->level_configs) {
        nimcp_free(config->level_configs);
        config->level_configs = NULL;
    }
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

predictive_hierarchy_t* pred_hier_create(const pred_hier_config_t* config) {
    if (!config || config->num_levels == 0 || !config->level_configs) {
        NIMCP_LOG_ERROR("Invalid configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pred_hier_create: required parameter is NULL (config, config->level_configs)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_create", 0.0f);


    predictive_hierarchy_t* hier = nimcp_calloc(1, sizeof(predictive_hierarchy_t));
    if (!hier) {
        NIMCP_LOG_ERROR("Failed to allocate hierarchy");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pred_hier_create: hier is NULL");
        return NULL;
    }

    memcpy(&hier->config, config, sizeof(pred_hier_config_t));
    hier->config.level_configs = NULL;

    mutex_attr_t attr = {.type = MUTEX_TYPE_NORMAL};
    hier->mutex = nimcp_mutex_create(&attr);
    if (!hier->mutex) {
        NIMCP_LOG_ERROR("Failed to create mutex");
        nimcp_free(hier);
        hier = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pred_hier_create: hier->mutex is NULL");
        return NULL;
    }

    hier->num_levels = config->num_levels;
    hier->levels = nimcp_calloc(config->num_levels, sizeof(pred_level_t*));
    if (!hier->levels) {
        NIMCP_LOG_ERROR("Failed to allocate level array");
        goto error;
    }

    for (uint32_t i = 0; i < config->num_levels; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && config->num_levels > 256) {
            predictive_hierarchy_heartbeat("predictive_h_loop",
                             (float)(i + 1) / (float)config->num_levels);
        }

        hier->levels[i] = create_level(&config->level_configs[i], i);
        if (!hier->levels[i]) {
            NIMCP_LOG_ERROR("Failed to create level %u", i);
            goto error;
        }
    }

    for (uint32_t i = 0; i < config->num_levels; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && config->num_levels > 256) {
            predictive_hierarchy_heartbeat("predictive_h_loop",
                             (float)(i + 1) / (float)config->num_levels);
        }

        if (i > 0) {
            hier->levels[i]->below = hier->levels[i - 1];
        }
        if (i < config->num_levels - 1) {
            hier->levels[i]->above = hier->levels[i + 1];
        }
    }

    hier->bottom = hier->levels[0];
    hier->top = hier->levels[config->num_levels - 1];

    hier->total_free_energy = 0.0f;
    hier->complexity = 0.0f;
    hier->accuracy = 0.0f;
    hier->training_mode = false;
    hier->step_count = 0;

    hier->stats.avg_level_error = nimcp_calloc(config->num_levels, sizeof(float));
    hier->stats.avg_level_precision = nimcp_calloc(config->num_levels, sizeof(float));
    if (!hier->stats.avg_level_error || !hier->stats.avg_level_precision) {
        goto error;
    }

#ifdef NIMCP_ENABLE_CUDA
    hier->gpu_ctx = NULL;
    hier->gpu_initialized = false;
    if (config->gpu_mode != PRED_HIER_GPU_DISABLED) {
        if (pred_hier_init_gpu(hier, NULL) != NIMCP_SUCCESS) {
            if (config->gpu_mode == PRED_HIER_GPU_REQUIRED) {
                NIMCP_LOG_ERROR("GPU required but init failed");
                goto error;
            }
            NIMCP_LOG_WARN("GPU init failed, using CPU");
        }
    }
#endif

    NIMCP_LOG_INFO("Created predictive hierarchy with %u levels",
                   config->num_levels);
    return hier;

error:
    pred_hier_destroy(hier);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pred_hier_create: resource allocation failed");
    return NULL;
}

void pred_hier_destroy(predictive_hierarchy_t* hier) {
    if (!hier) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_destroy", 0.0f);


    if (hier->levels) {
        for (uint32_t i = 0; i < hier->num_levels; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && hier->num_levels > 256) {
                predictive_hierarchy_heartbeat("predictive_h_loop",
                                 (float)(i + 1) / (float)hier->num_levels);
            }

            destroy_level(hier->levels[i]);
        }
        nimcp_free(hier->levels);
    }

    if (hier->stats.avg_level_error) {
        nimcp_free(hier->stats.avg_level_error);
    }
    if (hier->stats.avg_level_precision) {
        nimcp_free(hier->stats.avg_level_precision);
    }

#ifdef NIMCP_ENABLE_CUDA
    if (hier->gpu_ctx) {
        nimcp_gpu_context_destroy(hier->gpu_ctx);
    }
#endif

    if (hier->mutex) {
        nimcp_mutex_destroy(hier->mutex);
    }

    nimcp_free(hier);
    hier = NULL;
}

int pred_hier_reset(predictive_hierarchy_t* hier) {
    if (!hier) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_reset", 0.0f);


    nimcp_mutex_lock(hier->mutex);

    for (uint32_t i = 0; i < hier->num_levels; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hier->num_levels > 256) {
            predictive_hierarchy_heartbeat("predictive_h_loop",
                             (float)(i + 1) / (float)hier->num_levels);
        }

        pred_level_t* level = hier->levels[i];
        memset(level->state, 0, level->dim * sizeof(float));
        memset(level->prediction, 0, level->dim * sizeof(float));
        memset(level->prediction_error, 0, level->dim * sizeof(float));

        for (uint32_t j = 0; j < level->dim; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && level->dim > 256) {
                predictive_hierarchy_heartbeat("predictive_h_loop",
                                 (float)(j + 1) / (float)level->dim);
            }

            level->precision[j] = PRED_HIER_DEFAULT_PRECISION;
        }

        level->avg_error = 0.0f;
        level->avg_precision = PRED_HIER_DEFAULT_PRECISION;
        level->update_count = 0;
    }

    hier->total_free_energy = 0.0f;
    hier->step_count = 0;
    memset(&hier->stats, 0, sizeof(pred_hier_stats_t));

    nimcp_mutex_unlock(hier->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Internal Unlocked Helpers
 * ============================================================================ */

/**
 * @brief Internal forward pass (caller must hold mutex)
 */
static int forward_unlocked(predictive_hierarchy_t* hier, const float* input) {
    if (!hier || !hier->bottom || !hier->bottom->state || !input) return -1;
    if (hier->bottom->dim == 0 || hier->bottom->dim > 65536) return -1;
    /* Use memmove with validated size — prevents overflow even if dim was corrupted */
    uint32_t copy_bytes = hier->bottom->dim * sizeof(float);
    if (copy_bytes > 65536 * sizeof(float)) return -1;  /* Extra guard */
    memcpy(hier->bottom->state, input, copy_bytes);

    for (uint32_t i = 0; i < hier->num_levels; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hier->num_levels > 256) {
            predictive_hierarchy_heartbeat("predictive_h_loop",
                             (float)(i + 1) / (float)hier->num_levels);
        }

        pred_level_t* level = hier->levels[i];

        if (level->above && level->above->gen_weights) {
            apply_generative_model(level->above, level);
        }

        compute_prediction_error(level);
    }

    hier->stats.forward_passes++;
    return NIMCP_SUCCESS;
}

/**
 * @brief Internal backward pass (caller must hold mutex)
 */
static int backward_unlocked(predictive_hierarchy_t* hier) {
    for (int i = (int)hier->num_levels - 1; i >= 0; i--) {
        pred_level_t* level = hier->levels[i];

        if (level->below && level->gen_weights) {
            apply_generative_model(level, level->below);
        }
    }

    hier->stats.backward_passes++;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Inference API
 * ============================================================================ */

int pred_hier_forward(predictive_hierarchy_t* hier,
                       const float* input) {
    if (!hier || !input) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_forward", 0.0f);


    nimcp_mutex_lock(hier->mutex);
    int ret = forward_unlocked(hier, input);
    nimcp_mutex_unlock(hier->mutex);

    return ret;
}

int pred_hier_backward(predictive_hierarchy_t* hier) {
    if (!hier) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_backward", 0.0f);


    nimcp_mutex_lock(hier->mutex);
    int ret = backward_unlocked(hier);
    nimcp_mutex_unlock(hier->mutex);

    return ret;
}

/**
 * @brief Internal update (caller must hold mutex)
 */
static int update_unlocked(predictive_hierarchy_t* hier,
                            const float* input,
                            pred_hier_result_t* result) {
    int ret = forward_unlocked(hier, input);
    if (ret != NIMCP_SUCCESS) {
        return ret;
    }

    ret = backward_unlocked(hier);
    if (ret != NIMCP_SUCCESS) {
        return ret;
    }

    float kappa = hier->config.state_update_rate;
    for (uint32_t i = 1; i < hier->num_levels; i++) {
        pred_level_t* level = hier->levels[i];
        pred_level_t* below = level->below;

        for (uint32_t j = 0; j < level->dim; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && level->dim > 256) {
                predictive_hierarchy_heartbeat("predictive_h_loop",
                                 (float)(j + 1) / (float)level->dim);
            }

            float bottom_up = 0.0f;
            if (below) {
                bottom_up = below->precision[j] * below->prediction_error[j];
            }

            float top_down = level->precision[j] * level->prediction_error[j];
            float delta = kappa * (bottom_up - top_down);
            level->state[j] += delta;
        }

        level->update_count++;
    }

    hier->total_free_energy = pred_hier_compute_free_energy(hier);
    hier->step_count++;
    hier->stats.full_updates++;

    if (result) {
        result->total_free_energy = hier->total_free_energy;
        result->complexity = hier->complexity;
        result->accuracy = hier->accuracy;
        result->num_levels = hier->num_levels;

        for (uint32_t i = 0; i < hier->num_levels; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && hier->num_levels > 256) {
                predictive_hierarchy_heartbeat("predictive_h_loop",
                                 (float)(i + 1) / (float)hier->num_levels);
            }

            if (result->level_results) {
                pred_level_t* level = hier->levels[i];
                result->level_results[i].free_energy = compute_level_free_energy(level);
                result->level_results[i].precision_weighted_error = result->level_results[i].free_energy;
                result->level_results[i].level_index = i;
            }
        }
    }

    return NIMCP_SUCCESS;
}

int pred_hier_update(predictive_hierarchy_t* hier,
                      const float* input,
                      pred_hier_result_t* result) {
    if (!hier || !input) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_update", 0.0f);


    nimcp_mutex_lock(hier->mutex);
    int ret = update_unlocked(hier, input, result);
    nimcp_mutex_unlock(hier->mutex);

    return ret;
}

int pred_hier_get_prediction(const predictive_hierarchy_t* hier,
                              uint32_t level_index,
                              float* prediction) {
    if (!hier || !prediction || level_index >= hier->num_levels) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_get_predic", 0.0f);


    memcpy(prediction, hier->levels[level_index]->prediction,
           hier->levels[level_index]->dim * sizeof(float));
    return NIMCP_SUCCESS;
}

int pred_hier_get_error(const predictive_hierarchy_t* hier,
                         uint32_t level_index,
                         float* error) {
    if (!hier || !error || level_index >= hier->num_levels) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_get_error", 0.0f);


    memcpy(error, hier->levels[level_index]->prediction_error,
           hier->levels[level_index]->dim * sizeof(float));
    return NIMCP_SUCCESS;
}

int pred_hier_get_state(const predictive_hierarchy_t* hier,
                         uint32_t level_index,
                         float* state) {
    if (!hier || !state || level_index >= hier->num_levels) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_get_state", 0.0f);


    memcpy(state, hier->levels[level_index]->state,
           hier->levels[level_index]->dim * sizeof(float));
    return NIMCP_SUCCESS;
}

int pred_hier_set_state(predictive_hierarchy_t* hier,
                         uint32_t level_index,
                         const float* state) {
    if (!hier || !state || level_index >= hier->num_levels) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_set_state", 0.0f);


    nimcp_mutex_lock(hier->mutex);
    memcpy(hier->levels[level_index]->state, state,
           hier->levels[level_index]->dim * sizeof(float));
    nimcp_mutex_unlock(hier->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Free Energy API
 * ============================================================================ */

float pred_hier_compute_free_energy(predictive_hierarchy_t* hier) {
    if (!hier) {
        return NAN;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_compute_fr", 0.0f);


    float total_fe = 0.0f;
    float accuracy = 0.0f;
    float complexity = 0.0f;

    for (uint32_t i = 0; i < hier->num_levels; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hier->num_levels > 256) {
            predictive_hierarchy_heartbeat("predictive_h_loop",
                             (float)(i + 1) / (float)hier->num_levels);
        }

        float level_fe = compute_level_free_energy(hier->levels[i]);
        total_fe += level_fe;
        accuracy += level_fe;

        if (hier->stats.avg_level_error) {
            hier->stats.avg_level_error[i] = hier->levels[i]->avg_error;
        }
    }

    hier->accuracy = accuracy;
    hier->complexity = complexity;
    hier->total_free_energy = total_fe + hier->config.complexity_weight * complexity;

    if (isfinite(hier->total_free_energy)) {
        hier->stats.avg_free_energy = (hier->stats.avg_free_energy * 0.99f) +
                                       (hier->total_free_energy * 0.01f);
    }

    return hier->total_free_energy;
}

float pred_hier_get_level_free_energy(const predictive_hierarchy_t* hier,
                                       uint32_t level_index) {
    if (!hier || level_index >= hier->num_levels) {
        return NAN;
    }
    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_get_level_", 0.0f);


    return compute_level_free_energy(hier->levels[level_index]);
}

int pred_hier_set_precision(predictive_hierarchy_t* hier,
                             uint32_t level_index,
                             const float* precision) {
    if (!hier || !precision || level_index >= hier->num_levels) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_set_precis", 0.0f);


    nimcp_mutex_lock(hier->mutex);
    pred_level_t* level = hier->levels[level_index];

    for (uint32_t i = 0; i < level->dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && level->dim > 256) {
            predictive_hierarchy_heartbeat("predictive_h_loop",
                             (float)(i + 1) / (float)level->dim);
        }

        float p = precision[i];
        p = fmaxf(p, PRED_HIER_MIN_PRECISION);
        p = fminf(p, PRED_HIER_MAX_PRECISION);
        level->precision[i] = p;
    }

    nimcp_mutex_unlock(hier->mutex);
    return NIMCP_SUCCESS;
}

int pred_hier_get_precision(const predictive_hierarchy_t* hier,
                             uint32_t level_index,
                             float* precision) {
    if (!hier || !precision || level_index >= hier->num_levels) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_get_precis", 0.0f);


    memcpy(precision, hier->levels[level_index]->precision,
           hier->levels[level_index]->dim * sizeof(float));
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Training API
 * ============================================================================ */

int pred_hier_set_training(predictive_hierarchy_t* hier, bool training) {
    if (!hier) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_set_traini", 0.0f);


    nimcp_mutex_lock(hier->mutex);
    hier->training_mode = training;
    nimcp_mutex_unlock(hier->mutex);

    return NIMCP_SUCCESS;
}

int pred_hier_learn_step(predictive_hierarchy_t* hier,
                          const float* input,
                          float* loss) {
    if (!hier || !input) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_learn_step", 0.0f);


    nimcp_mutex_lock(hier->mutex);

    int ret = update_unlocked(hier, input, NULL);
    if (ret != NIMCP_SUCCESS) {
        nimcp_mutex_unlock(hier->mutex);
        return ret;
    }

    if (hier->config.enable_learning) {
        float lr = hier->config.weight_lr;

        for (uint32_t i = 1; i < hier->num_levels; i++) {
            pred_level_t* level = hier->levels[i];
            pred_level_t* below = level->below;

            if (!level->gen_weights || !below) {
                continue;
            }

            /* gen_weights is allocated as dim*dim; skip if below has larger dim */
            if (below->dim > level->dim) {
                continue;
            }

            for (uint32_t j = 0; j < below->dim; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && below->dim > 256) {
                    predictive_hierarchy_heartbeat("predictive_h_loop",
                                     (float)(j + 1) / (float)below->dim);
                }

                float error = below->prediction_error[j];
                float prec = below->precision[j];

                for (uint32_t k = 0; k < level->dim; k++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((k & 0xFF) == 0 && level->dim > 256) {
                        predictive_hierarchy_heartbeat("predictive_h_loop",
                                         (float)(k + 1) / (float)level->dim);
                    }

                    float grad = prec * error * level->state[k];
                    level->gen_weights[j * level->dim + k] += lr * grad;
                }

                if (level->gen_bias) {
                    level->gen_bias[j] += lr * prec * error;
                }
            }
        }
    }

    if (loss) {
        *loss = hier->total_free_energy;
    }

    nimcp_mutex_unlock(hier->mutex);
    return NIMCP_SUCCESS;
}

int pred_hier_update_precision(predictive_hierarchy_t* hier) {
    if (!hier) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_update_pre", 0.0f);


    nimcp_mutex_lock(hier->mutex);

    float alpha = hier->config.precision_lr;

    for (uint32_t i = 0; i < hier->num_levels; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hier->num_levels > 256) {
            predictive_hierarchy_heartbeat("predictive_h_loop",
                             (float)(i + 1) / (float)hier->num_levels);
        }

        pred_level_t* level = hier->levels[i];

        for (uint32_t j = 0; j < level->dim; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && level->dim > 256) {
                predictive_hierarchy_heartbeat("predictive_h_loop",
                                 (float)(j + 1) / (float)level->dim);
            }

            float error = level->prediction_error[j];
            float error_sq = error * error + 1e-8f;
            float new_prec = 1.0f / error_sq;

            new_prec = fminf(new_prec, PRED_HIER_MAX_PRECISION);
            new_prec = fmaxf(new_prec, PRED_HIER_MIN_PRECISION);

            level->precision[j] = (1.0f - alpha) * level->precision[j] +
                                  alpha * new_prec;
        }

        float avg_prec = 0.0f;
        for (uint32_t j = 0; j < level->dim; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && level->dim > 256) {
                predictive_hierarchy_heartbeat("predictive_h_loop",
                                 (float)(j + 1) / (float)level->dim);
            }

            avg_prec += level->precision[j];
        }
        level->avg_precision = (level->dim > 0) ? (avg_prec / level->dim) : 0.0f;

        if (hier->stats.avg_level_precision) {
            hier->stats.avg_level_precision[i] = level->avg_precision;
        }
    }

    nimcp_mutex_unlock(hier->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * GPU API
 * ============================================================================ */

#ifdef NIMCP_ENABLE_CUDA
int pred_hier_init_gpu(predictive_hierarchy_t* hier,
                        struct nimcp_gpu_context_s* gpu_ctx) {
    if (!hier) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_init_gpu", 0.0f);


    if (gpu_ctx) {
        hier->gpu_ctx = gpu_ctx;
    } else {
        hier->gpu_ctx = nimcp_gpu_context_create_auto();
        if (!hier->gpu_ctx) {
            return NIMCP_ERROR_GPU_NOT_AVAILABLE;
        }
    }

    hier->gpu_initialized = true;
    NIMCP_LOG_INFO("GPU initialized for predictive hierarchy");
    return NIMCP_SUCCESS;
}

bool pred_hier_has_gpu(const predictive_hierarchy_t* hier) {
    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_has_gpu", 0.0f);


    return hier && hier->gpu_initialized;
}
#endif

/* ============================================================================
 * Statistics API
 * ============================================================================ */

int pred_hier_get_stats(const predictive_hierarchy_t* hier,
                         pred_hier_stats_t* stats) {
    if (!hier || !stats) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_get_stats", 0.0f);


    stats->forward_passes = hier->stats.forward_passes;
    stats->backward_passes = hier->stats.backward_passes;
    stats->full_updates = hier->stats.full_updates;
    stats->avg_free_energy = hier->stats.avg_free_energy;
    stats->gpu_updates = hier->stats.gpu_updates;
    stats->cpu_updates = hier->stats.cpu_updates;

    return NIMCP_SUCCESS;
}

int pred_hier_reset_stats(predictive_hierarchy_t* hier) {
    if (!hier) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_reset_stat", 0.0f);


    nimcp_mutex_lock(hier->mutex);

    hier->stats.forward_passes = 0;
    hier->stats.backward_passes = 0;
    hier->stats.full_updates = 0;
    hier->stats.avg_free_energy = 0.0f;
    hier->stats.gpu_updates = 0;
    hier->stats.cpu_updates = 0;

    nimcp_mutex_unlock(hier->mutex);
    return NIMCP_SUCCESS;
}

uint32_t pred_hier_num_levels(const predictive_hierarchy_t* hier) {
    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_num_levels", 0.0f);


    return hier ? hier->num_levels : 0;
}

uint32_t pred_hier_level_dim(const predictive_hierarchy_t* hier,
                              uint32_t level_index) {
    if (!hier || level_index >= hier->num_levels) {
        return 0;
    }
    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_level_dim", 0.0f);


    return hier->levels[level_index]->dim;
}

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

int pred_hier_connect_bio_async(predictive_hierarchy_t* hier) {
    if (!hier) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_connect_bi", 0.0f);


    NIMCP_LOG_INFO("Bio-async connection (stub)");
    return NIMCP_SUCCESS;
}

int pred_hier_disconnect_bio_async(predictive_hierarchy_t* hier) {
    if (!hier) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_disconnect", 0.0f);


    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Result Management API
 * ============================================================================ */

pred_hier_result_t* pred_hier_result_create(uint32_t num_levels,
                                             const uint32_t* dims) {
    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_result_cre", 0.0f);


    if (num_levels == 0 || !dims) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pred_hier_result_create: dims is NULL");
        return NULL;
    }

    pred_hier_result_t* result = nimcp_calloc(1, sizeof(pred_hier_result_t));
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate result");

        return NULL;
    }

    result->level_results = nimcp_calloc(num_levels, sizeof(pred_level_result_t));
    if (!result->level_results) {
        nimcp_free(result);
        result = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pred_hier_result_create: result->level_results is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < num_levels; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_levels > 256) {
            predictive_hierarchy_heartbeat("predictive_h_loop",
                             (float)(i + 1) / (float)num_levels);
        }

        result->level_results[i].prediction = nimcp_calloc(dims[i], sizeof(float));
        result->level_results[i].error = nimcp_calloc(dims[i], sizeof(float));

        if (!result->level_results[i].prediction || !result->level_results[i].error) {
            pred_hier_result_destroy(result);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pred_hier_result_create: level allocation failed");
            return NULL;
        }
    }

    result->num_levels = num_levels;
    return result;
}

void pred_hier_result_destroy(pred_hier_result_t* result) {
    if (!result) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_hierarchy_heartbeat("predictive_h_pred_hier_result_des", 0.0f);


    if (result->level_results) {
        for (uint32_t i = 0; i < result->num_levels; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && result->num_levels > 256) {
                predictive_hierarchy_heartbeat("predictive_h_loop",
                                 (float)(i + 1) / (float)result->num_levels);
            }

            if (result->level_results[i].prediction) {
                nimcp_free(result->level_results[i].prediction);
            }
            if (result->level_results[i].error) {
                nimcp_free(result->level_results[i].error);
            }
        }
        nimcp_free(result->level_results);
    }

    nimcp_free(result);
    result = NULL;
}

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

const char* pred_hier_update_mode_to_string(pred_hier_update_mode_t mode) {
    switch (mode) {
        case PRED_HIER_UPDATE_SEQUENTIAL: return "SEQUENTIAL";
        case PRED_HIER_UPDATE_PARALLEL: return "PARALLEL";
        case PRED_HIER_UPDATE_INTERLEAVED: return "INTERLEAVED";
        default: return "UNKNOWN";
    }
}

const char* pred_hier_gen_model_to_string(pred_hier_gen_model_t type) {
    switch (type) {
        case PRED_HIER_GEN_LINEAR: return "LINEAR";
        case PRED_HIER_GEN_MLP: return "MLP";
        case PRED_HIER_GEN_CONV: return "CONV";
        case PRED_HIER_GEN_ATTENTION: return "ATTENTION";
        default: return "UNKNOWN";
    }
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void predictive_hierarchy_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_predictive_hierarchy_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training stubs
 * ============================================================================ */
int predictive_hierarchy_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "predictive_hierarchy_training_begin: NULL argument");
        return -1;
    }
    predictive_hierarchy_heartbeat("predictive_hierarchy_training_begin", 0.0f);
    return 0;
}

int predictive_hierarchy_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "predictive_hierarchy_training_end: NULL argument");
        return -1;
    }
    predictive_hierarchy_heartbeat("predictive_hierarchy_training_end", 1.0f);
    return 0;
}

int predictive_hierarchy_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "predictive_hierarchy_training_step: NULL argument");
        return -1;
    }
    predictive_hierarchy_heartbeat("predictive_hierarchy_training_step", progress);
    return 0;
}
