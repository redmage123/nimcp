/**
 * @file nimcp_cortical_predictive_coding.c
 * @brief Implementation of hierarchical predictive coding for cortical columns
 * @version 1.0.0
 * @date 2025-12-15
 */

#include "core/cortical_columns/nimcp_cortical_predictive_coding.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_learning_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cortical_predictive_coding)

/* ============================================================================
 * Constants
 * ============================================================================ */

#define PC_DEFAULT_PREDICTION_LR    NIMCP_DEFAULT_LEARNING_RATE
#define PC_DEFAULT_PRECISION_LR     NIMCP_DEFAULT_DECAY_RATE
#define PC_DEFAULT_ERROR_GAIN       1.0f
#define PC_DEFAULT_PREDICTION_DECAY NIMCP_WEIGHT_DECAY_DEFAULT
#define PC_DEFAULT_HIERARCHY_DEPTH  3
#define PC_INITIAL_PRECISION        1.0f
#define PC_MIN_PRECISION            0.001f
#define PC_MAX_PRECISION            100.0f
#define PC_EPSILON                  NIMCP_EPSILON_ADAM

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Initialize predictive layer
 *
 * WHAT: Allocate and zero-initialize layer arrays
 * WHY:  Set up clean state for predictions, errors, precisions
 * HOW:  Use nimcp_calloc for zero-initialized arrays
 */
static int init_predictive_layer(
    predictive_layer_t* layer,
    uint32_t num_units,
    pc_population_type_t type
) {
    NIMCP_CHECK_THROW(layer && num_units > 0, NIMCP_ERROR_INVALID_PARAM, "layer is NULL or num_units is 0");

    layer->num_units = num_units;
    layer->type = type;

    layer->predictions = (float*)nimcp_calloc(num_units, sizeof(float));
    if (!layer->predictions) {
        return NIMCP_ERROR_MEMORY;
    }

    layer->errors = (float*)nimcp_calloc(num_units, sizeof(float));
    if (!layer->errors) {
        nimcp_free(layer->predictions);
        return NIMCP_ERROR_MEMORY;
    }

    layer->precisions = (float*)nimcp_calloc(num_units, sizeof(float));
    if (!layer->precisions) {
        nimcp_free(layer->predictions);
        nimcp_free(layer->errors);
        return NIMCP_ERROR_MEMORY;
    }

    /* Initialize precisions to default value */
    for (uint32_t i = 0; i < num_units; i++) {
        layer->precisions[i] = PC_INITIAL_PRECISION;
    }

    return 0;
}

/**
 * @brief Free predictive layer resources
 *
 * WHAT: Release memory allocated for layer
 * WHY:  Prevent memory leaks
 * HOW:  Free all arrays and nullify pointers
 */
static void free_predictive_layer(predictive_layer_t* layer) {
    if (!layer) {
        return;
    }

    if (layer->predictions) {
        nimcp_free(layer->predictions);
        layer->predictions = NULL;
    }

    if (layer->errors) {
        nimcp_free(layer->errors);
        layer->errors = NULL;
    }

    if (layer->precisions) {
        nimcp_free(layer->precisions);
        layer->precisions = NULL;
    }

    layer->num_units = 0;
}

/**
 * @brief Clamp precision to valid range
 *
 * WHAT: Ensure precision stays within bounds
 * WHY:  Prevent numerical instability from extreme values
 * HOW:  Clamp to [PC_MIN_PRECISION, PC_MAX_PRECISION]
 */
static inline float clamp_precision(float precision) {
    if (precision < PC_MIN_PRECISION) {
        return PC_MIN_PRECISION;
    }
    if (precision > PC_MAX_PRECISION) {
        return PC_MAX_PRECISION;
    }
    return precision;
}

/**
 * @brief Compute squared error magnitude
 *
 * WHAT: Sum of squared errors for free energy calculation
 * WHY:  Component of variational free energy
 * HOW:  Σ ε²
 */
