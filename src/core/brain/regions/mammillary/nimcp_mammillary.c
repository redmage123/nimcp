/**
 * @file nimcp_mammillary.c
 * @brief Mammillary Bodies Implementation - Memory Consolidation Relay
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 */

#include "core/brain/regions/mammillary/nimcp_mammillary.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "constants/nimcp_frequency_constants.h"
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(mammillary, MESH_ADAPTER_CATEGORY_COGNITIVE)



/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

static uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static float normalize_angle(float angle) {
    while (angle < 0) angle += 2.0f * M_PI;
    while (angle >= 2.0f * M_PI) angle -= 2.0f * M_PI;
    return angle;
}

static float compute_hd_tuning(float preferred, float current, float width_deg) {
    float diff = fabsf(preferred - current);
    if (diff > M_PI) diff = 2.0f * M_PI - diff;
    float width_rad = width_deg * M_PI / 180.0f;
    if (fabsf(width_rad) < 1e-10f) return 0.0f;
    return expf(-(diff * diff) / (2.0f * width_rad * width_rad));
}

static float cosine_similarity(const float* a, const float* b, uint32_t dim) {
    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    if (norm_a < 1e-9f || norm_b < 1e-9f) return 0.0f;
    return dot / (sqrtf(norm_a) * sqrtf(norm_b));
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

mammillary_config_t mammillary_default_config(void) {
    mammillary_config_t config;
    memset(&config, 0, sizeof(config));

    config.num_hd_cells = MAMMILLARY_DEFAULT_HD_CELLS;
    config.num_relay_cells = MAMMILLARY_DEFAULT_RELAY_CELLS;
    config.num_spatial_cells = MAMMILLARY_DEFAULT_SPATIAL_CELLS;
    config.max_memory_traces = MAMMILLARY_MAX_MEMORY_TRACES;
    config.default_hd_tuning_width = 30.0f;
    config.consolidation_threshold = MAMMILLARY_CONSOLIDATION_THRESHOLD;
    config.relay_decay_rate = 0.01f;
    config.hd_drift_correction_rate = 0.1f;
    config.enable_papez_circuit = true;
    config.enable_spatial_processing = true;
    config.enable_head_direction = true;
    config.enable_background_consolidation = true;
    config.hippocampal_input_gain = 0.5f;  /* Start at moderate strength, not max */
    config.thalamic_output_gain = 1.0f;

    return config;
}

nimcp_mammillary_t* mammillary_create(const mammillary_config_t* config) {
    mammillary_config_t cfg = config ? *config : mammillary_default_config();

    nimcp_mammillary_t* mb = (nimcp_mammillary_t*)nimcp_calloc(1, sizeof(nimcp_mammillary_t));
    if (!mb) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mb is NULL");

        return NULL;

    }

    mb->config = cfg;
    mb->status = MAMMILLARY_STATUS_IDLE;
    mb->last_error = MAMMILLARY_ERROR_NONE;

    /* Allocate head direction cells */
    mb->num_hd_cells = cfg.num_hd_cells;
    mb->hd_cells = (nimcp_hd_cell_t*)nimcp_calloc(cfg.num_hd_cells, sizeof(nimcp_hd_cell_t));
    if (!mb->hd_cells) {
        nimcp_free(mb);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mammillary_create: mb->hd_cells is NULL");
        return NULL;
    }

    /* Initialize HD cells with evenly distributed preferred directions */
    for (uint32_t i = 0; i < cfg.num_hd_cells; i++) {
        mb->hd_cells[i].cell_id = i;
        mb->hd_cells[i].preferred_direction = (float)i * 2.0f * M_PI / cfg.num_hd_cells;
        mb->hd_cells[i].tuning_width = cfg.default_hd_tuning_width;
        mb->hd_cells[i].max_firing_rate = 100.0f;
        mb->hd_cells[i].baseline_rate = 1.0f;
        mb->hd_cells[i].state = HD_STATE_INACTIVE;
        mb->hd_cells[i].vestibular_weight = 0.6f;
        mb->hd_cells[i].visual_weight = 0.3f;
        mb->hd_cells[i].landmark_weight = 0.1f;
        mb->hd_cells[i].learning_rate = NIMCP_LEARNING_RATE_DEFAULT;
        mb->hd_cells[i].spike_threshold = 0.5f;
        mb->hd_cells[i].snn_neuron_id = i;
    }

    /* Allocate relay cells */
    mb->num_relay_cells = cfg.num_relay_cells;
    mb->relay_cells = (nimcp_relay_cell_t*)nimcp_calloc(cfg.num_relay_cells, sizeof(nimcp_relay_cell_t));
    if (!mb->relay_cells) {
        nimcp_free(mb->hd_cells);
        nimcp_free(mb);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mammillary_create: mb->relay_cells is NULL");
        return NULL;
    }

    /* Initialize relay cells */
    for (uint32_t i = 0; i < cfg.num_relay_cells; i++) {
        mb->relay_cells[i].cell_id = i;
        mb->relay_cells[i].persistence = 0.9f;
        mb->relay_cells[i].decay_rate = cfg.relay_decay_rate;
        mb->relay_cells[i].snn_neuron_id = cfg.num_hd_cells + i;
    }

    /* Allocate spatial cells */
    mb->num_spatial_cells = cfg.num_spatial_cells;
    mb->spatial_cells = (nimcp_spatial_cell_t*)nimcp_calloc(cfg.num_spatial_cells, sizeof(nimcp_spatial_cell_t));
    if (!mb->spatial_cells) {
        nimcp_free(mb->relay_cells);
        nimcp_free(mb->hd_cells);
        nimcp_free(mb);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mammillary_create: mb->spatial_cells is NULL");
        return NULL;
    }

    /* Initialize spatial cells */
    for (uint32_t i = 0; i < cfg.num_spatial_cells; i++) {
        mb->spatial_cells[i].cell_id = i;
    }

    /* Allocate memory traces */
    mb->max_memory_traces = cfg.max_memory_traces;
    mb->memory_traces = (nimcp_memory_trace_t*)nimcp_calloc(cfg.max_memory_traces, sizeof(nimcp_memory_trace_t));
    if (!mb->memory_traces) {
        nimcp_free(mb->spatial_cells);
        nimcp_free(mb->relay_cells);
        nimcp_free(mb->hd_cells);
        nimcp_free(mb);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mammillary_create: mb->memory_traces is NULL");
        return NULL;
    }

    /* Initialize state */
    mb->current_heading = 0.0f;
    mb->heading_confidence = 0.5f;
    mb->papez_phase = PAPEZ_PHASE_IDLE;
    mb->circuit_integrity = 1.0f;
    mb->consolidation_state = CONSOLIDATION_IDLE;
    mb->consolidation_rate = 0.1f;

    /* Initialize bridge defaults */
    mb->hippocampus_bridge.fornix_strength = 0.8f;
    mb->hippocampus_bridge.ca3_input_weight = 0.4f;
    mb->hippocampus_bridge.ca1_input_weight = 0.4f;
    mb->hippocampus_bridge.subiculum_input_weight = 0.2f;

    mb->thalamus_bridge.tract_strength = 0.8f;
    mb->thalamus_bridge.relay_efficiency = 0.9f;

    mb->vestibular_bridge.update_rate = NIMCP_HIGH_UPDATE_RATE_HZ;

    mb->creation_time = get_timestamp_ms();
    mb->status = MAMMILLARY_STATUS_READY;

    return mb;
}

