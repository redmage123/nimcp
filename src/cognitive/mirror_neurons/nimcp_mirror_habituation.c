/**
 * @file nimcp_mirror_habituation.c
 * @brief Mirror Neuron Habituation Implementation
 * @version 1.0.0
 * @date 2025-01-05
 *
 * WHAT: Implements habituation/response attenuation for mirror neurons
 * WHY:  Reduce response to repeated/predictable stimuli for efficiency
 * HOW:  Exponential decay model with spontaneous recovery
 */

#include "cognitive/mirror_neurons/nimcp_mirror_habituation.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(mirror_habituation, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Structure
//=============================================================================

struct habituation_system {
    habituation_config_t config;

    /** Pattern storage */
    habituation_record_t patterns[HABITUATION_MAX_PATTERNS];
    uint32_t pattern_count;

    /** Global state */
    float global_arousal;               /**< Modulates all habituation */
    uint64_t last_update_us;

    /** Statistics */
    habituation_stats_t stats;

    /** Thread safety */
    nimcp_mutex_t* mutex;

    /** Bio-async */
    bool bio_async_registered;
};

/**
 * @brief Compute feature similarity
 */
static float compute_feature_similarity(
    const float* a, const float* b, uint32_t dim
) {
    if (dim == 0) return 1.0f;

    float dot = 0.0f;
    float mag_a = 0.0f;
    float mag_b = 0.0f;

    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            mirror_habituation_heartbeat("mirror_habit_loop",
                             (float)(i + 1) / (float)dim);
        }

        dot += a[i] * b[i];
        mag_a += a[i] * a[i];
        mag_b += b[i] * b[i];
    }

    mag_a = sqrtf(mag_a);
    mag_b = sqrtf(mag_b);

    if (mag_a < 1e-6f || mag_b < 1e-6f) {
        return 0.0f;
    }

    return dot / (mag_a * mag_b);
}

/**
 * @brief Find best matching pattern
 */
static habituation_record_t* find_best_match(
    habituation_system_t* system,
    const stimulus_encoding_t* stimulus,
    float* out_similarity
) {
    habituation_record_t* best = NULL;
    float best_sim = 0.0f;

    for (uint32_t i = 0; i < system->pattern_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->pattern_count > 256) {
            mirror_habituation_heartbeat("mirror_habit_loop",
                             (float)(i + 1) / (float)system->pattern_count);
        }

        habituation_record_t* rec = &system->patterns[i];
        if (!rec->active) continue;

        /* Must match category */
        if (rec->stimulus.category != stimulus->category) continue;

        /* Compute similarity */
        float sim = habituation_stimulus_similarity(&rec->stimulus, stimulus);

        if (sim > best_sim) {
            best_sim = sim;
            best = rec;
        }
    }

    if (out_similarity) *out_similarity = best_sim;
    return best;
}

/**
 * @brief Create new pattern record
 */
static habituation_record_t* create_pattern(
    habituation_system_t* system,
    const stimulus_encoding_t* stimulus
) {
    if (system->pattern_count >= HABITUATION_MAX_PATTERNS) {
        /* Find oldest inactive or most habituated to overwrite */
        uint32_t oldest_idx = 0;
        uint64_t oldest_time = UINT64_MAX;

        for (uint32_t i = 0; i < HABITUATION_MAX_PATTERNS; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && HABITUATION_MAX_PATTERNS > 256) {
                mirror_habituation_heartbeat("mirror_habit_loop",
                                 (float)(i + 1) / (float)HABITUATION_MAX_PATTERNS);
            }

            if (!system->patterns[i].active) {
                oldest_idx = i;
                break;
            }
            if (system->patterns[i].last_exposure_us < oldest_time) {
                oldest_time = system->patterns[i].last_exposure_us;
                oldest_idx = i;
            }
        }

        habituation_record_t* rec = &system->patterns[oldest_idx];
        memset(rec, 0, sizeof(habituation_record_t));
        rec->stimulus = *stimulus;
        rec->active = true;
        rec->state = HABITUATION_STATE_NOVEL;
        rec->habituation_level = 0.0f;
        rec->response_gain = 1.0f;
        rec->first_exposure_us = nimcp_time_now_us();
        rec->last_exposure_us = rec->first_exposure_us;
        return rec;
    }

    habituation_record_t* rec = &system->patterns[system->pattern_count++];
    memset(rec, 0, sizeof(habituation_record_t));
    rec->stimulus = *stimulus;
    rec->active = true;
    rec->state = HABITUATION_STATE_NOVEL;
    rec->habituation_level = 0.0f;
    rec->response_gain = 1.0f;
    rec->first_exposure_us = nimcp_time_now_us();
    rec->last_exposure_us = rec->first_exposure_us;

    return rec;
}