static float compute_squared_error(const float* errors, uint32_t size) {
    if (!errors || size == 0) {
        return 0.0f;
    }

    float sum = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        sum += errors[i] * errors[i];
    }
    return sum;
}

/**
 * @brief Compute precision term for free energy
 *
 * WHAT: Uncertainty component of free energy
 * WHY:  F includes both accuracy and complexity terms
 * HOW:  ½ln|Π^{-1}| = -½Σln(Π)
 */
static float compute_precision_term(const float* precisions, uint32_t size) {
    if (!precisions || size == 0) {
        return 0.0f;
    }

    float sum = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        if (precisions[i] > PC_EPSILON) {
            sum -= 0.5f * logf(precisions[i]);
        }
    }
    return sum;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int cortical_predictive_default_config(predictive_config_t* config) {
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    config->prediction_learning_rate = PC_DEFAULT_PREDICTION_LR;
    config->precision_learning_rate = PC_DEFAULT_PRECISION_LR;
    config->error_gain = PC_DEFAULT_ERROR_GAIN;
    config->prediction_decay = PC_DEFAULT_PREDICTION_DECAY;
    config->enable_precision_weighting = true;
    config->enable_lateral_predictions = false;
    config->hierarchy_depth = PC_DEFAULT_HIERARCHY_DEPTH;

    return 0;
}

cortical_predictive_t* cortical_predictive_create(const predictive_config_t* config) {
    cortical_predictive_t* pc = (cortical_predictive_t*)nimcp_calloc(1, sizeof(cortical_predictive_t));
    if (!pc) {
        NIMCP_LOGGING_ERROR("Failed to allocate predictive coding system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pc is NULL");

        return NULL;
    }

    /* Apply configuration */
    if (config) {
        pc->config = *config;
    } else {
        cortical_predictive_default_config(&pc->config);
    }

    /* Initialize hierarchy structure */
    pc->hierarchy.levels = NULL;
    pc->hierarchy.num_levels = 0;
    pc->hierarchy.total_free_energy = 0.0f;
    pc->hierarchy.total_prediction_error = 0.0f;
    pc->hierarchy.inter_level_weights = NULL;
    pc->hierarchy.weights_size = 0;

    /* Initialize statistics */
    memset(&pc->stats, 0, sizeof(predictive_stats_t));
    pc->stats.min_free_energy = INFINITY;

    /* Create mutex */
    pc->mutex = (nimcp_mutex_t*)nimcp_malloc(sizeof(nimcp_mutex_t));
    if (!pc->mutex) {
        NIMCP_LOGGING_ERROR("Failed to allocate mutex");
        nimcp_free(pc);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_predictive_create: pc->mutex is NULL");
        return NULL;
    }

    if (nimcp_mutex_init(pc->mutex, NULL) != 0) {
        NIMCP_LOGGING_ERROR("Failed to initialize mutex");
        nimcp_free(pc->mutex);
        nimcp_free(pc);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "cortical_predictive_create: validation failed");
        return NULL;
    }

    /* Bio-async not connected initially */
    pc->bio_async_enabled = false;
    pc->bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Created predictive coding system");
    return pc;
}

void cortical_predictive_destroy(cortical_predictive_t* pc) {
    if (!pc) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (pc->bio_async_enabled) {
        cortical_predictive_disconnect_bio_async(pc);
    }

    /* Free hierarchy levels */
    if (pc->hierarchy.levels) {
        for (uint32_t i = 0; i < pc->hierarchy.num_levels; i++) {
            predictive_level_t* level = &pc->hierarchy.levels[i];
            free_predictive_layer(&level->prediction_pop);
            free_predictive_layer(&level->error_pop);

            if (level->lateral_predictions) {
                nimcp_free(level->lateral_predictions);
                level->lateral_predictions = NULL;
            }
        }
        nimcp_free(pc->hierarchy.levels);
    }

    /* Free inter-level weights */
    if (pc->hierarchy.inter_level_weights) {
        nimcp_free(pc->hierarchy.inter_level_weights);
    }

    /* Destroy mutex */
    if (pc->mutex) {
        nimcp_mutex_destroy(pc->mutex);
        nimcp_free(pc->mutex);
    }

    nimcp_free(pc);
    NIMCP_LOGGING_INFO("Destroyed predictive coding system");
}

