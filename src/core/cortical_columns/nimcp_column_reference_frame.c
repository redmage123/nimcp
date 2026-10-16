/**
 * @file nimcp_column_reference_frame.c
 * @brief Grid Cell Reference Frames for Cortical Columns — Implementation
 *
 * WHAT: Each cortical column gets a reference frame from entorhinal grid cells.
 *       Columns transform input through reference frames, building object models
 *       as "features at locations."
 * WHY:  Enables invariant object recognition — the same object recognized from
 *       different viewpoints by different columns with different phase offsets.
 * HOW:  Each hypercolumn binds to a grid module with unique phase offsets.
 *       Grid cell population vectors encode location. Feature-location pairs
 *       stored as associations. Movement updates via path integration.
 *
 * Based on Hawkins' Thousand Brains theory (Numenta, 2019).
 */

#include "core/cortical_columns/nimcp_column_reference_frame.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"

#include <math.h>
#include <string.h>
#include <float.h>

/* =========================================================================
 * Configuration
 * ========================================================================= */

void column_ref_frame_config_default(column_ref_frame_config_t* config) {
    if (!config) return;
    config->max_frames = COL_REF_FRAME_MAX_FRAMES;
    config->encoding_dim = COL_REF_FRAME_ENCODING_DIM;
    config->max_pairs_per_frame = COL_REF_FRAME_MAX_PAIRS;
    config->movement_threshold = 0.01f;
    config->association_learning_rate = 0.1f;
    config->recall_threshold = 0.3f;
    config->path_integration_gain = 1.0f;
}

/* =========================================================================
 * Lifecycle
 * ========================================================================= */

column_ref_frame_manager_t* column_ref_frame_create(const column_ref_frame_config_t* config) {
    if (!config) return NULL;

    column_ref_frame_manager_t* mgr = nimcp_calloc(1, sizeof(column_ref_frame_manager_t));
    if (!mgr) return NULL;

    uint32_t max_frames = config->max_frames;
    if (max_frames == 0) max_frames = COL_REF_FRAME_MAX_FRAMES;

    mgr->frames = nimcp_calloc(max_frames, sizeof(column_reference_frame_t));
    if (!mgr->frames) {
        nimcp_free(mgr);
        return NULL;
    }

    mgr->num_frames = 0;
    mgr->max_frames = max_frames;
    mgr->movement_threshold = config->movement_threshold;
    mgr->association_learning_rate = config->association_learning_rate;
    mgr->recall_threshold = config->recall_threshold;
    mgr->path_integration_gain = config->path_integration_gain;
    mgr->encoding_dim = config->encoding_dim > 0 ? config->encoding_dim
                                                   : COL_REF_FRAME_ENCODING_DIM;

    mgr->mutex = nimcp_mutex_create(NULL);

    NIMCP_LOGGING_INFO("column_ref_frame: created manager (max_frames=%u, encoding_dim=%u)",
                       mgr->max_frames, mgr->encoding_dim);
    return mgr;
}

void column_ref_frame_destroy(column_ref_frame_manager_t* mgr) {
    if (!mgr) return;
    nimcp_free(mgr->frames);
    if (mgr->mutex) nimcp_mutex_free(mgr->mutex);
    nimcp_free(mgr);
}

/* =========================================================================
 * Internal helpers
 * ========================================================================= */

/**
 * @brief Compute grid cell population encoding from a 3D location + phase offset.
 *
 * Uses multi-scale periodic encoding: for each dimension, we generate
 * sinusoidal basis functions at multiple spatial frequencies, offset by
 * the column's unique phase. This produces a distributed representation
 * where nearby locations have similar encodings.
 */
static void encode_location(const float* location, const float* phase_offset,
                             float* encoding, uint32_t encoding_dim) {
    uint32_t freqs_per_dim = encoding_dim / (COL_REF_FRAME_LOCATION_DIM * 2);
    if (freqs_per_dim == 0) freqs_per_dim = 1;

    uint32_t idx = 0;
    for (uint32_t d = 0; d < COL_REF_FRAME_LOCATION_DIM && idx < encoding_dim; d++) {
        float loc = location[d] + phase_offset[d];
        for (uint32_t f = 0; f < freqs_per_dim && idx < encoding_dim; f++) {
            float freq = (float)(f + 1) * 0.5f; /* Increasing spatial frequencies */
            float phase = loc * freq * 2.0f * 3.14159265f;
            encoding[idx++] = sinf(phase);
            if (idx < encoding_dim) {
                encoding[idx++] = cosf(phase);
            }
        }
    }

    /* Zero-pad remainder */
    while (idx < encoding_dim) {
        encoding[idx++] = 0.0f;
    }

    /* L2 normalize */
    float norm = 0.0f;
    for (uint32_t i = 0; i < encoding_dim; i++) {
        norm += encoding[i] * encoding[i];
    }
    norm = sqrtf(norm);
    if (norm > 1e-8f) {
        float inv = 1.0f / norm;
        for (uint32_t i = 0; i < encoding_dim; i++) {
            encoding[i] *= inv;
        }
    }
}

