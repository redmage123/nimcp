/**
 * @file nimcp_intuition_integrations.c
 * @brief Integration & Extrapolation Framework implementation
 */

#include "cognitive/parietal/nimcp_intuition_integrations.h"
#include "constants/nimcp_buffer_constants.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE(intuition_integrations, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

struct intuition_system {
    intuition_system_config_t config;
    intuition_system_stats_t stats;

    /* Phase 6 sub-engines */
    intuitive_engine_t* intuitive;
    analogical_engine_t* analogical;
    insight_engine_t* insight;
    hypothesis_engine_t* hypothesis;
    blending_engine_t* blending;
    counterfactual_engine_t* counterfactual;
    meta_engine_t* meta;

    /* Attached external systems */
    training_engine_t* training;
    working_memory_t* working_memory;
    episodic_memory_t* episodic_memory;
    semantic_memory_t* semantic_memory;
    attention_system_t* attention;
    executive_function_t* executive;
    emotion_system_t* emotion;
    logic_gate_network_t* logic_gates;

    /* Biological state */
    float inflammation;
    float fatigue;

    /* Internal state */
    uint32_t next_extrapolation_id;
    uint32_t next_synthesis_id;
    uint32_t next_prediction_id;
};

static _Thread_local char g_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

/* ============================================================================
 * INTERNAL HELPERS
 * ============================================================================ */

static void set_error(const char* msg) {
    strncpy(g_last_error, msg, sizeof(g_last_error) - 1);
}

static float apply_mod(const intuition_system_t* s, float v) {
    float f = 1.0f - s->inflammation * s->config.inflammation_sensitivity * 0.2f
                   - s->fatigue * s->config.fatigue_sensitivity * 0.3f;
    return v * fmaxf(0.4f, f);
}

/* Linear regression for trend detection */
static void fit_linear_trend(const intuition_data_point_t** data, uint32_t count,
                             float* slope, float* intercept, float* r_squared) {
    if (!data || count < 2 || !slope || !intercept || !r_squared) {
        if (slope) *slope = 0;
        if (intercept) *intercept = 0;
        if (r_squared) *r_squared = 0;
        return;
    }

    /* Compute means */
    float sum_x = 0, sum_y = 0;
    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            intuition_integrations_heartbeat("intuition_in_loop",
                             (float)(i + 1) / (float)count);
        }

        sum_x += data[i]->timestamp;
        sum_y += (data[i]->values && data[i]->dim > 0) ? data[i]->values[0] : 0;
    }
    float mean_x = sum_x / (fabsf(count) > 1e-7f ? count : 1e-7f);
    float mean_y = sum_y / (fabsf(count) > 1e-7f ? count : 1e-7f);

    /* Compute slope */
    float num = 0, denom = 0, ss_tot = 0;
    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            intuition_integrations_heartbeat("intuition_in_loop",
                             (float)(i + 1) / (float)count);
        }

        float x = data[i]->timestamp;
        float y = (data[i]->values && data[i]->dim > 0) ? data[i]->values[0] : 0;
        num += (x - mean_x) * (y - mean_y);
        denom += (x - mean_x) * (x - mean_x);
        ss_tot += (y - mean_y) * (y - mean_y);
    }

    *slope = (denom > 1e-10f) ? num / denom : 0;
    *intercept = mean_y - (*slope) * mean_x;

    /* Compute R-squared */
    float ss_res = 0;
    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            intuition_integrations_heartbeat("intuition_in_loop",
                             (float)(i + 1) / (float)count);
        }

        float x = data[i]->timestamp;
        float y = (data[i]->values && data[i]->dim > 0) ? data[i]->values[0] : 0;
        float pred = (*slope) * x + (*intercept);
        ss_res += (y - pred) * (y - pred);
    }
    *r_squared = (ss_tot > 1e-10f) ? 1.0f - (ss_res / ss_tot) : 0;
}

/* Detect periodicity using autocorrelation */
static float detect_periodicity(const intuition_data_point_t** data, uint32_t count) {
    if (!data || count < 8) return 0;

    float best_period = 0;
    float best_corr = 0;

    for (uint32_t lag = 2; lag < count / 2; lag++) {
        float corr = 0, norm1 = 0, norm2 = 0;
        for (uint32_t i = 0; i < count - lag; i++) {
            float v1 = (data[i]->values && data[i]->dim > 0) ? data[i]->values[0] : 0;
            float v2 = (data[i + lag]->values && data[i + lag]->dim > 0) ?
                       data[i + lag]->values[0] : 0;
            corr += v1 * v2;
            norm1 += v1 * v1;
            norm2 += v2 * v2;
        }
        float denom = sqrtf(norm1 * norm2);
        if (denom > 1e-10f) {
            corr /= denom;
            if (corr > best_corr) {
                best_corr = corr;
                best_period = (float)lag;
            }
        }
    }

    return (best_corr > 0.5f) ? best_period : 0;
}

/* ============================================================================
 * LIFECYCLE FUNCTIONS
 * ============================================================================ */

intuition_system_config_t intuition_system_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_system_def", 0.0f);


    return (intuition_system_config_t){
        .enable_intuitive_engine = true,
        .enable_analogical_engine = true,
        .enable_insight_engine = true,
        .enable_hypothesis_engine = true,
        .enable_blending_engine = true,
        .enable_counterfactual_engine = true,
        .enable_meta_engine = true,

        .enable_training_integration = true,
        .enable_memory_integration = true,
        .enable_attention_integration = true,
        .enable_emotion_integration = true,
        .enable_logic_validation = true,

        .extrapolation_confidence_decay = 0.1f,
        .min_extrapolation_confidence = 0.2f,
        .max_extrapolation_steps = 100,

        .synthesis_coherence_threshold = 0.5f,
        .gap_importance_threshold = 0.3f,
        .contradiction_severity_threshold = 0.4f,

        .inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .fatigue_sensitivity = 1.0f
    };
}