/* ============================================================================
 * Hierarchy Construction API Implementation
 * ============================================================================ */

int cortical_predictive_add_level(
    cortical_predictive_t* pc,
    uint32_t num_prediction_units,
    uint32_t num_error_units
) {
    NIMCP_CHECK_THROW(pc, NIMCP_ERROR_NULL_POINTER, "pc is NULL");
    NIMCP_CHECK_THROW(num_prediction_units > 0 && num_error_units > 0, NIMCP_ERROR_INVALID_PARAM, "num_prediction_units or num_error_units is 0");

    nimcp_mutex_lock(pc->mutex);

    /* Reallocate levels array */
    uint32_t new_num_levels = pc->hierarchy.num_levels + 1;
    predictive_level_t* new_levels = (predictive_level_t*)nimcp_calloc(
        new_num_levels,
        sizeof(predictive_level_t)
    );

    if (!new_levels) {
        nimcp_mutex_unlock(pc->mutex);
        return NIMCP_ERROR_MEMORY;
    }

    /* Copy existing levels */
    if (pc->hierarchy.levels) {
        memcpy(new_levels, pc->hierarchy.levels,
               pc->hierarchy.num_levels * sizeof(predictive_level_t));
        nimcp_free(pc->hierarchy.levels);
    }

    pc->hierarchy.levels = new_levels;

    /* Initialize new level */
    predictive_level_t* level = &pc->hierarchy.levels[pc->hierarchy.num_levels];
    level->level_index = pc->hierarchy.num_levels;
    level->level_precision = PC_INITIAL_PRECISION;
    level->lateral_predictions = NULL;

    /* Initialize prediction population */
    int result = init_predictive_layer(&level->prediction_pop, num_prediction_units,
                                       PC_POPULATION_PREDICTION);
    if (result != 0) {
        nimcp_mutex_unlock(pc->mutex);
        return result;
    }

    /* Initialize error population */
    result = init_predictive_layer(&level->error_pop, num_error_units,
                                   PC_POPULATION_ERROR);
    if (result != 0) {
        free_predictive_layer(&level->prediction_pop);
        nimcp_mutex_unlock(pc->mutex);
        return result;
    }

    /* Allocate lateral predictions if enabled */
    if (pc->config.enable_lateral_predictions) {
        level->lateral_predictions = (float*)nimcp_calloc(num_prediction_units, sizeof(float));
        if (!level->lateral_predictions) {
            free_predictive_layer(&level->prediction_pop);
            free_predictive_layer(&level->error_pop);
            nimcp_mutex_unlock(pc->mutex);
            return NIMCP_ERROR_MEMORY;
        }
    }

    pc->hierarchy.num_levels = new_num_levels;

    nimcp_mutex_unlock(pc->mutex);

    NIMCP_LOGGING_INFO("Added level %u with %u prediction units and %u error units",
                       level->level_index, num_prediction_units, num_error_units);
    return 0;
}

/* ============================================================================
 * Prediction and Error Computation API Implementation
 * ============================================================================ */

int cortical_predictive_compute_prediction(
    const cortical_predictive_t* pc,
    uint32_t level_idx,
    float* output,
    uint32_t output_size
) {
    NIMCP_CHECK_THROW(pc && output, NIMCP_ERROR_NULL_POINTER, "pc or output is NULL");
    NIMCP_CHECK_THROW(level_idx < pc->hierarchy.num_levels, NIMCP_ERROR_INVALID_PARAM, "level_idx exceeds num_levels");

    const predictive_level_t* level = &pc->hierarchy.levels[level_idx];
    uint32_t num_to_copy = (output_size < level->prediction_pop.num_units) ?
                           output_size : level->prediction_pop.num_units;

    memcpy(output, level->prediction_pop.predictions, num_to_copy * sizeof(float));
    return (int)num_to_copy;
}