/**
 * @brief Cosine similarity between two vectors.
 */
static float cosine_similarity(const float* a, const float* b, uint32_t dim) {
    float dot = 0.0f, na = 0.0f, nb = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        na += a[i] * a[i];
        nb += b[i] * b[i];
    }
    float denom = sqrtf(na) * sqrtf(nb);
    if (denom < 1e-8f) return 0.0f;
    return dot / denom;
}

/* =========================================================================
 * API
 * ========================================================================= */

int column_ref_frame_bind_column(column_ref_frame_manager_t* mgr,
                                  uint32_t column_id, uint32_t grid_module_idx,
                                  const float* phase_offset) {
    if (!mgr || !phase_offset) return -1;
    if (mgr->num_frames >= mgr->max_frames) return -1;

    uint32_t idx = mgr->num_frames;
    column_reference_frame_t* frame = &mgr->frames[idx];
    memset(frame, 0, sizeof(column_reference_frame_t));

    frame->column_id = column_id;
    frame->grid_module_idx = grid_module_idx;
    memcpy(frame->phase_offset, phase_offset,
           COL_REF_FRAME_LOCATION_DIM * sizeof(float));

    /* Initial location at origin, compute initial encoding */
    encode_location(frame->location, frame->phase_offset,
                    frame->location_encoding, mgr->encoding_dim);

    mgr->num_frames++;

    NIMCP_LOGGING_DEBUG("column_ref_frame: bound column %u to grid module %u (frame %u)",
                        column_id, grid_module_idx, idx);
    return (int)idx;
}

int column_ref_frame_connect_entorhinal(column_ref_frame_manager_t* mgr,
                                         void* entorhinal_ctx) {
    if (!mgr) return -1;
    /* Stub: store context for future entorhinal grid cell integration */
    (void)entorhinal_ctx;
    NIMCP_LOGGING_INFO("column_ref_frame: entorhinal connection registered (stub)");
    return 0;
}

int column_ref_frame_update_location(column_ref_frame_manager_t* mgr,
                                      uint32_t frame_idx,
                                      const float* movement) {
    if (!mgr || !movement) return -1;
    if (frame_idx >= mgr->num_frames) return -1;

    /* Check movement magnitude against threshold */
    float mag_sq = 0.0f;
    for (uint32_t d = 0; d < COL_REF_FRAME_LOCATION_DIM; d++) {
        mag_sq += movement[d] * movement[d];
    }
    if (mag_sq < mgr->movement_threshold * mgr->movement_threshold) {
        return 0; /* Below threshold, no update */
    }

    column_reference_frame_t* frame = &mgr->frames[frame_idx];

    /* Path integration: update location by movement scaled by gain */
    for (uint32_t d = 0; d < COL_REF_FRAME_LOCATION_DIM; d++) {
        frame->location[d] += movement[d] * mgr->path_integration_gain;
    }

    /* Recompute population encoding */
    encode_location(frame->location, frame->phase_offset,
                    frame->location_encoding, mgr->encoding_dim);

    mgr->stats.total_location_updates++;
    return 0;
}