void mammillary_destroy(nimcp_mammillary_t* mb) {
    if (!mb) return;

    /* Free memory trace contents */
    for (uint32_t i = 0; i < mb->num_memory_traces; i++) {
        if (mb->memory_traces[i].content) {
            nimcp_free(mb->memory_traces[i].content);
        }
    }

    /* Free relay cell weights */
    for (uint32_t i = 0; i < mb->num_relay_cells; i++) {
        if (mb->relay_cells[i].input_weights) nimcp_free(mb->relay_cells[i].input_weights);
        if (mb->relay_cells[i].output_weights) nimcp_free(mb->relay_cells[i].output_weights);
        if (mb->relay_cells[i].memory_tuning) nimcp_free(mb->relay_cells[i].memory_tuning);
    }

    /* Free spatial cell tuning */
    for (uint32_t i = 0; i < mb->num_spatial_cells; i++) {
        if (mb->spatial_cells[i].spatial_tuning) nimcp_free(mb->spatial_cells[i].spatial_tuning);
    }

    if (mb->memory_traces) nimcp_free(mb->memory_traces);
    if (mb->spatial_cells) nimcp_free(mb->spatial_cells);
    if (mb->relay_cells) nimcp_free(mb->relay_cells);
    if (mb->hd_cells) nimcp_free(mb->hd_cells);

    /* Free entorhinal bridge grid phase */
    if (mb->entorhinal_bridge.grid_phase) nimcp_free(mb->entorhinal_bridge.grid_phase);

    nimcp_free(mb);
}

int mammillary_reset(nimcp_mammillary_t* mb) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_reset: mb is NULL");
        return -1;
    }

    /* Reset HD cells */
    for (uint32_t i = 0; i < mb->num_hd_cells; i++) {
        mb->hd_cells[i].current_firing_rate = 0.0f;
        mb->hd_cells[i].state = HD_STATE_INACTIVE;
        mb->hd_cells[i].membrane_potential = 0.0f;
        mb->hd_cells[i].eligibility_trace = 0.0f;
    }

    /* Reset relay cells */
    for (uint32_t i = 0; i < mb->num_relay_cells; i++) {
        mb->relay_cells[i].activation = 0.0f;
        mb->relay_cells[i].membrane_potential = 0.0f;
        mb->relay_cells[i].refractory = false;
    }

    /* Reset spatial cells */
    for (uint32_t i = 0; i < mb->num_spatial_cells; i++) {
        mb->spatial_cells[i].activation = 0.0f;
        mb->spatial_cells[i].grid_cell_input = 0.0f;
        mb->spatial_cells[i].place_cell_input = 0.0f;
        mb->spatial_cells[i].hd_cell_input = 0.0f;
    }

    /* Clear memory traces */
    for (uint32_t i = 0; i < mb->num_memory_traces; i++) {
        if (mb->memory_traces[i].content) {
            nimcp_free(mb->memory_traces[i].content);
            mb->memory_traces[i].content = NULL;
        }
        memset(&mb->memory_traces[i], 0, sizeof(nimcp_memory_trace_t));
    }
    mb->num_memory_traces = 0;

    /* Reset state */
    mb->current_heading = 0.0f;
    mb->heading_confidence = 0.5f;
    mb->angular_velocity = 0.0f;
    mb->hd_population_vector[0] = 1.0f;
    mb->hd_population_vector[1] = 0.0f;
    mb->papez_phase = PAPEZ_PHASE_IDLE;
    mb->papez_activity = 0.0f;
    mb->consolidation_state = CONSOLIDATION_IDLE;
    mb->traces_pending_consolidation = 0;

    /* Reset statistics */
    mb->updates_processed = 0;
    mb->relay_operations = 0;
    mb->consolidations_completed = 0;
    mb->hd_corrections = 0;

    mb->status = MAMMILLARY_STATUS_READY;
    mb->last_error = MAMMILLARY_ERROR_NONE;

    return 0;
}

int mammillary_update(nimcp_mammillary_t* mb, float dt) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_update: mb is NULL");
        return -1;
    }

    mb->last_update_time = get_timestamp_ms();

    /* Update head direction cells */
    if (mb->config.enable_head_direction) {
        float cos_sum = 0.0f, sin_sum = 0.0f;
        for (uint32_t i = 0; i < mb->num_hd_cells; i++) {
            float tuning = compute_hd_tuning(
                mb->hd_cells[i].preferred_direction,
                mb->current_heading,
                mb->hd_cells[i].tuning_width
            );
            mb->hd_cells[i].current_firing_rate =
                mb->hd_cells[i].baseline_rate +
                (mb->hd_cells[i].max_firing_rate - mb->hd_cells[i].baseline_rate) * tuning;

            mb->hd_cells[i].state = (tuning > 0.5f) ? HD_STATE_ACTIVE : HD_STATE_INACTIVE;

            /* Update population vector */
            float rate = mb->hd_cells[i].current_firing_rate;
            cos_sum += rate * cosf(mb->hd_cells[i].preferred_direction);
            sin_sum += rate * sinf(mb->hd_cells[i].preferred_direction);
        }

        float norm = sqrtf(cos_sum * cos_sum + sin_sum * sin_sum);
        if (norm > 0.001f) {
            mb->hd_population_vector[0] = cos_sum / norm;
            mb->hd_population_vector[1] = sin_sum / norm;
            mb->heading_confidence = fminf(1.0f, norm / (mb->num_hd_cells * 50.0f));
        }
    }

    /* Update relay cells */
    for (uint32_t i = 0; i < mb->num_relay_cells; i++) {
        mb->relay_cells[i].activation *= (1.0f - mb->relay_cells[i].decay_rate * dt);
        if (mb->relay_cells[i].activation < 0.01f) {
            mb->relay_cells[i].activation = 0.0f;
        }
    }

    /* Update spatial cells */
    if (mb->config.enable_spatial_processing) {
        for (uint32_t i = 0; i < mb->num_spatial_cells; i++) {
            float input = mb->spatial_cells[i].grid_cell_input * 0.4f +
                         mb->spatial_cells[i].place_cell_input * 0.4f +
                         mb->spatial_cells[i].hd_cell_input * 0.2f;
            mb->spatial_cells[i].activation =
                mb->spatial_cells[i].activation * 0.9f + input * 0.1f;
        }
    }

    /* Update consolidation */
    if (mb->config.enable_background_consolidation && mb->traces_pending_consolidation > 0) {
        mammillary_update_consolidation(mb, dt);
    }

    /* Update Papez circuit if active */
    if (mb->config.enable_papez_circuit && mb->papez_phase != PAPEZ_PHASE_IDLE) {
        mammillary_process_papez_cycle(mb);
    }

    /* Decay memory traces */
    for (uint32_t i = 0; i < mb->num_memory_traces; i++) {
        if (mb->memory_traces[i].content) {
            mb->memory_traces[i].age += dt;
            /* Very slow decay for consolidated memories */
            if (mb->memory_traces[i].state == CONSOLIDATION_COMPLETE) {
                mb->memory_traces[i].strength *= (1.0f - 0.0001f * dt);
            }
        }
    }

    mb->updates_processed++;
    return 0;
}

/*=============================================================================
 * HEAD DIRECTION FUNCTIONS
 *===========================================================================*/

int mammillary_update_head_direction(nimcp_mammillary_t* mb,
                                      float angular_velocity,
                                      float dt) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_update: mb is NULL");
        return -1;
    }

    mb->angular_velocity = angular_velocity;
    mb->current_heading = normalize_angle(mb->current_heading + angular_velocity * dt);
    mb->status = MAMMILLARY_STATUS_HD_PROCESSING;

    /* Apply vestibular integration */
    if (mb->vestibular_bridge.initialized) {
        float vest_contribution = mb->vestibular_bridge.angular_velocity[2] *
                                  mb->hd_cells[0].vestibular_weight;
        mb->current_heading = normalize_angle(mb->current_heading + vest_contribution * dt);
    }

    /* Update head direction cells */
    for (uint32_t i = 0; i < mb->num_hd_cells; i++) {
        float tuning = compute_hd_tuning(
            mb->hd_cells[i].preferred_direction,
            mb->current_heading,
            mb->hd_cells[i].tuning_width
        );
        mb->hd_cells[i].current_firing_rate =
            mb->hd_cells[i].baseline_rate +
            (mb->hd_cells[i].max_firing_rate - mb->hd_cells[i].baseline_rate) * tuning;

        if (tuning > 0.5f) {
            mb->hd_cells[i].state = HD_STATE_ACTIVE;
        } else if (mb->hd_cells[i].state == HD_STATE_ACTIVE) {
            mb->hd_cells[i].state = HD_STATE_INACTIVE;
        }
    }

    mb->status = MAMMILLARY_STATUS_READY;
    return 0;
}

