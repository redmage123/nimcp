/**
 * @file nimcp_novelty_detection.c
 * @brief Novelty/Surprise Detection Implementation
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "core/brain/regions/locus_coeruleus/nimcp_novelty_detection.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(novelty_detection, MESH_ADAPTER_CATEGORY_COGNITIVE)

static uint32_t simple_hash(const float* data, uint32_t size) {
    uint32_t hash = 0;
    for (uint32_t i = 0; i < size && i < 32; i++) {
        union { float f; uint32_t u; } conv;
        conv.f = data[i];
        hash ^= conv.u;
        hash = (hash << 7) | (hash >> 25);
        hash += i;
    }
    return hash;
}

static float compute_input_magnitude(const float* input, uint32_t size) {
    if (size == 0) return 0.0f;
    float sum = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        sum += input[i] * input[i];
    }
    return sqrtf(sum / (float)size);
}

//=============================================================================
// Lifecycle Implementation
//=============================================================================

nimcp_novelty_config_t nimcp_novelty_default_config(void) {
    nimcp_novelty_config_t config;
    memset(&config, 0, sizeof(config));

    config.detector_type = NOVELTY_DETECTOR_HYBRID;
    config.input_dimension = 64;

    config.novelty_threshold = NOVELTY_DEFAULT_THRESHOLD;
    config.surprise_threshold = 0.4f;
    config.burst_threshold = 0.6f;

    config.habituation_tau_ms = NOVELTY_HABITUATION_TAU_MS;
    config.surprise_decay_tau_ms = NOVELTY_SURPRISE_TAU_MS;
    config.novelty_decay_tau_ms = 200.0f;

    config.stats_learning_rate = NIMCP_LEARNING_RATE_COARSE;
    config.predictor_learning_rate = 0.05f;
    config.habituation_rate = 0.1f;

    config.sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config.z_score_threshold = 2.0f;

    return config;
}

int nimcp_novelty_init(
    nimcp_novelty_system_t* system,
    const nimcp_novelty_config_t* config
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;
    }

    memset(system, 0, sizeof(nimcp_novelty_system_t));

    nimcp_novelty_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = nimcp_novelty_default_config();
    }

    system->detector_type = cfg.detector_type;
    system->input_dimension = cfg.input_dimension;

    /* Allocate feature statistics */
    if (cfg.input_dimension > 0 && cfg.input_dimension <= NOVELTY_MAX_INPUT_DIM) {
        system->feature_stats = (nimcp_novelty_stats_t*)nimcp_calloc(
            cfg.input_dimension, sizeof(nimcp_novelty_stats_t));
        if (!system->feature_stats) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_novelty_init: system->feature_stats is NULL");
            return -1;
        }

        /* Initialize statistics for each feature */
        for (uint32_t i = 0; i < cfg.input_dimension; i++) {
            system->feature_stats[i].mean = 0.0f;
            system->feature_stats[i].variance = 1.0f;
            system->feature_stats[i].min = 0.0f;
            system->feature_stats[i].max = 0.0f;
            system->feature_stats[i].sample_count = 0;
            system->feature_stats[i].learning_rate = cfg.stats_learning_rate;
        }
    }

    /* Initialize predictor */
    system->predictor.input_dim = cfg.input_dimension;
    system->predictor.predicted = NULL;
    system->predictor.weights = NULL;
    system->predictor.prediction_error = 0.0f;
    system->predictor.learning_rate = cfg.predictor_learning_rate;

    /* Initialize habituation memory */
    system->memory_index = 0;
    system->memory_count = 0;

    /* Initialize current state */
    system->current_novelty = 0.0f;
    system->current_surprise = 0.0f;
    system->exploration_drive = 0.5f;

    /* Initialize thresholds */
    system->novelty_threshold = cfg.novelty_threshold;
    system->surprise_threshold = cfg.surprise_threshold;
    system->burst_threshold = cfg.burst_threshold;

    /* Initialize time constants */
    system->habituation_tau = cfg.habituation_tau_ms;
    system->surprise_decay_tau = cfg.surprise_decay_tau_ms;
    system->novelty_decay_tau = cfg.novelty_decay_tau_ms;

    system->initialized = true;
    system->current_time = 0.0f;

    return 0;
}

int nimcp_novelty_shutdown(nimcp_novelty_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;
    }

    if (system->feature_stats) {
        nimcp_free(system->feature_stats);
    }
    if (system->predictor.predicted) {
        nimcp_free(system->predictor.predicted);
    }
    if (system->predictor.weights) {
        nimcp_free(system->predictor.weights);
    }

    memset(system, 0, sizeof(nimcp_novelty_system_t));
    return 0;
}

