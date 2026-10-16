/**
 * @file nimcp_precognition.c
 * @brief Implementation of superhuman advanced pattern prediction module
 *
 * WHAT: Provides precognition-like advanced pattern prediction capabilities
 * WHY:  Enable superhuman foresight through long-horizon probabilistic forecasting
 * HOW:  Hierarchical temporal prediction, probabilistic state modeling, anomaly detection
 *
 * @version Phase T12: Superhuman Enhancement Modules
 * @date 2026-01-13
 */

#include "superhuman/nimcp_precognition.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/rng/nimcp_rand.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_learning_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(precognition)

/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define PRECOGNITION_LOG_MODULE "PRECOGNITION"

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Circular buffer for observation history
 */
typedef struct {
    observation_t* buffer;
    uint32_t capacity;
    uint32_t head;
    uint32_t count;
} history_buffer_t;

/**
 * @brief Simple prediction model state
 */
typedef struct {
    float* weights;                      /**< Feature weights */
    float* bias;                         /**< Feature bias */
    uint32_t feature_dim;
    float learning_rate;
    float momentum;
    float* velocity;                     /**< Momentum velocity */
} prediction_model_t;

/**
 * @brief Pending prediction for verification
 */
typedef struct pending_prediction {
    uint64_t prediction_id;
    uncertain_time_t predicted_time;
    float* predicted_features;
    float* predicted_variance;
    uint32_t feature_count;
    struct pending_prediction* next;
} pending_prediction_t;

/**
 * @brief Internal module structure
 */
struct precognition_module {
    /* Configuration */
    precognition_config_t config;

    /* Observation history */
    history_buffer_t history;

    /* Prediction models (one per horizon) */
    prediction_model_t models[HORIZON_COUNT];

    /* Running statistics for anomaly detection */
    float* running_mean;
    float* running_variance;
    uint64_t stats_count;

    /* Causal model */
    causal_model_t causal_model;

    /* Pending predictions for verification */
    pending_prediction_t* pending_predictions;
    uint64_t next_prediction_id;

    /* State */
    precognition_status_t status;
    precognition_error_t last_error;

    /* Callbacks */
    precognition_prediction_callback_t prediction_callback;
    void* prediction_user_data;
    precognition_anomaly_callback_t anomaly_callback;
    void* anomaly_user_data;
    precognition_warning_callback_t warning_callback;
    void* warning_user_data;
    precognition_verification_callback_t verification_callback;
    void* verification_user_data;

    /* Statistics */
    precognition_stats_t stats;
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Set error state
 */
static void set_error(precognition_module_t* module, precognition_error_t error) {
    if (!module) return;
    module->last_error = error;
    if (error != PRECOGNITION_ERROR_NONE) {
        module->status = PRECOGNITION_STATUS_ERROR;
        LOG_ERROR("[%s] Error: %d", PRECOGNITION_LOG_MODULE, error);
    }
}

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/**
 * @brief Convert horizon to approximate milliseconds
 */
static float horizon_to_ms(prediction_horizon_t horizon) {
    switch (horizon) {
        case HORIZON_IMMEDIATE: return 100.0f;       /* 100 ms */
        case HORIZON_SHORT:     return 10000.0f;     /* 10 seconds */
        case HORIZON_MEDIUM:    return 600000.0f;    /* 10 minutes */
        case HORIZON_LONG:      return 3600000.0f;   /* 1 hour */
        case HORIZON_EXTENDED:  return 86400000.0f;  /* 1 day */
        case HORIZON_DISTANT:   return 604800000.0f; /* 1 week */
        default: return 1000.0f;
    }
}

/**
 * @brief Compute confidence decay for horizon
 */
static float compute_confidence_decay(
    precognition_module_t* module,
    prediction_horizon_t horizon
) {
    float base_decay = module->config.confidence_decay;
    return powf(base_decay, (float)horizon);
}

/**
 * @brief Convert confidence to level
 */
static confidence_level_t confidence_to_level(float confidence) {
    if (confidence < 0.2f) return CONFIDENCE_VERY_LOW;
    if (confidence < 0.4f) return CONFIDENCE_LOW;
    if (confidence < 0.6f) return CONFIDENCE_MODERATE;
    if (confidence < 0.8f) return CONFIDENCE_HIGH;
    return CONFIDENCE_VERY_HIGH;
}

/**
 * @brief Initialize history buffer
 */
static bool init_history_buffer(history_buffer_t* buffer, uint32_t capacity) {
    buffer->buffer = nimcp_calloc(capacity, sizeof(observation_t));
    if (!buffer->buffer) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, capacity * sizeof(observation_t),
                           "init_history_buffer: Failed to allocate history buffer");
        return false;
    }
    buffer->capacity = capacity;
    buffer->head = 0;
    buffer->count = 0;
    return true;
}

/**
 * @brief Free history buffer
 */
static void free_history_buffer(history_buffer_t* buffer) {
    if (!buffer->buffer) return;

    for (uint32_t i = 0; i < buffer->count; i++) {
        uint32_t idx = (buffer->head + buffer->capacity - buffer->count + i) % buffer->capacity;
        if (buffer->buffer[idx].features) {
            nimcp_free(buffer->buffer[idx].features);
        }
    }
    nimcp_free(buffer->buffer);
    buffer->buffer = NULL;
}

/**
 * @brief Add to history buffer
 */
static bool add_to_history(history_buffer_t* buffer, const observation_t* obs) {
    /* Free old entry if overwriting */
    if (buffer->count == buffer->capacity) {
        uint32_t old_idx = (buffer->head + 1) % buffer->capacity;
        if (buffer->buffer[old_idx].features) {
            nimcp_free(buffer->buffer[old_idx].features);
            buffer->buffer[old_idx].features = NULL;
        }
    }

    /* Copy observation */
    buffer->head = (buffer->head + 1) % buffer->capacity;
    buffer->buffer[buffer->head] = *obs;

    if (obs->features && obs->feature_count > 0) {
        buffer->buffer[buffer->head].features = nimcp_calloc(obs->feature_count, sizeof(float));
        if (!buffer->buffer[buffer->head].features) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, obs->feature_count * sizeof(float),
                               "add_to_history: Failed to copy observation features");
            return false;
        }
        memcpy(buffer->buffer[buffer->head].features, obs->features,
               obs->feature_count * sizeof(float));
    }

    if (buffer->count < buffer->capacity) {
        buffer->count++;
    }

    return true;
}

/**
 * @brief Get observation from history (0 = most recent)
 */
static observation_t* get_from_history(history_buffer_t* buffer, uint32_t steps_back) {
    if (steps_back >= buffer->count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "get_from_history: capacity exceeded");
        return NULL;
    }
    uint32_t idx = (buffer->head + buffer->capacity - steps_back) % buffer->capacity;
    return &buffer->buffer[idx];
}

/**
 * @brief Initialize prediction model
 */
static bool init_prediction_model(prediction_model_t* model, uint32_t feature_dim, float lr) {
    model->feature_dim = feature_dim;
    model->learning_rate = lr;
    model->momentum = 0.9f;

    model->weights = nimcp_calloc(feature_dim * feature_dim, sizeof(float));
    model->bias = nimcp_calloc(feature_dim, sizeof(float));
    model->velocity = nimcp_calloc(feature_dim * feature_dim, sizeof(float));

    if (!model->weights || !model->bias || !model->velocity) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, feature_dim * feature_dim * sizeof(float),
                           "init_prediction_model: Failed to allocate model arrays");
        if (model->weights) nimcp_free(model->weights);
        if (model->bias) nimcp_free(model->bias);
        if (model->velocity) nimcp_free(model->velocity);
        return false;
    }

    /* Initialize with small random weights */
    for (uint32_t i = 0; i < feature_dim * feature_dim; i++) {
        model->weights[i] = (nimcp_rand_uniform() - 0.5f) * 0.1f;
    }
    /* Identity component on diagonal */
    for (uint32_t i = 0; i < feature_dim; i++) {
        model->weights[i * feature_dim + i] += 0.9f;
    }

    return true;
}