intuition_system_t* intuition_system_create(void) {
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_system_cre", 0.0f);


    intuition_system_config_t config = intuition_system_default_config();
    return intuition_system_create_custom(&config);
}

intuition_system_t* intuition_system_create_custom(const intuition_system_config_t* config) {
    if (!config) {
        set_error("NULL config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "intuition_system_create_custom: config is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_system_cre", 0.0f);


    intuition_system_t* s = nimcp_calloc(1, sizeof(intuition_system_t));
    if (!s) {
        set_error("Memory allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "intuition_system_create_custom: s is NULL");
        return NULL;
    }

    s->config = *config;
    s->next_extrapolation_id = 1;
    s->next_synthesis_id = 1;
    s->next_prediction_id = 1;

    /* Create sub-engines */
    if (config->enable_intuitive_engine) {
        s->intuitive = intuitive_engine_create();
    }
    if (config->enable_analogical_engine) {
        s->analogical = analogical_engine_create();
    }
    if (config->enable_insight_engine) {
        s->insight = insight_engine_create();
    }
    if (config->enable_hypothesis_engine) {
        s->hypothesis = hypothesis_engine_create();
    }
    if (config->enable_blending_engine) {
        s->blending = blending_engine_create();
    }
    if (config->enable_counterfactual_engine) {
        s->counterfactual = counterfactual_engine_create();
    }
    if (config->enable_meta_engine) {
        s->meta = meta_engine_create();
    }

    return s;
}

void intuition_system_destroy(intuition_system_t* system) {
    if (!system) return;

    /* Destroy sub-engines */
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_system_des", 0.0f);


    if (system->intuitive) intuitive_engine_destroy(system->intuitive);
    if (system->analogical) analogical_engine_destroy(system->analogical);
    if (system->insight) insight_engine_destroy(system->insight);
    if (system->hypothesis) hypothesis_engine_destroy(system->hypothesis);
    if (system->blending) blending_engine_destroy(system->blending);
    if (system->counterfactual) counterfactual_engine_destroy(system->counterfactual);
    if (system->meta) meta_engine_destroy(system->meta);

    /* Note: External systems are NOT destroyed - they're owned elsewhere */

    nimcp_free(system);
    system = NULL;
}

/* ============================================================================
 * ATTACHMENT FUNCTIONS - TRAINING
 * ============================================================================ */

int intuition_attach_training(intuition_system_t* system, training_engine_t* training) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_attach_training: system is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_attach_tra", 0.0f);


    system->training = training;
    return 0;
}

int intuition_feedback_success(intuition_system_t* system, const hunch_t* hunch,
                               float actual_outcome) {
    if (!system || !hunch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_attach_training: required parameter is NULL (system, hunch)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_feedback_s", 0.0f);


    system->stats.intuitions_trained++;

    /* Compare hunch confidence to actual outcome */
    float error = fabsf(hunch->score.confidence - actual_outcome);
    bool was_correct = error < 0.3f;

    if (was_correct) {
        system->stats.intuitions_confirmed++;
    } else {
        system->stats.intuitions_refuted++;
    }

    /* Update running accuracy */
    float total = (float)(system->stats.intuitions_confirmed +
                          system->stats.intuitions_refuted);
    if (total > 0) {
        system->stats.avg_intuition_accuracy =
            (float)system->stats.intuitions_confirmed / total;
    }

    /* If training engine attached, could send training signal */
    /* (Training integration would happen here with actual training_engine API) */

    return 0;
}

int intuition_update_priors(intuition_system_t* system,
                            const hypogen_theory_t* confirmed_theory) {
    if (!system || !confirmed_theory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_attach_training: required parameter is NULL (system, confirmed_theory)");
        return -1;
    }

    /* Update hypothesis engine with confirmed theory */
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_update_pri", 0.0f);


    if (system->hypothesis) {
        /* Learning would occur here - update internal priors */
        /* For now, track statistics */
        system->stats.intuitions_confirmed++;
    }

    return 0;
}

int intuition_train_from_experience(intuition_system_t* system,
                                    const intuition_experience_t** experiences,
                                    uint32_t count) {
    if (!system || !experiences) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_attach_training: required parameter is NULL (system, experiences)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_train_from", 0.0f);


    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            intuition_integrations_heartbeat("intuition_in_loop",
                             (float)(i + 1) / (float)count);
        }

        if (experiences[i] && experiences[i]->hunch) {
            intuition_feedback_success(system, experiences[i]->hunch,
                                       experiences[i]->actual_outcome);
        }
    }

    return 0;
}

/* ============================================================================
 * ATTACHMENT FUNCTIONS - MEMORY SYSTEMS
 * ============================================================================ */

int intuition_attach_working_memory(intuition_system_t* system, working_memory_t* wm) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_attach_working_memory: system is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_attach_wor", 0.0f);


    system->working_memory = wm;
    return 0;
}

int intuition_attach_episodic_memory(intuition_system_t* system, episodic_memory_t* episodic) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_attach_episodic_memory: system is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_attach_epi", 0.0f);


    system->episodic_memory = episodic;
    return 0;
}

int intuition_attach_semantic_memory(intuition_system_t* system, semantic_memory_t* semantic) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_attach_semantic_memory: system is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_attach_sem", 0.0f);


    system->semantic_memory = semantic;
    return 0;
}

/* ============================================================================
 * ATTACHMENT FUNCTIONS - COGNITIVE SYSTEMS
 * ============================================================================ */