int nimcp_novelty_reset(nimcp_novelty_system_t* system) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_novelty_reset: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    /* Reset statistics */
    if (system->feature_stats) {
        for (uint32_t i = 0; i < system->input_dimension; i++) {
            system->feature_stats[i].mean = 0.0f;
            system->feature_stats[i].variance = 1.0f;
            system->feature_stats[i].min = 0.0f;
            system->feature_stats[i].max = 0.0f;
            system->feature_stats[i].sample_count = 0;
        }
    }

    /* Clear habituation memory */
    memset(system->habituation_memory, 0, sizeof(system->habituation_memory));
    system->memory_index = 0;
    system->memory_count = 0;

    /* Reset state */
    system->current_novelty = 0.0f;
    system->current_surprise = 0.0f;
    system->exploration_drive = 0.5f;
    memset(&system->last_result, 0, sizeof(system->last_result));

    system->current_time = 0.0f;

    return 0;
}

//=============================================================================
// Detection Implementation
//=============================================================================

int nimcp_novelty_detect(
    nimcp_novelty_system_t* system,
    const float* input,
    uint32_t input_size,
    nimcp_novelty_result_t* result
) {
    if (!system || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_novelty_detect: required parameter is NULL (system, result)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_novelty_detect: system->initialized is NULL");
        return -1;
    }

    memset(result, 0, sizeof(nimcp_novelty_result_t));

    if (!input || input_size == 0) {
        return 0;
    }

    /* Compute different novelty components based on detector type */
    float statistical_novelty = 0.0f;
    float prediction_error = 0.0f;
    float familiarity = 1.0f;

    switch (system->detector_type) {
        case NOVELTY_DETECTOR_STATISTICAL:
            statistical_novelty = nimcp_novelty_compute_statistical(system, input, input_size);
            break;

        case NOVELTY_DETECTOR_PREDICTIVE:
            prediction_error = nimcp_novelty_compute_surprise(system, input, input_size);
            break;

        case NOVELTY_DETECTOR_FAMILIARITY:
            familiarity = nimcp_novelty_get_familiarity(system, input, input_size);
            break;

        case NOVELTY_DETECTOR_HYBRID:
        default:
            statistical_novelty = nimcp_novelty_compute_statistical(system, input, input_size);
            prediction_error = nimcp_novelty_compute_surprise(system, input, input_size);
            familiarity = nimcp_novelty_get_familiarity(system, input, input_size);
            break;
    }

    /* Combine novelty components */
    float combined_novelty;
    if (system->detector_type == NOVELTY_DETECTOR_HYBRID) {
        combined_novelty = 0.4f * statistical_novelty +
                           0.3f * prediction_error +
                           0.3f * (1.0f - familiarity);
    } else if (system->detector_type == NOVELTY_DETECTOR_FAMILIARITY) {
        combined_novelty = 1.0f - familiarity;
    } else if (system->detector_type == NOVELTY_DETECTOR_PREDICTIVE) {
        combined_novelty = prediction_error;
    } else {
        combined_novelty = statistical_novelty;
    }

    combined_novelty = nimcp_clampf(combined_novelty, 0.0f, 1.0f);

    /* Fill result structure */
    result->novelty_score = combined_novelty;
    result->surprise_magnitude = prediction_error;
    result->familiarity = familiarity;
    result->deviation = statistical_novelty * 3.0f;  /* Approximate z-score */
    result->prediction_error = prediction_error;

    /* Classify event type */
    if (combined_novelty < 0.1f) {
        result->event_type = NOVELTY_EVENT_NONE;
    } else if (combined_novelty < 0.3f) {
        result->event_type = NOVELTY_EVENT_MILD;
    } else if (combined_novelty < 0.5f) {
        result->event_type = NOVELTY_EVENT_MODERATE;
    } else if (combined_novelty < 0.8f) {
        result->event_type = NOVELTY_EVENT_HIGH;
    } else {
        result->event_type = NOVELTY_EVENT_EXTREME;
    }

    /* Determine if phasic burst should be triggered */
    result->should_trigger_burst = combined_novelty > system->burst_threshold;
    result->recommended_burst_intensity = nimcp_clampf(combined_novelty, 0.0f, 1.0f);

    /* Update system state */
    if (combined_novelty > system->current_novelty) {
        system->current_novelty = combined_novelty;
    }
    system->current_surprise = prediction_error;
    system->exploration_drive = 0.3f + 0.7f * familiarity;  /* More familiar = more explore */

    /* Update habituation */
    nimcp_novelty_habituate(system, input, input_size);

    /* Store result */
    system->last_result = *result;

    return 0;
}