/**
 * @brief Free prediction model
 */
static void free_prediction_model(prediction_model_t* model) {
    if (model->weights) nimcp_free(model->weights);
    if (model->bias) nimcp_free(model->bias);
    if (model->velocity) nimcp_free(model->velocity);
    memset(model, 0, sizeof(prediction_model_t));
}

/**
 * @brief Run prediction model forward
 */
static void model_forward(
    prediction_model_t* model,
    const float* input,
    float* output,
    uint32_t feature_count
) {
    uint32_t dim = (feature_count < model->feature_dim) ? feature_count : model->feature_dim;

    memset(output, 0, feature_count * sizeof(float));

    for (uint32_t i = 0; i < dim; i++) {
        output[i] = model->bias[i];
        for (uint32_t j = 0; j < dim; j++) {
            output[i] += model->weights[j * model->feature_dim + i] * input[j];
        }
    }
}

/**
 * @brief Update running statistics
 */
static void update_running_stats(
    precognition_module_t* module,
    const float* features,
    uint32_t count
) {
    if (!module->running_mean || !module->running_variance) return;

    uint32_t dim = (count < module->config.state_dim) ? count : module->config.state_dim;

    module->stats_count++;
    float n = (float)module->stats_count;

    for (uint32_t i = 0; i < dim; i++) {
        float delta = features[i] - module->running_mean[i];
        module->running_mean[i] += delta / n;
        float delta2 = features[i] - module->running_mean[i];
        module->running_variance[i] += delta * delta2;
    }
}

/**
 * @brief Compute Z-score for features
 */
static float compute_z_score(
    precognition_module_t* module,
    const float* features,
    uint32_t count
) {
    if (!module->running_mean || !module->running_variance || module->stats_count < 10) {
        return 0.0f;
    }

    uint32_t dim = (count < module->config.state_dim) ? count : module->config.state_dim;
    float total_z = 0.0f;

    for (uint32_t i = 0; i < dim; i++) {
        float variance = module->running_variance[i] / (float)(module->stats_count - 1);
        float std = sqrtf(fmaxf(variance, 0.0001f));
        float z = fabsf(features[i] - module->running_mean[i]) / std;
        total_z += z * z;
    }

    return sqrtf(total_z / (float)dim);
}

/**
 * @brief Free pending prediction
 */
static void free_pending_prediction(pending_prediction_t* pred) {
    if (!pred) return;
    if (pred->predicted_features) nimcp_free(pred->predicted_features);
    if (pred->predicted_variance) nimcp_free(pred->predicted_variance);
    nimcp_free(pred);
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

precognition_config_t precognition_default_config(void) {
    precognition_config_t config;
    memset(&config, 0, sizeof(config));

    config.history_length = PRECOGNITION_DEFAULT_HISTORY_LENGTH;
    config.max_horizon_steps = PRECOGNITION_DEFAULT_MAX_HORIZON_STEPS;
    config.num_future_samples = PRECOGNITION_DEFAULT_NUM_FUTURES;
    config.state_dim = PRECOGNITION_DEFAULT_STATE_DIM;
    config.hidden_dim = PRECOGNITION_DEFAULT_HIDDEN_DIM;
    config.num_prediction_heads = HORIZON_COUNT;
    config.min_confidence = PRECOGNITION_DEFAULT_MIN_CONFIDENCE;
    config.confidence_decay = NIMCP_EMA_DECAY_FAST;
    config.temporal_discount = NIMCP_REWARD_DISCOUNT_DEFAULT;
    config.anomaly_threshold = PRECOGNITION_DEFAULT_ANOMALY_THRESHOLD;
    config.novelty_threshold = 2.0f;
    config.enable_early_warning = true;
    config.enable_causal_inference = true;
    config.causal_threshold = 0.3f;
    config.max_causal_depth = 3;
    config.learning_rate = PRECOGNITION_DEFAULT_LEARNING_RATE;
    config.enable_online_learning = true;
    config.enable_meta_learning = false;
    config.enable_uncertainty_estimation = true;
    config.enable_counterfactual = true;
    config.max_parallel_predictions = 4;

    return config;
}

precognition_module_t* precognition_create(const precognition_config_t* config) {
    LOG_INFO("[%s] Creating precognition module", PRECOGNITION_LOG_MODULE);

    precognition_module_t* module = nimcp_calloc(1, sizeof(precognition_module_t));
    if (!module) {
        LOG_ERROR("[%s] Failed to allocate module", PRECOGNITION_LOG_MODULE);
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(precognition_module_t),
                           "precognition_create: Failed to allocate module");
        return NULL;
    }

    /* Set configuration */
    if (config) {
        module->config = *config;
    } else {
        module->config = precognition_default_config();
    }

    /* Initialize history buffer */
    if (!init_history_buffer(&module->history, module->config.history_length)) {
        LOG_ERROR("[%s] Failed to allocate history buffer", PRECOGNITION_LOG_MODULE);
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                           module->config.history_length * sizeof(observation_t),
                           "precognition_create: Failed to allocate history buffer");
        precognition_destroy(module);
        return NULL;
    }

    /* Initialize prediction models */
    for (int h = 0; h < HORIZON_COUNT; h++) {
        float lr = module->config.learning_rate * powf(0.5f, (float)h);
        if (!init_prediction_model(&module->models[h], module->config.state_dim, lr)) {
            LOG_ERROR("[%s] Failed to initialize prediction model %d", PRECOGNITION_LOG_MODULE, h);
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                               module->config.state_dim * module->config.state_dim * sizeof(float),
                               "precognition_create: Failed to initialize prediction model %d", h);
            precognition_destroy(module);
            return NULL;
        }
    }

    /* Initialize running statistics */
    module->running_mean = nimcp_calloc(module->config.state_dim, sizeof(float));
    module->running_variance = nimcp_calloc(module->config.state_dim, sizeof(float));
    if (!module->running_mean || !module->running_variance) {
        LOG_ERROR("[%s] Failed to allocate statistics buffers", PRECOGNITION_LOG_MODULE);
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                           module->config.state_dim * sizeof(float) * 2,
                           "precognition_create: Failed to allocate statistics buffers");
        precognition_destroy(module);
        return NULL;
    }

    /* Initialize causal model */
    module->causal_model.links = NULL;
    module->causal_model.link_count = 0;
    module->causal_model.num_variables = module->config.state_dim;

    module->next_prediction_id = 1;
    module->status = PRECOGNITION_STATUS_IDLE;
    module->last_error = PRECOGNITION_ERROR_NONE;

    LOG_INFO("[%s] Precognition module created (history=%u, state_dim=%u)",
             PRECOGNITION_LOG_MODULE, module->config.history_length, module->config.state_dim);

    return module;
}

void precognition_destroy(precognition_module_t* module) {
    if (!module) return;

    LOG_INFO("[%s] Destroying precognition module", PRECOGNITION_LOG_MODULE);

    /* Free history */
    free_history_buffer(&module->history);

    /* Free prediction models */
    for (int h = 0; h < HORIZON_COUNT; h++) {
        free_prediction_model(&module->models[h]);
    }

    /* Free statistics */
    if (module->running_mean) nimcp_free(module->running_mean);
    if (module->running_variance) nimcp_free(module->running_variance);

    /* Free causal model */
    if (module->causal_model.links) nimcp_free(module->causal_model.links);

    /* Free pending predictions */
    pending_prediction_t* pred = module->pending_predictions;
    while (pred) {
        pending_prediction_t* next = pred->next;
        free_pending_prediction(pred);
        pred = next;
    }

    nimcp_free(module);
    LOG_DEBUG("[%s] Module destroyed", PRECOGNITION_LOG_MODULE);
}

