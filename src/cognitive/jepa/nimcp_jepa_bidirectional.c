/**
 * @file nimcp_jepa_bidirectional.c
 * @brief Bidirectional JEPA Predictor Implementation
 * @version 1.0.0
 * @date 2025-01-04
 */

#include "cognitive/jepa/nimcp_jepa_bidirectional.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(jepa_bidirectional, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* Logging macros - wrap LOG_* for consistent usage */
#define NIMCP_LOG_INFO(...)  LOG_INFO(__VA_ARGS__)
#define NIMCP_LOG_WARN(...)  LOG_WARN(__VA_ARGS__)
#define NIMCP_LOG_ERROR(...) LOG_ERROR(__VA_ARGS__)

/* ============================================================================
 * Private Helpers
 * ============================================================================ */

/**
 * @brief Create a direction-specific predictor
 */
static int create_direction_predictor(jepa_direction_state_t* dir_state,
                                        jepa_direction_t direction,
                                        const jepa_bidir_config_t* config) {
    NIMCP_CHECK_THROW(dir_state && config, NIMCP_ERROR_INVALID_PARAM, "dir_state or config is NULL");

    jepa_predictor_config_t pred_config;
    jepa_predictor_default_config(&pred_config);

    pred_config.input_dim = config->embedding_dim;
    pred_config.output_dim = config->embedding_dim;
    pred_config.hidden_dim = config->hidden_dim;
    pred_config.num_layers = config->num_layers;
    pred_config.activation = config->activation;
    pred_config.learning_rate = config->learning_rate;
    pred_config.weight_decay = config->weight_decay;
    pred_config.dropout_rate = config->dropout_rate;
    pred_config.enable_fep = config->enable_fep;
    pred_config.initial_precision = config->initial_precision;

    dir_state->predictor = jepa_predictor_create(&pred_config);
    if (!dir_state->predictor) {
        return NIMCP_ERROR_MEMORY;
    }

    dir_state->direction = direction;
    dir_state->precision = config->initial_precision;
    dir_state->precision_ema = config->initial_precision;
    dir_state->prediction_count = 0;
    dir_state->cumulative_error = 0.0f;
    dir_state->enabled = false;

    return NIMCP_SUCCESS;
}

/**
 * @brief Destroy direction predictor
 */
static void destroy_direction_predictor(jepa_direction_state_t* dir_state) {
    if (dir_state && dir_state->predictor) {
        jepa_predictor_destroy(dir_state->predictor);
        dir_state->predictor = NULL;
    }
}

/**
 * @brief Should use GPU for this operation?
 */