int cortical_predictive_compute_error(
    cortical_predictive_t* pc,
    uint32_t level_idx,
    const float* observation,
    uint32_t obs_size,
    float* output,
    uint32_t output_size
) {
    NIMCP_CHECK_THROW(pc && observation && output, NIMCP_ERROR_NULL_POINTER, "pc, observation, or output is NULL");
    NIMCP_CHECK_THROW(level_idx < pc->hierarchy.num_levels, NIMCP_ERROR_INVALID_PARAM, "level_idx exceeds num_levels");

    nimcp_mutex_lock(pc->mutex);

    predictive_level_t* level = &pc->hierarchy.levels[level_idx];
    uint32_t num_units = level->error_pop.num_units;
    uint32_t num_to_process = (obs_size < num_units) ? obs_size : num_units;

    /* Compute error = observation - prediction */
    for (uint32_t i = 0; i < num_to_process; i++) {
        float prediction = (i < level->prediction_pop.num_units) ?
                          level->prediction_pop.predictions[i] : 0.0f;
        level->error_pop.errors[i] = observation[i] - prediction;
    }

    /* Apply error gain */
    for (uint32_t i = 0; i < num_to_process; i++) {
        level->error_pop.errors[i] *= pc->config.error_gain;
    }

    /* Copy to output */
    uint32_t num_to_copy = (output_size < num_to_process) ? output_size : num_to_process;
    memcpy(output, level->error_pop.errors, num_to_copy * sizeof(float));

    nimcp_mutex_unlock(pc->mutex);

    return (int)num_to_copy;
}

int cortical_predictive_weight_by_precision(
    const cortical_predictive_t* pc,
    uint32_t level_idx,
    const float* errors,
    uint32_t error_size,
    float* weighted_errors,
    uint32_t output_size
) {
    NIMCP_CHECK_THROW(pc && errors && weighted_errors, NIMCP_ERROR_NULL_POINTER, "pc, errors, or weighted_errors is NULL");
    NIMCP_CHECK_THROW(level_idx < pc->hierarchy.num_levels, NIMCP_ERROR_INVALID_PARAM, "level_idx exceeds num_levels");

    if (!pc->config.enable_precision_weighting) {
        /* If precision weighting disabled, just copy errors */
        uint32_t num_to_copy = (error_size < output_size) ? error_size : output_size;
        memcpy(weighted_errors, errors, num_to_copy * sizeof(float));
        return 0;
    }

    const predictive_level_t* level = &pc->hierarchy.levels[level_idx];
    uint32_t num_units = level->error_pop.num_units;
    uint32_t num_to_process = (error_size < num_units) ? error_size : num_units;
    num_to_process = (num_to_process < output_size) ? num_to_process : output_size;

    /* Multiply errors by precision weights */
    for (uint32_t i = 0; i < num_to_process; i++) {
        weighted_errors[i] = errors[i] * level->error_pop.precisions[i];
    }

    return 0;
}

/* ============================================================================
 * Message Passing API Implementation
 * ============================================================================ */