bool precognition_reset(precognition_module_t* module) {
    if (!module) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "precognition_reset: NULL module pointer");
        return false;
    }

    LOG_DEBUG("[%s] Resetting module", PRECOGNITION_LOG_MODULE);

    /* Clear history */
    for (uint32_t i = 0; i < module->history.count; i++) {
        uint32_t idx = (module->history.head + module->history.capacity - module->history.count + i)
                       % module->history.capacity;
        if (module->history.buffer[idx].features) {
            nimcp_free(module->history.buffer[idx].features);
            module->history.buffer[idx].features = NULL;
        }
    }
    module->history.head = 0;
    module->history.count = 0;

    /* Reset statistics */
    memset(module->running_mean, 0, module->config.state_dim * sizeof(float));
    memset(module->running_variance, 0, module->config.state_dim * sizeof(float));
    module->stats_count = 0;

    /* Clear pending predictions */
    pending_prediction_t* pred = module->pending_predictions;
    while (pred) {
        pending_prediction_t* next = pred->next;
        free_pending_prediction(pred);
        pred = next;
    }
    module->pending_predictions = NULL;

    /* Reset stats */
    memset(&module->stats, 0, sizeof(precognition_stats_t));

    module->status = PRECOGNITION_STATUS_IDLE;
    module->last_error = PRECOGNITION_ERROR_NONE;

    return true;
}

/*=============================================================================
 * OBSERVATION AND HISTORY
 *===========================================================================*/

bool precognition_observe(
    precognition_module_t* module,
    const observation_t* observation
) {
    if (!module || !observation || !observation->features || observation->feature_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "precognition_observe: Invalid parameters (module=%p, observation=%p)",
                              (void*)module, (void*)observation);
        set_error(module, PRECOGNITION_ERROR_INVALID_INPUT);
        return false;
    }

    LOG_DEBUG("[%s] Adding observation (features=%u, time=%lu)",
              PRECOGNITION_LOG_MODULE, observation->feature_count,
              (unsigned long)observation->time.timestamp_ms);

    /* Add to history */
    if (!add_to_history(&module->history, observation)) {
        set_error(module, PRECOGNITION_ERROR_INTERNAL);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "precognition_observe: add_to_history is NULL");
        return false;
    }

    /* Update running statistics */
    update_running_stats(module, observation->features, observation->feature_count);

    /* Online model learning if enabled */
    if (module->config.enable_online_learning && module->history.count >= 2) {
        observation_t* prev = get_from_history(&module->history, 1);
        if (prev && prev->features) {
            /* Simple gradient descent update for immediate horizon model */
            prediction_model_t* model = &module->models[HORIZON_IMMEDIATE];
            float* predicted = nimcp_calloc(observation->feature_count, sizeof(float));
            if (predicted) {
                model_forward(model, prev->features, predicted, observation->feature_count);

                /* Compute error and update */
                uint32_t dim = (observation->feature_count < model->feature_dim)
                               ? observation->feature_count : model->feature_dim;
                for (uint32_t i = 0; i < dim; i++) {
                    float error = observation->features[i] - predicted[i];
                    model->bias[i] += model->learning_rate * error;
                    for (uint32_t j = 0; j < dim; j++) {
                        float grad = error * prev->features[j];
                        model->velocity[j * model->feature_dim + i] =
                            model->momentum * model->velocity[j * model->feature_dim + i] +
                            model->learning_rate * grad;
                        model->weights[j * model->feature_dim + i] +=
                            model->velocity[j * model->feature_dim + i];
                    }
                }
                nimcp_free(predicted);
            }
        }
    }

    module->stats.history_entries = module->history.count;

    return true;
}

bool precognition_observe_features(
    precognition_module_t* module,
    const float* features,
    uint32_t feature_count
) {
    if (!module || !features || feature_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "precognition_observe_features: Invalid parameters");
        set_error(module, PRECOGNITION_ERROR_INVALID_INPUT);
        return false;
    }

    observation_t obs;
    obs.features = (float*)features;  /* Will be copied in observe */
    obs.feature_count = feature_count;
    obs.time.timestamp_ms = get_current_time_ms();
    obs.time.uncertainty_ms = 1.0f;
    obs.observation_noise = 0.01f;

    return precognition_observe(module, &obs);
}

bool precognition_get_history(
    const precognition_module_t* module,
    observation_t* observations,
    uint32_t max_count,
    uint32_t* count
) {
    if (!module || !observations || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "precognition_get_history: NULL parameter");
        return false;
    }

    uint32_t to_copy = (max_count < module->history.count) ? max_count : module->history.count;

    for (uint32_t i = 0; i < to_copy; i++) {
        observation_t* src = get_from_history((history_buffer_t*)&module->history, i);
        if (src) {
            observations[i] = *src;
            observations[i].features = NULL;  /* Don't copy feature pointer */
        }
    }

    *count = to_copy;
    return true;
}

bool precognition_clear_history(precognition_module_t* module) {
    if (!module) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "precognition_clear_history: NULL module pointer");
        return false;
    }

    for (uint32_t i = 0; i < module->history.count; i++) {
        uint32_t idx = (module->history.head + module->history.capacity - module->history.count + i)
                       % module->history.capacity;
        if (module->history.buffer[idx].features) {
            nimcp_free(module->history.buffer[idx].features);
            module->history.buffer[idx].features = NULL;
        }
    }
    module->history.head = 0;
    module->history.count = 0;
    module->stats.history_entries = 0;

    return true;
}

/*=============================================================================
 * LONG-HORIZON FORECASTING
 *===========================================================================*/