int nimcp_novelty_update(nimcp_novelty_system_t* system, float dt) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_novelty_update: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    if (dt <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_novelty_update: validation failed");
        return -1;
    }

    /* Decay novelty signal */
    float alpha = expf(-dt / system->novelty_decay_tau);
    system->current_novelty *= alpha;

    /* Decay surprise */
    alpha = expf(-dt / system->surprise_decay_tau);
    system->current_surprise *= alpha;

    /* Update habituation decay */
    for (uint32_t i = 0; i < system->memory_count; i++) {
        nimcp_habituation_entry_t* entry = &system->habituation_memory[i];

        /* Time since last seen increases */
        entry->last_seen_time += dt;

        /* Habituation decays over time (dishabituation) */
        float decay = expf(-dt / system->habituation_tau);
        entry->habituation_level *= decay;

        /* Familiarity also decays slowly */
        entry->familiarity *= expf(-dt / (system->habituation_tau * 10.0f));
    }

    system->current_time += dt;
    return 0;
}

float nimcp_novelty_compute_statistical(
    nimcp_novelty_system_t* system,
    const float* input,
    uint32_t input_size
) {
    if (!system || !input || input_size == 0) {
        return 0.0f;
    }

    /* Use feature statistics if available */
    uint32_t use_size = input_size;
    if (use_size > system->input_dimension) {
        use_size = system->input_dimension;
    }

    float total_z_score = 0.0f;
    uint32_t valid_features = 0;

    if (system->feature_stats) {
        for (uint32_t i = 0; i < use_size; i++) {
            nimcp_novelty_stats_t* stats = &system->feature_stats[i];

            /* Update running statistics */
            float old_mean = stats->mean;
            stats->mean += stats->learning_rate * (input[i] - stats->mean);

            float diff = input[i] - old_mean;
            float diff2 = input[i] - stats->mean;
            stats->variance += stats->learning_rate * (diff * diff2 - stats->variance);
            if (stats->variance < 0.01f) {
                stats->variance = 0.01f;
            }

            stats->sample_count++;

            if (input[i] < stats->min || stats->sample_count == 1) {
                stats->min = input[i];
            }
            if (input[i] > stats->max || stats->sample_count == 1) {
                stats->max = input[i];
            }

            /* Compute z-score for this feature */
            float z = fabsf(input[i] - stats->mean) / sqrtf(stats->variance);
            total_z_score += z;
            valid_features++;
        }
    }

    if (valid_features == 0) {
        /* Fall back to simple magnitude-based novelty */
        float mag = compute_input_magnitude(input, input_size);
        return nimcp_clampf(mag, 0.0f, 1.0f);
    }

    /* Convert average z-score to novelty (0-1) */
    float avg_z = total_z_score / valid_features;
    float novelty = 1.0f - expf(-avg_z * avg_z / 8.0f);

    return nimcp_clampf(novelty, 0.0f, 1.0f);
}

float nimcp_novelty_compute_surprise(
    nimcp_novelty_system_t* system,
    const float* input,
    uint32_t input_size
) {
    if (!system || !input || input_size == 0) {
        return 0.0f;
    }

    /* Simple prediction based on recent mean */
    float input_mag = compute_input_magnitude(input, input_size);

    /* Prediction is based on recent statistics */
    float predicted_mag = 0.5f;  /* Default prediction */

    if (system->feature_stats && system->input_dimension > 0) {
        float sum = 0.0f;
        for (uint32_t i = 0; i < system->input_dimension && i < input_size; i++) {
            sum += system->feature_stats[i].mean * system->feature_stats[i].mean;
        }
        predicted_mag = sqrtf(sum / (float)system->input_dimension);
    }

    /* Prediction error */
    float error = fabsf(input_mag - predicted_mag);

    /* Normalize error to 0-1 range */
    float surprise = 1.0f - expf(-error * 2.0f);

    system->predictor.prediction_error = surprise;

    return nimcp_clampf(surprise, 0.0f, 1.0f);
}