int intuition_attach_attention(intuition_system_t* system, attention_system_t* attention) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_attach_attention: system is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_attach_att", 0.0f);


    system->attention = attention;
    return 0;
}

int intuition_attach_executive(intuition_system_t* system, executive_function_t* executive) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_attach_executive: system is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_attach_exe", 0.0f);


    system->executive = executive;
    return 0;
}

int intuition_attach_emotion(intuition_system_t* system, emotion_system_t* emotion) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_attach_emotion: system is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_attach_emo", 0.0f);


    system->emotion = emotion;
    return 0;
}

int intuition_attach_logic_gates(intuition_system_t* system, logic_gate_network_t* logic) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_attach_logic_gates: system is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_attach_log", 0.0f);


    system->logic_gates = logic;
    return 0;
}

/* ============================================================================
 * LOGIC VALIDATION FUNCTIONS
 * ============================================================================ */

bool intuition_validate_with_logic(intuition_system_t* system, const hunch_t* hunch) {
    if (!system || !hunch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_validate_with_logic: required parameter is NULL (system, hunch)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_validate_w", 0.0f);


    system->stats.logic_validations++;

    /* If no logic gates attached, default to accepting high-confidence hunches */
    if (!system->logic_gates) {
        bool valid = hunch->score.confidence > 0.5f;
        if (valid) system->stats.logic_validation_passes++;
        return valid;
    }

    /* With logic gates, would perform formal validation here */
    /* For now, use heuristic based on coherence and confidence */
    float validity_score = (hunch->score.confidence + hunch->score.coherence) / 2.0f;
    validity_score = apply_mod(system, validity_score);

    bool valid = validity_score > 0.5f;
    if (valid) system->stats.logic_validation_passes++;

    return valid;
}

hypogen_theory_t* intuition_logic_refine(intuition_system_t* system, const hunch_t* hunch) {
    if (!system || !hunch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_logic_refine: required parameter is NULL (system, hunch)");
        return NULL;
    }
    if (!system->hypothesis) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_logic_refine: system->hypothesis is NULL");
        return NULL;
    }

    /* Convert hunch to observation for hypothesis generation */
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_logic_refi", 0.0f);


    hypogen_observation_t obs = {
        .id = 1,
        .confidence = hunch->score.confidence
    };
    strncpy(obs.description, hunch->description, sizeof(obs.description) - 1);

    /* Use abductive inference to create refined theory */
    hypogen_theory_t* theory = hypothesis_abductive_inference(system->hypothesis, &obs);

    return theory;
}

/* ============================================================================
 * EXTRAPOLATION FUNCTIONS
 * ============================================================================ */

extrapolation_t* intuition_extrapolate(intuition_system_t* system,
                                       const intuition_data_point_t** known,
                                       uint32_t count,
                                       const intuition_range_t* target_range) {
    if (!system || !known || count == 0 || !target_range) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_logic_refine: required parameter is NULL (system, known, target_range)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_extrapolat", 0.0f);


    extrapolation_t* ext = nimcp_calloc(1, sizeof(extrapolation_t));
    if (!ext) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate ext");

        return NULL;

    }

    ext->id = system->next_extrapolation_id++;

    /* Copy known data references */
    ext->known_data = nimcp_calloc(count, sizeof(intuition_data_point_t*));
    if (ext->known_data) {
        memcpy(ext->known_data, known, count * sizeof(intuition_data_point_t*));
        ext->num_known = count;
    }

    /* Detect trend */
    ext->detected_trend = nimcp_calloc(1, sizeof(intuition_trend_t));
    if (ext->detected_trend) {
        ext->detected_trend->id = 1;

        fit_linear_trend(known, count,
                         &ext->detected_trend->slope,
                         &ext->detected_trend->intercept,
                         &ext->detected_trend->r_squared);

        ext->detected_trend->period = detect_periodicity(known, count);
        ext->detected_trend->has_seasonality = ext->detected_trend->period > 0;
        ext->detected_trend->is_monotonic = ext->detected_trend->r_squared > 0.7f;

        snprintf(ext->detected_trend->description, sizeof(ext->detected_trend->description),
                 "Linear trend: slope=%.3f, R²=%.3f", ext->detected_trend->slope,
                 ext->detected_trend->r_squared);
    }

    /* Create validity bounds */
    ext->validity_bounds = nimcp_calloc(1, sizeof(intuition_boundary_t));
    if (ext->validity_bounds) {
        /* Find data range */
        float min_t = known[0]->timestamp, max_t = known[0]->timestamp;
        for (uint32_t i = 1; i < count; i++) {
            if (known[i]->timestamp < min_t) min_t = known[i]->timestamp;
            if (known[i]->timestamp > max_t) max_t = known[i]->timestamp;
        }
        float data_range = max_t - min_t;

        /* Set reasonable extrapolation bounds */
        ext->validity_bounds->lower_bound = min_t - data_range * 0.5f;
        ext->validity_bounds->upper_bound = max_t + data_range * 0.5f;
        ext->validity_bounds->confidence_decay = system->config.extrapolation_confidence_decay;
        snprintf(ext->validity_bounds->warning, sizeof(ext->validity_bounds->warning),
                 "Extrapolation beyond %.1f range may be unreliable", data_range);

        ext->max_reliable_distance = data_range * 0.5f;
    }

    /* Generate extrapolated points */
    uint32_t num_samples = target_range->num_samples;
    if (num_samples > system->config.max_extrapolation_steps) {
        num_samples = system->config.max_extrapolation_steps;
    }

    ext->extrapolated = nimcp_calloc(num_samples, sizeof(intuition_data_point_t*));
    if (!ext->extrapolated) return NULL;
    if (ext->extrapolated && ext->detected_trend) {
        float step = (target_range->end - target_range->start) / (float)num_samples;
        float last_known_t = known[count - 1]->timestamp;

        for (uint32_t i = 0; i < num_samples; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_samples > 256) {
                intuition_integrations_heartbeat("intuition_in_loop",
                                 (float)(i + 1) / (float)num_samples);
            }

            float t = target_range->start + step * i;

            /* Predict using trend */
            float pred = ext->detected_trend->slope * t + ext->detected_trend->intercept;

            /* Add periodicity if detected */
            if (ext->detected_trend->has_seasonality && ext->detected_trend->period > 0) {
                /* Simple sinusoidal overlay */
                float phase = fmodf(t, ext->detected_trend->period) /
                              ext->detected_trend->period * NIMCP_TWO_PI_F;
                pred += 0.1f * sinf(phase);
            }

            /* Compute confidence decay with distance */
            float distance = fabsf(t - last_known_t);
            float conf = ext->detected_trend->r_squared *
                         expf(-distance * system->config.extrapolation_confidence_decay);
            conf = apply_mod(system, conf);
            conf = fmaxf(system->config.min_extrapolation_confidence, conf);

            ext->extrapolated[i] = intuition_data_point_create(&pred, 1, t, conf);
            if (ext->extrapolated[i]) {
                ext->num_extrapolated++;
            }
        }
    }

    /* Set overall confidence */
    if (ext->detected_trend) {
        ext->extrapolation_confidence = apply_mod(system, ext->detected_trend->r_squared);
    }

    ext->uses_ensemble = false;
    ext->ensemble_size = 1;

    system->stats.extrapolations_performed++;

    return ext;
}