bool precognition_predict(
    precognition_module_t* module,
    prediction_horizon_t horizon,
    uint32_t num_samples,
    prediction_ensemble_t* ensemble
) {
    if (!module || !ensemble || horizon >= HORIZON_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "precognition_predict: Invalid parameters (module=%p, ensemble=%p, horizon=%d)",
                              (void*)module, (void*)ensemble, (int)horizon);
        set_error(module, PRECOGNITION_ERROR_INVALID_INPUT);
        return false;
    }

    if (module->history.count < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                              "precognition_predict: Insufficient history (%u < 2)",
                              module->history.count);
        set_error(module, PRECOGNITION_ERROR_INSUFFICIENT_HISTORY);
        return false;
    }

    module->status = PRECOGNITION_STATUS_PREDICTING;
    memset(ensemble, 0, sizeof(prediction_ensemble_t));

    LOG_DEBUG("[%s] Generating prediction (horizon=%d, samples=%u)",
              PRECOGNITION_LOG_MODULE, horizon, num_samples);

    /* Get most recent observation */
    observation_t* current = get_from_history(&module->history, 0);
    if (!current || !current->features) {
        set_error(module, PRECOGNITION_ERROR_INSUFFICIENT_HISTORY);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "precognition_predict: required parameter is NULL (current, current->features)");
        return false;
    }

    /* Allocate trajectories */
    num_samples = (num_samples > 0) ? num_samples : module->config.num_future_samples;
    ensemble->trajectories = nimcp_calloc(num_samples, sizeof(future_trajectory_t));
    ensemble->weights = nimcp_calloc(num_samples, sizeof(float));
    if (!ensemble->trajectories || !ensemble->weights) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                           num_samples * sizeof(future_trajectory_t),
                           "precognition_predict: Failed to allocate trajectories");
        precognition_free_ensemble(ensemble);
        set_error(module, PRECOGNITION_ERROR_PREDICTION_FAILED);
        return false;
    }

    /* Determine number of prediction steps */
    uint32_t steps = (uint32_t)((int)horizon + 1) * 5;  /* More steps for longer horizons */
    if (steps > module->config.max_horizon_steps) {
        steps = module->config.max_horizon_steps;
    }

    float step_ms = horizon_to_ms(horizon) / (float)steps;
    float base_confidence = 1.0f;

    /* Generate trajectories */
    for (uint32_t s = 0; s < num_samples; s++) {
        future_trajectory_t* traj = &ensemble->trajectories[s];
        traj->states = nimcp_calloc(steps, sizeof(predicted_state_t));
        if (!traj->states) continue;

        traj->state_count = steps;
        traj->horizon = horizon;
        traj->prediction_id = module->next_prediction_id++;

        /* Initialize with current state */
        float* state = nimcp_calloc(current->feature_count, sizeof(float));
        if (!state) {
            nimcp_free(traj->states);
            traj->states = NULL;
            continue;
        }
        memcpy(state, current->features, current->feature_count * sizeof(float));

        float confidence = base_confidence;
        uint64_t current_time = current->time.timestamp_ms;

        /* Propagate through time */
        for (uint32_t t = 0; t < steps; t++) {
            predicted_state_t* pred = &traj->states[t];

            /* Use appropriate model for horizon */
            prediction_model_t* model = &module->models[(int)horizon < HORIZON_COUNT ?
                                                        (int)horizon : HORIZON_COUNT - 1];

            /* Allocate prediction arrays */
            pred->mean = nimcp_calloc(current->feature_count, sizeof(float));
            pred->variance = nimcp_calloc(current->feature_count, sizeof(float));
            if (!pred->mean || !pred->variance) continue;

            /* Run prediction */
            model_forward(model, state, pred->mean, current->feature_count);
            pred->feature_count = current->feature_count;

            /* Add noise for sample diversity */
            if (s > 0) {
                for (uint32_t i = 0; i < current->feature_count; i++) {
                    float noise = (nimcp_rand_uniform() - 0.5f) * 0.1f;
                    pred->mean[i] += noise * (1.0f - confidence);
                }
            }

            /* Estimate variance (grows with time) */
            float var_scale = 1.0f - confidence * confidence;
            for (uint32_t i = 0; i < current->feature_count; i++) {
                pred->variance[i] = var_scale * 0.1f;
            }

            /* Update state for next step */
            memcpy(state, pred->mean, current->feature_count * sizeof(float));

            /* Update time and confidence */
            current_time += (uint64_t)step_ms;
            pred->time.timestamp_ms = current_time;
            pred->time.uncertainty_ms = step_ms * var_scale;
            pred->confidence = confidence;
            pred->confidence_level = confidence_to_level(confidence);
            pred->dominant_pattern = PATTERN_LINEAR;  /* Simplified */

            /* Decay confidence */
            confidence *= module->config.confidence_decay;
        }

        nimcp_free(state);

        /* Calculate trajectory probability */
        traj->trajectory_probability = 1.0f / (float)num_samples;
        traj->cumulative_confidence = confidence;
        ensemble->weights[s] = traj->trajectory_probability;
    }

    ensemble->trajectory_count = num_samples;
    ensemble->max_horizon = horizon;

    /* Compute consensus prediction */
    if (ensemble->trajectory_count > 0 && ensemble->trajectories[0].state_count > 0) {
        predicted_state_t* final_state = &ensemble->trajectories[0].states[
            ensemble->trajectories[0].state_count - 1];
        ensemble->consensus.mean = nimcp_calloc(current->feature_count, sizeof(float));
        ensemble->consensus.variance = nimcp_calloc(current->feature_count, sizeof(float));

        if (ensemble->consensus.mean && ensemble->consensus.variance) {
            /* Average across trajectories */
            for (uint32_t s = 0; s < ensemble->trajectory_count; s++) {
                if (!ensemble->trajectories[s].states) continue;
                predicted_state_t* state = &ensemble->trajectories[s].states[
                    ensemble->trajectories[s].state_count - 1];
                if (!state->mean) continue;

                for (uint32_t i = 0; i < current->feature_count; i++) {
                    ensemble->consensus.mean[i] += state->mean[i] * ensemble->weights[s];
                }
            }
            ensemble->consensus.feature_count = current->feature_count;
            ensemble->consensus.time = final_state->time;
            ensemble->consensus.confidence = final_state->confidence;
            ensemble->consensus.confidence_level = confidence_to_level(final_state->confidence);
        }
    }

    /* Calculate ensemble uncertainty */
    float var_sum = 0.0f;
    for (uint32_t s = 0; s < ensemble->trajectory_count; s++) {
        if (!ensemble->trajectories[s].states) continue;
        predicted_state_t* state = &ensemble->trajectories[s].states[
            ensemble->trajectories[s].state_count - 1];
        if (!state->mean || !ensemble->consensus.mean) continue;

        for (uint32_t i = 0; i < current->feature_count; i++) {
            float diff = state->mean[i] - ensemble->consensus.mean[i];
            var_sum += diff * diff;
        }
    }
    ensemble->ensemble_uncertainty = sqrtf(var_sum / (float)(ensemble->trajectory_count * current->feature_count + 1));

    /* Update statistics */
    module->stats.total_predictions++;

    /* Invoke callback */
    if (module->prediction_callback) {
        module->prediction_callback(ensemble, module->prediction_user_data);
    }

    module->status = PRECOGNITION_STATUS_IDLE;

    LOG_DEBUG("[%s] Prediction complete (trajectories=%u, uncertainty=%.3f)",
              PRECOGNITION_LOG_MODULE, ensemble->trajectory_count, ensemble->ensemble_uncertainty);

    return true;
}

bool precognition_predict_steps(
    precognition_module_t* module,
    uint32_t steps,
    float step_size_ms,
    prediction_ensemble_t* ensemble
) {
    if (!module || !ensemble || steps == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "precognition_predict_steps: Invalid parameters");
        set_error(module, PRECOGNITION_ERROR_INVALID_INPUT);
        return false;
    }

    /* Determine appropriate horizon based on total time */
    float total_ms = steps * step_size_ms;
    prediction_horizon_t horizon = HORIZON_IMMEDIATE;
    if (total_ms > 604800000.0f) horizon = HORIZON_DISTANT;
    else if (total_ms > 86400000.0f) horizon = HORIZON_EXTENDED;
    else if (total_ms > 3600000.0f) horizon = HORIZON_LONG;
    else if (total_ms > 600000.0f) horizon = HORIZON_MEDIUM;
    else if (total_ms > 10000.0f) horizon = HORIZON_SHORT;

    return precognition_predict(module, horizon, module->config.num_future_samples, ensemble);
}

bool precognition_predict_most_likely(
    precognition_module_t* module,
    prediction_horizon_t horizon,
    future_trajectory_t* trajectory
) {
    if (!module || !trajectory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "precognition_predict_most_likely: NULL parameter");
        set_error(module, PRECOGNITION_ERROR_INVALID_INPUT);
        return false;
    }

    prediction_ensemble_t ensemble;
    if (!precognition_predict(module, horizon, 1, &ensemble)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "precognition_predict_most_likely: precognition_predict is NULL");
        return false;
    }

    if (ensemble.trajectory_count > 0 && ensemble.trajectories) {
        *trajectory = ensemble.trajectories[0];
        ensemble.trajectories[0].states = NULL;  /* Transfer ownership */
    }

    /* Partially free ensemble (keep transferred trajectory) */
    if (ensemble.weights) nimcp_free(ensemble.weights);
    if (ensemble.consensus.mean) nimcp_free(ensemble.consensus.mean);
    if (ensemble.consensus.variance) nimcp_free(ensemble.consensus.variance);
    if (ensemble.trajectories) nimcp_free(ensemble.trajectories);

    return true;
}