int mammillary_set_hd_from_landmark(nimcp_mammillary_t* mb,
                                     float landmark_bearing,
                                     float confidence) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_update: mb is NULL");
        return -1;
    }

    /* Blend landmark bearing with current estimate based on confidence */
    float blend_weight = confidence * mb->hd_cells[0].landmark_weight;
    float diff = landmark_bearing - mb->current_heading;

    /* Normalize difference to [-pi, pi] */
    while (diff > M_PI) diff -= 2.0f * M_PI;
    while (diff < -M_PI) diff += 2.0f * M_PI;

    mb->current_heading = normalize_angle(mb->current_heading + diff * blend_weight);
    mb->heading_confidence = fminf(1.0f, mb->heading_confidence + confidence * 0.1f);
    mb->hd_corrections++;

    return 0;
}

float mammillary_get_head_direction(nimcp_mammillary_t* mb) {
    if (!mb) return 0.0f;
    return mb->current_heading;
}

float mammillary_get_hd_confidence(nimcp_mammillary_t* mb) {
    if (!mb) return 0.0f;
    return mb->heading_confidence;
}

int mammillary_get_hd_population_vector(nimcp_mammillary_t* mb,
                                         float* vector,
                                         uint32_t* dim) {
    if (!mb || !vector || !dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_get_hd_confidence: required parameter is NULL (mb, vector, dim)");
        return -1;
    }

    vector[0] = mb->hd_population_vector[0];
    vector[1] = mb->hd_population_vector[1];
    *dim = 2;

    return 0;
}

int mammillary_correct_hd_drift(nimcp_mammillary_t* mb, float true_heading) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_correct_hd_drift: mb is NULL");
        return -1;
    }

    float error = true_heading - mb->current_heading;
    while (error > M_PI) error -= 2.0f * M_PI;
    while (error < -M_PI) error += 2.0f * M_PI;

    mb->current_heading = normalize_angle(
        mb->current_heading + error * mb->config.hd_drift_correction_rate
    );

    /* Update all HD cells */
    for (uint32_t i = 0; i < mb->num_hd_cells; i++) {
        mb->hd_cells[i].drift_rate = error;
        if (fabsf(error) > 0.1f) {
            mb->hd_cells[i].state = HD_STATE_CORRECTING;
        }
    }

    mb->hd_corrections++;
    mb->heading_confidence = fminf(1.0f, mb->heading_confidence + 0.1f);

    return 0;
}

int mammillary_get_active_hd_cells(nimcp_mammillary_t* mb,
                                    uint32_t* cell_ids,
                                    float* firing_rates,
                                    uint32_t max_cells,
                                    uint32_t* num_active) {
    if (!mb || !cell_ids || !firing_rates || !num_active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_correct_hd_drift: required parameter is NULL (mb, cell_ids, firing_rates, num_active)");
        return -1;
    }

    uint32_t count = 0;
    for (uint32_t i = 0; i < mb->num_hd_cells && count < max_cells; i++) {
        if (mb->hd_cells[i].state == HD_STATE_ACTIVE ||
            mb->hd_cells[i].current_firing_rate > mb->hd_cells[i].baseline_rate * 2.0f) {
            cell_ids[count] = mb->hd_cells[i].cell_id;
            firing_rates[count] = mb->hd_cells[i].current_firing_rate;
            count++;
        }
    }

    *num_active = count;
    return 0;
}

/*=============================================================================
 * MEMORY RELAY FUNCTIONS
 *===========================================================================*/

int mammillary_receive_hippocampal_input(nimcp_mammillary_t* mb,
                                          const float* trace,
                                          uint32_t trace_dim,
                                          memory_trace_type_t type,
                                          float emotional_valence,
                                          uint32_t* trace_id_out) {
    if (!mb || !trace || trace_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_correct_hd_drift: required parameter is NULL (mb, trace)");
        return -1;
    }

    if (mb->num_memory_traces >= mb->max_memory_traces) {
        mb->last_error = MAMMILLARY_ERROR_MEMORY_FULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mammillary_correct_hd_drift: capacity exceeded");
        return -1;
    }

    /* Find empty slot */
    uint32_t slot = mb->num_memory_traces;
    for (uint32_t i = 0; i < mb->max_memory_traces; i++) {
        if (!mb->memory_traces[i].content) {
            slot = i;
            break;
        }
    }

    nimcp_memory_trace_t* mt = &mb->memory_traces[slot];

    /* Allocate and copy content */
    mt->content = (float*)nimcp_malloc(trace_dim * sizeof(float));
    if (!mt->content) {
        mb->last_error = MAMMILLARY_ERROR_INTERNAL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mammillary_correct_hd_drift: mt->content is NULL");
        return -1;
    }
    memcpy(mt->content, trace, trace_dim * sizeof(float));

    mt->trace_id = slot;
    mt->type = type;
    mt->content_dim = trace_dim;
    mt->strength = mb->config.hippocampal_input_gain;
    mt->age = 0.0f;
    mt->retrieval_count = 0;
    mt->state = CONSOLIDATION_ENCODING;
    mt->emotional_intensity = fabsf(emotional_valence);
    mt->valence = emotional_valence;
    mt->encoding_timestamp = get_timestamp_ms();
    mt->heading = mb->current_heading;
    mt->has_spatial_context = false;

    if (slot >= mb->num_memory_traces) {
        mb->num_memory_traces = slot + 1;
    }

    mb->traces_pending_consolidation++;
    mb->status = MAMMILLARY_STATUS_RELAYING;

    /* Activate relevant relay cells */
    for (uint32_t i = 0; i < mb->num_relay_cells; i++) {
        float match = (float)(i % 6 == (int)type) * 0.5f + 0.5f;
        mb->relay_cells[i].activation = fmaxf(mb->relay_cells[i].activation, match * 0.8f);
        mb->relay_cells[i].last_activation_time = (float)get_timestamp_ms() / 1000.0f;
    }

    if (trace_id_out) *trace_id_out = slot;
    return 0;
}

int mammillary_relay_to_thalamus(nimcp_mammillary_t* mb, uint32_t trace_id) {
    if (!mb || trace_id >= mb->max_memory_traces) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mammillary_relay_to_thalamus: mb is NULL");
        return -1;
    }

    nimcp_memory_trace_t* mt = &mb->memory_traces[trace_id];
    if (!mt->content) {
        mb->last_error = MAMMILLARY_ERROR_RELAY_FAILED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_relay_to_thalamus: mt->content is NULL");
        return -1;
    }

    /* Copy to thalamus output buffer */
    uint32_t copy_size = (mt->content_dim < 256) ? mt->content_dim : 256;
    for (uint32_t i = 0; i < copy_size; i++) {
        mb->thalamus_bridge.output_buffer[i] =
            mt->content[i] * mb->thalamus_bridge.tract_strength;
    }
    mb->thalamus_bridge.output_buffer_size = copy_size;
    mb->thalamus_bridge.last_output_time = (float)get_timestamp_ms() / 1000.0f;

    /* Update trace state */
    mt->state = CONSOLIDATION_RELAYING;
    mt->thalamic_projection_strength = mb->thalamus_bridge.relay_efficiency;

    mb->relay_operations++;
    return 0;
}