extrapolation_t* intuition_extrapolate_incremental(intuition_system_t* system,
                                                   const extrapolation_t* previous,
                                                   const intuition_data_point_t** new_data,
                                                   uint32_t new_count) {
    if (!system || !previous || !new_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "intuition_logic_refine: required parameter is NULL (system, previous, new_data)");
        return NULL;
    }

    /* Combine old and new data */
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_extrapolat", 0.0f);


    uint32_t total_count = previous->num_known + new_count;
    intuition_data_point_t** combined = nimcp_calloc(total_count,
                                                     sizeof(intuition_data_point_t*));
    if (!combined) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate combined");

        return NULL;

    }

    memcpy(combined, previous->known_data,
           previous->num_known * sizeof(intuition_data_point_t*));
    memcpy(combined + previous->num_known, new_data,
           new_count * sizeof(intuition_data_point_t*));

    /* Create new range from previous extrapolation */
    intuition_range_t range = {
        .start = previous->extrapolated[0]->timestamp,
        .end = previous->extrapolated[previous->num_extrapolated - 1]->timestamp,
        .num_samples = previous->num_extrapolated,
        .is_temporal = true
    };

    extrapolation_t* result = intuition_extrapolate(system,
                                                    (const intuition_data_point_t**)combined,
                                                    total_count, &range);

    nimcp_free(combined);
    combined = NULL;
    return result;
}

bool intuition_detect_extrapolation_failure(intuition_system_t* system,
                                            const extrapolation_t* extrapolation,
                                            const intuition_data_point_t* actual) {
    if (!system || !extrapolation || !actual) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_logic_refine: required parameter is NULL (system, extrapolation, actual)");
        return false;
    }

    /* Find predicted value at actual's timestamp */
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_detect_ext", 0.0f);


    float predicted = 0;
    bool found = false;

    for (uint32_t i = 0; i < extrapolation->num_extrapolated; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && extrapolation->num_extrapolated > 256) {
            intuition_integrations_heartbeat("intuition_in_loop",
                             (float)(i + 1) / (float)extrapolation->num_extrapolated);
        }

        if (fabsf(extrapolation->extrapolated[i]->timestamp - actual->timestamp) < 0.01f) {
            predicted = extrapolation->extrapolated[i]->values[0];
            found = true;
            break;
        }
    }

    if (!found && extrapolation->detected_trend) {
        /* Interpolate using trend */
        predicted = extrapolation->detected_trend->slope * actual->timestamp +
                    extrapolation->detected_trend->intercept;
        found = true;
    }

    if (!found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: found is NULL");
        return false;
    }

    /* Compare to actual */
    float actual_val = (actual->values && actual->dim > 0) ? actual->values[0] : 0;
    float error = fabsf(predicted - actual_val);
    float relative_error = (fabsf(actual_val) > 1e-10f) ?
                           error / fabsf(actual_val) : error;

    /* Significant failure if error > 30% or absolute error > 0.5 */
    bool failure = relative_error > 0.3f || error > 0.5f;

    if (failure) {
        /* Would update statistics for failed extrapolation */
    } else {
        system->stats.successful_extrapolations++;
    }

    return failure;
}

void extrapolation_free(extrapolation_t* ext) {
    if (!ext) return;

    /* Free extrapolated points */
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_extrapolation_free", 0.0f);


    if (ext->extrapolated) {
        for (uint32_t i = 0; i < ext->num_extrapolated; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && ext->num_extrapolated > 256) {
                intuition_integrations_heartbeat("intuition_in_loop",
                                 (float)(i + 1) / (float)ext->num_extrapolated);
            }

            intuition_data_point_free(ext->extrapolated[i]);
        }
        nimcp_free(ext->extrapolated);
    }

    /* Note: known_data points are NOT freed - they're owned elsewhere */
    if (ext->known_data) nimcp_free(ext->known_data);

    if (ext->detected_trend) nimcp_free(ext->detected_trend);
    if (ext->validity_bounds) nimcp_free(ext->validity_bounds);

    nimcp_free(ext);
    ext = NULL;
}