/**
 * @brief Update record history
 */
static void update_history(habituation_record_t* rec, float response) {
    rec->response_history[rec->history_index] = response;
    rec->time_history[rec->history_index] = nimcp_time_now_us();

    rec->history_index = (rec->history_index + 1) % HABITUATION_HISTORY_SIZE;
    if (rec->history_count < HABITUATION_HISTORY_SIZE) {
        rec->history_count++;
    }
}

/**
 * @brief Compute inter-stimulus interval
 */
static float compute_isi(habituation_record_t* rec, uint64_t current_us) {
    if (rec->exposure_count == 0) return 0.0f;

    float isi_ms = (float)(current_us - rec->last_exposure_us) / 1000.0f;
    return isi_ms;
}

//=============================================================================
// Public API Implementation
//=============================================================================

habituation_config_t habituation_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    mirror_habituation_heartbeat("mirror_habit_habituation_config_d", 0.0f);


    habituation_config_t config = {
        .habituation_rate = 0.15f,
        .asymptote = 0.1f,              /* Never fully habituate */
        .recovery_rate = 0.01f,
        .recovery_time_constant_ms = 5000.0f,

        .generalization_width = 0.7f,
        .enable_generalization = true,

        .dishabituation_threshold = 0.8f,
        .dishabituation_boost = 0.5f,

        .sensitization_threshold = 0.9f,
        .sensitization_gain = 1.3f,
        .enable_sensitization = true,

        .temporal_window_ms = 1000.0f,
        .expected_isi_ms = 500.0f,

        .enable_simd = true,
        .bio_async_enabled = true
    };
    return config;
}

habituation_system_t* habituation_create(const habituation_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    mirror_habituation_heartbeat("mirror_habit_habituation_create", 0.0f);


    habituation_system_t* system = nimcp_calloc(1, sizeof(habituation_system_t));
    if (!system) {
        nimcp_log(LOG_LEVEL_ERROR, "Habituation: Failed to allocate system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "habituation_create: system is NULL");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        system->config = *config;
    } else {
        system->config = habituation_config_default();
    }

    /* Initialize state */
    system->pattern_count = 0;
    system->global_arousal = 1.0f;
    system->last_update_us = nimcp_time_now_us();

    /* Create mutex */
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_NORMAL;
    system->mutex = nimcp_mutex_create(&attr);
    if (!system->mutex) {
        nimcp_log(LOG_LEVEL_ERROR, "Habituation: Failed to create mutex");
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "habituation_create: system->mutex is NULL");
        return NULL;
    }

    /* Register with bio-async if enabled */
    if (system->config.bio_async_enabled) {
        habituation_register_bio_async(system);
    }

    system->stats.pattern_capacity = HABITUATION_MAX_PATTERNS;

    nimcp_log(LOG_LEVEL_INFO, "Habituation: Created system (rate=%.2f, asymptote=%.2f)",
              system->config.habituation_rate, system->config.asymptote);

    return system;
}

void habituation_destroy(habituation_system_t* system) {
    if (!system) return;

    /* Phase 8: Heartbeat at operation start */
    mirror_habituation_heartbeat("mirror_habit_habituation_destroy", 0.0f);


    if (system->bio_async_registered) {
        habituation_unregister_bio_async(system);
    }

    if (system->mutex) {
        nimcp_mutex_destroy(system->mutex);
    }

    nimcp_free(system);
    system = NULL;
    nimcp_log(LOG_LEVEL_DEBUG, "Habituation: Destroyed system");
}

//=============================================================================
// Core Processing API
//=============================================================================