static inline bool should_use_gpu(const jepa_bidirectional_t* bidir) {
#ifdef NIMCP_ENABLE_CUDA
    if (!bidir || !bidir->gpu_initialized) {
        return false;
    }
    if (bidir->config.gpu_mode == JEPA_BIDIR_GPU_DISABLED) {
        return false;
    }
    if (bidir->config.gpu_mode == JEPA_BIDIR_GPU_REQUIRED ||
        bidir->config.gpu_mode == JEPA_BIDIR_GPU_PREFERRED) {
        return true;
    }
    return false;
#else
    (void)bidir;
    return false;
#endif
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int jepa_bidir_default_config(jepa_bidir_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    jepa_bidirectional_heartbeat("jepa_bidirec_jepa_bidir_default_c", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_INVALID_PARAM, "config is NULL");

    memset(config, 0, sizeof(*config));

    config->embedding_dim = 256;
    config->hidden_dim = 512;
    config->num_layers = 2;
    config->activation = JEPA_ACT_GELU;

    config->enable_forward = true;
    config->enable_backward = true;
    config->enable_lateral = false;
    config->enable_hierarchical = false;
    config->enable_masked = false;
    config->enable_associative = false;

    config->learning_rate = 0.001f;
    config->weight_decay = 0.01f;
    config->precision_lr = JEPA_BIDIR_PRECISION_LR;
    config->dropout_rate = 0.1f;

    config->enable_fep = true;
    config->initial_precision = JEPA_BIDIR_DEFAULT_PRECISION;

    config->gpu_mode = JEPA_BIDIR_GPU_AUTO;
    config->min_batch_for_gpu = 32;

    config->enable_bio_async = false;

    return NIMCP_SUCCESS;
}

int jepa_bidir_validate_config(const jepa_bidir_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    jepa_bidirectional_heartbeat("jepa_bidirec_jepa_bidir_validate_", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_INVALID_PARAM, "config is NULL");
    if (config->embedding_dim == 0 || config->embedding_dim > 4096) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (config->hidden_dim == 0 || config->hidden_dim > 8192) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (config->num_layers == 0 || config->num_layers > 16) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (config->learning_rate <= 0.0f || config->learning_rate > 1.0f) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

jepa_bidirectional_t* jepa_bidirectional_create(const jepa_bidir_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    jepa_bidirectional_heartbeat("jepa_bidirec_create", 0.0f);


    jepa_bidir_config_t default_config;
    if (!config) {
        jepa_bidir_default_config(&default_config);
        config = &default_config;
    }

    if (jepa_bidir_validate_config(config) != NIMCP_SUCCESS) {
        NIMCP_LOG_ERROR("Invalid configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "jepa_bidirectional_create: validation failed");
        return NULL;
    }

    jepa_bidirectional_t* bidir = nimcp_calloc(1, sizeof(jepa_bidirectional_t));
    if (!bidir) {
        NIMCP_LOG_ERROR("Failed to allocate bidirectional predictor");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "jepa_bidirectional_create: bidir is NULL");
        return NULL;
    }

    memcpy(&bidir->config, config, sizeof(jepa_bidir_config_t));

    mutex_attr_t attr = {.type = MUTEX_TYPE_NORMAL};
    bidir->mutex = nimcp_mutex_create(&attr);
    if (!bidir->mutex) {
        NIMCP_LOG_ERROR("Failed to create mutex");
        nimcp_free(bidir);
        bidir = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "jepa_bidirectional_create: bidir->mutex is NULL");
        return NULL;
    }

    uint32_t dir_count = 0;
    int result = 0;

    if (config->enable_forward) {
        result = create_direction_predictor(&bidir->directions[JEPA_DIR_FORWARD],
                                             JEPA_DIR_FORWARD, config);
        if (result != NIMCP_SUCCESS) {
            goto error;
        }
        bidir->directions[JEPA_DIR_FORWARD].enabled = true;
        dir_count++;
    }

    if (config->enable_backward) {
        result = create_direction_predictor(&bidir->directions[JEPA_DIR_BACKWARD],
                                             JEPA_DIR_BACKWARD, config);
        if (result != NIMCP_SUCCESS) {
            goto error;
        }
        bidir->directions[JEPA_DIR_BACKWARD].enabled = true;
        dir_count++;
    }

    if (config->enable_lateral) {
        result = create_direction_predictor(&bidir->directions[JEPA_DIR_LATERAL],
                                             JEPA_DIR_LATERAL, config);
        if (result != NIMCP_SUCCESS) {
            goto error;
        }
        bidir->directions[JEPA_DIR_LATERAL].enabled = true;
        dir_count++;
    }

    if (config->enable_hierarchical) {
        result = create_direction_predictor(&bidir->directions[JEPA_DIR_HIERARCHICAL_UP],
                                             JEPA_DIR_HIERARCHICAL_UP, config);
        if (result != NIMCP_SUCCESS) {
            goto error;
        }
        bidir->directions[JEPA_DIR_HIERARCHICAL_UP].enabled = true;
        dir_count++;

        result = create_direction_predictor(&bidir->directions[JEPA_DIR_HIERARCHICAL_DOWN],
                                             JEPA_DIR_HIERARCHICAL_DOWN, config);
        if (result != NIMCP_SUCCESS) {
            goto error;
        }
        bidir->directions[JEPA_DIR_HIERARCHICAL_DOWN].enabled = true;
        dir_count++;
    }

    if (config->enable_masked) {
        result = create_direction_predictor(&bidir->directions[JEPA_DIR_MASKED],
                                             JEPA_DIR_MASKED, config);
        if (result != NIMCP_SUCCESS) {
            goto error;
        }
        bidir->directions[JEPA_DIR_MASKED].enabled = true;
        dir_count++;
    }

    if (config->enable_associative) {
        result = create_direction_predictor(&bidir->directions[JEPA_DIR_ASSOCIATIVE],
                                             JEPA_DIR_ASSOCIATIVE, config);
        if (result != NIMCP_SUCCESS) {
            goto error;
        }
        bidir->directions[JEPA_DIR_ASSOCIATIVE].enabled = true;
        dir_count++;
    }

    bidir->active_direction_count = dir_count;
    bidir->state = JEPA_BIDIR_STATE_IDLE;
    bidir->training_mode = false;
    bidir->step_count = 0;
    bidir->total_free_energy = 0.0f;

#ifdef NIMCP_ENABLE_CUDA
    bidir->gpu_ctx = NULL;
    bidir->gpu_initialized = false;
    if (config->gpu_mode != JEPA_BIDIR_GPU_DISABLED) {
        if (jepa_bidirectional_init_gpu(bidir, NULL) != NIMCP_SUCCESS) {
            if (config->gpu_mode == JEPA_BIDIR_GPU_REQUIRED) {
                NIMCP_LOG_ERROR("GPU required but init failed");
                goto error;
            }
            NIMCP_LOG_WARN("GPU init failed, using CPU");
        }
    }
#endif

    NIMCP_LOG_INFO("Created bidirectional JEPA with %u directions", dir_count);
    return bidir;

error:
    jepa_bidirectional_destroy(bidir);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "jepa_bidirectional_create: operation failed");
    return NULL;
}