bool precognition_predict_feature(
    precognition_module_t* module,
    uint32_t feature_index,
    prediction_horizon_t horizon,
    float* predictions,
    float* confidence,
    uint32_t max_points,
    uint32_t* point_count
) {
    if (!module || !predictions || !confidence || !point_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "precognition_predict_feature: NULL parameter");
        set_error(module, PRECOGNITION_ERROR_INVALID_INPUT);
        return false;
    }

    prediction_ensemble_t ensemble;
    if (!precognition_predict(module, horizon, 1, &ensemble)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "precognition_predict_feature: precognition_predict is NULL");
        return false;
    }

    if (ensemble.trajectory_count == 0 || !ensemble.trajectories) {
        precognition_free_ensemble(&ensemble);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "precognition_predict_feature: ensemble is NULL");
        return false;
    }

    future_trajectory_t* traj = &ensemble.trajectories[0];
    uint32_t to_copy = (max_points < traj->state_count) ? max_points : traj->state_count;

    for (uint32_t i = 0; i < to_copy; i++) {
        if (traj->states[i].mean && feature_index < traj->states[i].feature_count) {
            predictions[i] = traj->states[i].mean[feature_index];
            confidence[i] = traj->states[i].confidence;
        }
    }

    *point_count = to_copy;
    precognition_free_ensemble(&ensemble);

    return true;
}

/*=============================================================================
 * PROBABILISTIC FUTURE STATES
 *===========================================================================*/

bool precognition_get_distribution(
    precognition_module_t* module,
    float time_ahead_ms,
    predicted_state_t* state
) {
    if (!module || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "precognition_get_distribution: NULL parameter");
        set_error(module, PRECOGNITION_ERROR_INVALID_INPUT);
        return false;
    }

    /* Determine horizon and predict */
    prediction_horizon_t horizon = HORIZON_IMMEDIATE;
    if (time_ahead_ms > 604800000.0f) horizon = HORIZON_DISTANT;
    else if (time_ahead_ms > 86400000.0f) horizon = HORIZON_EXTENDED;
    else if (time_ahead_ms > 3600000.0f) horizon = HORIZON_LONG;
    else if (time_ahead_ms > 600000.0f) horizon = HORIZON_MEDIUM;
    else if (time_ahead_ms > 10000.0f) horizon = HORIZON_SHORT;

    prediction_ensemble_t ensemble;
    if (!precognition_predict(module, horizon, module->config.num_future_samples, &ensemble)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "precognition_get_distribution: precognition_predict is NULL");
        return false;
    }

    /* Copy consensus to output */
    *state = ensemble.consensus;
    ensemble.consensus.mean = NULL;
    ensemble.consensus.variance = NULL;

    precognition_free_ensemble(&ensemble);
    return true;
}

bool precognition_event_probability(
    precognition_module_t* module,
    uint32_t feature_index,
    float threshold,
    bool above_threshold,
    prediction_horizon_t horizon,
    float* probability
) {
    if (!module || !probability) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "precognition_event_probability: NULL parameter");
        set_error(module, PRECOGNITION_ERROR_INVALID_INPUT);
        return false;
    }

    prediction_ensemble_t ensemble;
    if (!precognition_predict(module, horizon, module->config.num_future_samples, &ensemble)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "precognition_event_probability: precognition_predict is NULL");
        return false;
    }

    /* Count trajectories meeting condition */
    uint32_t count = 0;
    for (uint32_t s = 0; s < ensemble.trajectory_count; s++) {
        if (!ensemble.trajectories[s].states) continue;
        predicted_state_t* final_state = &ensemble.trajectories[s].states[
            ensemble.trajectories[s].state_count - 1];
        if (!final_state->mean || feature_index >= final_state->feature_count) continue;

        bool meets = above_threshold ?
                     (final_state->mean[feature_index] > threshold) :
                     (final_state->mean[feature_index] < threshold);
        if (meets) count++;
    }

    *probability = (float)count / (float)ensemble.trajectory_count;
    precognition_free_ensemble(&ensemble);

    return true;
}

bool precognition_confidence_interval(
    precognition_module_t* module,
    float confidence_pct,
    prediction_horizon_t horizon,
    float* lower_bound,
    float* upper_bound,
    uint32_t feature_count
) {
    if (!module || !lower_bound || !upper_bound || feature_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "precognition_confidence_interval: Invalid parameters");
        set_error(module, PRECOGNITION_ERROR_INVALID_INPUT);
        return false;
    }

    prediction_ensemble_t ensemble;
    if (!precognition_predict(module, horizon, module->config.num_future_samples, &ensemble)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "precognition_confidence_interval: precognition_predict is NULL");
        return false;
    }

    /* Use consensus mean and variance for interval */
    if (!ensemble.consensus.mean || !ensemble.consensus.variance) {
        precognition_free_ensemble(&ensemble);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "precognition_confidence_interval: required parameter is NULL (ensemble, ensemble)");
        return false;
    }

    float z = 1.96f;  /* 95% confidence */
    if (confidence_pct >= 99.0f) z = 2.576f;
    else if (confidence_pct >= 95.0f) z = 1.96f;
    else if (confidence_pct >= 90.0f) z = 1.645f;

    uint32_t dim = (feature_count < ensemble.consensus.feature_count)
                   ? feature_count : ensemble.consensus.feature_count;

    for (uint32_t i = 0; i < dim; i++) {
        float std = sqrtf(ensemble.consensus.variance[i] + ensemble.ensemble_uncertainty);
        lower_bound[i] = ensemble.consensus.mean[i] - z * std;
        upper_bound[i] = ensemble.consensus.mean[i] + z * std;
    }

    precognition_free_ensemble(&ensemble);
    return true;
}

/*=============================================================================
 * ANOMALY PREDICTION
 *===========================================================================*/

bool precognition_detect_anomaly(
    precognition_module_t* module,
    detected_anomaly_t* anomaly
) {
    if (!module || !anomaly) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "precognition_detect_anomaly: NULL parameter");
        set_error(module, PRECOGNITION_ERROR_INVALID_INPUT);
        return false;
    }

    if (module->history.count < 10) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "precognition_detect_anomaly: validation failed");
        return false;  /* Need enough history */
    }

    module->status = PRECOGNITION_STATUS_DETECTING_ANOMALY;
    memset(anomaly, 0, sizeof(detected_anomaly_t));

    /* Get current observation */
    observation_t* current = get_from_history(&module->history, 0);
    if (!current || !current->features) {
        module->status = PRECOGNITION_STATUS_IDLE;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "precognition_detect_anomaly: required parameter is NULL (current, current->features)");
        return false;
    }

    /* Compute Z-score */
    float z_score = compute_z_score(module, current->features, current->feature_count);

    if (z_score > module->config.anomaly_threshold) {
        anomaly->anomaly_id = module->stats.anomalies_detected + 1;
        anomaly->detection_time.timestamp_ms = get_current_time_ms();
        anomaly->predicted_time = anomaly->detection_time;
        anomaly->z_score = z_score;

        /* Determine severity */
        if (z_score > 5.0f) anomaly->severity = ANOMALY_CRITICAL;
        else if (z_score > 4.0f) anomaly->severity = ANOMALY_MAJOR;
        else if (z_score > 3.5f) anomaly->severity = ANOMALY_MODERATE;
        else anomaly->severity = ANOMALY_MINOR;

        /* Copy features */
        anomaly->observed_features = nimcp_calloc(current->feature_count, sizeof(float));
        anomaly->expected_features = nimcp_calloc(current->feature_count, sizeof(float));
        anomaly->feature_deviations = nimcp_calloc(current->feature_count, sizeof(float));
        anomaly->feature_count = current->feature_count;

        if (anomaly->observed_features) {
            memcpy(anomaly->observed_features, current->features,
                   current->feature_count * sizeof(float));
        }
        if (anomaly->expected_features && module->running_mean) {
            memcpy(anomaly->expected_features, module->running_mean,
                   current->feature_count * sizeof(float));
        }
        if (anomaly->feature_deviations && module->running_mean && module->running_variance) {
            for (uint32_t i = 0; i < current->feature_count; i++) {
                float variance = module->running_variance[i] / (float)(module->stats_count - 1);
                float std = sqrtf(fmaxf(variance, 0.0001f));
                anomaly->feature_deviations[i] = fabsf(current->features[i] - module->running_mean[i]) / std;
            }
        }

        snprintf(anomaly->description, sizeof(anomaly->description),
                 "Anomaly detected: Z-score %.2f (%s severity)",
                 z_score, z_score > 4.0f ? "critical/major" : "moderate/minor");

        module->stats.anomalies_detected++;

        /* Invoke callback */
        if (module->anomaly_callback) {
            module->anomaly_callback(anomaly, module->anomaly_user_data);
        }

        module->status = PRECOGNITION_STATUS_IDLE;
        return true;
    }

    module->status = PRECOGNITION_STATUS_IDLE;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "precognition_detect_anomaly: validation failed");
    return false;
}