bool habituation_process(
    habituation_system_t* system,
    const stimulus_encoding_t* stimulus,
    float intensity,
    habituation_result_t* result
) {
    if (!system || !stimulus || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "habituation_process: required parameter is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_habituation_heartbeat("mirror_habit_habituation_process", 0.0f);


    nimcp_mutex_lock(system->mutex);

    uint64_t now = nimcp_time_now_us();
    memset(result, 0, sizeof(habituation_result_t));

    /* Find matching pattern */
    float similarity = 0.0f;
    habituation_record_t* rec = find_best_match(system, stimulus, &similarity);

    bool is_new = false;
    bool needs_generalize = false;

    if (!rec || similarity < system->config.generalization_width) {
        /* Novel stimulus or insufficient match */
        rec = create_pattern(system, stimulus);
        is_new = true;
        result->is_new_pattern = true;
        system->stats.novel_stimuli++;
    } else {
        /* Existing pattern - check if generalization applies */
        needs_generalize = (similarity < 0.95f) && system->config.enable_generalization;
    }

    result->matched_pattern_id = rec->stimulus.stimulus_id;
    result->match_similarity = is_new ? 1.0f : similarity;

    /* Compute ISI */
    float isi_ms = compute_isi(rec, now);
    rec->time_since_last_ms = isi_ms;

    /* Check for dishabituation */
    if (!is_new && intensity > system->config.dishabituation_threshold) {
        /* Strong stimulus dishabituates */
        rec->habituation_level *= (1.0f - system->config.dishabituation_boost);
        rec->state = HABITUATION_STATE_DISHABITUATED;
        result->dishabituated = true;
        system->stats.dishabituation_events++;
    }

    /* Check for sensitization */
    if (system->config.enable_sensitization &&
        intensity > system->config.sensitization_threshold) {
        rec->state = HABITUATION_STATE_SENSITIZED;
        result->sensitized = true;
        system->stats.sensitization_events++;
    }

    /* Apply habituation update */
    if (!is_new) {
        /* Standard exponential habituation */
        float target = system->config.asymptote;
        float rate = system->config.habituation_rate * intensity;

        /* Generalization reduces habituation transfer */
        if (needs_generalize) {
            rate *= similarity;
        }

        rec->habituation_level += rate * (1.0f - rec->habituation_level);
        rec->habituation_level = nimcp_clampf(rec->habituation_level, 0.0f, 1.0f - target);

        /* Update state */
        if (rec->habituation_level > 0.8f) {
            rec->state = HABITUATION_STATE_HABITUATED;
            system->stats.habituated_stimuli++;
        } else if (rec->habituation_level > 0.0f) {
            rec->state = HABITUATION_STATE_HABITUATING;
        }
    }

    /* Compute response gain */
    float base_gain = 1.0f - rec->habituation_level;

    /* Apply sensitization boost */
    if (result->sensitized) {
        base_gain *= system->config.sensitization_gain;
    }

    /* Apply global arousal modulation */
    base_gain *= system->global_arousal;

    rec->response_gain = nimcp_clampf(base_gain, system->config.asymptote, 1.5f);
    result->response_gain = rec->response_gain;

    /* Compute novelty */
    result->novelty = is_new ? 1.0f : (1.0f - similarity) + (1.0f - rec->habituation_level) * 0.5f;
    result->novelty = nimcp_clampf(result->novelty, 0.0f, 1.0f);

    result->state = rec->state;

    /* Temporal prediction */
    if (rec->exposure_count > 1) {
        result->expected_next_exposure_ms = rec->inter_stimulus_interval_ms;
        result->temporal_surprise = fabsf(isi_ms - rec->inter_stimulus_interval_ms) /
                                    (rec->inter_stimulus_interval_ms + 1.0f);
    }

    /* Update record */
    float avg_isi = rec->inter_stimulus_interval_ms;
    rec->inter_stimulus_interval_ms = avg_isi * 0.9f + isi_ms * 0.1f;
    rec->exposure_count++;
    rec->last_exposure_us = now;

    update_history(rec, result->response_gain);

    /* Update statistics */
    system->stats.total_stimuli++;
    system->stats.avg_response_gain =
        system->stats.avg_response_gain * 0.95f + result->response_gain * 0.05f;
    system->stats.avg_habituation_level =
        system->stats.avg_habituation_level * 0.95f + rec->habituation_level * 0.05f;
    system->stats.avg_novelty =
        system->stats.avg_novelty * 0.95f + result->novelty * 0.05f;
    system->stats.active_patterns = system->pattern_count;

    nimcp_mutex_unlock(system->mutex);
    return true;
}

float habituation_query(
    const habituation_system_t* system,
    const stimulus_encoding_t* stimulus
) {
    if (!system || !stimulus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "habituation_query: required parameter is NULL");
        return 1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_habituation_heartbeat("mirror_habit_habituation_query", 0.0f);


    float similarity = 0.0f;
    habituation_record_t* rec = find_best_match(
        (habituation_system_t*)system, stimulus, &similarity);

    if (!rec || similarity < system->config.generalization_width) {
        return 1.0f;  /* Novel = full response */
    }

    return rec->response_gain;
}

uint32_t habituation_process_batch(
    habituation_system_t* system,
    const stimulus_encoding_t* stimuli,
    const float* intensities,
    habituation_result_t* results,
    uint32_t count
) {
    if (!system || !stimuli || !results || count == 0) {
        if (!system || !stimuli || !results) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "habituation_process_batch: required parameter is NULL");
            return 0;
        }
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_habituation_heartbeat("mirror_habit_habituation_process_", 0.0f);


    uint32_t processed = 0;
    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            mirror_habituation_heartbeat("mirror_habit_loop",
                             (float)(i + 1) / (float)count);
        }

        float intensity = intensities ? intensities[i] : 0.5f;
        if (habituation_process(system, &stimuli[i], intensity, &results[i])) {
            processed++;
        }
    }

    if (system->config.enable_simd && count >= HABITUATION_SIMD_THRESHOLD) {
        system->stats.simd_operations++;
    }

    return processed;
}