void jepa_bidirectional_destroy(jepa_bidirectional_t* bidir) {
    if (!bidir) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_bidirectional_heartbeat("jepa_bidirec_destroy", 0.0f);


    for (int i = 0; i < JEPA_DIR_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && JEPA_DIR_COUNT > 256) {
            jepa_bidirectional_heartbeat("jepa_bidirec_loop",
                             (float)(i + 1) / (float)JEPA_DIR_COUNT);
        }

        destroy_direction_predictor(&bidir->directions[i]);
    }

#ifdef NIMCP_ENABLE_CUDA
    if (bidir->gpu_ctx) {
        nimcp_gpu_context_destroy(bidir->gpu_ctx);
        bidir->gpu_ctx = NULL;
    }
#endif

    if (bidir->mutex) {
        nimcp_mutex_destroy(bidir->mutex);
    }

    nimcp_free(bidir);
    bidir = NULL;
}

int jepa_bidirectional_reset(jepa_bidirectional_t* bidir) {
    /* Phase 8: Heartbeat at operation start */
    jepa_bidirectional_heartbeat("jepa_bidirec_reset", 0.0f);


    NIMCP_CHECK_THROW(bidir, NIMCP_ERROR_INVALID_PARAM, "bidir is NULL");

    nimcp_mutex_lock(bidir->mutex);

    for (int i = 0; i < JEPA_DIR_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && JEPA_DIR_COUNT > 256) {
            jepa_bidirectional_heartbeat("jepa_bidirec_loop",
                             (float)(i + 1) / (float)JEPA_DIR_COUNT);
        }

        if (bidir->directions[i].predictor) {
            jepa_predictor_reset(bidir->directions[i].predictor);
            bidir->directions[i].precision = bidir->config.initial_precision;
            bidir->directions[i].precision_ema = bidir->config.initial_precision;
            bidir->directions[i].prediction_count = 0;
            bidir->directions[i].cumulative_error = 0.0f;
        }
    }

    bidir->state = JEPA_BIDIR_STATE_IDLE;
    bidir->step_count = 0;
    bidir->total_free_energy = 0.0f;
    memset(&bidir->stats, 0, sizeof(bidir->stats));

    nimcp_mutex_unlock(bidir->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Prediction API
 * ============================================================================ */

int jepa_bidirectional_predict(jepa_bidirectional_t* bidir,
                                jepa_direction_t direction,
                                const jepa_latent_t* input,
                                jepa_bidir_result_t* result) {
    /* Phase 8: Heartbeat at operation start */
    jepa_bidirectional_heartbeat("jepa_bidirec_predict", 0.0f);


    NIMCP_CHECK_THROW(bidir && input && result && direction < JEPA_DIR_COUNT, NIMCP_ERROR_INVALID_PARAM, "invalid parameters");

    nimcp_mutex_lock(bidir->mutex);

    jepa_direction_state_t* dir_state = &bidir->directions[direction];
    if (!dir_state->enabled || !dir_state->predictor) {
        nimcp_mutex_unlock(bidir->mutex);
        NIMCP_LOG_ERROR("Direction %s not enabled",
                        jepa_direction_to_string(direction));
        return NIMCP_ERROR_NOT_FOUND;
    }

    bidir->state = JEPA_BIDIR_STATE_PREDICTING;

    int ret = jepa_predictor_predict(dir_state->predictor, input, result->prediction);
    if (ret != NIMCP_SUCCESS) {
        bidir->state = JEPA_BIDIR_STATE_ERROR;
        nimcp_mutex_unlock(bidir->mutex);
        return ret;
    }

    result->direction = direction;
    result->precision = dir_state->precision;
    result->confidence = 1.0f / (1.0f + expf(-dir_state->precision));
    result->free_energy = 0.0f;

    dir_state->prediction_count++;
    bidir->stats.total_predictions++;

    switch (direction) {
        case JEPA_DIR_FORWARD:
            bidir->stats.forward_predictions++;
            break;
        case JEPA_DIR_BACKWARD:
            bidir->stats.backward_predictions++;
            break;
        case JEPA_DIR_LATERAL:
            bidir->stats.lateral_predictions++;
            break;
        case JEPA_DIR_HIERARCHICAL_UP:
        case JEPA_DIR_HIERARCHICAL_DOWN:
            bidir->stats.hierarchical_predictions++;
            break;
        default:
            break;
    }

    if (should_use_gpu(bidir)) {
        bidir->stats.gpu_predictions++;
    } else {
        bidir->stats.cpu_predictions++;
    }

    bidir->state = JEPA_BIDIR_STATE_IDLE;
    nimcp_mutex_unlock(bidir->mutex);

    return NIMCP_SUCCESS;
}

int jepa_bidirectional_predict_multi(jepa_bidirectional_t* bidir,
                                      const jepa_bidir_multi_request_t* request,
                                      jepa_bidir_multi_result_t* result) {
    /* Phase 8: Heartbeat at operation start */
    jepa_bidirectional_heartbeat("jepa_bidirec_predict_multi", 0.0f);


    NIMCP_CHECK_THROW(bidir && request && result, NIMCP_ERROR_INVALID_PARAM, "bidir, request, or result is NULL");
    NIMCP_CHECK_THROW(request->num_directions > 0 && request->directions && request->inputs, NIMCP_ERROR_INVALID_PARAM, "invalid request parameters");

    nimcp_mutex_lock(bidir->mutex);
    bidir->state = JEPA_BIDIR_STATE_PREDICTING;

    result->num_results = request->num_directions;
    result->total_free_energy = 0.0f;
    float total_confidence = 0.0f;
    uint32_t successful_predictions = 0;

    for (uint32_t i = 0; i < request->num_directions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && request->num_directions > 256) {
            jepa_bidirectional_heartbeat("jepa_bidirec_loop",
                             (float)(i + 1) / (float)request->num_directions);
        }

        jepa_direction_t dir = request->directions[i];
        if (dir >= JEPA_DIR_COUNT) {
            continue;
        }

        jepa_direction_state_t* dir_state = &bidir->directions[dir];
        if (!dir_state->enabled || !dir_state->predictor) {
            continue;
        }

        int ret = jepa_predictor_predict(dir_state->predictor,
                                          request->inputs[i],
                                          result->results[i].prediction);
        if (ret == NIMCP_SUCCESS) {
            result->results[i].direction = dir;
            result->results[i].precision = dir_state->precision;
            result->results[i].confidence = 1.0f / (1.0f + expf(-dir_state->precision));
            total_confidence += result->results[i].confidence;
            dir_state->prediction_count++;
            successful_predictions++;
        }
    }

    /* Divide by count of successful predictions, not total requested */
    result->avg_confidence = (successful_predictions > 0)
        ? total_confidence / (float)successful_predictions
        : 0.0f;
    bidir->stats.total_predictions += request->num_directions;

    bidir->state = JEPA_BIDIR_STATE_IDLE;
    nimcp_mutex_unlock(bidir->mutex);

    return NIMCP_SUCCESS;
}