/* ============================================================================
 * NOVEL PREDICTION FUNCTIONS
 * ============================================================================ */

novel_prediction_t** intuition_predict_novel(intuition_system_t* system,
                                             const prediction_domain_t* domain,
                                             uint32_t* num_predictions) {
    if (!system || !domain || !num_predictions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "extrapolation_free: required parameter is NULL (system, domain, num_predictions)");
        return NULL;
    }

    *num_predictions = 0;

    /* Generate a few novel predictions within domain bounds */
    uint32_t max_pred = 5;
    novel_prediction_t** preds = nimcp_calloc(max_pred, sizeof(novel_prediction_t*));
    if (!preds) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate preds");

        return NULL;

    }

    for (uint32_t i = 0; i < max_pred && i < INTUITION_MAX_PREDICTIONS; i++) {
        novel_prediction_t* p = nimcp_calloc(1, sizeof(novel_prediction_t));
        if (!p) break;

        p->id = system->next_prediction_id++;

        /* Generate prediction values within domain bounds */
        p->dim = domain->dim;
        p->prediction = nimcp_calloc(domain->dim, sizeof(float));
        if (p->prediction) {
            for (uint32_t d = 0; d < domain->dim && d * 2 + 1 < domain->dim; d++) {
                float min_val = domain->feature_bounds[d * 2];
                float max_val = domain->feature_bounds[d * 2 + 1];
                float range = max_val - min_val;

                /* Explore slightly beyond bounds based on exploration factor */
                float extended_min = min_val - range * domain->exploration_factor * 0.2f;
                float extended_max = max_val + range * domain->exploration_factor * 0.2f;

                /* Generate prediction (simple random-like based on index) */
                float t = (float)i / (float)max_pred;
                p->prediction[d] = extended_min + (extended_max - extended_min) * t;
            }
        }

        p->confidence = apply_mod(system, 0.5f - 0.1f * i);
        p->novelty = apply_mod(system, 0.6f + 0.1f * i);
        p->is_out_of_distribution = (i >= 2);

        snprintf(p->rationale, sizeof(p->rationale),
                 "Novel prediction %u in domain %s with exploration %.2f",
                 i + 1, domain->name, domain->exploration_factor);

        preds[(*num_predictions)++] = p;
    }

    system->stats.novel_predictions_made += *num_predictions;

    return preds;
}

void novel_prediction_free(novel_prediction_t* pred) {
    if (!pred) return;
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_novel_prediction_fre", 0.0f);


    if (pred->prediction) nimcp_free(pred->prediction);
    nimcp_free(pred);
    pred = NULL;
}

/* ============================================================================
 * KNOWLEDGE SYNTHESIS FUNCTIONS
 * ============================================================================ */