int mammillary_process_papez_cycle(nimcp_mammillary_t* mb) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_process_papez_cycle: mb is NULL");
        return -1;
    }

    switch (mb->papez_phase) {
        case PAPEZ_PHASE_IDLE:
            /* No active cycle */
            break;

        case PAPEZ_PHASE_HIPPOCAMPAL_INPUT:
            /* Process incoming from hippocampus via fornix */
            mb->papez_activity = mb->hippocampus_bridge.fornix_strength;
            mb->papez_phase = PAPEZ_PHASE_MAMMILLARY_RELAY;
            break;

        case PAPEZ_PHASE_MAMMILLARY_RELAY:
            /* Internal processing in mammillary bodies */
            for (uint32_t i = 0; i < mb->num_relay_cells; i++) {
                if (mb->relay_cells[i].activation > 0.1f) {
                    mb->relay_cells[i].activation *= mb->relay_cells[i].persistence;
                }
            }
            mb->papez_phase = PAPEZ_PHASE_THALAMIC_OUTPUT;
            break;

        case PAPEZ_PHASE_THALAMIC_OUTPUT:
            /* Relay to anterior thalamus via mammillothalamic tract */
            mb->papez_activity *= mb->thalamus_bridge.relay_efficiency;
            mb->papez_phase = PAPEZ_PHASE_CINGULATE_FEEDBACK;
            break;

        case PAPEZ_PHASE_CINGULATE_FEEDBACK:
            /* Receive feedback from cingulate cortex */
            if (mb->cingulate_bridge.initialized) {
                mb->papez_activity += mb->cingulate_bridge.feedback_strength * 0.2f;
            }
            mb->papez_phase = PAPEZ_PHASE_COMPLETE;
            break;

        case PAPEZ_PHASE_COMPLETE:
            /* Cycle complete, return to idle or restart */
            mb->papez_phase = PAPEZ_PHASE_IDLE;
            mb->papez_activity *= 0.5f;
            break;
    }

    return 0;
}

papez_phase_t mammillary_get_papez_phase(nimcp_mammillary_t* mb) {
    if (!mb) return PAPEZ_PHASE_IDLE;
    return mb->papez_phase;
}

float mammillary_get_papez_activity(nimcp_mammillary_t* mb) {
    if (!mb) return 0.0f;
    return mb->papez_activity;
}

int mammillary_advance_papez_phase(nimcp_mammillary_t* mb) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_advance_papez_phase: mb is NULL");
        return -1;
    }

    if (mb->papez_phase == PAPEZ_PHASE_IDLE) {
        mb->papez_phase = PAPEZ_PHASE_HIPPOCAMPAL_INPUT;
        mb->papez_activity = 1.0f;
    } else {
        mammillary_process_papez_cycle(mb);
    }

    return 0;
}

/*=============================================================================
 * MEMORY CONSOLIDATION FUNCTIONS
 *===========================================================================*/

int mammillary_start_consolidation(nimcp_mammillary_t* mb, uint32_t trace_id) {
    if (!mb || trace_id >= mb->max_memory_traces) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mammillary_start_consolidation: mb is NULL");
        return -1;
    }

    nimcp_memory_trace_t* mt = &mb->memory_traces[trace_id];
    if (!mt->content) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_start_consolidation: mt->content is NULL");
        return -1;
    }

    mt->state = CONSOLIDATION_STRENGTHENING;
    mb->consolidation_state = CONSOLIDATION_STRENGTHENING;
    mb->status = MAMMILLARY_STATUS_CONSOLIDATING;

    return 0;
}

int mammillary_update_consolidation(nimcp_mammillary_t* mb, float dt) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_update_consolidation: mb is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < mb->num_memory_traces; i++) {
        nimcp_memory_trace_t* mt = &mb->memory_traces[i];
        if (!mt->content) continue;

        switch (mt->state) {
            case CONSOLIDATION_ENCODING:
                mt->state = CONSOLIDATION_RELAYING;
                break;

            case CONSOLIDATION_RELAYING:
                /* Auto-relay after encoding */
                mammillary_relay_to_thalamus(mb, i);
                mt->state = CONSOLIDATION_STRENGTHENING;
                break;

            case CONSOLIDATION_STRENGTHENING:
                /* Strengthen based on emotional intensity and retrieval */
                mt->strength += mb->consolidation_rate * dt *
                               (1.0f + mt->emotional_intensity * 0.5f);

                if (mt->strength >= mb->config.consolidation_threshold) {
                    mt->state = CONSOLIDATION_COMPLETE;
                    mb->consolidations_completed++;
                    mb->traces_pending_consolidation--;
                }
                break;

            case CONSOLIDATION_COMPLETE:
            case CONSOLIDATION_IDLE:
                /* Already consolidated or idle */
                break;
        }
    }

    if (mb->traces_pending_consolidation == 0) {
        mb->consolidation_state = CONSOLIDATION_IDLE;
        mb->status = MAMMILLARY_STATUS_READY;
    }

    return 0;
}

consolidation_state_t mammillary_get_consolidation_state(nimcp_mammillary_t* mb) {
    if (!mb) return CONSOLIDATION_IDLE;
    return mb->consolidation_state;
}

const nimcp_memory_trace_t* mammillary_get_trace(nimcp_mammillary_t* mb,
                                                  uint32_t trace_id) {
    if (!mb || trace_id >= mb->max_memory_traces) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mammillary_get_consolidation_state: mb is NULL");
        return NULL;
    }
    if (!mb->memory_traces[trace_id].content) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mammillary_get_consolidation_state: mb->memory_traces is NULL");
        return NULL;
    }
    return &mb->memory_traces[trace_id];
}

int mammillary_strengthen_trace(nimcp_mammillary_t* mb,
                                 uint32_t trace_id,
                                 float amount) {
    if (!mb || trace_id >= mb->max_memory_traces) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mammillary_get_consolidation_state: mb is NULL");
        return -1;
    }

    nimcp_memory_trace_t* mt = &mb->memory_traces[trace_id];
    if (!mt->content) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_get_consolidation_state: mt->content is NULL");
        return -1;
    }

    mt->strength = fminf(1.0f, mt->strength + amount);
    mt->retrieval_count++;
    mt->last_retrieval = (float)get_timestamp_ms() / 1000.0f;

    return 0;
}

int mammillary_get_traces_by_type(nimcp_mammillary_t* mb,
                                   memory_trace_type_t type,
                                   uint32_t* trace_ids,
                                   uint32_t max_traces,
                                   uint32_t* num_found) {
    if (!mb || !trace_ids || !num_found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_get_consolidation_state: required parameter is NULL (mb, trace_ids, num_found)");
        return -1;
    }

    uint32_t count = 0;
    for (uint32_t i = 0; i < mb->max_memory_traces && count < max_traces; i++) {
        if (mb->memory_traces[i].content && mb->memory_traces[i].type == type) {
            trace_ids[count++] = i;
        }
    }

    *num_found = count;
    return 0;
}

int mammillary_get_strongest_traces(nimcp_mammillary_t* mb,
                                     uint32_t* trace_ids,
                                     float* strengths,
                                     uint32_t max_traces,
                                     uint32_t* num_returned) {
    if (!mb || !trace_ids || !strengths || !num_returned) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_get_consolidation_state: required parameter is NULL (mb, trace_ids, strengths, num_returned)");
        return -1;
    }

    /* Simple insertion sort to get top traces */
    uint32_t count = 0;
    for (uint32_t i = 0; i < mb->max_memory_traces; i++) {
        if (!mb->memory_traces[i].content) continue;

        float str = mb->memory_traces[i].strength;

        /* Find insertion position */
        uint32_t pos = count;
        for (uint32_t j = 0; j < count; j++) {
            if (str > strengths[j]) {
                pos = j;
                break;
            }
        }

        if (pos < max_traces) {
            /* Shift elements */
            for (uint32_t j = (count < max_traces ? count : max_traces - 1); j > pos; j--) {
                trace_ids[j] = trace_ids[j - 1];
                strengths[j] = strengths[j - 1];
            }
            trace_ids[pos] = i;
            strengths[pos] = str;
            if (count < max_traces) count++;
        }
    }

    *num_returned = count;
    return 0;
}

int mammillary_decay_traces(nimcp_mammillary_t* mb, float decay_factor) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_decay_traces: mb is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < mb->max_memory_traces; i++) {
        if (mb->memory_traces[i].content) {
            mb->memory_traces[i].strength *= (1.0f - decay_factor);
        }
    }

    return 0;
}