int jepa_bidirectional_predict_forward(jepa_bidirectional_t* bidir,
                                        const jepa_latent_t* input,
                                        jepa_latent_t* prediction) {
    /* Phase 8: Heartbeat at operation start */
    jepa_bidirectional_heartbeat("jepa_bidirec_predict_forward", 0.0f);


    jepa_bidir_result_t result = {.prediction = prediction};
    return jepa_bidirectional_predict(bidir, JEPA_DIR_FORWARD, input, &result);
}

int jepa_bidirectional_predict_backward(jepa_bidirectional_t* bidir,
                                         const jepa_latent_t* input,
                                         jepa_latent_t* prediction) {
    /* Phase 8: Heartbeat at operation start */
    jepa_bidirectional_heartbeat("jepa_bidirec_predict_backward", 0.0f);


    jepa_bidir_result_t result = {.prediction = prediction};
    return jepa_bidirectional_predict(bidir, JEPA_DIR_BACKWARD, input, &result);
}

int jepa_bidirectional_predict_lateral(jepa_bidirectional_t* bidir,
                                        const jepa_latent_t* input,
                                        jepa_latent_t* prediction) {
    /* Phase 8: Heartbeat at operation start */
    jepa_bidirectional_heartbeat("jepa_bidirec_predict_lateral", 0.0f);


    jepa_bidir_result_t result = {.prediction = prediction};
    return jepa_bidirectional_predict(bidir, JEPA_DIR_LATERAL, input, &result);
}