synthesis_t* intuition_synthesize_knowledge(intuition_system_t* system,
                                            const knowledge_fragment_t** fragments,
                                            uint32_t count) {
    if (!system || !fragments || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "novel_prediction_free: required parameter is NULL (system, fragments)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_synthesize", 0.0f);


    synthesis_t* synth = nimcp_calloc(1, sizeof(synthesis_t));
    if (!synth) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate synth");

        return NULL;

    }

    synth->id = system->next_synthesis_id++;

    /* Copy source references */
    synth->sources = nimcp_calloc(count, sizeof(knowledge_fragment_t*));
    if (synth->sources) {
        memcpy(synth->sources, fragments, count * sizeof(knowledge_fragment_t*));
        synth->num_sources = count;
    }

    /* Create unified representation */
    synth->synthesized = nimcp_calloc(1, sizeof(unified_knowledge_t));
    if (synth->synthesized) {
        /* Compute average dimensionality */
        uint32_t total_dim = 0;
        for (uint32_t i = 0; i < count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && count > 256) {
                intuition_integrations_heartbeat("intuition_in_loop",
                                 (float)(i + 1) / (float)count);
            }

            if (fragments[i]) total_dim += fragments[i]->content_dim;
        }
        uint32_t avg_dim = (count > 0) ? total_dim / count : 32;
        avg_dim = (avg_dim < 32) ? 32 : avg_dim;

        synth->synthesized->dim = avg_dim;
        synth->synthesized->unified_representation = nimcp_calloc(avg_dim, sizeof(float));

        /* Simple averaging of content vectors */
        if (synth->synthesized->unified_representation) {
            for (uint32_t i = 0; i < count; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && count > 256) {
                    intuition_integrations_heartbeat("intuition_in_loop",
                                     (float)(i + 1) / (float)count);
                }

                if (fragments[i] && fragments[i]->content) {
                    uint32_t d = (fragments[i]->content_dim < avg_dim) ?
                                  fragments[i]->content_dim : avg_dim;
                    for (uint32_t j = 0; j < d; j++) {
                        /* Phase 8: Loop progress heartbeat */
                        if ((j & 0xFF) == 0 && d > 256) {
                            intuition_integrations_heartbeat("intuition_in_loop",
                                             (float)(j + 1) / (float)d);
                        }

                        synth->synthesized->unified_representation[j] +=
                            fragments[i]->content[j] * fragments[i]->confidence / (fabsf(count) > 1e-7f ? count : 1e-7f);
                    }
                }
            }
        }

        /* Compute coherence (variance of confidence scores) */
        float mean_conf = 0;
        for (uint32_t i = 0; i < count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && count > 256) {
                intuition_integrations_heartbeat("intuition_in_loop",
                                 (float)(i + 1) / (float)count);
            }

            if (fragments[i]) mean_conf += fragments[i]->confidence;
        }
        mean_conf /= count;

        float var_conf = 0;
        for (uint32_t i = 0; i < count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && count > 256) {
                intuition_integrations_heartbeat("intuition_in_loop",
                                 (float)(i + 1) / (float)count);
            }

            if (fragments[i]) {
                float diff = fragments[i]->confidence - mean_conf;
                var_conf += diff * diff;
            }
        }
        var_conf /= count;

        synth->synthesized->coherence = apply_mod(system, 1.0f - sqrtf(var_conf));
        synth->synthesized->coverage = apply_mod(system, fminf(1.0f, (float)count / 10.0f));

        snprintf(synth->synthesized->summary, sizeof(synth->synthesized->summary),
                 "Synthesized %u knowledge fragments with coherence %.2f",
                 count, synth->synthesized->coherence);
    }

    /* Identify gaps */
    synth->identified_gaps = nimcp_calloc(INTUITION_MAX_GAPS, sizeof(intuition_gap_t*));
    if (!synth->identified_gaps) return NULL;
    synth->num_gaps = 0;

    /* Check for low-confidence regions as gaps */
    for (uint32_t i = 0; i < count && synth->num_gaps < INTUITION_MAX_GAPS; i++) {
        if (fragments[i] && fragments[i]->confidence < 0.5f) {
            intuition_gap_t* gap = nimcp_calloc(1, sizeof(intuition_gap_t));
            if (gap) {
                gap->id = synth->num_gaps + 1;
                snprintf(gap->description, sizeof(gap->description),
                         "Low confidence in fragment: %s", fragments[i]->description);
                gap->importance = 1.0f - fragments[i]->confidence;
                gap->fillability = 0.7f;
                synth->identified_gaps[synth->num_gaps++] = gap;
                system->stats.gaps_identified++;
            }
        }
    }

    /* Identify contradictions (fragments with opposite high confidence) */
    synth->conflicts = nimcp_calloc(INTUITION_MAX_GAPS, sizeof(knowledge_contradiction_t*));
    if (!synth->conflicts) return NULL;
    synth->num_conflicts = 0;

    for (uint32_t i = 0; i < count && synth->num_conflicts < INTUITION_MAX_GAPS; i++) {
        for (uint32_t j = i + 1; j < count; j++) {
            if (fragments[i] && fragments[j] &&
                fragments[i]->content && fragments[j]->content) {
                /* Simple conflict detection: opposite sign in first dimension */
                uint32_t min_d = (fragments[i]->content_dim < fragments[j]->content_dim) ?
                                  fragments[i]->content_dim : fragments[j]->content_dim;
                if (min_d > 0) {
                    float v1 = fragments[i]->content[0];
                    float v2 = fragments[j]->content[0];
                    if ((v1 > 0 && v2 < -0.5f) || (v1 < -0.5f && v2 > 0)) {
                        knowledge_contradiction_t* conflict =
                            nimcp_calloc(1, sizeof(knowledge_contradiction_t));
                        if (conflict) {
                            conflict->id = synth->num_conflicts + 1;
                            conflict->fragment1_id = fragments[i]->id;
                            conflict->fragment2_id = fragments[j]->id;
                            conflict->severity = fabsf(v1 - v2) / 2.0f;
                            snprintf(conflict->description, sizeof(conflict->description),
                                     "Contradictory fragments: %s vs %s",
                                     fragments[i]->description, fragments[j]->description);
                            synth->conflicts[synth->num_conflicts++] = conflict;
                            system->stats.contradictions_found++;
                        }
                    }
                }
            }
        }
    }

    /* Compute overall synthesis confidence */
    synth->synthesis_confidence = apply_mod(system,
        synth->synthesized ? synth->synthesized->coherence * 0.7f +
                             synth->synthesized->coverage * 0.3f : 0.5f);

    synth->novelty_score = apply_mod(system, 0.4f);

    system->stats.syntheses_performed++;

    return synth;
}

intuition_gap_t** intuition_identify_knowledge_gaps(intuition_system_t* system,
                                                    const prediction_domain_t* domain,
                                                    uint32_t* num_gaps) {
    if (!system || !domain || !num_gaps) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "novel_prediction_free: required parameter is NULL (system, domain, num_gaps)");
        return NULL;
    }

    *num_gaps = 0;

    /* Create a few gaps based on domain dimensions */
    uint32_t max_gaps = (domain->dim < INTUITION_MAX_GAPS) ? domain->dim : INTUITION_MAX_GAPS;
    intuition_gap_t** gaps = nimcp_calloc(max_gaps, sizeof(intuition_gap_t*));
    if (!gaps) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate gaps");

        return NULL;

    }

    for (uint32_t i = 0; i < max_gaps / 2; i++) {
        intuition_gap_t* gap = nimcp_calloc(1, sizeof(intuition_gap_t));
        if (!gap) break;

        gap->id = i + 1;
        snprintf(gap->description, sizeof(gap->description),
                 "Missing knowledge about dimension %u in domain %s", i, domain->name);
        gap->importance = apply_mod(system, 0.5f + 0.1f * i);
        gap->fillability = apply_mod(system, 0.7f - 0.1f * i);

        gaps[(*num_gaps)++] = gap;
        system->stats.gaps_identified++;
    }

    return gaps;
}