int mammillary_remove_trace(nimcp_mammillary_t* mb, uint32_t trace_id) {
    if (!mb || trace_id >= mb->max_memory_traces) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mammillary_remove_trace: mb is NULL");
        return -1;
    }

    nimcp_memory_trace_t* mt = &mb->memory_traces[trace_id];
    if (!mt->content) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_remove_trace: mt->content is NULL");
        return -1;
    }

    nimcp_free(mt->content);
    memset(mt, 0, sizeof(nimcp_memory_trace_t));

    /* Update count */
    uint32_t active_count = 0;
    for (uint32_t i = 0; i < mb->max_memory_traces; i++) {
        if (mb->memory_traces[i].content) active_count++;
    }
    mb->num_memory_traces = active_count;

    return 0;
}

/*=============================================================================
 * SPATIAL PROCESSING FUNCTIONS
 *===========================================================================*/

int mammillary_update_spatial_cells(nimcp_mammillary_t* mb,
                                     const float* position,
                                     uint32_t dim) {
    if (!mb || !position || dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_remove_trace: required parameter is NULL (mb, position)");
        return -1;
    }

    mb->status = MAMMILLARY_STATUS_SPATIAL_ENCODING;

    for (uint32_t i = 0; i < mb->num_spatial_cells; i++) {
        /* Compute distance from cell's preferred position */
        float dist_sq = 0.0f;
        for (uint32_t d = 0; d < dim && d < 3; d++) {
            float diff = position[d] - mb->spatial_cells[i].position_encoding[d];
            dist_sq += diff * diff;
        }

        /* Gaussian activation based on distance */
        float sigma = 10.0f;  /* Spatial tuning width */
        mb->spatial_cells[i].activation = expf(-dist_sq / (2.0f * sigma * sigma));
    }

    mb->status = MAMMILLARY_STATUS_READY;
    return 0;
}

int mammillary_encode_spatial_memory(nimcp_mammillary_t* mb,
                                      const float* position,
                                      float heading,
                                      const float* context,
                                      uint32_t context_dim,
                                      uint32_t* trace_id_out) {
    if (!mb || !position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_remove_trace: required parameter is NULL (mb, position)");
        return -1;
    }

    /* Create memory trace with spatial context */
    uint32_t trace_id;
    int result = mammillary_receive_hippocampal_input(
        mb, context ? context : position,
        context ? context_dim : 3,
        MEMORY_TRACE_SPATIAL,
        0.0f,
        &trace_id
    );

    if (result != 0) return result;

    /* Add spatial context */
    nimcp_memory_trace_t* mt = &mb->memory_traces[trace_id];
    mt->position[0] = position[0];
    mt->position[1] = position[1];
    mt->position[2] = position[2];
    mt->heading = heading;
    mt->has_spatial_context = true;

    if (trace_id_out) *trace_id_out = trace_id;
    return 0;
}

int mammillary_retrieve_spatial_context(nimcp_mammillary_t* mb,
                                         const float* position,
                                         uint32_t dim,
                                         float* context,
                                         uint32_t* context_dim) {
    if (!mb || !position || !context || !context_dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_remove_trace: required parameter is NULL (mb, position, context, context_dim)");
        return -1;
    }

    /* Find closest spatial memory */
    float best_match = 0.0f;
    uint32_t best_trace = UINT32_MAX;

    for (uint32_t i = 0; i < mb->max_memory_traces; i++) {
        nimcp_memory_trace_t* mt = &mb->memory_traces[i];
        if (!mt->content || !mt->has_spatial_context) continue;

        float dist_sq = 0.0f;
        for (uint32_t d = 0; d < dim && d < 3; d++) {
            float diff = position[d] - mt->position[d];
            dist_sq += diff * diff;
        }

        float match = expf(-dist_sq / 100.0f);  /* Spatial matching kernel */
        if (match > best_match) {
            best_match = match;
            best_trace = i;
        }
    }

    if (best_trace == UINT32_MAX) {
        *context_dim = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mammillary_remove_trace: validation failed");
        return -1;
    }

    /* Return the matched memory's content as context */
    nimcp_memory_trace_t* mt = &mb->memory_traces[best_trace];
    uint32_t copy_dim = mt->content_dim;
    memcpy(context, mt->content, copy_dim * sizeof(float));
    *context_dim = copy_dim;

    return 0;
}

int mammillary_get_spatial_activity(nimcp_mammillary_t* mb,
                                     float* activity,
                                     uint32_t max_cells,
                                     uint32_t* num_cells) {
    if (!mb || !activity || !num_cells) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_remove_trace: required parameter is NULL (mb, activity, num_cells)");
        return -1;
    }

    uint32_t count = (mb->num_spatial_cells < max_cells) ?
                     mb->num_spatial_cells : max_cells;

    for (uint32_t i = 0; i < count; i++) {
        activity[i] = mb->spatial_cells[i].activation;
    }

    *num_cells = count;
    return 0;
}

/*=============================================================================
 * BIDIRECTIONAL DATA FLOW
 *===========================================================================*/

int mammillary_process_incoming(nimcp_mammillary_t* mb) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_process_incoming: mb is NULL");
        return -1;
    }

    /* Process hippocampus input */
    if (mb->hippocampus_bridge.initialized && mb->hippocampus_bridge.input_buffer_size > 0) {
        mammillary_receive_hippocampal_input(
            mb,
            mb->hippocampus_bridge.input_buffer,
            mb->hippocampus_bridge.input_buffer_size,
            MEMORY_TRACE_EPISODIC,
            0.0f,
            NULL
        );
        mb->hippocampus_bridge.input_buffer_size = 0;
    }

    /* Process vestibular input for head direction */
    if (mb->vestibular_bridge.initialized) {
        mammillary_update_head_direction(
            mb,
            mb->vestibular_bridge.angular_velocity[2],
            0.01f
        );
    }

    /* Process entorhinal grid cell input */
    if (mb->entorhinal_bridge.initialized && mb->entorhinal_bridge.grid_phase) {
        for (uint32_t i = 0; i < mb->num_spatial_cells; i++) {
            uint32_t grid_idx = i % mb->entorhinal_bridge.grid_dim;
            mb->spatial_cells[i].grid_cell_input =
                mb->entorhinal_bridge.grid_phase[grid_idx];
        }
    }

    /* Process hypothalamus modulation */
    if (mb->hypothalamus_bridge.initialized) {
        /* Arousal affects consolidation rate */
        mb->consolidation_rate = 0.1f *
            (1.0f + mb->hypothalamus_bridge.arousal_level * 0.5f);

        /* Stress can impair relay */
        mb->thalamus_bridge.relay_efficiency =
            0.9f * (1.0f - mb->hypothalamus_bridge.stress_level * 0.3f);
    }

    /* Process cingulate feedback */
    if (mb->cingulate_bridge.initialized) {
        mb->cingulate_bridge.error_signal *= 0.9f;  /* Decay error signal */
    }

    return 0;
}

int mammillary_send_outgoing(nimcp_mammillary_t* mb) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_send_outgoing: mb is NULL");
        return -1;
    }

    /* Send to thalamus */
    if (mb->thalamus_bridge.initialized && mb->thalamus_bridge.output_buffer_size > 0) {
        /* Output buffer already prepared by relay operations */
        mb->thalamus_bridge.active_channels = mb->thalamus_bridge.output_buffer_size;
    }

    /* Send head direction to relevant systems */
    if (mb->entorhinal_bridge.initialized) {
        mb->entorhinal_bridge.path_integration_gain =
            mb->heading_confidence * 0.8f + 0.2f;
    }

    /* Update cerebellum with timing info */
    if (mb->cerebellum_bridge.initialized) {
        mb->cerebellum_bridge.timing_precision = mb->heading_confidence;
    }

    return 0;
}

int mammillary_bidirectional_update(nimcp_mammillary_t* mb, float dt) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_bidirectional_update: mb is NULL");
        return -1;
    }

    mammillary_process_incoming(mb);
    mammillary_update(mb, dt);
    mammillary_send_outgoing(mb);

    return 0;
}

int mammillary_sync_hippocampus(nimcp_mammillary_t* mb) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_sync_hippocampus: mb is NULL");
        return -1;
    }

    mb->hippocampus_bridge.last_sync_time = (float)get_timestamp_ms() / 1000.0f;
    return 0;
}