//=============================================================================
// Stimulus Encoding API
//=============================================================================

void habituation_encode_action(
    uint32_t action_type,
    uint32_t effector,
    uint32_t agent_id,
    const float* features,
    uint32_t feature_count,
    stimulus_encoding_t* encoding
) {
    if (!encoding) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "habituation_encode_action: encoding is NULL");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_habituation_heartbeat("mirror_habit_habituation_encode_a", 0.0f);


    memset(encoding, 0, sizeof(stimulus_encoding_t));

    encoding->stimulus_id = (action_type << 16) | (effector << 8) | (agent_id & 0xFF);
    encoding->category = STIMULUS_CATEGORY_ACTION;
    encoding->action_type = action_type;
    encoding->effector = effector;
    encoding->agent_id = agent_id;

    if (features && feature_count > 0) {
        uint32_t copy_count = (feature_count > HABITUATION_FEATURE_DIM) ?
                               HABITUATION_FEATURE_DIM : feature_count;
        memcpy(encoding->features, features, copy_count * sizeof(float));
        encoding->feature_count = copy_count;
    }

    /* Compute context hash */
    encoding->context_hash = action_type ^ (effector << 8) ^ (agent_id << 16);
}

float habituation_stimulus_similarity(
    const stimulus_encoding_t* a,
    const stimulus_encoding_t* b
) {
    if (!a || !b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "habituation_stimulus_similarity: required parameter is NULL");
        return 0.0f;
    }

    /* Category mismatch = no similarity */
    if (a->category != b->category) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    mirror_habituation_heartbeat("mirror_habit_habituation_stimulus", 0.0f);


    float similarity = 0.0f;

    /* Category-specific comparison */
    switch (a->category) {
        case STIMULUS_CATEGORY_ACTION:
            /* Weight: action_type > effector > agent > features */
            if (a->action_type == b->action_type) {
                similarity += 0.4f;
            }
            if (a->effector == b->effector) {
                similarity += 0.3f;
            }
            if (a->agent_id == b->agent_id) {
                similarity += 0.1f;
            }
            /* Feature similarity */
            if (a->feature_count > 0 && b->feature_count > 0) {
                similarity += 0.2f * compute_feature_similarity(
                    a->features, b->features,
                    (a->feature_count < b->feature_count) ? a->feature_count : b->feature_count
                );
            }
            break;

        case STIMULUS_CATEGORY_AGENT:
            similarity = (a->agent_id == b->agent_id) ? 1.0f : 0.0f;
            break;

        case STIMULUS_CATEGORY_CONTEXT:
            similarity = (a->context_hash == b->context_hash) ? 1.0f : 0.0f;
            break;

        default:
            /* Feature-only comparison */
            similarity = compute_feature_similarity(
                a->features, b->features,
                (a->feature_count < b->feature_count) ? a->feature_count : b->feature_count
            );
    }

    return nimcp_clampf(similarity, 0.0f, 1.0f);
}

//=============================================================================
// Dishabituation API
//=============================================================================