float nimcp_novelty_get_familiarity(
    nimcp_novelty_system_t* system,
    const float* input,
    uint32_t input_size
) {
    if (!system || !input || input_size == 0) {
        return 0.0f;
    }

    /* Hash the input for comparison */
    uint32_t input_hash = simple_hash(input, input_size);
    float hash_float = (float)input_hash / (float)0xFFFFFFFF;

    /* Search habituation memory for similar stimuli */
    float max_familiarity = 0.0f;

    for (uint32_t i = 0; i < system->memory_count; i++) {
        nimcp_habituation_entry_t* entry = &system->habituation_memory[i];

        /* Compare hash (simplified similarity) */
        float hash_diff = fabsf(entry->stimulus_hash - hash_float);
        float similarity = expf(-hash_diff * 10.0f);

        /* Familiarity weighted by similarity */
        float effective_familiarity = entry->familiarity * similarity;
        if (effective_familiarity > max_familiarity) {
            max_familiarity = effective_familiarity;
        }
    }

    return nimcp_clampf(max_familiarity, 0.0f, 1.0f);
}

//=============================================================================
// Habituation Implementation
//=============================================================================

int nimcp_novelty_habituate(
    nimcp_novelty_system_t* system,
    const float* input,
    uint32_t input_size
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_novelty_habituate: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    if (!input || input_size == 0) {
        return 0;
    }

    /* Hash the input */
    uint32_t input_hash = simple_hash(input, input_size);
    float hash_float = (float)input_hash / (float)0xFFFFFFFF;

    /* Look for existing entry */
    nimcp_habituation_entry_t* matching_entry = NULL;
    float best_similarity = 0.0f;

    for (uint32_t i = 0; i < system->memory_count; i++) {
        nimcp_habituation_entry_t* entry = &system->habituation_memory[i];
        float hash_diff = fabsf(entry->stimulus_hash - hash_float);
        float similarity = expf(-hash_diff * 10.0f);

        if (similarity > 0.8f && similarity > best_similarity) {
            matching_entry = entry;
            best_similarity = similarity;
        }
    }

    if (matching_entry) {
        /* Update existing entry */
        matching_entry->exposure_count += 1.0f;
        matching_entry->familiarity += 0.1f * (1.0f - matching_entry->familiarity);
        matching_entry->habituation_level += 0.1f * (1.0f - matching_entry->habituation_level);
        matching_entry->last_seen_time = 0.0f;
    } else {
        /* Create new entry */
        uint32_t idx = system->memory_index;
        nimcp_habituation_entry_t* entry = &system->habituation_memory[idx];

        entry->stimulus_hash = hash_float;
        entry->familiarity = 0.1f;
        entry->exposure_count = 1.0f;
        entry->last_seen_time = 0.0f;
        entry->habituation_level = 0.0f;

        system->memory_index = (system->memory_index + 1) % NOVELTY_MEMORY_SIZE;
        if (system->memory_count < NOVELTY_MEMORY_SIZE) {
            system->memory_count++;
        }
    }

    return 0;
}

int nimcp_novelty_dishabituate(nimcp_novelty_system_t* system, float strength) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_novelty_dishabituate: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    strength = nimcp_clampf(strength, 0.0f, 1.0f);

    /* Reduce habituation for all entries */
    for (uint32_t i = 0; i < system->memory_count; i++) {
        nimcp_habituation_entry_t* entry = &system->habituation_memory[i];
        entry->habituation_level *= (1.0f - strength);
    }

    return 0;
}

int nimcp_novelty_clear_habituation(nimcp_novelty_system_t* system) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_novelty_clear_habituation: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    memset(system->habituation_memory, 0, sizeof(system->habituation_memory));
    system->memory_index = 0;
    system->memory_count = 0;

    return 0;
}

//=============================================================================
// Query Implementation
//=============================================================================

float nimcp_novelty_get_current(const nimcp_novelty_system_t* system) {
    if (!system || !system->initialized) {
        return 0.0f;
    }
    return system->current_novelty;
}

float nimcp_novelty_get_surprise(const nimcp_novelty_system_t* system) {
    if (!system || !system->initialized) {
        return 0.0f;
    }
    return system->current_surprise;
}

float nimcp_novelty_get_exploration_drive(const nimcp_novelty_system_t* system) {
    if (!system || !system->initialized) {
        return 0.5f;
    }
    return system->exploration_drive;
}

int nimcp_novelty_get_last_result(
    const nimcp_novelty_system_t* system,
    nimcp_novelty_result_t* result
) {
    if (!system || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_novelty_get_last_result: required parameter is NULL (system, result)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_novelty_get_last_result: system->initialized is NULL");
        return -1;
    }

    *result = system->last_result;
    return 0;
}