int mammillary_sync_thalamus(nimcp_mammillary_t* mb) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_sync_thalamus: mb is NULL");
        return -1;
    }

    /* Ensure relay cells are synchronized with thalamic projections */
    for (uint32_t i = 0; i < mb->num_relay_cells; i++) {
        if (mb->relay_cells[i].activation > 0.1f) {
            mammillary_relay_to_thalamus(mb, i);
        }
    }

    return 0;
}

int mammillary_send_to_thalamus(nimcp_mammillary_t* mb) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_send_to_thalamus: mb is NULL");
        return -1;
    }
    return mammillary_sync_thalamus(mb);
}

int mammillary_receive_cingulate_feedback(nimcp_mammillary_t* mb) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_receive_cingulate_feedback: mb is NULL");
        return -1;
    }

    if (mb->cingulate_bridge.initialized) {
        /* Modulate consolidation based on attention */
        mb->consolidation_rate *= (1.0f + mb->cingulate_bridge.attention_modulation * 0.5f);
        mb->cingulate_bridge.last_feedback_time = (float)get_timestamp_ms() / 1000.0f;
    }

    return 0;
}

/*=============================================================================
 * BRIDGE INITIALIZATION FUNCTIONS
 *===========================================================================*/

int mammillary_init_hippocampus_bridge(nimcp_mammillary_t* mb, void* hippocampus) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_init_hippocampus_bridge: mb is NULL");
        return -1;
    }
    mb->hippocampus_bridge.initialized = true;
    mb->hippocampus_bridge.hippocampus_ref = hippocampus;
    mb->hippocampus_bridge.fornix_strength = 0.8f;
    mb->hippocampus_bridge.ca3_input_weight = 0.4f;
    mb->hippocampus_bridge.ca1_input_weight = 0.4f;
    mb->hippocampus_bridge.subiculum_input_weight = 0.2f;
    return 0;
}

int mammillary_init_thalamus_bridge(nimcp_mammillary_t* mb, void* thalamus) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_init_thalamus_bridge: mb is NULL");
        return -1;
    }
    mb->thalamus_bridge.initialized = true;
    mb->thalamus_bridge.thalamus_ref = thalamus;
    mb->thalamus_bridge.tract_strength = 0.8f;
    mb->thalamus_bridge.relay_efficiency = 0.9f;
    return 0;
}

int mammillary_init_cingulate_bridge(nimcp_mammillary_t* mb, void* cingulate) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_init_cingulate_bridge: mb is NULL");
        return -1;
    }
    mb->cingulate_bridge.initialized = true;
    mb->cingulate_bridge.cingulate_ref = cingulate;
    mb->cingulate_bridge.feedback_strength = 0.5f;
    mb->cingulate_bridge.attention_modulation = 0.5f;
    return 0;
}

int mammillary_init_hypothalamus_bridge(nimcp_mammillary_t* mb, void* hypothalamus) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_init_hypothalamus_bridge: mb is NULL");
        return -1;
    }
    mb->hypothalamus_bridge.initialized = true;
    mb->hypothalamus_bridge.hypothalamus_ref = hypothalamus;
    mb->hypothalamus_bridge.modulation_strength = 0.5f;
    mb->hypothalamus_bridge.circadian_phase = 0.0f;
    return 0;
}

int mammillary_init_vestibular_bridge(nimcp_mammillary_t* mb, void* vestibular) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_init_vestibular_bridge: mb is NULL");
        return -1;
    }
    mb->vestibular_bridge.initialized = true;
    mb->vestibular_bridge.vestibular_ref = vestibular;
    mb->vestibular_bridge.update_rate = NIMCP_HIGH_UPDATE_RATE_HZ;
    mb->vestibular_bridge.calibration_offset = 0.0f;
    return 0;
}

int mammillary_init_entorhinal_bridge(nimcp_mammillary_t* mb, void* entorhinal) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_init_entorhinal_bridge: mb is NULL");
        return -1;
    }
    mb->entorhinal_bridge.initialized = true;
    mb->entorhinal_bridge.entorhinal_ref = entorhinal;
    mb->entorhinal_bridge.grid_cell_input_weight = 0.5f;
    mb->entorhinal_bridge.path_integration_gain = 1.0f;
    return 0;
}

int mammillary_init_security_bridge(nimcp_mammillary_t* mb, void* security_ctx, void* security_ops) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_init_security_bridge: mb is NULL");
        return -1;
    }
    mb->security_bridge.initialized = true;
    mb->security_bridge.security_ctx = security_ctx;
    mb->security_bridge.security_ops = security_ops;
    mb->security_bridge.access_level = 1;
    mb->security_bridge.memory_encryption_enabled = false;
    return 0;
}

int mammillary_init_immune_bridge(nimcp_mammillary_t* mb, void* immune) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_init_immune_bridge: mb is NULL");
        return -1;
    }
    mb->immune_bridge.initialized = true;
    mb->immune_bridge.immune_system = immune;
    mb->immune_bridge.health_score = 1.0f;
    mb->immune_bridge.inflammation_level = 0.0f;
    return 0;
}

int mammillary_init_bio_async_bridge(nimcp_mammillary_t* mb, void* runtime) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_init_bio_async_bridge: mb is NULL");
        return -1;
    }
    mb->bio_async_bridge.initialized = true;
    mb->bio_async_bridge.runtime = runtime;
    mb->bio_async_bridge.async_efficiency = 1.0f;
    mb->bio_async_bridge.background_consolidation = true;
    return 0;
}

int mammillary_init_logging_bridge(nimcp_mammillary_t* mb, void* logger) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_init_logging_bridge: mb is NULL");
        return -1;
    }
    mb->logging_bridge.initialized = true;
    mb->logging_bridge.logger = logger;
    mb->logging_bridge.log_level = 2;
    strncpy(mb->logging_bridge.log_prefix, "MAMMILLARY", 31);
    return 0;
}

int mammillary_init_resonance_bridge(nimcp_mammillary_t* mb, nimcp_prime_resonance_t* resonance) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_init_resonance_bridge: mb is NULL");
        return -1;
    }
    mb->resonance_bridge.initialized = true;
    mb->resonance_bridge.resonance = resonance;
    mb->resonance_bridge.theta_phase = 0.0f;
    mb->resonance_bridge.gamma_phase = 0.0f;
    mb->resonance_bridge.frequency = 6.0f;      /* Theta band for memory timing */
    mb->resonance_bridge.resonance_amplitude = 0.5f;
    mb->resonance_bridge.papez_coupling = 0.7f;
    mb->resonance_bridge.hd_theta_sync = 0.0f;
    mb->resonance_bridge.synchronized = false;
    mb->resonance_bridge.phase_updates = 0;
    return 0;
}

int mammillary_update_resonance_phase(nimcp_mammillary_t* mb, float theta_phase, float gamma_phase) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_update_resonance_phase: mb is NULL");
        return -1;
    }
    if (!mb->resonance_bridge.initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_update_resonance_phase: mb->resonance_bridge is NULL");
        return -1;
    }

    mb->resonance_bridge.theta_phase = theta_phase;
    mb->resonance_bridge.gamma_phase = gamma_phase;
    mb->resonance_bridge.phase_updates++;

    /* Compute head direction-theta synchronization */
    /* HD cells fire in phase with theta rhythm */
    float hd_phase = fmodf(mb->current_heading, NIMCP_TWO_PI_F);
    float phase_diff = fabsf(hd_phase - theta_phase);
    if (phase_diff > 3.14159265f) phase_diff = NIMCP_TWO_PI_F - phase_diff;
    mb->resonance_bridge.hd_theta_sync = 1.0f - (phase_diff / NIMCP_PI_F);

    mb->resonance_bridge.synchronized = (mb->resonance_bridge.hd_theta_sync > 0.5f);

    return 0;
}

float mammillary_get_theta_phase(const nimcp_mammillary_t* mb) {
    if (!mb || !mb->resonance_bridge.initialized) return 0.0f;
    return mb->resonance_bridge.theta_phase;
}

float mammillary_get_hd_theta_sync(const nimcp_mammillary_t* mb) {
    if (!mb || !mb->resonance_bridge.initialized) return 0.0f;
    return mb->resonance_bridge.hd_theta_sync;
}