void habituation_trigger_dishabituation(
    habituation_system_t* system,
    float strength
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "habituation_trigger_dishabituation: system is NULL");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_habituation_heartbeat("mirror_habit_habituation_trigger_", 0.0f);


    nimcp_mutex_lock(system->mutex);

    for (uint32_t i = 0; i < system->pattern_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->pattern_count > 256) {
            mirror_habituation_heartbeat("mirror_habit_loop",
                             (float)(i + 1) / (float)system->pattern_count);
        }

        if (!system->patterns[i].active) continue;

        system->patterns[i].habituation_level *= (1.0f - strength);
        system->patterns[i].state = HABITUATION_STATE_DISHABITUATED;
    }

    system->stats.dishabituation_events++;

    nimcp_mutex_unlock(system->mutex);
}

void habituation_dishabituate_category(
    habituation_system_t* system,
    stimulus_category_t category,
    float strength
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "habituation_dishabituate_category: system is NULL");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_habituation_heartbeat("mirror_habit_habituation_dishabit", 0.0f);


    nimcp_mutex_lock(system->mutex);

    for (uint32_t i = 0; i < system->pattern_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->pattern_count > 256) {
            mirror_habituation_heartbeat("mirror_habit_loop",
                             (float)(i + 1) / (float)system->pattern_count);
        }

        if (!system->patterns[i].active) continue;
        if (system->patterns[i].stimulus.category != category) continue;

        system->patterns[i].habituation_level *= (1.0f - strength);
        system->patterns[i].state = HABITUATION_STATE_DISHABITUATED;
    }

    nimcp_mutex_unlock(system->mutex);
}

bool habituation_would_dishabituate(
    const habituation_system_t* system,
    const stimulus_encoding_t* stimulus,
    float intensity
) {
    if (!system || !stimulus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "habituation_would_dishabituate: required parameter is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_habituation_heartbeat("mirror_habit_habituation_would_di", 0.0f);


    return intensity > system->config.dishabituation_threshold;
}

//=============================================================================
// Recovery API
//=============================================================================

void habituation_update_recovery(
    habituation_system_t* system,
    float dt_ms
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "habituation_update_recovery: system is NULL");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_habituation_heartbeat("mirror_habit_habituation_update_r", 0.0f);


    nimcp_mutex_lock(system->mutex);

    float recovery_factor = 1.0f - expf(-dt_ms / system->config.recovery_time_constant_ms);
    float recovery_amount = system->config.recovery_rate * recovery_factor;

    for (uint32_t i = 0; i < system->pattern_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->pattern_count > 256) {
            mirror_habituation_heartbeat("mirror_habit_loop",
                             (float)(i + 1) / (float)system->pattern_count);
        }

        if (!system->patterns[i].active) continue;

        habituation_record_t* rec = &system->patterns[i];

        /* Update time tracking */
        rec->time_since_last_ms += dt_ms;

        /* Spontaneous recovery */
        if (rec->habituation_level > 0.0f) {
            rec->habituation_level -= recovery_amount;
            rec->habituation_level = nimcp_clampf(rec->habituation_level, 0.0f, 1.0f);
            rec->response_gain = 1.0f - rec->habituation_level;

            if (rec->state == HABITUATION_STATE_HABITUATED &&
                rec->habituation_level < 0.5f) {
                rec->state = HABITUATION_STATE_RECOVERING;
            }
        }
    }

    system->last_update_us = nimcp_time_now_us();

    nimcp_mutex_unlock(system->mutex);
}

const habituation_record_t* habituation_get_record(
    const habituation_system_t* system,
    uint32_t pattern_id
) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_habituation_heartbeat("mirror_habit_habituation_get_reco", 0.0f);


    for (uint32_t i = 0; i < system->pattern_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->pattern_count > 256) {
            mirror_habituation_heartbeat("mirror_habit_loop",
                             (float)(i + 1) / (float)system->pattern_count);
        }

        if (system->patterns[i].stimulus.stimulus_id == pattern_id) {
            return &system->patterns[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "habituation_get_record: validation failed");
    return NULL;
}