bool precognition_predict_anomalies(
    precognition_module_t* module,
    prediction_horizon_t horizon,
    detected_anomaly_t* anomalies,
    uint32_t max_anomalies,
    uint32_t* anomaly_count
) {
    if (!module || !anomalies || !anomaly_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "precognition_predict_anomalies: NULL parameter");
        set_error(module, PRECOGNITION_ERROR_INVALID_INPUT);
        return false;
    }

    *anomaly_count = 0;

    prediction_ensemble_t ensemble;
    if (!precognition_predict(module, horizon, module->config.num_future_samples, &ensemble)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "precognition_predict_anomalies: precognition_predict is NULL");
        return false;
    }

    /* Check each trajectory for anomalous predictions */
    for (uint32_t s = 0; s < ensemble.trajectory_count && *anomaly_count < max_anomalies; s++) {
        if (!ensemble.trajectories[s].states) continue;

        for (uint32_t t = 0; t < ensemble.trajectories[s].state_count && *anomaly_count < max_anomalies; t++) {
            predicted_state_t* state = &ensemble.trajectories[s].states[t];
            if (!state->mean) continue;

            float z_score = compute_z_score(module, state->mean, state->feature_count);

            if (z_score > module->config.anomaly_threshold) {
                detected_anomaly_t* anomaly = &anomalies[*anomaly_count];
                memset(anomaly, 0, sizeof(detected_anomaly_t));

                anomaly->anomaly_id = module->stats.anomalies_detected + *anomaly_count + 1;
                anomaly->detection_time.timestamp_ms = get_current_time_ms();
                anomaly->predicted_time = state->time;
                anomaly->z_score = z_score;

                if (z_score > 5.0f) anomaly->severity = ANOMALY_CRITICAL;
                else if (z_score > 4.0f) anomaly->severity = ANOMALY_MAJOR;
                else if (z_score > 3.5f) anomaly->severity = ANOMALY_MODERATE;
                else anomaly->severity = ANOMALY_MINOR;

                snprintf(anomaly->description, sizeof(anomaly->description),
                         "Predicted anomaly at t+%.0fms: Z-score %.2f",
                         (float)(state->time.timestamp_ms - anomaly->detection_time.timestamp_ms),
                         z_score);

                (*anomaly_count)++;
                break;  /* One anomaly per trajectory */
            }
        }
    }

    precognition_free_ensemble(&ensemble);
    return true;
}

bool precognition_check_early_warning(
    precognition_module_t* module,
    early_warning_t* warning
) {
    if (!module || !warning) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "precognition_check_early_warning: NULL parameter");
        set_error(module, PRECOGNITION_ERROR_INVALID_INPUT);
        return false;
    }

    if (!module->config.enable_early_warning) {
        return false;
    }

    memset(warning, 0, sizeof(early_warning_t));

    /* Check for predicted anomalies */
    detected_anomaly_t anomalies[4];
    uint32_t anomaly_count = 0;

    if (precognition_predict_anomalies(module, HORIZON_SHORT, anomalies, 4, &anomaly_count) &&
        anomaly_count > 0) {

        /* Generate warning for highest severity anomaly */
        detected_anomaly_t* worst = &anomalies[0];
        for (uint32_t i = 1; i < anomaly_count; i++) {
            if (anomalies[i].severity > worst->severity ||
                (anomalies[i].severity == worst->severity && anomalies[i].z_score > worst->z_score)) {
                worst = &anomalies[i];
            }
        }

        warning->alert_id = module->stats.warnings_issued + 1;
        warning->alert_time.timestamp_ms = get_current_time_ms();
        warning->event_time = worst->predicted_time;
        warning->probability = fminf(1.0f, worst->z_score / 5.0f);
        warning->lead_time_ms = (float)(worst->predicted_time.timestamp_ms - warning->alert_time.timestamp_ms);
        warning->expected_severity = worst->severity;
        warning->confidence = 1.0f - (1.0f / worst->z_score);

        snprintf(warning->message, sizeof(warning->message),
                 "EARLY WARNING: %s severity event predicted in %.0f ms (probability: %.1f%%)",
                 worst->severity == ANOMALY_CRITICAL ? "Critical" :
                 worst->severity == ANOMALY_MAJOR ? "Major" : "Moderate",
                 warning->lead_time_ms, warning->probability * 100.0f);

        module->stats.warnings_issued++;

        /* Invoke callback */
        if (module->warning_callback) {
            module->warning_callback(warning, module->warning_user_data);
        }

        return true;
    }

    return false;
}

bool precognition_set_anomaly_threshold(
    precognition_module_t* module,
    float z_score_threshold
) {
    if (!module || z_score_threshold <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "precognition_set_anomaly_threshold: Invalid parameters (module=%p, threshold=%.2f)",
                              (void*)module, z_score_threshold);
        set_error(module, PRECOGNITION_ERROR_INVALID_INPUT);
        return false;
    }

    module->config.anomaly_threshold = z_score_threshold;
    return true;
}

/*=============================================================================
 * CAUSAL MODELING
 *===========================================================================*/

bool precognition_learn_causal_model(precognition_module_t* module) {
    if (!module) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "precognition_learn_causal_model: NULL module pointer");
        set_error(module, PRECOGNITION_ERROR_INVALID_INPUT);
        return false;
    }

    if (!module->config.enable_causal_inference) {
        return true;
    }

    if (module->history.count < 50) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                              "precognition_learn_causal_model: Insufficient history (%u < 50)",
                              module->history.count);
        set_error(module, PRECOGNITION_ERROR_INSUFFICIENT_HISTORY);
        return false;
    }

    module->status = PRECOGNITION_STATUS_MODELING_CAUSAL;

    LOG_DEBUG("[%s] Learning causal model", PRECOGNITION_LOG_MODULE);

    /* Simple Granger causality approximation */
    /* For each pair of features, check if past of one predicts future of other */

    observation_t* current = get_from_history(&module->history, 0);
    if (!current) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                              "precognition_learn_causal_model: Failed to get current observation");
        module->status = PRECOGNITION_STATUS_IDLE;
        return false;
    }

    uint32_t num_vars = (current->feature_count < module->config.state_dim)
                        ? current->feature_count : module->config.state_dim;

    /* Allocate space for potential links */
    uint32_t max_links = num_vars * num_vars;
    causal_link_t* links = nimcp_calloc(max_links, sizeof(causal_link_t));
    if (!links) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, max_links * sizeof(causal_link_t),
                           "precognition_learn_causal_model: Failed to allocate causal links");
        module->status = PRECOGNITION_STATUS_IDLE;
        return false;
    }

    uint32_t link_count = 0;

    /* Compute cross-correlations with lag */
    for (uint32_t i = 0; i < num_vars && link_count < max_links; i++) {
        for (uint32_t j = 0; j < num_vars && link_count < max_links; j++) {
            if (i == j) continue;

            /* Compute lagged correlation */
            float sum_xy = 0.0f, sum_x = 0.0f, sum_y = 0.0f;
            float sum_x2 = 0.0f, sum_y2 = 0.0f;
            uint32_t count = 0;

            for (uint32_t t = 1; t < module->history.count; t++) {
                observation_t* prev = get_from_history(&module->history, t);
                observation_t* curr = get_from_history(&module->history, t - 1);
                if (!prev || !curr || !prev->features || !curr->features) continue;
                if (i >= prev->feature_count || j >= curr->feature_count) continue;

                float x = prev->features[i];
                float y = curr->features[j];

                sum_xy += x * y;
                sum_x += x;
                sum_y += y;
                sum_x2 += x * x;
                sum_y2 += y * y;
                count++;
            }

            if (count < 10) continue;

            float n = (float)count;
            float denom = sqrtf((n * sum_x2 - sum_x * sum_x) * (n * sum_y2 - sum_y * sum_y));
            if (denom < 0.0001f) continue;

            float correlation = (n * sum_xy - sum_x * sum_y) / denom;

            if (fabsf(correlation) > module->config.causal_threshold) {
                links[link_count].cause_index = i;
                links[link_count].effect_index = j;
                links[link_count].type = CAUSAL_DIRECT;
                links[link_count].strength = fabsf(correlation);
                links[link_count].lag_ms = 1.0f;  /* One step lag */
                links[link_count].confidence = fminf(1.0f, (float)count / 100.0f);
                link_count++;
            }
        }
    }

    /* Update causal model */
    if (module->causal_model.links) {
        nimcp_free(module->causal_model.links);
    }
    module->causal_model.links = links;
    module->causal_model.link_count = link_count;
    module->causal_model.num_variables = num_vars;
    module->causal_model.model_fit = 0.5f;  /* Placeholder */
    module->causal_model.last_update_ms = get_current_time_ms();

    module->stats.causal_links_discovered = link_count;
    module->stats.causal_model_fit = module->causal_model.model_fit;

    module->status = PRECOGNITION_STATUS_IDLE;

    LOG_DEBUG("[%s] Causal model learned: %u links", PRECOGNITION_LOG_MODULE, link_count);

    return true;
}