int mammillary_sync_papez_to_theta(nimcp_mammillary_t* mb) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_sync_papez_to_theta: mb is NULL");
        return -1;
    }
    if (!mb->resonance_bridge.initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_sync_papez_to_theta: mb->resonance_bridge is NULL");
        return -1;
    }

    /* Papez circuit timing is synchronized to theta rhythm */
    /* Memory consolidation is most effective at specific theta phases */
    float optimal_phase = 0.0f; /* Peak theta for encoding */
    float phase_diff = fabsf(mb->resonance_bridge.theta_phase - optimal_phase);
    if (phase_diff > 3.14159265f) phase_diff = NIMCP_TWO_PI_F - phase_diff;

    /* Modulate Papez circuit activity based on theta phase */
    float coupling = mb->resonance_bridge.papez_coupling;
    float phase_modulation = 1.0f - (phase_diff / NIMCP_PI_F);

    /* Apply modulation to consolidation rate */
    if (mb->consolidation_state == CONSOLIDATION_ENCODING ||
        mb->consolidation_state == CONSOLIDATION_STRENGTHENING) {
        mb->consolidation_rate += phase_modulation * coupling * 0.01f;
        if (mb->consolidation_rate > 1.0f) {
            mb->consolidation_rate = 1.0f;
        }
    }

    return 0;
}

int mammillary_init_cognitive_bridge(nimcp_mammillary_t* mb, void* cognitive, void* training) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_init_cognitive_bridge: mb is NULL");
        return -1;
    }
    mb->cognitive_bridge.initialized = true;
    mb->cognitive_bridge.cognitive_ctx = cognitive;
    mb->cognitive_bridge.training_ctx = training;
    mb->cognitive_bridge.learning_rate = NIMCP_LEARNING_RATE_DEFAULT;
    return 0;
}

int mammillary_init_logic_bridge(nimcp_mammillary_t* mb, void* logic) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_init_logic_bridge: mb is NULL");
        return -1;
    }
    mb->logic_bridge.initialized = true;
    mb->logic_bridge.logic_ctx = logic;
    mb->logic_bridge.inference_confidence = 0.8f;
    return 0;
}

int mammillary_init_substrate_bridge(nimcp_mammillary_t* mb, void* substrate) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_init_substrate_bridge: mb is NULL");
        return -1;
    }
    mb->substrate_bridge.initialized = true;
    mb->substrate_bridge.substrate = substrate;
    mb->substrate_bridge.atp_level = 1.0f;
    mb->substrate_bridge.oxygen_level = 1.0f;
    mb->substrate_bridge.glucose_level = 1.0f;
    mb->substrate_bridge.homeostasis_ok = true;
    return 0;
}

int mammillary_init_thalamic_bridge(nimcp_mammillary_t* mb, void* thalamic_layers) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_init_thalamic_bridge: mb is NULL");
        return -1;
    }
    mb->thalamic_bridge.initialized = true;
    mb->thalamic_bridge.thalamic_layers = thalamic_layers;
    mb->thalamic_bridge.relay_gain = 1.0f;
    mb->thalamic_bridge.gating_level = 0.5f;
    return 0;
}

int mammillary_init_perception_bridge(nimcp_mammillary_t* mb, void* perception) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_init_perception_bridge: mb is NULL");
        return -1;
    }
    mb->perception_bridge.initialized = true;
    mb->perception_bridge.perception_ctx = perception;
    mb->perception_bridge.salience_threshold = 0.5f;
    mb->perception_bridge.attention_level = 0.5f;
    return 0;
}

int mammillary_init_snn_bridge(nimcp_mammillary_t* mb, void* snn) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_init_snn_bridge: mb is NULL");
        return -1;
    }
    mb->snn_bridge.initialized = true;
    mb->snn_bridge.snn_network = snn;
    mb->snn_bridge.global_learning_rate = NIMCP_LEARNING_RATE_DEFAULT;
    mb->snn_bridge.plasticity_enabled = true;
    return 0;
}

int mammillary_init_swarm_bridge(nimcp_mammillary_t* mb, void* swarm, void* dragonfly) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_init_swarm_bridge: mb is NULL");
        return -1;
    }
    mb->swarm_bridge.initialized = true;
    mb->swarm_bridge.swarm_ctx = swarm;
    mb->swarm_bridge.dragonfly_ctx = dragonfly;
    mb->swarm_bridge.swarm_coherence = 0.8f;
    return 0;
}

int mammillary_init_cerebellum_bridge(nimcp_mammillary_t* mb, void* cerebellum) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_init_cerebellum_bridge: mb is NULL");
        return -1;
    }
    mb->cerebellum_bridge.initialized = true;
    mb->cerebellum_bridge.cerebellum = cerebellum;
    mb->cerebellum_bridge.timing_precision = 0.9f;
    mb->cerebellum_bridge.motor_learning_rate = NIMCP_LEARNING_RATE_DEFAULT;
    return 0;
}

int mammillary_init_medulla_bridge(nimcp_mammillary_t* mb, void* medulla) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_init_medulla_bridge: mb is NULL");
        return -1;
    }
    mb->medulla_bridge.initialized = true;
    mb->medulla_bridge.medulla = medulla;
    mb->medulla_bridge.autonomic_state = 0.5f;
    mb->medulla_bridge.emergency_mode = false;
    return 0;
}

int mammillary_init_omni_bridge(nimcp_mammillary_t* mb, void* omni) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_init_omni_bridge: mb is NULL");
        return -1;
    }
    mb->omni_bridge.initialized = true;
    mb->omni_bridge.omni_ctx = omni;
    mb->omni_bridge.integration_weight = 0.5f;
    mb->omni_bridge.synchronization_quality = 1.0f;
    return 0;
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

mammillary_status_t mammillary_get_status(nimcp_mammillary_t* mb) {
    if (!mb) return MAMMILLARY_STATUS_ERROR;
    return mb->status;
}

mammillary_error_t mammillary_get_last_error(nimcp_mammillary_t* mb) {
    if (!mb) return MAMMILLARY_ERROR_INTERNAL;
    return mb->last_error;
}

const char* mammillary_error_string(mammillary_error_t error) {
    switch (error) {
        case MAMMILLARY_ERROR_NONE: return "No error";
        case MAMMILLARY_ERROR_INVALID_INPUT: return "Invalid input";
        case MAMMILLARY_ERROR_MEMORY_FULL: return "Memory full";
        case MAMMILLARY_ERROR_RELAY_FAILED: return "Relay failed";
        case MAMMILLARY_ERROR_HD_DRIFT: return "Head direction drift";
        case MAMMILLARY_ERROR_CONSOLIDATION_FAILED: return "Consolidation failed";
        case MAMMILLARY_ERROR_CIRCUIT_BROKEN: return "Circuit broken";
        case MAMMILLARY_ERROR_INTERNAL: return "Internal error";
        default: return "Unknown error";
    }
}