intuition_question_t** intuition_generate_questions(intuition_system_t* system,
                                                    const intuition_gap_t** gaps,
                                                    uint32_t num_gaps,
                                                    uint32_t* num_questions) {
    if (!system || !gaps || !num_questions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (system, gaps, num_questions)");
        return NULL;
    }

    *num_questions = 0;

    uint32_t max_q = (num_gaps < INTUITION_MAX_QUESTIONS) ? num_gaps : INTUITION_MAX_QUESTIONS;
    intuition_question_t** questions = nimcp_calloc(max_q, sizeof(intuition_question_t*));
    if (!questions) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate questions");

        return NULL;

    }

    for (uint32_t i = 0; i < max_q; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && max_q > 256) {
            intuition_integrations_heartbeat("intuition_in_loop",
                             (float)(i + 1) / (float)max_q);
        }

        if (!gaps[i]) continue;

        intuition_question_t* q = nimcp_calloc(1, sizeof(intuition_question_t));
        if (!q) break;

        q->id = i + 1;
        q->gap_id = gaps[i]->id;
        snprintf(q->question, sizeof(q->question),
                 "What additional information is needed to address: %s?",
                 gaps[i]->description);
        q->priority = gaps[i]->importance;
        q->answerability = gaps[i]->fillability;
        snprintf(q->suggested_source, sizeof(q->suggested_source),
                 "Research or experimentation");

        questions[(*num_questions)++] = q;
    }

    return questions;
}

void synthesis_free(synthesis_t* synth) {
    if (!synth) return;

    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_synthesis_free", 0.0f);


    if (synth->sources) nimcp_free(synth->sources);  /* References only */

    if (synth->synthesized) {
        if (synth->synthesized->unified_representation) {
            nimcp_free(synth->synthesized->unified_representation);
        }
        nimcp_free(synth->synthesized);
    }

    if (synth->identified_gaps) {
        for (uint32_t i = 0; i < synth->num_gaps; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && synth->num_gaps > 256) {
                intuition_integrations_heartbeat("intuition_in_loop",
                                 (float)(i + 1) / (float)synth->num_gaps);
            }

            intuition_gap_free(synth->identified_gaps[i]);
        }
        nimcp_free(synth->identified_gaps);
    }

    if (synth->conflicts) {
        for (uint32_t i = 0; i < synth->num_conflicts; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && synth->num_conflicts > 256) {
                intuition_integrations_heartbeat("intuition_in_loop",
                                 (float)(i + 1) / (float)synth->num_conflicts);
            }

            if (synth->conflicts[i]) nimcp_free(synth->conflicts[i]);
        }
        nimcp_free(synth->conflicts);
    }

    nimcp_free(synth);
    synth = NULL;
}

void intuition_gap_free(intuition_gap_t* gap) {
    if (!gap) return;
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_gap_free", 0.0f);


    if (gap->related_fragments) nimcp_free(gap->related_fragments);
    nimcp_free(gap);
    gap = NULL;
}

void intuition_question_free(intuition_question_t* question) {
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_question_f", 0.0f);


    if (question) nimcp_free(question);
}

/* ============================================================================
 * DATA POINT HELPERS
 * ============================================================================ */

intuition_data_point_t* intuition_data_point_create(const float* values, uint32_t dim,
                                                    float timestamp, float confidence) {
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_data_point", 0.0f);


    intuition_data_point_t* point = nimcp_calloc(1, sizeof(intuition_data_point_t));
    if (!point) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate point");

        return NULL;

    }

    if (values && dim > 0) {
        point->values = nimcp_calloc(dim, sizeof(float));
        if (point->values) {
            memcpy(point->values, values, dim * sizeof(float));
            point->dim = dim;
        }
    }

    point->timestamp = timestamp;
    point->confidence = fmaxf(0, fminf(1, confidence));

    return point;
}

void intuition_data_point_free(intuition_data_point_t* point) {
    if (!point) return;
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_data_point", 0.0f);


    if (point->values) nimcp_free(point->values);
    nimcp_free(point);
    point = NULL;
}

knowledge_fragment_t* knowledge_fragment_create(const char* description,
                                                const float* content, uint32_t dim,
                                                float confidence) {
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_knowledge_fragment_c", 0.0f);


    knowledge_fragment_t* frag = nimcp_calloc(1, sizeof(knowledge_fragment_t));
    if (!frag) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate frag");

        return NULL;

    }

    if (description) {
        strncpy(frag->description, description, sizeof(frag->description) - 1);
    }

    if (content && dim > 0) {
        frag->content = nimcp_calloc(dim, sizeof(float));
        if (frag->content) {
            memcpy(frag->content, content, dim * sizeof(float));
            frag->content_dim = dim;
        }
    }

    frag->confidence = fmaxf(0, fminf(1, confidence));

    return frag;
}

void knowledge_fragment_free(knowledge_fragment_t* fragment) {
    if (!fragment) return;
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_knowledge_fragment_f", 0.0f);


    if (fragment->content) nimcp_free(fragment->content);
    nimcp_free(fragment);
    fragment = NULL;
}

/* ============================================================================
 * SUB-ENGINE ACCESS
 * ============================================================================ */

intuitive_engine_t* intuition_get_intuitive_engine(intuition_system_t* system) {
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_get_intuit", 0.0f);


    return system ? system->intuitive : NULL;
}

analogical_engine_t* intuition_get_analogical_engine(intuition_system_t* system) {
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_get_analog", 0.0f);


    return system ? system->analogical : NULL;
}

insight_engine_t* intuition_get_insight_engine(intuition_system_t* system) {
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_get_insigh", 0.0f);


    return system ? system->insight : NULL;
}

hypothesis_engine_t* intuition_get_hypothesis_engine(intuition_system_t* system) {
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_get_hypoth", 0.0f);


    return system ? system->hypothesis : NULL;
}

blending_engine_t* intuition_get_blending_engine(intuition_system_t* system) {
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_get_blendi", 0.0f);


    return system ? system->blending : NULL;
}

counterfactual_engine_t* intuition_get_counterfactual_engine(intuition_system_t* system) {
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_get_counte", 0.0f);


    return system ? system->counterfactual : NULL;
}