int jepa_bidirectional_predict_hierarchical(jepa_bidirectional_t* bidir,
                                             const jepa_latent_t* input,
                                             bool up,
                                             jepa_latent_t* prediction) {
    /* Phase 8: Heartbeat at operation start */
    jepa_bidirectional_heartbeat("jepa_bidirec_predict_hierarchical", 0.0f);


    jepa_direction_t dir = up ? JEPA_DIR_HIERARCHICAL_UP : JEPA_DIR_HIERARCHICAL_DOWN;
    jepa_bidir_result_t result = {.prediction = prediction};
    return jepa_bidirectional_predict(bidir, dir, input, &result);
}

/* ============================================================================
 * Free Energy API
 * ============================================================================ */

float jepa_bidirectional_compute_free_energy(jepa_bidirectional_t* bidir) {
    if (!bidir) {
        return NAN;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_bidirectional_heartbeat("jepa_bidirec_compute_free_energy", 0.0f);


    nimcp_mutex_lock(bidir->mutex);

    float total_fe = 0.0f;
    uint32_t active_count = 0;

    for (int i = 0; i < JEPA_DIR_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && JEPA_DIR_COUNT > 256) {
            jepa_bidirectional_heartbeat("jepa_bidirec_loop",
                             (float)(i + 1) / (float)JEPA_DIR_COUNT);
        }

        jepa_direction_state_t* dir_state = &bidir->directions[i];
        if (dir_state->enabled && dir_state->predictor) {
            float avg_error = 0.0f;
            if (dir_state->prediction_count > 0) {
                avg_error = dir_state->cumulative_error /
                           (float)dir_state->prediction_count;
            }
            total_fe += dir_state->precision * avg_error;
            active_count++;
        }
    }

    bidir->total_free_energy = total_fe;
    if (isfinite(total_fe)) {
        bidir->stats.avg_free_energy = (bidir->stats.avg_free_energy * 0.99f) +
                                       (total_fe * 0.01f);
    }

    nimcp_mutex_unlock(bidir->mutex);
    return total_fe;
}

/**
 * @brief Internal unlocked precision update helper
 * @note Caller must hold bidir->mutex
 */
static int update_precision_unlocked(jepa_bidirectional_t* bidir,
                                      jepa_direction_t direction,
                                      float error) {
    jepa_direction_state_t* dir_state = &bidir->directions[direction];
    if (!dir_state->enabled) {
        return NIMCP_ERROR_NOT_FOUND;
    }

    float error_sq = error * error;
    dir_state->cumulative_error += error_sq;

    float inv_variance = 1.0f / (error_sq + 1e-8f);
    inv_variance = fminf(inv_variance, JEPA_BIDIR_MAX_PRECISION);
    inv_variance = fmaxf(inv_variance, JEPA_BIDIR_MIN_PRECISION);

    float alpha = bidir->config.precision_lr;
    dir_state->precision_ema = (1.0f - alpha) * dir_state->precision_ema +
                               alpha * inv_variance;
    dir_state->precision = dir_state->precision_ema;

    bidir->stats.avg_precision[direction] = dir_state->precision;

    return NIMCP_SUCCESS;
}

int jepa_bidirectional_update_precision(jepa_bidirectional_t* bidir,
                                         jepa_direction_t direction,
                                         float error) {
    /* Phase 8: Heartbeat at operation start */
    jepa_bidirectional_heartbeat("jepa_bidirec_update_precision", 0.0f);


    NIMCP_CHECK_THROW(bidir && direction < JEPA_DIR_COUNT, NIMCP_ERROR_INVALID_PARAM, "bidir is NULL or invalid direction");

    nimcp_mutex_lock(bidir->mutex);
    int ret = update_precision_unlocked(bidir, direction, error);
    nimcp_mutex_unlock(bidir->mutex);

    return ret;
}

float jepa_bidirectional_get_precision(const jepa_bidirectional_t* bidir,
                                        jepa_direction_t direction) {
    if (!bidir || direction >= JEPA_DIR_COUNT) {
        return NAN;
    }
    /* Phase 8: Heartbeat at operation start */
    jepa_bidirectional_heartbeat("jepa_bidirec_get_precision", 0.0f);

    nimcp_mutex_lock(bidir->mutex);
    float precision = bidir->directions[direction].precision;
    nimcp_mutex_unlock(bidir->mutex);

    return precision;
}