int cortical_predictive_propagate_up(
    cortical_predictive_t* pc,
    uint32_t source_level
) {
    NIMCP_CHECK_THROW(pc, NIMCP_ERROR_NULL_POINTER, "pc is NULL");

    if (source_level >= pc->hierarchy.num_levels - 1) {
        /* Highest level has nowhere to propagate to */
        return 0;
    }

    nimcp_mutex_lock(pc->mutex);

    predictive_level_t* src = &pc->hierarchy.levels[source_level];
    predictive_level_t* dst = &pc->hierarchy.levels[source_level + 1];

    /* Weight errors by precision */
    float* weighted_errors = (float*)nimcp_calloc(src->error_pop.num_units, sizeof(float));
    if (!weighted_errors) {
        nimcp_mutex_unlock(pc->mutex);
        return NIMCP_ERROR_MEMORY;
    }

    if (pc->config.enable_precision_weighting) {
        for (uint32_t i = 0; i < src->error_pop.num_units; i++) {
            weighted_errors[i] = src->error_pop.errors[i] * src->error_pop.precisions[i];
        }
    } else {
        memcpy(weighted_errors, src->error_pop.errors,
               src->error_pop.num_units * sizeof(float));
    }

    /* Simple propagation: sum weighted errors and add to destination predictions */
    /* In a full implementation, this would use proper connectivity matrices */
    float error_sum = 0.0f;
    for (uint32_t i = 0; i < src->error_pop.num_units; i++) {
        error_sum += weighted_errors[i];
    }

    /* Distribute to destination prediction units */
    float contribution = error_sum / (float)dst->prediction_pop.num_units;
    for (uint32_t i = 0; i < dst->prediction_pop.num_units; i++) {
        dst->prediction_pop.predictions[i] += contribution * pc->config.prediction_learning_rate;
    }

    nimcp_free(weighted_errors);
    nimcp_mutex_unlock(pc->mutex);

    return 0;
}

int cortical_predictive_propagate_down(
    cortical_predictive_t* pc,
    uint32_t source_level
) {
    NIMCP_CHECK_THROW(pc, NIMCP_ERROR_NULL_POINTER, "pc is NULL");

    if (source_level == 0) {
        /* Lowest level has nowhere to propagate to */
        return 0;
    }

    nimcp_mutex_lock(pc->mutex);

    predictive_level_t* src = &pc->hierarchy.levels[source_level];
    predictive_level_t* dst = &pc->hierarchy.levels[source_level - 1];

    /* Simple propagation: sum predictions and distribute to lower level */
    float pred_sum = 0.0f;
    for (uint32_t i = 0; i < src->prediction_pop.num_units; i++) {
        pred_sum += src->prediction_pop.predictions[i];
    }

    /* Distribute to destination prediction units */
    float contribution = pred_sum / (float)dst->prediction_pop.num_units;
    for (uint32_t i = 0; i < dst->prediction_pop.num_units; i++) {
        dst->prediction_pop.predictions[i] = contribution;
    }

    nimcp_mutex_unlock(pc->mutex);

    return 0;
}

/* ============================================================================
 * Learning API Implementation
 * ============================================================================ */

int cortical_predictive_update_predictions(
    cortical_predictive_t* pc,
    uint32_t level_idx
) {
    NIMCP_CHECK_THROW(pc, NIMCP_ERROR_NULL_POINTER, "pc is NULL");
    NIMCP_CHECK_THROW(level_idx < pc->hierarchy.num_levels, NIMCP_ERROR_INVALID_PARAM, "level_idx exceeds num_levels");

    nimcp_mutex_lock(pc->mutex);

    predictive_level_t* level = &pc->hierarchy.levels[level_idx];

    /* Update predictions based on errors at this level */
    /* Δμ = η * (errors from below - weighted errors going up) */
    for (uint32_t i = 0; i < level->prediction_pop.num_units; i++) {
        float error_signal = (i < level->error_pop.num_units) ?
                            level->error_pop.errors[i] : 0.0f;

        /* Apply learning rate and decay */
        float delta = pc->config.prediction_learning_rate * error_signal;
        level->prediction_pop.predictions[i] += delta;
        level->prediction_pop.predictions[i] *= (1.0f - pc->config.prediction_decay);
    }

    pc->stats.prediction_updates++;
    pc->stats.total_updates++;

    nimcp_mutex_unlock(pc->mutex);

    return 0;
}