void habituation_clear_pattern(
    habituation_system_t* system,
    uint32_t pattern_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "habituation_clear_pattern: system is NULL");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_habituation_heartbeat("mirror_habit_habituation_clear_pa", 0.0f);


    nimcp_mutex_lock(system->mutex);

    for (uint32_t i = 0; i < system->pattern_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->pattern_count > 256) {
            mirror_habituation_heartbeat("mirror_habit_loop",
                             (float)(i + 1) / (float)system->pattern_count);
        }

        if (system->patterns[i].stimulus.stimulus_id == pattern_id) {
            system->patterns[i].active = false;
            system->patterns[i].habituation_level = 0.0f;
            system->patterns[i].state = HABITUATION_STATE_NOVEL;
            break;
        }
    }

    nimcp_mutex_unlock(system->mutex);
}

//=============================================================================
// SIMD Optimization API
//=============================================================================

void habituation_simd_similarities(
    const float* query_features,
    const float* pattern_features,
    float* similarities,
    uint32_t pattern_count,
    uint32_t feature_dim
) {
    /* Scalar implementation */
    /* Phase 8: Heartbeat at operation start */
    mirror_habituation_heartbeat("mirror_habit_habituation_simd_sim", 0.0f);


    for (uint32_t p = 0; p < pattern_count; p++) {
        /* Phase 8: Loop progress heartbeat */
        if ((p & 0xFF) == 0 && pattern_count > 256) {
            mirror_habituation_heartbeat("mirror_habit_loop",
                             (float)(p + 1) / (float)pattern_count);
        }

        const float* pattern = pattern_features + p * feature_dim;
        similarities[p] = compute_feature_similarity(query_features, pattern, feature_dim);
    }
}

void habituation_simd_update(
    float* habituation_levels,
    const float* intensities,
    float habituation_rate,
    float asymptote,
    uint32_t count
) {
    /* Phase 8: Heartbeat at operation start */
    mirror_habituation_heartbeat("mirror_habit_habituation_simd_upd", 0.0f);


    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            mirror_habituation_heartbeat("mirror_habit_loop",
                             (float)(i + 1) / (float)count);
        }

        float rate = habituation_rate * intensities[i];
        habituation_levels[i] += rate * (1.0f - habituation_levels[i]);
        habituation_levels[i] = nimcp_clampf(habituation_levels[i], 0.0f, 1.0f - asymptote);
    }
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

bool habituation_register_bio_async(habituation_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "habituation_register_bio_async: system is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_habituation_heartbeat("mirror_habit_habituation_register", 0.0f);


    system->bio_async_registered = true;
    nimcp_log(LOG_LEVEL_DEBUG, "Habituation: Registered with bio-async");
    return true;
}

void habituation_unregister_bio_async(habituation_system_t* system) {
    if (!system) return;
    /* Phase 8: Heartbeat at operation start */
    mirror_habituation_heartbeat("mirror_habit_habituation_unregist", 0.0f);


    system->bio_async_registered = false;
    nimcp_log(LOG_LEVEL_DEBUG, "Habituation: Unregistered from bio-async");
}

//=============================================================================
// Statistics API
//=============================================================================

bool habituation_get_stats(
    const habituation_system_t* system,
    habituation_stats_t* stats
) {
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "habituation_get_stats: required parameter is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_habituation_heartbeat("mirror_habit_habituation_get_stat", 0.0f);


    nimcp_mutex_lock(((habituation_system_t*)system)->mutex);
    *stats = system->stats;
    nimcp_mutex_unlock(((habituation_system_t*)system)->mutex);

    return true;
}

void habituation_reset_stats(habituation_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "habituation_reset_stats: system is NULL");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_habituation_heartbeat("mirror_habit_habituation_reset_st", 0.0f);


    nimcp_mutex_lock(system->mutex);
    uint32_t capacity = system->stats.pattern_capacity;
    memset(&system->stats, 0, sizeof(habituation_stats_t));
    system->stats.pattern_capacity = capacity;
    nimcp_mutex_unlock(system->mutex);
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void mirror_habituation_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_mirror_habituation_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training stubs
 * ============================================================================ */
int mirror_habituation_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_habituation_training_begin: NULL argument");
        return -1;
    }
    mirror_habituation_heartbeat_instance(NULL, "mirror_habituation_training_begin", 0.0f);
    return 0;
}

int mirror_habituation_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_habituation_training_end: NULL argument");
        return -1;
    }
    mirror_habituation_heartbeat_instance(NULL, "mirror_habituation_training_end", 1.0f);
    return 0;
}

int mirror_habituation_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_habituation_training_step: NULL argument");
        return -1;
    }
    mirror_habituation_heartbeat_instance(NULL, "mirror_habituation_training_step", progress);
    return 0;
}