int column_ref_frame_encode_feature_at_location(column_ref_frame_manager_t* mgr,
                                                  uint32_t frame_idx,
                                                  const float* feature, uint32_t feat_dim,
                                                  uint32_t object_id) {
    if (!mgr || !feature) return -1;
    if (frame_idx >= mgr->num_frames) return -1;

    column_reference_frame_t* frame = &mgr->frames[frame_idx];

    /* Check if we already have this feature-location pair for this object */
    uint32_t copy_dim = feat_dim < COL_REF_FRAME_FEATURE_DIM
                      ? feat_dim : COL_REF_FRAME_FEATURE_DIM;

    for (uint32_t i = 0; i < frame->num_pairs; i++) {
        feature_location_pair_t* pair = &frame->pairs[i];
        if (pair->object_id != object_id) continue;

        /* Check if location is close */
        float loc_sim = cosine_similarity(pair->location, frame->location,
                                          COL_REF_FRAME_LOCATION_DIM);
        if (loc_sim > 0.9f) {
            /* Update existing pair: blend features */
            float lr = mgr->association_learning_rate;
            for (uint32_t f = 0; f < copy_dim; f++) {
                pair->feature[f] = (1.0f - lr) * pair->feature[f] + lr * feature[f];
            }
            pair->confidence += lr * (1.0f - pair->confidence);
            mgr->stats.total_feature_bindings++;
            return 0;
        }
    }

    /* New pair */
    if (frame->num_pairs >= COL_REF_FRAME_MAX_PAIRS) return -1;

    feature_location_pair_t* pair = &frame->pairs[frame->num_pairs];
    memset(pair, 0, sizeof(feature_location_pair_t));
    memcpy(pair->feature, feature, copy_dim * sizeof(float));
    memcpy(pair->location, frame->location,
           COL_REF_FRAME_LOCATION_DIM * sizeof(float));
    pair->confidence = mgr->association_learning_rate;
    pair->object_id = object_id;

    frame->num_pairs++;
    mgr->stats.total_feature_bindings++;
    return 0;
}

int column_ref_frame_get_location_encoding(const column_ref_frame_manager_t* mgr,
                                            uint32_t frame_idx,
                                            float* encoding, uint32_t max_dim) {
    if (!mgr || !encoding) return -1;
    if (frame_idx >= mgr->num_frames) return -1;

    uint32_t copy_dim = max_dim < mgr->encoding_dim ? max_dim : mgr->encoding_dim;
    memcpy(encoding, mgr->frames[frame_idx].location_encoding,
           copy_dim * sizeof(float));
    return 0;
}

int column_ref_frame_recall_feature_at(column_ref_frame_manager_t* mgr,
                                        uint32_t frame_idx,
                                        float* recalled_feature, uint32_t feat_dim,
                                        uint32_t* object_id, float* confidence) {
    if (!mgr) return -1;
    if (frame_idx >= mgr->num_frames) return -1;

    const column_reference_frame_t* frame = &mgr->frames[frame_idx];

    /* Find best matching pair at current location */
    float best_sim = -1.0f;
    int best_idx = -1;

    for (uint32_t i = 0; i < frame->num_pairs; i++) {
        float loc_sim = cosine_similarity(frame->pairs[i].location,
                                          frame->location,
                                          COL_REF_FRAME_LOCATION_DIM);
        float combined = loc_sim * frame->pairs[i].confidence;
        if (combined > best_sim) {
            best_sim = combined;
            best_idx = (int)i;
        }
    }

    mgr->stats.total_recalls++;

    if (best_idx < 0 || best_sim < mgr->recall_threshold) {
        return 1; /* No match */
    }

    const feature_location_pair_t* pair = &frame->pairs[best_idx];
    if (recalled_feature) {
        uint32_t copy_dim = feat_dim < COL_REF_FRAME_FEATURE_DIM
                          ? feat_dim : COL_REF_FRAME_FEATURE_DIM;
        memcpy(recalled_feature, pair->feature, copy_dim * sizeof(float));
    }
    if (object_id) *object_id = pair->object_id;
    if (confidence) *confidence = best_sim;

    mgr->stats.successful_recalls++;

    /* Update mean recall confidence (running average) */
    float n = (float)mgr->stats.successful_recalls;
    mgr->stats.mean_recall_confidence =
        mgr->stats.mean_recall_confidence * ((n - 1.0f) / n) + best_sim / n;

    return 0;
}

int column_ref_frame_predict_next_location(const column_ref_frame_manager_t* mgr,
                                            uint32_t frame_idx,
                                            const float* movement,
                                            float* predicted_location) {
    if (!mgr || !movement || !predicted_location) return -1;
    if (frame_idx >= mgr->num_frames) return -1;

    const column_reference_frame_t* frame = &mgr->frames[frame_idx];

    for (uint32_t d = 0; d < COL_REF_FRAME_LOCATION_DIM; d++) {
        predicted_location[d] = frame->location[d] +
                                movement[d] * mgr->path_integration_gain;
    }

    return 0;
}

int column_ref_frame_get_stats(const column_ref_frame_manager_t* mgr,
                                column_ref_frame_stats_t* stats) {
    if (!mgr || !stats) return -1;
    *stats = mgr->stats;
    return 0;
}