int cortical_predictive_update_precisions(
    cortical_predictive_t* pc,
    uint32_t level_idx
) {
    NIMCP_CHECK_THROW(pc, NIMCP_ERROR_NULL_POINTER, "pc is NULL");
    NIMCP_CHECK_THROW(level_idx < pc->hierarchy.num_levels, NIMCP_ERROR_INVALID_PARAM, "level_idx exceeds num_levels");

    nimcp_mutex_lock(pc->mutex);

    predictive_level_t* level = &pc->hierarchy.levels[level_idx];

    /* Update precision based on error variance */
    /* Δln(Π) = α * (ε² - 1/Π) */
    for (uint32_t i = 0; i < level->error_pop.num_units; i++) {
        float error_sq = level->error_pop.errors[i] * level->error_pop.errors[i];
        float precision = level->error_pop.precisions[i];

        /* Precision update */
        float delta_log_precision = pc->config.precision_learning_rate *
                                   (error_sq - 1.0f / (precision + PC_EPSILON));

        /* Update precision (in log space then exponentiate) */
        float log_precision = logf(precision + PC_EPSILON);
        log_precision += delta_log_precision;
        precision = expf(log_precision);

        /* Clamp to valid range */
        level->error_pop.precisions[i] = clamp_precision(precision);
    }

    /* Update level precision (average) */
    float avg_precision = 0.0f;
    for (uint32_t i = 0; i < level->error_pop.num_units; i++) {
        avg_precision += level->error_pop.precisions[i];
    }
    if (level->error_pop.num_units > 0) {
        level->level_precision = avg_precision / (float)level->error_pop.num_units;
    }

    pc->stats.precision_updates++;
    pc->stats.total_updates++;

    nimcp_mutex_unlock(pc->mutex);

    return 0;
}

/* ============================================================================
 * Free Energy API Implementation
 * ============================================================================ */