int jepa_bidirectional_set_precision(jepa_bidirectional_t* bidir,
                                      jepa_direction_t direction,
                                      float precision) {
    /* Phase 8: Heartbeat at operation start */
    jepa_bidirectional_heartbeat("jepa_bidirec_set_precision", 0.0f);


    NIMCP_CHECK_THROW(bidir && direction < JEPA_DIR_COUNT, NIMCP_ERROR_INVALID_PARAM, "bidir is NULL or invalid direction");
    NIMCP_CHECK_THROW(precision >= JEPA_BIDIR_MIN_PRECISION && precision <= JEPA_BIDIR_MAX_PRECISION, NIMCP_ERROR_INVALID_PARAM, "precision out of range");

    nimcp_mutex_lock(bidir->mutex);
    bidir->directions[direction].precision = precision;
    bidir->directions[direction].precision_ema = precision;
    nimcp_mutex_unlock(bidir->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Training API
 * ============================================================================ */

int jepa_bidirectional_set_training(jepa_bidirectional_t* bidir, bool training) {
    /* Phase 8: Heartbeat at operation start */
    jepa_bidirectional_heartbeat("jepa_bidirec_set_training", 0.0f);


    NIMCP_CHECK_THROW(bidir, NIMCP_ERROR_INVALID_PARAM, "bidir is NULL");

    nimcp_mutex_lock(bidir->mutex);

    bidir->training_mode = training;
    bidir->state = training ? JEPA_BIDIR_STATE_TRAINING : JEPA_BIDIR_STATE_IDLE;

    for (int i = 0; i < JEPA_DIR_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && JEPA_DIR_COUNT > 256) {
            jepa_bidirectional_heartbeat("jepa_bidirec_loop",
                             (float)(i + 1) / (float)JEPA_DIR_COUNT);
        }

        if (bidir->directions[i].predictor) {
            jepa_predictor_set_training(bidir->directions[i].predictor, training);
        }
    }

    nimcp_mutex_unlock(bidir->mutex);
    return NIMCP_SUCCESS;
}

int jepa_bidirectional_train_step(jepa_bidirectional_t* bidir,
                                   jepa_direction_t direction,
                                   const jepa_latent_t* input,
                                   const jepa_latent_t* target,
                                   float* loss) {
    /* Phase 8: Heartbeat at operation start */
    jepa_bidirectional_heartbeat("jepa_bidirec_train_step", 0.0f);


    NIMCP_CHECK_THROW(bidir && input && target && direction < JEPA_DIR_COUNT, NIMCP_ERROR_INVALID_PARAM, "invalid parameters");

    nimcp_mutex_lock(bidir->mutex);

    jepa_direction_state_t* dir_state = &bidir->directions[direction];
    if (!dir_state->enabled || !dir_state->predictor) {
        nimcp_mutex_unlock(bidir->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    float step_loss = 0.0f;
    int ret = jepa_predictor_train_step(dir_state->predictor, input, target, &step_loss);

    if (ret == NIMCP_SUCCESS) {
        update_precision_unlocked(bidir, direction, sqrtf(step_loss));
        bidir->step_count++;

        if (loss) {
            *loss = step_loss;
        }
    }

    nimcp_mutex_unlock(bidir->mutex);
    return ret;
}

int jepa_bidirectional_set_direction_enabled(jepa_bidirectional_t* bidir,
                                              jepa_direction_t direction,
                                              bool enabled) {
    /* Phase 8: Heartbeat at operation start */
    jepa_bidirectional_heartbeat("jepa_bidirec_set_direction_enable", 0.0f);


    NIMCP_CHECK_THROW(bidir && direction < JEPA_DIR_COUNT, NIMCP_ERROR_INVALID_PARAM, "bidir is NULL or invalid direction");

    nimcp_mutex_lock(bidir->mutex);

    if (enabled && !bidir->directions[direction].predictor) {
        int ret = create_direction_predictor(&bidir->directions[direction],
                                              direction, &bidir->config);
        if (ret != NIMCP_SUCCESS) {
            nimcp_mutex_unlock(bidir->mutex);
            return ret;
        }
        bidir->active_direction_count++;
    }

    bidir->directions[direction].enabled = enabled;

    nimcp_mutex_unlock(bidir->mutex);
    return NIMCP_SUCCESS;
}

bool jepa_bidirectional_is_direction_enabled(const jepa_bidirectional_t* bidir,
                                              jepa_direction_t direction) {
    if (!bidir || direction >= JEPA_DIR_COUNT) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    jepa_bidirectional_heartbeat("jepa_bidirec_is_direction_enabled", 0.0f);

    nimcp_mutex_lock(bidir->mutex);
    bool enabled = bidir->directions[direction].enabled;
    nimcp_mutex_unlock(bidir->mutex);

    return enabled;
}

/* ============================================================================
 * GPU API
 * ============================================================================ */

#ifdef NIMCP_ENABLE_CUDA
int jepa_bidirectional_init_gpu(jepa_bidirectional_t* bidir,
                                 struct nimcp_gpu_context_s* gpu_ctx) {
    /* Phase 8: Heartbeat at operation start */
    jepa_bidirectional_heartbeat("jepa_bidirec_init_gpu", 0.0f);


    NIMCP_CHECK_THROW(bidir, NIMCP_ERROR_INVALID_PARAM, "bidir is NULL");

    if (gpu_ctx) {
        bidir->gpu_ctx = gpu_ctx;
    } else {
        bidir->gpu_ctx = nimcp_gpu_context_create_auto();
        if (!bidir->gpu_ctx) {
            return NIMCP_ERROR_GPU_NOT_AVAILABLE;
        }
    }

    bidir->gpu_initialized = true;
    NIMCP_LOG_INFO("GPU initialized for bidirectional JEPA");
    return NIMCP_SUCCESS;
}

bool jepa_bidirectional_has_gpu(const jepa_bidirectional_t* bidir) {
    /* Phase 8: Heartbeat at operation start */
    jepa_bidirectional_heartbeat("jepa_bidirec_has_gpu", 0.0f);


    return bidir && bidir->gpu_initialized;
}

struct nimcp_gpu_context_s* jepa_bidirectional_get_gpu_ctx(
    const jepa_bidirectional_t* bidir) {
    return bidir ? bidir->gpu_ctx : NULL;
}
#endif

/* ============================================================================
 * Statistics API
 * ============================================================================ */

int jepa_bidirectional_get_stats(const jepa_bidirectional_t* bidir,
                                  jepa_bidir_stats_t* stats) {
    /* Phase 8: Heartbeat at operation start */
    jepa_bidirectional_heartbeat("jepa_bidirec_get_stats", 0.0f);


    NIMCP_CHECK_THROW(bidir && stats, NIMCP_ERROR_INVALID_PARAM, "bidir or stats is NULL");
    memcpy(stats, &bidir->stats, sizeof(jepa_bidir_stats_t));
    return NIMCP_SUCCESS;
}

int jepa_bidirectional_reset_stats(jepa_bidirectional_t* bidir) {
    /* Phase 8: Heartbeat at operation start */
    jepa_bidirectional_heartbeat("jepa_bidirec_reset_stats", 0.0f);


    NIMCP_CHECK_THROW(bidir, NIMCP_ERROR_INVALID_PARAM, "bidir is NULL");

    nimcp_mutex_lock(bidir->mutex);
    memset(&bidir->stats, 0, sizeof(jepa_bidir_stats_t));
    nimcp_mutex_unlock(bidir->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

int jepa_bidirectional_connect_bio_async(jepa_bidirectional_t* bidir) {
    /* Phase 8: Heartbeat at operation start */
    jepa_bidirectional_heartbeat("jepa_bidirec_connect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(bidir, NIMCP_ERROR_INVALID_PARAM, "bidir is NULL");
    NIMCP_LOG_INFO("Bio-async connection (stub)");
    return NIMCP_SUCCESS;
}

int jepa_bidirectional_disconnect_bio_async(jepa_bidirectional_t* bidir) {
    /* Phase 8: Heartbeat at operation start */
    jepa_bidirectional_heartbeat("jepa_bidirec_disconnect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(bidir, NIMCP_ERROR_INVALID_PARAM, "bidir is NULL");
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

const char* jepa_direction_to_string(jepa_direction_t direction) {
    switch (direction) {
        case JEPA_DIR_FORWARD: return "FORWARD";
        case JEPA_DIR_BACKWARD: return "BACKWARD";
        case JEPA_DIR_LATERAL: return "LATERAL";
        case JEPA_DIR_HIERARCHICAL_UP: return "HIERARCHICAL_UP";
        case JEPA_DIR_HIERARCHICAL_DOWN: return "HIERARCHICAL_DOWN";
        case JEPA_DIR_MASKED: return "MASKED";
        case JEPA_DIR_ASSOCIATIVE: return "ASSOCIATIVE";
        default: return "UNKNOWN";
    }
}

const char* jepa_bidir_state_to_string(jepa_bidir_state_t state) {
    switch (state) {
        case JEPA_BIDIR_STATE_IDLE: return "IDLE";
        case JEPA_BIDIR_STATE_PREDICTING: return "PREDICTING";
        case JEPA_BIDIR_STATE_TRAINING: return "TRAINING";
        case JEPA_BIDIR_STATE_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

/* ============================================================================
 * Result Management API
 * ============================================================================ */

jepa_bidir_result_t* jepa_bidir_result_create(uint32_t dim) {
    /* Phase 8: Heartbeat at operation start */
    jepa_bidirectional_heartbeat("jepa_bidirec_jepa_bidir_result_cr", 0.0f);


    jepa_bidir_result_t* result = nimcp_calloc(1, sizeof(jepa_bidir_result_t));
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate result");

        return NULL;
    }

    result->prediction = jepa_latent_create_dim(dim);
    if (!result->prediction) {
        nimcp_free(result);
        result = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "jepa_bidir_result_create: result->prediction is NULL");
        return NULL;
    }

    return result;
}

void jepa_bidir_result_destroy(jepa_bidir_result_t* result) {
    if (!result) {
        return;
    }
    /* Phase 8: Heartbeat at operation start */
    jepa_bidirectional_heartbeat("jepa_bidirec_jepa_bidir_result_de", 0.0f);


    if (result->prediction) {
        jepa_latent_destroy(result->prediction);
    }
    nimcp_free(result);
    result = NULL;
}

jepa_bidir_multi_result_t* jepa_bidir_multi_result_create(uint32_t num_directions,
                                                           uint32_t dim) {
    /* Phase 8: Heartbeat at operation start */
    jepa_bidirectional_heartbeat("jepa_bidirec_jepa_bidir_multi_res", 0.0f);


    jepa_bidir_multi_result_t* result = nimcp_calloc(1, sizeof(jepa_bidir_multi_result_t));
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate result");

        return NULL;
    }

    result->results = nimcp_calloc(num_directions, sizeof(jepa_bidir_result_t));
    if (!result->results) {
        nimcp_free(result);
        result = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "jepa_bidir_result_destroy: result->results is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < num_directions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_directions > 256) {
            jepa_bidirectional_heartbeat("jepa_bidirec_loop",
                             (float)(i + 1) / (float)num_directions);
        }

        result->results[i].prediction = jepa_latent_create_dim(dim);
        if (!result->results[i].prediction) {
            for (uint32_t j = 0; j < i; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && i > 256) {
                    jepa_bidirectional_heartbeat("jepa_bidirec_loop",
                                     (float)(j + 1) / (float)i);
                }

                jepa_latent_destroy(result->results[j].prediction);
            }
            nimcp_free(result->results);
            nimcp_free(result);
            result = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "jepa_bidir_result_destroy: validation failed");
            return NULL;
        }
    }

    result->num_results = num_directions;
    return result;
}

void jepa_bidir_multi_result_destroy(jepa_bidir_multi_result_t* result) {
    if (!result) {
        return;
    }
    /* Phase 8: Heartbeat at operation start */
    jepa_bidirectional_heartbeat("jepa_bidirec_jepa_bidir_multi_res", 0.0f);


    if (result->results) {
        for (uint32_t i = 0; i < result->num_results; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && result->num_results > 256) {
                jepa_bidirectional_heartbeat("jepa_bidirec_loop",
                                 (float)(i + 1) / (float)result->num_results);
            }

            if (result->results[i].prediction) {
                jepa_latent_destroy(result->results[i].prediction);
            }
        }
        nimcp_free(result->results);
    }
    nimcp_free(result);
    result = NULL;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent + Training Lifecycle
 * ============================================================================ */

void jepa_bidirectional_set_instance_health_agent(void* instance,
                                                   nimcp_health_agent_t* agent) {
    (void)instance;
    /* Non-bridge: set global agent as fallback */
    g_jepa_bidirectional_health_agent = agent;
}

int jepa_bidirectional_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_bidirectional_training_begin: NULL argument");
        return -1;
    }
    jepa_bidirectional_heartbeat_instance(g_jepa_bidirectional_health_agent,
                                          "jepa_bidirec_training_begin", 0.0f);
    (void)instance;
    return 0;
}

int jepa_bidirectional_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_bidirectional_training_end: NULL argument");
        return -1;
    }
    jepa_bidirectional_heartbeat_instance(g_jepa_bidirectional_health_agent,
                                          "jepa_bidirec_training_end", 1.0f);
    (void)instance;
    return 0;
}

int jepa_bidirectional_training_step(void* instance, uint32_t step) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_bidirectional_training_step: NULL argument");
        return -1;
    }
    float progress = (step % 100) / 100.0f;
    jepa_bidirectional_heartbeat_instance(g_jepa_bidirectional_health_agent,
                                          "jepa_bidirec_training_step", progress);
    (void)instance;
    return 0;
}