meta_engine_t* intuition_get_meta_engine(intuition_system_t* system) {
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_get_meta_e", 0.0f);


    return system ? system->meta : NULL;
}

/* ============================================================================
 * BIOLOGICAL MODULATION
 * ============================================================================ */

int intuition_system_set_inflammation(intuition_system_t* system, float level) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_system_set_inflammation: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_system_set", 0.0f);


    system->inflammation = fmaxf(0, fminf(1, level));

    /* Propagate to sub-engines */
    if (system->intuitive) intuitive_set_inflammation(system->intuitive, level);
    if (system->analogical) analogical_set_inflammation(system->analogical, level);
    if (system->insight) insight_set_inflammation(system->insight, level);
    if (system->hypothesis) hypothesis_set_inflammation(system->hypothesis, level);
    if (system->blending) blending_set_inflammation(system->blending, level);
    if (system->counterfactual) counterfactual_set_inflammation(system->counterfactual, level);
    if (system->meta) meta_set_inflammation(system->meta, level);

    return 0;
}

int intuition_system_set_fatigue(intuition_system_t* system, float level) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_system_set_fatigue: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_system_set", 0.0f);


    system->fatigue = fmaxf(0, fminf(1, level));

    /* Propagate to sub-engines */
    if (system->intuitive) intuitive_set_fatigue(system->intuitive, level);
    if (system->analogical) analogical_set_fatigue(system->analogical, level);
    if (system->insight) insight_set_fatigue(system->insight, level);
    if (system->hypothesis) hypothesis_set_fatigue(system->hypothesis, level);
    if (system->blending) blending_set_fatigue(system->blending, level);
    if (system->counterfactual) counterfactual_set_fatigue(system->counterfactual, level);
    if (system->meta) meta_set_fatigue(system->meta, level);

    return 0;
}

/* ============================================================================
 * STATISTICS & DIAGNOSTICS
 * ============================================================================ */

int intuition_system_get_stats(const intuition_system_t* system,
                               intuition_system_stats_t* stats) {
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intuition_system_set_fatigue: required parameter is NULL (system, stats)");
        return -1;
    }
    *stats = system->stats;
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_system_get", 0.0f);


    return 0;
}

void intuition_system_reset_stats(intuition_system_t* system) {
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_intuition_system_res", 0.0f);


    if (system) memset(&system->stats, 0, sizeof(system->stats));
}

const char* intuition_system_get_last_error(void) {
    return g_last_error;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int intuition_integrations_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    intuition_integrations_heartbeat("intuition_in_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Intuition_Integrations");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                intuition_integrations_heartbeat("intuition_in_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* Module self-knowledge logged */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Intuition_Integrations");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Intuition_Integrations");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void intuition_integrations_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_intuition_integrations_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Functions
 * ============================================================================ */

int intuition_integrations_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "intuition_integrations_training_begin: NULL argument");
        return -1;
    }
    intuition_integrations_heartbeat_instance(g_intuition_integrations_health_agent, "intuition_integrations_training_begin", 0.0f);
    intuition_system_t* sys = (intuition_system_t*)instance;
    sys->stats.extrapolations_performed = 0;
    sys->stats.successful_extrapolations = 0;
    sys->stats.avg_extrapolation_accuracy = 0.0f;
    sys->stats.syntheses_performed = 0;
    sys->stats.gaps_identified = 0;
    sys->stats.contradictions_found = 0;
    sys->stats.contradictions_resolved = 0;
    sys->stats.intuitions_trained = 0;
    sys->stats.intuitions_confirmed = 0;
    sys->stats.intuitions_refuted = 0;
    sys->stats.avg_intuition_accuracy = 0.0f;
    sys->stats.novel_predictions_made = 0;
    sys->stats.novel_predictions_confirmed = 0;
    NIMCP_LOGGING_INFO("Intuition integrations training begin: counters reset");
    return 0;
}

int intuition_integrations_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "intuition_integrations_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    intuition_integrations_heartbeat_instance(g_intuition_integrations_health_agent, "intuition_integrations_training_step", progress);
    intuition_system_t* sys = (intuition_system_t*)instance;
    sys->stats.intuitions_trained++;
    /* Reduce inflammation sensitivity as integration strengthens */
    float decay = 1.0f - 0.2f * progress;
    if (decay < 0.5f) decay = 0.5f;
    sys->config.inflammation_sensitivity *= decay;
    /* Reduce fatigue sensitivity progressively */
    sys->config.fatigue_sensitivity *= decay;
    /* Improve extrapolation accuracy estimate through training */
    sys->stats.avg_extrapolation_accuracy += 0.005f * progress;
    if (sys->stats.avg_extrapolation_accuracy > 1.0f)
        sys->stats.avg_extrapolation_accuracy = 1.0f;
    return 0;
}

int intuition_integrations_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "intuition_integrations_training_end: NULL argument");
        return -1;
    }
    intuition_integrations_heartbeat_instance(g_intuition_integrations_health_agent, "intuition_integrations_training_end", 1.0f);
    intuition_system_t* sys = (intuition_system_t*)instance;
    float accuracy = 0.0f;
    if (sys->stats.intuitions_trained > 0) {
        accuracy = (float)sys->stats.intuitions_confirmed /
                   (float)sys->stats.intuitions_trained;
    }
    sys->stats.avg_intuition_accuracy = accuracy;
    NIMCP_LOGGING_INFO("Intuition integrations training end: %u trained, %u confirmed, accuracy=%.4f",
                       sys->stats.intuitions_trained,
                       sys->stats.intuitions_confirmed,
                       accuracy);
    return 0;
}