int cortical_predictive_compute_free_energy(
    const cortical_predictive_t* pc,
    float* free_energy
) {
    NIMCP_CHECK_THROW(pc && free_energy, NIMCP_ERROR_NULL_POINTER, "pc or free_energy is NULL");

    float total_fe = 0.0f;

    /* Sum free energy across all levels */
    /* F = Σ [½ε^T Π ε + ½ln|Π^{-1}|] */
    for (uint32_t i = 0; i < pc->hierarchy.num_levels; i++) {
        const predictive_level_t* level = &pc->hierarchy.levels[i];

        /* Accuracy term: ½ε^T Π ε */
        float accuracy = 0.0f;
        for (uint32_t j = 0; j < level->error_pop.num_units; j++) {
            float error = level->error_pop.errors[j];
            float precision = level->error_pop.precisions[j];
            accuracy += 0.5f * error * precision * error;
        }

        /* Complexity term: ½ln|Π^{-1}| = -½Σln(Π) */
        float complexity = compute_precision_term(level->error_pop.precisions,
                                                  level->error_pop.num_units);

        total_fe += accuracy + complexity;
    }

    *free_energy = total_fe;
    return 0;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

int cortical_predictive_get_stats(
    const cortical_predictive_t* pc,
    predictive_stats_t* stats
) {
    NIMCP_CHECK_THROW(pc && stats, NIMCP_ERROR_NULL_POINTER, "pc or stats is NULL");

    nimcp_mutex_lock(pc->mutex);
    *stats = pc->stats;
    nimcp_mutex_unlock(pc->mutex);

    return 0;
}

int cortical_predictive_get_predictions(
    const cortical_predictive_t* pc,
    uint32_t level_idx,
    float* predictions,
    uint32_t size
) {
    NIMCP_CHECK_THROW(pc && predictions, NIMCP_ERROR_NULL_POINTER, "pc or predictions is NULL");
    NIMCP_CHECK_THROW(level_idx < pc->hierarchy.num_levels, NIMCP_ERROR_INVALID_PARAM, "level_idx exceeds num_levels");

    const predictive_level_t* level = &pc->hierarchy.levels[level_idx];
    uint32_t num_to_copy = (size < level->prediction_pop.num_units) ?
                           size : level->prediction_pop.num_units;

    memcpy(predictions, level->prediction_pop.predictions, num_to_copy * sizeof(float));
    return (int)num_to_copy;
}

int cortical_predictive_get_errors(
    const cortical_predictive_t* pc,
    uint32_t level_idx,
    float* errors,
    uint32_t size
) {
    NIMCP_CHECK_THROW(pc && errors, NIMCP_ERROR_NULL_POINTER, "pc or errors is NULL");
    NIMCP_CHECK_THROW(level_idx < pc->hierarchy.num_levels, NIMCP_ERROR_INVALID_PARAM, "level_idx exceeds num_levels");

    const predictive_level_t* level = &pc->hierarchy.levels[level_idx];
    uint32_t num_to_copy = (size < level->error_pop.num_units) ?
                           size : level->error_pop.num_units;

    memcpy(errors, level->error_pop.errors, num_to_copy * sizeof(float));
    return (int)num_to_copy;
}

int cortical_predictive_get_precisions(
    const cortical_predictive_t* pc,
    uint32_t level_idx,
    float* precisions,
    uint32_t size
) {
    NIMCP_CHECK_THROW(pc && precisions, NIMCP_ERROR_NULL_POINTER, "pc or precisions is NULL");
    NIMCP_CHECK_THROW(level_idx < pc->hierarchy.num_levels, NIMCP_ERROR_INVALID_PARAM, "level_idx exceeds num_levels");

    const predictive_level_t* level = &pc->hierarchy.levels[level_idx];
    uint32_t num_to_copy = (size < level->error_pop.num_units) ?
                           size : level->error_pop.num_units;

    memcpy(precisions, level->error_pop.precisions, num_to_copy * sizeof(float));
    return (int)num_to_copy;
}

/* ============================================================================
 * Bio-async API Implementation
 * ============================================================================ */

int cortical_predictive_connect_bio_async(cortical_predictive_t* pc) {
    NIMCP_CHECK_THROW(pc, NIMCP_ERROR_NULL_POINTER, "pc is NULL");

    if (pc->bio_async_enabled) {
        return 0;  /* Already connected */
    }

    nimcp_mutex_lock(pc->mutex);

    bio_module_info_t info = {
        .module_id = BIO_MODULE_CORTICAL_PREDICTIVE,
        .module_name = "cortical_predictive_coding",
        .inbox_capacity = 32,
        .user_data = pc
    };

    pc->bio_ctx = bio_router_register_module(&info);
    if (pc->bio_ctx) {
        pc->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available");
    }

    nimcp_mutex_unlock(pc->mutex);

    return 0;
}

int cortical_predictive_disconnect_bio_async(cortical_predictive_t* pc) {
    NIMCP_CHECK_THROW(pc, NIMCP_ERROR_NULL_POINTER, "pc is NULL");

    if (!pc->bio_async_enabled) {
        return 0;  /* Not connected */
    }

    nimcp_mutex_lock(pc->mutex);

    if (pc->bio_ctx) {
        bio_router_unregister_module(pc->bio_ctx);
        pc->bio_ctx = NULL;
    }

    pc->bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");

    nimcp_mutex_unlock(pc->mutex);

    return 0;
}

bool cortical_predictive_is_bio_async_connected(const cortical_predictive_t* pc) {
    if (!pc) {
        return false;
    }
    return pc->bio_async_enabled;
}

/* ============================================================================
 * KG Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for cortical predictive coding module self-knowledge
 * WHY:  Enable self-awareness and introspection about this module's role
 * HOW:  Query KG for entity info, log observations, check relations
 */
int cortical_predictive_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Cortical_Predictive_Coding_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            NIMCP_LOGGING_DEBUG("Cortical predictive coding self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Cortical_Predictive_Coding_Module");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Cortical_Predictive_Coding_Module");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}