const char* mammillary_status_string(mammillary_status_t status) {
    switch (status) {
        case MAMMILLARY_STATUS_IDLE: return "Idle";
        case MAMMILLARY_STATUS_READY: return "Ready";
        case MAMMILLARY_STATUS_RELAYING: return "Relaying";
        case MAMMILLARY_STATUS_CONSOLIDATING: return "Consolidating";
        case MAMMILLARY_STATUS_HD_PROCESSING: return "HD processing";
        case MAMMILLARY_STATUS_SPATIAL_ENCODING: return "Spatial encoding";
        case MAMMILLARY_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}

int mammillary_get_stats(nimcp_mammillary_t* mb, mammillary_stats_t* stats) {
    if (!mb || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_get_stats: required parameter is NULL (mb, stats)");
        return -1;
    }

    memset(stats, 0, sizeof(mammillary_stats_t));

    stats->total_memory_traces = mb->num_memory_traces;
    stats->relay_operations = mb->relay_operations;
    stats->hd_updates = mb->updates_processed;
    stats->current_heading = mb->current_heading;
    stats->hd_accuracy = mb->heading_confidence;
    stats->last_update_time = mb->last_update_time;
    stats->updates_processed = mb->updates_processed;

    /* Count consolidated and decayed traces */
    for (uint32_t i = 0; i < mb->max_memory_traces; i++) {
        if (mb->memory_traces[i].content) {
            if (mb->memory_traces[i].state == CONSOLIDATION_COMPLETE) {
                stats->traces_consolidated++;
            }
            stats->avg_consolidation_strength += mb->memory_traces[i].strength;
        }
    }

    if (stats->total_memory_traces > 0) {
        stats->avg_consolidation_strength /= stats->total_memory_traces;
    }

    /* Calculate relay efficiency */
    float total_relay = 0.0f;
    for (uint32_t i = 0; i < mb->num_relay_cells; i++) {
        total_relay += mb->relay_cells[i].activation;
    }
    stats->avg_relay_efficiency = (mb->num_relay_cells > 0) ? (total_relay / mb->num_relay_cells) : 0.0f;

    return 0;
}

int mammillary_get_config(nimcp_mammillary_t* mb, mammillary_config_t* config) {
    if (!mb || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_get_config: required parameter is NULL (mb, config)");
        return -1;
    }
    *config = mb->config;
    return 0;
}

float mammillary_get_health_status(nimcp_mammillary_t* mb) {
    if (!mb) return 0.0f;

    float health = 1.0f;

    /* Consider immune status */
    if (mb->immune_bridge.initialized) {
        health *= mb->immune_bridge.health_score;
    }

    /* Consider substrate status */
    if (mb->substrate_bridge.initialized) {
        health *= (mb->substrate_bridge.atp_level * 0.5f +
                   mb->substrate_bridge.oxygen_level * 0.3f +
                   mb->substrate_bridge.glucose_level * 0.2f);
    }

    /* Consider circuit integrity */
    health *= mb->circuit_integrity;

    /* Consider error state */
    if (mb->status == MAMMILLARY_STATUS_ERROR) {
        health *= 0.5f;
    }

    return fminf(1.0f, fmaxf(0.0f, health));
}

float mammillary_get_circuit_integrity(nimcp_mammillary_t* mb) {
    if (!mb) return 0.0f;
    return mb->circuit_integrity;
}

int mammillary_log_diagnostics(nimcp_mammillary_t* mb) {
    if (!mb) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_log_diagnostics: mb is NULL");
        return -1;
    }

    /* Diagnostics would be logged via logging bridge if available */
    /* For now, just validate state */
    mb->circuit_integrity = 1.0f;

    /* Check hippocampus connection */
    if (!mb->hippocampus_bridge.initialized) {
        mb->circuit_integrity *= 0.8f;
    }

    /* Check thalamus connection */
    if (!mb->thalamus_bridge.initialized) {
        mb->circuit_integrity *= 0.8f;
    }

    return 0;
}

/*=============================================================================
 * CELL ACTIVITY QUERIES
 *===========================================================================*/

size_t mammillary_get_hd_cell_activity(nimcp_mammillary_t* mb,
                                        float* activity,
                                        size_t max_cells) {
    if (!mb || !activity) return 0;

    size_t count = (mb->num_hd_cells < max_cells) ? mb->num_hd_cells : max_cells;
    for (size_t i = 0; i < count; i++) {
        activity[i] = (fabsf(mb->hd_cells[i].max_firing_rate) > 1e-10f) ?
                      (mb->hd_cells[i].current_firing_rate / mb->hd_cells[i].max_firing_rate) : 0.0f;
    }

    return count;
}

size_t mammillary_get_relay_cell_activity(nimcp_mammillary_t* mb,
                                           float* activity,
                                           size_t max_cells) {
    if (!mb || !activity) return 0;

    size_t count = (mb->num_relay_cells < max_cells) ? mb->num_relay_cells : max_cells;
    for (size_t i = 0; i < count; i++) {
        activity[i] = mb->relay_cells[i].activation;
    }

    return count;
}

size_t mammillary_get_spatial_cell_activity(nimcp_mammillary_t* mb,
                                             float* activity,
                                             size_t max_cells) {
    if (!mb || !activity) return 0;

    size_t count = (mb->num_spatial_cells < max_cells) ? mb->num_spatial_cells : max_cells;
    for (size_t i = 0; i < count; i++) {
        activity[i] = mb->spatial_cells[i].activation;
    }

    return count;
}

/*=============================================================================
 * SERIALIZATION
 *===========================================================================*/

size_t mammillary_get_serialization_size(nimcp_mammillary_t* mb) {
    if (!mb) return 0;

    size_t size = sizeof(mammillary_config_t);
    size += sizeof(mammillary_status_t);
    size += sizeof(uint32_t) * 4;  /* Counts */
    size += sizeof(float) * 8;     /* Head direction state */

    /* HD cells */
    size += mb->num_hd_cells * sizeof(nimcp_hd_cell_t);

    /* Relay cells (basic, without dynamic weights) */
    size += mb->num_relay_cells * sizeof(uint32_t) * 2;  /* IDs and activation */

    /* Spatial cells (basic) */
    size += mb->num_spatial_cells * sizeof(float) * 5;

    /* Memory traces (estimate) */
    size += mb->num_memory_traces * 256;  /* Rough estimate per trace */

    return size;
}

int mammillary_serialize(nimcp_mammillary_t* mb,
                          uint8_t* buffer,
                          size_t buffer_size,
                          size_t* bytes_written) {
    if (!mb || !buffer || !bytes_written) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mammillary_get_serialization_size: required parameter is NULL (mb, buffer, bytes_written)");
        return -1;
    }

    size_t required = mammillary_get_serialization_size(mb);
    if (buffer_size < required) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mammillary_get_serialization_size: validation failed");
        return -1;
    }

    size_t offset = 0;

    /* Write config */
    memcpy(buffer + offset, &mb->config, sizeof(mammillary_config_t));
    offset += sizeof(mammillary_config_t);

    /* Write status */
    memcpy(buffer + offset, &mb->status, sizeof(mammillary_status_t));
    offset += sizeof(mammillary_status_t);

    /* Write counts */
    memcpy(buffer + offset, &mb->num_hd_cells, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(buffer + offset, &mb->num_relay_cells, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(buffer + offset, &mb->num_spatial_cells, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(buffer + offset, &mb->num_memory_traces, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    /* Write head direction state */
    memcpy(buffer + offset, &mb->current_heading, sizeof(float));
    offset += sizeof(float);
    memcpy(buffer + offset, &mb->heading_confidence, sizeof(float));
    offset += sizeof(float);
    memcpy(buffer + offset, mb->hd_population_vector, sizeof(float) * 2);
    offset += sizeof(float) * 2;

    *bytes_written = offset;
    return 0;
}

nimcp_mammillary_t* mammillary_deserialize(const uint8_t* buffer,
                                            size_t buffer_size,
                                            size_t* bytes_read) {
    if (!buffer || buffer_size < sizeof(mammillary_config_t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mammillary_get_serialization_size: buffer is NULL");
        return NULL;
    }

    size_t offset = 0;

    /* Read config */
    mammillary_config_t config;
    memcpy(&config, buffer + offset, sizeof(mammillary_config_t));
    offset += sizeof(mammillary_config_t);

    /* Create instance */
    nimcp_mammillary_t* mb = mammillary_create(&config);
    if (!mb) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mb is NULL");

        return NULL;

    }

    /* Read status */
    memcpy(&mb->status, buffer + offset, sizeof(mammillary_status_t));
    offset += sizeof(mammillary_status_t);

    /* Skip counts (already set from config) */
    offset += sizeof(uint32_t) * 4;

    /* Read head direction state */
    memcpy(&mb->current_heading, buffer + offset, sizeof(float));
    offset += sizeof(float);
    memcpy(&mb->heading_confidence, buffer + offset, sizeof(float));
    offset += sizeof(float);
    memcpy(mb->hd_population_vector, buffer + offset, sizeof(float) * 2);
    offset += sizeof(float) * 2;

    if (bytes_read) *bytes_read = offset;
    return mb;
}