bool precognition_get_causal_model(
    const precognition_module_t* module,
    causal_model_t* model
) {
    if (!module || !model) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "precognition_get_causal_model: NULL parameter");
        return false;
    }

    *model = module->causal_model;
    model->links = NULL;  /* Don't copy pointer */
    return true;
}

bool precognition_query_causal_effect(
    precognition_module_t* module,
    uint32_t cause_index,
    uint32_t effect_index,
    float* strength,
    float* confidence
) {
    if (!module || !strength || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "precognition_query_causal_effect: NULL parameter");
        set_error(module, PRECOGNITION_ERROR_INVALID_INPUT);
        return false;
    }

    for (uint32_t i = 0; i < module->causal_model.link_count; i++) {
        if (module->causal_model.links[i].cause_index == cause_index &&
            module->causal_model.links[i].effect_index == effect_index) {
            *strength = module->causal_model.links[i].strength;
            *confidence = module->causal_model.links[i].confidence;
            return true;
        }
    }

    *strength = 0.0f;
    *confidence = 0.0f;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "precognition_query_causal_effect: operation failed");
    return false;
}

bool precognition_counterfactual(
    precognition_module_t* module,
    const counterfactual_query_t* query,
    counterfactual_result_t* result
) {
    if (!module || !query || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "precognition_counterfactual: NULL parameter");
        set_error(module, PRECOGNITION_ERROR_INVALID_INPUT);
        return false;
    }

    if (!module->config.enable_counterfactual) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "precognition_counterfactual: module->config is NULL");
        return false;
    }

    memset(result, 0, sizeof(counterfactual_result_t));

    /* Generate factual prediction */
    if (!precognition_predict(module, query->horizon, module->config.num_future_samples,
                              &result->factual)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "precognition_counterfactual: module->config is NULL");
        return false;
    }

    /* Apply intervention and generate counterfactual
     * (Simplified: just modify starting state and predict) */

    /* Get current state and modify */
    observation_t* current = get_from_history(&module->history, 0);
    if (!current || !current->features) {
        precognition_free_ensemble(&result->factual);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "precognition_counterfactual: required parameter is NULL (current, current->features)");
        return false;
    }

    /* Create modified observation */
    float* modified = nimcp_calloc(current->feature_count, sizeof(float));
    if (!modified) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, current->feature_count * sizeof(float),
                           "precognition_counterfactual: Failed to allocate modified features");
        precognition_free_ensemble(&result->factual);
        return false;
    }
    memcpy(modified, current->features, current->feature_count * sizeof(float));

    /* Apply interventions */
    for (uint32_t i = 0; i < query->intervention_count; i++) {
        uint32_t idx = (uint32_t)query->intervention_features[i];
        if (idx < current->feature_count) {
            modified[idx] = query->intervention_values[i];
        }
    }

    /* Temporarily modify history for counterfactual prediction */
    float* original = current->features;
    current->features = modified;

    /* Generate counterfactual prediction */
    precognition_predict(module, query->horizon, module->config.num_future_samples,
                        &result->counterfactual);

    /* Restore original */
    current->features = original;
    nimcp_free(modified);

    /* Calculate causal effect */
    if (result->factual.consensus.mean && result->counterfactual.consensus.mean) {
        float effect_sum = 0.0f;
        uint32_t dim = result->factual.consensus.feature_count;
        for (uint32_t i = 0; i < dim; i++) {
            float diff = result->counterfactual.consensus.mean[i] -
                        result->factual.consensus.mean[i];
            effect_sum += diff * diff;
        }
        result->causal_effect = sqrtf(effect_sum / (float)dim);
        result->effect_confidence = fminf(result->factual.consensus.confidence,
                                          result->counterfactual.consensus.confidence);
    }

    return true;
}

/*=============================================================================
 * VERIFICATION AND LEARNING
 *===========================================================================*/

bool precognition_verify_prediction(
    precognition_module_t* module,
    uint64_t prediction_id,
    const observation_t* observation,
    verification_result_t* result
) {
    if (!module || !observation || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "precognition_verify_prediction: NULL parameter");
        set_error(module, PRECOGNITION_ERROR_INVALID_INPUT);
        return false;
    }

    memset(result, 0, sizeof(verification_result_t));
    result->prediction_id = prediction_id;
    result->observed = *observation;
    result->observed.features = NULL;

    /* Find pending prediction */
    pending_prediction_t* pred = module->pending_predictions;
    pending_prediction_t* prev = NULL;

    while (pred) {
        if (pred->prediction_id == prediction_id) {
            /* Calculate error */
            if (pred->predicted_features && observation->features) {
                float error_sum = 0.0f;
                bool within_ci = true;
                uint32_t dim = (pred->feature_count < observation->feature_count)
                               ? pred->feature_count : observation->feature_count;

                for (uint32_t i = 0; i < dim; i++) {
                    float diff = observation->features[i] - pred->predicted_features[i];
                    error_sum += diff * diff;

                    /* Check if within confidence interval */
                    if (pred->predicted_variance) {
                        float std = sqrtf(pred->predicted_variance[i]);
                        if (fabsf(diff) > 1.96f * std) {
                            within_ci = false;
                        }
                    }
                }

                result->prediction_error = sqrtf(error_sum / (float)dim);
                result->within_confidence = within_ci;
                result->calibration_score = within_ci ? 1.0f : 0.0f;

                /* Copy predicted state */
                result->predicted.mean = nimcp_calloc(dim, sizeof(float));
                if (result->predicted.mean) {
                    memcpy(result->predicted.mean, pred->predicted_features, dim * sizeof(float));
                    result->predicted.feature_count = dim;
                }
            }

            /* Update statistics */
            module->stats.verified_predictions++;
            if (result->within_confidence) {
                module->stats.accurate_predictions++;
            }

            /* Remove from pending list */
            if (prev) {
                prev->next = pred->next;
            } else {
                module->pending_predictions = pred->next;
            }
            free_pending_prediction(pred);

            /* Invoke callback */
            if (module->verification_callback) {
                module->verification_callback(result, module->verification_user_data);
            }

            return true;
        }
        prev = pred;
        pred = pred->next;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "precognition_verify_prediction: operation failed");
    return false;
}

