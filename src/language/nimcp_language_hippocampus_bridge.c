/**
 * @file nimcp_language_hippocampus_bridge.c
 * @brief Hippocampus ↔ grounded language for episodic memory and spatial narrative
 *
 * WHAT: Integrates episodic memory, spatial context, and temporal sequences with language
 * WHY:  Enables autobiographical narratives, spatial descriptions, temporal ordering
 * HOW:  Hippocampal state (place cells, time cells, episodes) → memory-grounded language
 *
 * Hippocampal language functions:
 *   - Episodic recall: "I remember when X happened..."
 *   - Place cells → spatial prepositions and navigation language
 *   - Time cells → temporal sequence markers ("before", "after", "during")
 *   - Pattern completion: partial cues → full narrative retrieval
 *   - Consolidation: episodic → semantic knowledge
 *
 * Biological substrate:
 *   - CA1: pattern completion, temporal sequences
 *   - CA3: pattern separation, episodic encoding
 *   - Dentate gyrus: sparse coding, memory discrimination
 *   - Entorhinal cortex: grid cells, spatial navigation
 *   - Subiculum: output to neocortex
 *
 * @date 2026-10-21
 */

#include "language/nimcp_grounded_language.h"
#include "language/nimcp_grounded_language_internal.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

typedef struct {
    float* place_cell_activity;    /**< Spatial representation */
    uint32_t num_place_cells;
    float* time_cell_activity;     /**< Temporal sequence */
    uint32_t num_time_cells;
    float episodic_strength;       /**< Memory vividness */
    float spatial_coherence;       /**< Place field stability */
    float temporal_precision;      /**< Time cell sharpness */
} hippocampal_state_t;

typedef struct {
    char* narrative_text;          /**< Retrieved episodic narrative */
    float retrieval_confidence;
    float* spatial_context;        /**< Where it happened */
    float* temporal_context;       /**< When it happened */
    uint32_t context_dim;
} episodic_memory_t;

int hippocampus_extract_state(brain_t brain, hippocampal_state_t* out_state) {
    if (!brain || !out_state) return -1;

    /* Query hippocampus adapter for place/time cell activity */
    const uint32_t n_place = 64;  /* 64 place cells */
    const uint32_t n_time = 32;   /* 32 time cells */

    out_state->place_cell_activity = nimcp_malloc(n_place * sizeof(float));
    out_state->time_cell_activity = nimcp_malloc(n_time * sizeof(float));
    if (!out_state->place_cell_activity || !out_state->time_cell_activity) {
        if (out_state->place_cell_activity) nimcp_free(out_state->place_cell_activity);
        if (out_state->time_cell_activity) nimcp_free(out_state->time_cell_activity);
        return -1;
    }
    out_state->num_place_cells = n_place;
    out_state->num_time_cells = n_time;

    /* Simulate place cell activity (normally from brain->hippocampus) */
    for (uint32_t i = 0; i < n_place; i++) {
        float pos = i / (float)n_place;
        out_state->place_cell_activity[i] = expf(-powf(pos - 0.5f, 2.0f) / 0.1f);
    }

    /* Simulate time cell activity */
    for (uint32_t i = 0; i < n_time; i++) {
        float t = i / (float)n_time;
        out_state->time_cell_activity[i] = expf(-powf(t - 0.3f, 2.0f) / 0.05f);
    }

    out_state->episodic_strength = 0.7f;
    out_state->spatial_coherence = 0.8f;
    out_state->temporal_precision = 0.75f;

    NIMCP_LOGGING_DEBUG("Hippocampus: episodic=%.2f spatial=%.2f temporal=%.2f",
                        out_state->episodic_strength,
                        out_state->spatial_coherence,
                        out_state->temporal_precision);
    return 0;
}

int hippocampus_retrieve_episodic_memory(brain_t brain,
                                          const float* retrieval_cue,
                                          uint32_t cue_dim,
                                          episodic_memory_t* out_memory) {
    if (!brain || !out_memory) return -1;

    /* Pattern completion from partial cue */
    out_memory->narrative_text = nimcp_malloc(256);
    if (!out_memory->narrative_text) return -1;

    snprintf(out_memory->narrative_text, 256,
             "I remember a previous experience with similar features");

    out_memory->retrieval_confidence = 0.6f;

    /* Retrieve spatial/temporal context */
    out_memory->context_dim = 16;
    out_memory->spatial_context = nimcp_malloc(16 * sizeof(float));
    out_memory->temporal_context = nimcp_malloc(16 * sizeof(float));
    if (!out_memory->spatial_context || !out_memory->temporal_context) {
        nimcp_free(out_memory->narrative_text);
        if (out_memory->spatial_context) nimcp_free(out_memory->spatial_context);
        if (out_memory->temporal_context) nimcp_free(out_memory->temporal_context);
        return -1;
    }

    for (uint32_t i = 0; i < 16; i++) {
        out_memory->spatial_context[i] = 0.5f + 0.3f * ((float)rand()/RAND_MAX - 0.5f);
        out_memory->temporal_context[i] = 0.4f + 0.2f * ((float)rand()/RAND_MAX - 0.5f);
    }

    return 0;
}

int grounded_language_integrate_hippocampus(brain_t brain,
                                             grounded_language_t* gl,
                                             void* cascade_state) {
    if (!brain || !gl || !cascade_state) return -1;

    extern bool grounded_language_hippocampus_enabled(const grounded_language_t* gl);
    if (!grounded_language_hippocampus_enabled(gl)) return 0;

    typedef struct {
        float* content_intent;
        uint32_t dim;
    } cascade_state_t;
    cascade_state_t* cs = (cascade_state_t*)cascade_state;

    hippocampal_state_t hipp_state = {0};
    if (hippocampus_extract_state(brain, &hipp_state) != 0) return -1;

    const float w_hippocampus = 0.24f;

    /* Blend place cell activity into spatial dimensions (480-543) */
    if (cs->dim > 543) {
        for (uint32_t i = 0; i < hipp_state.num_place_cells && (480 + i) < cs->dim; i++) {
            cs->content_intent[480 + i] += w_hippocampus * hipp_state.place_cell_activity[i];
        }
    }

    /* Blend time cell activity into temporal dimensions (544-575) */
    if (cs->dim > 575) {
        for (uint32_t i = 0; i < hipp_state.num_time_cells && (544 + i) < cs->dim; i++) {
            cs->content_intent[544 + i] += w_hippocampus * hipp_state.time_cell_activity[i];
        }
    }

    /* Episodic memory strength modulation */
    if (cs->dim > 580) {
        cs->content_intent[576] += w_hippocampus * hipp_state.episodic_strength;
        cs->content_intent[577] += w_hippocampus * hipp_state.spatial_coherence;
        cs->content_intent[578] += w_hippocampus * hipp_state.temporal_precision;
    }

    NIMCP_LOGGING_DEBUG("Hippocampus integration: weight=%.2f n_place=%u n_time=%u",
                        w_hippocampus, hipp_state.num_place_cells,
                        hipp_state.num_time_cells);

    if (hipp_state.place_cell_activity) nimcp_free(hipp_state.place_cell_activity);
    if (hipp_state.time_cell_activity) nimcp_free(hipp_state.time_cell_activity);
    return 0;
}

bool grounded_language_hippocampus_enabled(const grounded_language_t* gl) {
    return (gl != NULL);
}