bool precognition_learn_from_error(
    precognition_module_t* module,
    const verification_result_t* result
) {
    if (!module || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "precognition_learn_from_error: NULL parameter");
        set_error(module, PRECOGNITION_ERROR_INVALID_INPUT);
        return false;
    }

    if (!module->config.enable_online_learning) {
        return true;
    }

    /* Update model based on prediction error */
    module->stats.mean_prediction_error =
        (module->stats.mean_prediction_error * (module->stats.verified_predictions - 1) +
         result->prediction_error) / module->stats.verified_predictions;

    module->stats.calibration_score =
        (module->stats.calibration_score * (module->stats.verified_predictions - 1) +
         result->calibration_score) / module->stats.verified_predictions;

    return true;
}

bool precognition_get_accuracy(
    const precognition_module_t* module,
    prediction_horizon_t horizon,
    float* accuracy,
    float* calibration
) {
    if (!module || !accuracy || !calibration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "precognition_get_accuracy: NULL parameter");
        return false;
    }

    if (module->stats.verified_predictions == 0) {
        *accuracy = 0.0f;
        *calibration = 0.0f;
        return true;
    }

    if (horizon < HORIZON_COUNT) {
        *accuracy = module->stats.accuracy_by_horizon[horizon];
    } else {
        *accuracy = (float)module->stats.accurate_predictions /
                    (float)module->stats.verified_predictions;
    }
    *calibration = module->stats.calibration_score;

    return true;
}

/*=============================================================================
 * CALLBACKS
 *===========================================================================*/

bool precognition_set_prediction_callback(
    precognition_module_t* module,
    precognition_prediction_callback_t callback,
    void* user_data
) {
    if (!module) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "precognition_set_prediction_callback: NULL module pointer");
        return false;
    }
    module->prediction_callback = callback;
    module->prediction_user_data = user_data;
    return true;
}

bool precognition_set_anomaly_callback(
    precognition_module_t* module,
    precognition_anomaly_callback_t callback,
    void* user_data
) {
    if (!module) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "precognition_set_anomaly_callback: NULL module pointer");
        return false;
    }
    module->anomaly_callback = callback;
    module->anomaly_user_data = user_data;
    return true;
}

bool precognition_set_warning_callback(
    precognition_module_t* module,
    precognition_warning_callback_t callback,
    void* user_data
) {
    if (!module) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "precognition_set_warning_callback: NULL module pointer");
        return false;
    }
    module->warning_callback = callback;
    module->warning_user_data = user_data;
    return true;
}

bool precognition_set_verification_callback(
    precognition_module_t* module,
    precognition_verification_callback_t callback,
    void* user_data
) {
    if (!module) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "precognition_set_verification_callback: NULL module pointer");
        return false;
    }
    module->verification_callback = callback;
    module->verification_user_data = user_data;
    return true;
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

precognition_status_t precognition_get_status(const precognition_module_t* module) {
    if (!module) return PRECOGNITION_STATUS_ERROR;
    return module->status;
}

precognition_error_t precognition_get_last_error(const precognition_module_t* module) {
    if (!module) return PRECOGNITION_ERROR_NOT_INITIALIZED;
    return module->last_error;
}

const char* precognition_error_string(precognition_error_t error) {
    switch (error) {
        case PRECOGNITION_ERROR_NONE: return "No error";
        case PRECOGNITION_ERROR_INVALID_INPUT: return "Invalid input";
        case PRECOGNITION_ERROR_PREDICTION_FAILED: return "Prediction failed";
        case PRECOGNITION_ERROR_INSUFFICIENT_HISTORY: return "Insufficient history";
        case PRECOGNITION_ERROR_HORIZON_EXCEEDED: return "Horizon exceeded";
        case PRECOGNITION_ERROR_MODEL_DIVERGENCE: return "Model divergence";
        case PRECOGNITION_ERROR_CONFIDENCE_TOO_LOW: return "Confidence too low";
        case PRECOGNITION_ERROR_ANOMALY_OVERFLOW: return "Anomaly overflow";
        case PRECOGNITION_ERROR_CAUSAL_LOOP: return "Causal loop detected";
        case PRECOGNITION_ERROR_NOT_INITIALIZED: return "Not initialized";
        case PRECOGNITION_ERROR_INTERNAL: return "Internal error";
        default: return "Unknown error";
    }
}

const char* precognition_status_string(precognition_status_t status) {
    switch (status) {
        case PRECOGNITION_STATUS_IDLE: return "Idle";
        case PRECOGNITION_STATUS_PREDICTING: return "Predicting";
        case PRECOGNITION_STATUS_ANALYZING: return "Analyzing";
        case PRECOGNITION_STATUS_DETECTING_ANOMALY: return "Detecting anomaly";
        case PRECOGNITION_STATUS_MODELING_CAUSAL: return "Modeling causal";
        case PRECOGNITION_STATUS_READY: return "Ready";
        case PRECOGNITION_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}

bool precognition_get_stats(const precognition_module_t* module, precognition_stats_t* stats) {
    if (!module || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "precognition_get_stats: NULL parameter");
        return false;
    }
    *stats = module->stats;
    return true;
}

bool precognition_get_config(const precognition_module_t* module, precognition_config_t* config) {
    if (!module || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "precognition_get_config: NULL parameter");
        return false;
    }
    *config = module->config;
    return true;
}

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

void precognition_free_ensemble(prediction_ensemble_t* ensemble) {
    if (!ensemble) return;

    if (ensemble->trajectories) {
        for (uint32_t s = 0; s < ensemble->trajectory_count; s++) {
            precognition_free_trajectory(&ensemble->trajectories[s]);
        }
        nimcp_free(ensemble->trajectories);
    }
    if (ensemble->weights) nimcp_free(ensemble->weights);
    if (ensemble->consensus.mean) nimcp_free(ensemble->consensus.mean);
    if (ensemble->consensus.variance) nimcp_free(ensemble->consensus.variance);

    memset(ensemble, 0, sizeof(prediction_ensemble_t));
}

void precognition_free_trajectory(future_trajectory_t* trajectory) {
    if (!trajectory) return;

    if (trajectory->states) {
        for (uint32_t i = 0; i < trajectory->state_count; i++) {
            if (trajectory->states[i].mean) nimcp_free(trajectory->states[i].mean);
            if (trajectory->states[i].variance) nimcp_free(trajectory->states[i].variance);
        }
        nimcp_free(trajectory->states);
    }

    memset(trajectory, 0, sizeof(future_trajectory_t));
}

void precognition_free_causal_model(causal_model_t* model) {
    if (!model) return;
    if (model->links) nimcp_free(model->links);
    memset(model, 0, sizeof(causal_model_t));
}

void precognition_free_counterfactual(counterfactual_result_t* result) {
    if (!result) return;
    precognition_free_ensemble(&result->factual);
    precognition_free_ensemble(&result->counterfactual);
    memset(result, 0, sizeof(counterfactual_result_t));
}

const char* precognition_horizon_string(prediction_horizon_t horizon) {
    switch (horizon) {
        case HORIZON_IMMEDIATE: return "Immediate";
        case HORIZON_SHORT: return "Short-term";
        case HORIZON_MEDIUM: return "Medium-term";
        case HORIZON_LONG: return "Long-term";
        case HORIZON_EXTENDED: return "Extended";
        case HORIZON_DISTANT: return "Distant";
        default: return "Unknown";
    }
}

const char* precognition_confidence_string(confidence_level_t level) {
    switch (level) {
        case CONFIDENCE_VERY_LOW: return "Very Low";
        case CONFIDENCE_LOW: return "Low";
        case CONFIDENCE_MODERATE: return "Moderate";
        case CONFIDENCE_HIGH: return "High";
        case CONFIDENCE_VERY_HIGH: return "Very High";
        default: return "Unknown";
    }
}

uint64_t precognition_get_current_time_ms(void) {
    return get_current_time_ms();
}
