//=============================================================================
// nimcp_influence_blending.c - Multi-Influence Blending System
//=============================================================================
/**
 * @file nimcp_influence_blending.c
 * @brief Blends multiple creative influences into coherent new styles
 *
 * WHAT: Combines multiple style influences with weighted contributions
 * WHY:  Enable creative synthesis from diverse inspirations
 * HOW:  Weighted embedding interpolation with coherence optimization
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#include "cognitive/creative/inspiration/nimcp_influence_blending.h"
#include "cognitive/creative/inspiration/nimcp_style_representation.h"
#include "cognitive/creative/nimcp_creative.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

#define LOG_MODULE "INFLUENCE_BLEND"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/exception/nimcp_exception_macros.h"

BRIDGE_BOILERPLATE_MESH_ONLY(influence_blending, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define DEFAULT_MAX_INFLUENCES 16
#define SLERP_THRESHOLD 0.9995f

//=============================================================================
// Config Defaults
//=============================================================================

void influence_blender_config_defaults(influence_blender_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(influence_blender_config_t));

    config->default_mode = BLEND_MODE_LINEAR;
    config->constraints = BLEND_CONSTRAINT_COHERENCE;
    config->coherence_threshold = 0.6f;
    config->originality_threshold = 0.2f;

    config->optimize_weights = false;
    config->optimization_iterations = 100;
    config->learning_rate = 0.01f;

    config->max_influences = DEFAULT_MAX_INFLUENCES;
    config->min_influence_weight = 0.05f;
}

//=============================================================================
// Internal Helpers
//=============================================================================

static float compute_coherence(const style_embedding_t* blend,
                                const creative_influence_t* influences,
                                uint32_t num_influences) {
    if (!blend || !influences || num_influences == 0) return 0.0f;

    /* Coherence = weighted average similarity to contributing positive influences */
    float weighted_sim = 0.0f;
    float total_weight = 0.0f;

    for (uint32_t i = 0; i < num_influences; i++) {
        if (influences[i].is_positive && influences[i].weight > 0.0f) {
            float sim = style_embedding_similarity(blend, &influences[i].style);
            weighted_sim += sim * influences[i].weight;
            total_weight += influences[i].weight;
        }
    }

    return total_weight > 0.0f ? weighted_sim / total_weight : 0.0f;
}

static float compute_originality(const style_embedding_t* blend,
                                  const creative_influence_t* influences,
                                  uint32_t num_influences) {
    if (!blend || !influences || num_influences == 0) return 1.0f;

    /* Originality = 1 - max similarity to any single influence */
    float max_sim = 0.0f;

    for (uint32_t i = 0; i < num_influences; i++) {
        if (influences[i].is_positive) {
            float sim = style_embedding_similarity(blend, &influences[i].style);
            if (sim > max_sim) max_sim = sim;
        }
    }

    return 1.0f - max_sim;
}

static int blend_linear(const creative_influence_t* influences,
                        uint32_t num_influences,
                        style_embedding_t* out) {
    if (!influences || num_influences == 0 || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "influence_blender_config_defaults: required parameter is NULL (influences, out)");
        return -1;
    }

    /* Get dimension from first influence */
    uint32_t dim = influences[0].style.embedding_dim;
    if (dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "influence_blender_config_defaults: dim is zero");
        return -1;
    }

    style_embedding_create(out, dim);

    /* Compute weighted sum */
    float total_positive_weight = 0.0f;
    float total_negative_weight = 0.0f;

    for (uint32_t i = 0; i < num_influences; i++) {
        if (influences[i].is_positive) {
            total_positive_weight += influences[i].weight;
        } else {
            total_negative_weight += influences[i].weight;
        }
    }

    if (total_positive_weight < 0.001f) {
        LOG_WARN(LOG_MODULE, "No positive influences");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "influence_blender_config_defaults: validation failed");
        return -1;
    }

    /* Blend: sum positive influences, subtract negative */
    for (uint32_t d = 0; d < dim; d++) {
        float val = 0.0f;

        for (uint32_t i = 0; i < num_influences; i++) {
            float w = influences[i].weight;
            if (influences[i].is_positive) {
                w /= total_positive_weight;
            } else {
                w = -w * 0.5f;  /* Negative influences have less effect */
            }
            val += influences[i].style.embedding[d] * w;
        }

        out->embedding[d] = val;
    }

    style_embedding_normalize(out);

    return 0;
}

static int blend_spherical(const creative_influence_t* influences,
                           uint32_t num_influences,
                           style_embedding_t* out) {
    if (!influences || num_influences == 0 || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "influence_blender_config_defaults: required parameter is NULL (influences, out)");
        return -1;
    }

    /* For spherical interpolation with multiple points, iteratively slerp pairs */
    /* Start with first influence */

    uint32_t dim = influences[0].style.embedding_dim;
    style_embedding_create(out, dim);

    /* Copy first positive influence */
    bool found_first = false;
    float accumulated_weight = 0.0f;

    for (uint32_t i = 0; i < num_influences; i++) {
        if (!influences[i].is_positive) continue;

        if (!found_first) {
            memcpy(out->embedding, influences[i].style.embedding, dim * sizeof(float));
            accumulated_weight = influences[i].weight;
            found_first = true;
            continue;
        }

        /* Slerp with next influence */
        float w1 = accumulated_weight;
        float w2 = influences[i].weight;
        float t = w2 / (w1 + w2);

        /* Compute dot product */
        float dot = 0.0f;
        for (uint32_t d = 0; d < dim; d++) {
            dot += out->embedding[d] * influences[i].style.embedding[d];
        }

        if (dot > SLERP_THRESHOLD) {
            /* Nearly parallel, use linear */
            for (uint32_t d = 0; d < dim; d++) {
                out->embedding[d] = out->embedding[d] * (1.0f - t) + influences[i].style.embedding[d] * t;
            }
        } else {
            /* Spherical interpolation */
            float theta = acosf(dot);
            float sin_theta = sinf(theta);
            float s0 = sinf((1.0f - t) * theta) / sin_theta;
            float s1 = sinf(t * theta) / sin_theta;

            for (uint32_t d = 0; d < dim; d++) {
                out->embedding[d] = out->embedding[d] * s0 + influences[i].style.embedding[d] * s1;
            }
        }

        accumulated_weight = w1 + w2;
    }

    /* Apply negative influences */
    for (uint32_t i = 0; i < num_influences; i++) {
        if (influences[i].is_positive) continue;

        float neg_factor = influences[i].weight * 0.3f;
        for (uint32_t d = 0; d < dim; d++) {
            out->embedding[d] -= influences[i].style.embedding[d] * neg_factor;
        }
    }

    style_embedding_normalize(out);

    return 0;
}

static int blend_adaptive(influence_blender_t* blender,
                          const creative_influence_t* influences,
                          uint32_t num_influences,
                          style_embedding_t* out) {
    if (!blender || !influences || num_influences == 0 || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "influence_blender_config_defaults: required parameter is NULL (blender, influences, out)");
        return -1;
    }

    /* Compute pairwise compatibilities and adjust weights */
    float* adjusted_weights = nimcp_calloc(num_influences, sizeof(float));
    if (!adjusted_weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "influence_blender_config_defaults: adjusted_weights is NULL");
        return -1;
    }

    /* Copy original weights */
    for (uint32_t i = 0; i < num_influences; i++) {
        adjusted_weights[i] = influences[i].weight;
    }

    /* Adjust based on compatibility */
    for (uint32_t i = 0; i < num_influences; i++) {
        if (!influences[i].is_positive) continue;

        float compat_sum = 0.0f;
        for (uint32_t j = 0; j < num_influences; j++) {
            if (i == j || !influences[j].is_positive) continue;

            float sim = style_embedding_similarity(&influences[i].style, &influences[j].style);
            compat_sum += sim * influences[j].weight;
        }

        /* Boost weight of compatible influences */
        adjusted_weights[i] *= (1.0f + compat_sum * 0.5f);
    }

    /* Normalize weights */
    float total = 0.0f;
    for (uint32_t i = 0; i < num_influences; i++) {
        if (influences[i].is_positive) {
            total += adjusted_weights[i];
        }
    }

    if (total > 0.0f) {
        for (uint32_t i = 0; i < num_influences; i++) {
            if (influences[i].is_positive) {
                adjusted_weights[i] /= total;
            }
        }
    }

    /* Create temporary influences with adjusted weights */
    creative_influence_t* adj_influences = nimcp_calloc(num_influences,
                                                         sizeof(creative_influence_t));
    if (!adj_influences) {
        nimcp_free(adjusted_weights);
        adjusted_weights = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: adj_influences is NULL");
        return -1;
    }

    /* Deep-copy influences to avoid shared ownership of style.embedding pointers */
    for (uint32_t i = 0; i < num_influences; i++) {
        adj_influences[i] = influences[i];
        adj_influences[i].weight = adjusted_weights[i];
        /* Deep-copy the embedding pointer */
        if (influences[i].style.embedding && influences[i].style.embedding_dim > 0) {
            adj_influences[i].style.embedding =
                nimcp_calloc(influences[i].style.embedding_dim, sizeof(float));
            if (adj_influences[i].style.embedding) {
                memcpy(adj_influences[i].style.embedding,
                       influences[i].style.embedding,
                       influences[i].style.embedding_dim * sizeof(float));
            }
        }
    }

    /* Blend with linear method using adjusted weights */
    int result = blend_linear(adj_influences, num_influences, out);

    /* Free deep-copied embeddings */
    for (uint32_t i = 0; i < num_influences; i++) {
        nimcp_free(adj_influences[i].style.embedding);
    }
    nimcp_free(adj_influences);
    adj_influences = NULL;
    nimcp_free(adjusted_weights);
    adjusted_weights = NULL;

    return result;
}

//=============================================================================
// Lifecycle API
//=============================================================================

influence_blender_t* influence_blender_create(
    const influence_blender_config_t* config,
    style_representer_t* style_repr) {

    influence_blender_t* blender = nimcp_calloc(1, sizeof(influence_blender_t));
    if (!blender) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate influence blender");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: blender is NULL");
        return NULL;
    }

    if (config) {
        blender->config = *config;
    } else {
        influence_blender_config_defaults(&blender->config);
    }

    blender->style_repr = style_repr;

    /* Allocate influence storage */
    blender->influences_capacity = blender->config.max_influences;
    blender->current_influences = nimcp_calloc(blender->influences_capacity,
                                                sizeof(creative_influence_t));
    if (!blender->current_influences) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate influences");
        nimcp_free(blender);
        blender = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: blender->current_influences is NULL");
        return NULL;
    }

    LOG_INFO(LOG_MODULE, "Influence blender created (max=%u)", blender->influences_capacity);

    return blender;
}

void influence_blender_destroy(influence_blender_t* blender) {
    if (!blender) return;

    /* Free influence embeddings */
    for (uint32_t i = 0; i < blender->num_current_influences; i++) {
        style_embedding_destroy(&blender->current_influences[i].style);
    }

    nimcp_free(blender->current_influences);
    nimcp_free(blender);
    blender = NULL;

    LOG_INFO(LOG_MODULE, "Influence blender destroyed");
}

//=============================================================================
// Influence Management API
//=============================================================================

void influence_blender_clear(influence_blender_t* blender) {
    if (!blender) return;

    for (uint32_t i = 0; i < blender->num_current_influences; i++) {
        style_embedding_destroy(&blender->current_influences[i].style);
    }
    blender->num_current_influences = 0;
}

int influence_blender_add(influence_blender_t* blender,
                          const style_embedding_t* style,
                          float weight,
                          const char* source_work,
                          const char* source_artist) {
    if (!blender || !style) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "influence_blender_clear: required parameter is NULL (blender, style)");
        return -1;
    }

    if (blender->num_current_influences >= blender->influences_capacity) {
        LOG_WARN(LOG_MODULE, "Influence capacity reached");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "influence_blender_clear: capacity exceeded");
        return -1;
    }

    if (weight < blender->config.min_influence_weight) {
        LOG_DEBUG(LOG_MODULE, "Weight below minimum threshold");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "influence_blender_clear: validation failed");
        return -1;
    }

    creative_influence_t* inf = &blender->current_influences[blender->num_current_influences];
    memset(inf, 0, sizeof(creative_influence_t));

    style_embedding_clone(style, &inf->style);
    inf->weight = weight;
    inf->is_positive = true;

    if (source_work) {
        strncpy(inf->source_work, source_work, sizeof(inf->source_work) - 1);
    }
    if (source_artist) {
        strncpy(inf->source_artist, source_artist, sizeof(inf->source_artist) - 1);
    }

    blender->num_current_influences++;

    return 0;
}

int influence_blender_add_negative(influence_blender_t* blender,
                                    const style_embedding_t* style,
                                    float weight,
                                    const char* source_work) {
    if (!blender || !style) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "influence_blender_clear: required parameter is NULL (blender, style)");
        return -1;
    }

    if (blender->num_current_influences >= blender->influences_capacity) {
        LOG_WARN(LOG_MODULE, "Influence capacity reached");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "influence_blender_clear: capacity exceeded");
        return -1;
    }

    creative_influence_t* inf = &blender->current_influences[blender->num_current_influences];
    memset(inf, 0, sizeof(creative_influence_t));

    style_embedding_clone(style, &inf->style);
    inf->weight = weight;
    inf->is_positive = false;

    if (source_work) {
        strncpy(inf->source_work, source_work, sizeof(inf->source_work) - 1);
    }

    blender->num_current_influences++;

    return 0;
}

int influence_blender_add_archetype(influence_blender_t* blender,
                                     art_modality_t modality,
                                     int32_t archetype_id,
                                     float weight,
                                     bool is_positive) {
    if (!blender || !blender->style_repr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "influence_blender_clear: required parameter is NULL (blender, blender->style_repr)");
        return -1;
    }

    style_embedding_t emb;
    memset(&emb, 0, sizeof(emb));

    if (style_repr_get_archetype_embedding(blender->style_repr, modality,
                                            archetype_id, &emb) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "influence_blender_clear: required parameter is NULL (blender, blender->style_repr)");
        return -1;
    }

    int result = 0;
    if (is_positive) {
        result = influence_blender_add(blender, &emb, weight, NULL, NULL);
    } else {
        result = influence_blender_add_negative(blender, &emb, weight, NULL);
    }

    style_embedding_destroy(&emb);

    return result;
}

int influence_blender_remove(influence_blender_t* blender, uint32_t index) {
    if (!blender || index >= blender->num_current_influences) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "influence_blender_remove: blender is NULL");
        return -1;
    }

    style_embedding_destroy(&blender->current_influences[index].style);

    /* Shift remaining */
    memmove(&blender->current_influences[index],
            &blender->current_influences[index + 1],
            (blender->num_current_influences - index - 1) * sizeof(creative_influence_t));

    blender->num_current_influences--;

    return 0;
}

int influence_blender_set_weight(influence_blender_t* blender,
                                  uint32_t index, float new_weight) {
    if (!blender || index >= blender->num_current_influences) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "influence_blender_remove: blender is NULL");
        return -1;
    }

    blender->current_influences[index].weight = new_weight;

    return 0;
}

//=============================================================================
// Blending API
//=============================================================================

int influence_blender_blend(influence_blender_t* blender,
                            blend_mode_t mode,
                            influence_blend_result_t* result) {
    if (!blender || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "influence_blender_remove: required parameter is NULL (blender, result)");
        return -1;
    }

    return influence_blender_blend_explicit(blender,
                                             blender->current_influences,
                                             blender->num_current_influences,
                                             mode, result);
}

int influence_blender_blend_explicit(influence_blender_t* blender,
                                      const creative_influence_t* influences,
                                      uint32_t num_influences,
                                      blend_mode_t mode,
                                      influence_blend_result_t* result) {
    if (!blender || !influences || num_influences == 0 || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "influence_blender_remove: required parameter is NULL (blender, influences, result)");
        return -1;
    }

    memset(result, 0, sizeof(influence_blend_result_t));

    /* Use default mode if specified */
    if ((int)mode < 0) {
        mode = blender->config.default_mode;
    }

    int blend_result = 0;

    switch (mode) {
        case BLEND_MODE_LINEAR:
            blend_result = blend_linear(influences, num_influences, &result->style);
            break;
        case BLEND_MODE_SPHERICAL:
            blend_result = blend_spherical(influences, num_influences, &result->style);
            break;
        case BLEND_MODE_ADAPTIVE:
            blend_result = blend_adaptive(blender, influences, num_influences, &result->style);
            break;
        case BLEND_MODE_HIERARCHICAL:
            /* Fall back to linear for now */
            blend_result = blend_linear(influences, num_influences, &result->style);
            break;
        case BLEND_MODE_ADVERSARIAL:
            /* Fall back to adaptive for now */
            blend_result = blend_adaptive(blender, influences, num_influences, &result->style);
            break;
        default:
            blend_result = -1;
    }

    if (blend_result < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "influence_blender_remove: validation failed");
        return -1;
    }

    /* Compute metrics */
    result->coherence = compute_coherence(&result->style, influences, num_influences);
    result->originality = compute_originality(&result->style, influences, num_influences);
    result->num_influences = num_influences;

    /* Check constraints */
    result->is_valid = true;

    if (blender->config.constraints & BLEND_CONSTRAINT_COHERENCE) {
        if (result->coherence < blender->config.coherence_threshold) {
            result->is_valid = false;
            LOG_DEBUG(LOG_MODULE, "Blend failed coherence constraint (%.2f < %.2f)",
                      result->coherence, blender->config.coherence_threshold);
        }
    }

    if (blender->config.constraints & BLEND_CONSTRAINT_ORIGINALITY) {
        if (result->originality < blender->config.originality_threshold) {
            result->is_valid = false;
            LOG_DEBUG(LOG_MODULE, "Blend failed originality constraint (%.2f < %.2f)",
                      result->originality, blender->config.originality_threshold);
        }
    }

    blender->blends_performed++;
    if (isfinite(result->coherence)) {
        blender->avg_blend_coherence = blender->avg_blend_coherence * 0.95f +
                                       result->coherence * 0.05f;
    }

    return 0;
}

//=============================================================================
// Analysis API
//=============================================================================

int influence_blender_analyze(influence_blender_t* blender,
                               blend_analysis_t* analysis) {
    if (!blender || !analysis) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "influence_blender_remove: required parameter is NULL (blender, analysis)");
        return -1;
    }

    memset(analysis, 0, sizeof(blend_analysis_t));

    uint32_t n = blender->num_current_influences;
    if (n == 0) return 0;

    /* Compute pairwise compatibilities */
    uint32_t num_pairs = n * (n - 1) / 2;
    analysis->compatibilities = nimcp_calloc(num_pairs, sizeof(influence_compatibility_t));
    if (!analysis->compatibilities) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "influence_blender_remove: analysis->compatibilities is NULL");
        return -1;
    }

    uint32_t pair_idx = 0;
    float total_compat = 0.0f;
    float max_tension = 0.0f;

    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = i + 1; j < n; j++) {
            influence_compatibility_t* compat = &analysis->compatibilities[pair_idx];
            compat->influence_a_idx = (int32_t)i;
            compat->influence_b_idx = (int32_t)j;

            float sim = style_embedding_similarity(&blender->current_influences[i].style,
                                                    &blender->current_influences[j].style);
            compat->compatibility = (sim + 1.0f) / 2.0f;  /* Map [-1,1] to [0,1] */
            compat->tension = 1.0f - compat->compatibility;

            total_compat += compat->compatibility;
            if (compat->tension > max_tension) {
                max_tension = compat->tension;
            }

            if (compat->compatibility > 0.7f) {
                snprintf(compat->compatibility_reason, sizeof(compat->compatibility_reason),
                         "Highly compatible styles");
            } else if (compat->compatibility > 0.4f) {
                snprintf(compat->compatibility_reason, sizeof(compat->compatibility_reason),
                         "Moderately compatible styles");
            } else {
                snprintf(compat->compatibility_reason, sizeof(compat->compatibility_reason),
                         "Contrasting styles - creative tension");
            }

            pair_idx++;
        }
    }

    analysis->num_compatibilities = pair_idx;
    analysis->avg_compatibility = pair_idx > 0 ? total_compat / pair_idx : 0.0f;
    analysis->max_tension = max_tension;
    analysis->is_feasible = analysis->avg_compatibility > 0.3f || n <= 2;

    /* Generate suggested weights */
    analysis->suggested_weights = nimcp_calloc(n, sizeof(float));
    analysis->num_suggestions = n;

    if (analysis->suggested_weights) {
        float total_weight = 0.0f;
        for (uint32_t i = 0; i < n; i++) {
            /* Base weight on compatibility with others */
            float compat_score = 0.0f;
            for (uint32_t p = 0; p < pair_idx; p++) {
                if (analysis->compatibilities[p].influence_a_idx == (int32_t)i ||
                    analysis->compatibilities[p].influence_b_idx == (int32_t)i) {
                    compat_score += analysis->compatibilities[p].compatibility;
                }
            }
            analysis->suggested_weights[i] = 0.5f + compat_score * 0.1f;
            total_weight += analysis->suggested_weights[i];
        }

        /* Normalize */
        if (total_weight > 0.0f) {
            for (uint32_t i = 0; i < n; i++) {
                analysis->suggested_weights[i] /= total_weight;
            }
        }
    }

    snprintf(analysis->suggestion_rationale, sizeof(analysis->suggestion_rationale),
             "Weights based on pairwise compatibility analysis");

    return 0;
}

float influence_blender_compatibility(const influence_blender_t* blender,
                                       const style_embedding_t* style_a,
                                       const style_embedding_t* style_b) {
    (void)blender;
    float sim = style_embedding_similarity(style_a, style_b);
    return (sim + 1.0f) / 2.0f;  /* Map to [0,1] */
}

int influence_blender_optimize_weights(influence_blender_t* blender) {
    if (!blender || blender->num_current_influences == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "influence_blender_optimize_weights: blender is NULL");
        return -1;
    }

    /* Simple optimization: adjust weights to maximize coherence */
    uint32_t n = blender->num_current_influences;
    float* weights = nimcp_calloc(n, sizeof(float));
    if (!weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "influence_blender_optimize_weights: weights is NULL");
        return -1;
    }

    /* Initialize with current weights */
    for (uint32_t i = 0; i < n; i++) {
        weights[i] = blender->current_influences[i].weight;
    }

    /* Gradient descent to maximize coherence */
    style_embedding_t blend;
    memset(&blend, 0, sizeof(blend));

    for (uint32_t iter = 0; iter < blender->config.optimization_iterations; iter++) {
        /* Set weights */
        for (uint32_t i = 0; i < n; i++) {
            blender->current_influences[i].weight = weights[i];
        }

        /* Compute blend */
        style_embedding_destroy(&blend);
        blend_linear(blender->current_influences, n, &blend);
        float coherence = compute_coherence(&blend, blender->current_influences, n);

        /* Compute gradients (numerical) */
        float* gradients = nimcp_calloc(n, sizeof(float));
        if (!gradients) {
            style_embedding_destroy(&blend);
            nimcp_free(weights);
            return -1;
        }

        for (uint32_t i = 0; i < n; i++) {
            float orig = weights[i];
            float eps = 0.01f;

            weights[i] = orig + eps;
            for (uint32_t j = 0; j < n; j++) {
                blender->current_influences[j].weight = weights[j];
            }
            style_embedding_destroy(&blend);
            blend_linear(blender->current_influences, n, &blend);
            float coh_plus = compute_coherence(&blend, blender->current_influences, n);

            weights[i] = orig - eps;
            for (uint32_t j = 0; j < n; j++) {
                blender->current_influences[j].weight = weights[j];
            }
            style_embedding_destroy(&blend);
            blend_linear(blender->current_influences, n, &blend);
            float coh_minus = compute_coherence(&blend, blender->current_influences, n);

            gradients[i] = (coh_plus - coh_minus) / (2.0f * eps);
            weights[i] = orig;
        }

        /* Update weights */
        for (uint32_t i = 0; i < n; i++) {
            weights[i] += blender->config.learning_rate * gradients[i];
            if (weights[i] < blender->config.min_influence_weight) {
                weights[i] = blender->config.min_influence_weight;
            }
        }

        /* Normalize */
        float total = 0.0f;
        for (uint32_t i = 0; i < n; i++) {
            if (blender->current_influences[i].is_positive) {
                total += weights[i];
            }
        }
        if (total > 0.0f) {
            for (uint32_t i = 0; i < n; i++) {
                if (blender->current_influences[i].is_positive) {
                    weights[i] /= total;
                }
            }
        }

        nimcp_free(gradients);
        gradients = NULL;
        (void)coherence;
    }

    /* Apply final weights */
    for (uint32_t i = 0; i < n; i++) {
        blender->current_influences[i].weight = weights[i];
    }

    nimcp_free(weights);
    weights = NULL;
    style_embedding_destroy(&blend);

    return 0;
}

//=============================================================================
// Preset Blends API
//=============================================================================

int influence_blender_homage(influence_blender_t* blender,
                              const style_embedding_t* primary,
                              const style_embedding_t* accent,
                              influence_blend_result_t* result) {
    if (!blender || !primary || !accent || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "influence_blender_optimize_weights: required parameter is NULL (blender, primary, accent, result)");
        return -1;
    }

    influence_blender_clear(blender);
    influence_blender_add(blender, primary, 0.8f, "primary", NULL);
    influence_blender_add(blender, accent, 0.2f, "accent", NULL);

    return influence_blender_blend(blender, BLEND_MODE_LINEAR, result);
}

int influence_blender_fusion(influence_blender_t* blender,
                              const style_embedding_t* styles,
                              uint32_t num_styles,
                              influence_blend_result_t* result) {
    if (!blender || !styles || num_styles == 0 || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "influence_blender_optimize_weights: required parameter is NULL (blender, styles, result)");
        return -1;
    }

    influence_blender_clear(blender);

    float equal_weight = 1.0f / num_styles;
    for (uint32_t i = 0; i < num_styles; i++) {
        influence_blender_add(blender, &styles[i], equal_weight, NULL, NULL);
    }

    return influence_blender_blend(blender, BLEND_MODE_SPHERICAL, result);
}

int influence_blender_contrast(influence_blender_t* blender,
                                const style_embedding_t* toward,
                                const style_embedding_t* away,
                                influence_blend_result_t* result) {
    if (!blender || !toward || !away || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "influence_blender_optimize_weights: required parameter is NULL (blender, toward, away, result)");
        return -1;
    }

    influence_blender_clear(blender);
    influence_blender_add(blender, toward, 1.0f, "target", NULL);
    influence_blender_add_negative(blender, away, 0.5f, "avoid");

    return influence_blender_blend(blender, BLEND_MODE_LINEAR, result);
}

//=============================================================================
// Iteration API
//=============================================================================

int influence_blender_refine(influence_blender_t* blender,
                              const influence_blend_result_t* current,
                              float coherence_target,
                              uint32_t iterations,
                              influence_blend_result_t* result) {
    if (!blender || !current || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "influence_blender_optimize_weights: required parameter is NULL (blender, current, result)");
        return -1;
    }

    /* Copy current result - deep copy to avoid use-after-free from shallow copy */
    *result = *current;
    /* Deep-clone embeddings */
    style_embedding_clone(&current->blended_style, &result->blended_style);
    style_embedding_clone(&current->style, &result->style);
    /* Deep-clone influence_weights pointer to avoid dangling reference */
    if (current->influence_weights && current->num_influences > 0) {
        result->influence_weights = nimcp_calloc(current->num_influences, sizeof(float));
        if (result->influence_weights) {
            memcpy(result->influence_weights, current->influence_weights,
                   current->num_influences * sizeof(float));
        }
    } else {
        result->influence_weights = NULL;
    }

    if (current->coherence >= coherence_target) {
        return 0;  /* Already meets target */
    }

    /* Iteratively blend toward higher coherence */
    for (uint32_t iter = 0; iter < iterations; iter++) {
        /* Re-blend with slightly adjusted weights favoring compatible influences */
        influence_blend_result_t new_result;
        memset(&new_result, 0, sizeof(new_result));

        if (influence_blender_blend(blender, BLEND_MODE_ADAPTIVE, &new_result) < 0) {
            break;
        }

        if (new_result.coherence_score > result->coherence_score) {
            style_embedding_destroy(&result->blended_style);
            style_embedding_destroy(&result->style);
            nimcp_free(result->influence_weights);
            *result = new_result;

            if (result->coherence_score >= coherence_target) {
                break;
            }
        } else {
            creative_blend_result_free(&new_result);
        }
    }

    return 0;
}

uint32_t influence_blender_variations(influence_blender_t* blender,
                                       const influence_blend_result_t* base,
                                       uint32_t num_variations,
                                       float variation_strength,
                                       influence_blend_result_t* results) {
    if (!blender || !base || !results) return 0;

    uint32_t generated = 0;

    for (uint32_t v = 0; v < num_variations; v++) {
        /* Create variation by perturbing the base */
        style_embedding_t varied;
        style_embedding_clone(&base->blended_style, &varied);

        /* Add random perturbation */
        uint32_t seed = v * 12345 + 67890;
        for (uint32_t d = 0; d < varied.embedding_dim; d++) {
            seed = seed * 1103515245 + 12345;
            float noise = ((float)(seed % 1000) / 500.0f - 1.0f) * variation_strength;
            varied.embedding[d] += noise;
        }
        style_embedding_normalize(&varied);

        /* Create result */
        memset(&results[generated], 0, sizeof(influence_blend_result_t));
        results[generated].blended_style = varied;
        results[generated].coherence_score = compute_coherence(&varied,
                                                          blender->current_influences,
                                                          blender->num_current_influences);
        results[generated].novelty_score = compute_originality(&varied,
                                                              blender->current_influences,
                                                              blender->num_current_influences);
        results[generated].num_influences = blender->num_current_influences;

        generated++;
    }

    return generated;
}

//=============================================================================
// Cleanup
//=============================================================================

void blend_analysis_free(blend_analysis_t* analysis) {
    if (!analysis) return;

    if (analysis->compatibilities) {
        nimcp_free(analysis->compatibilities);
        analysis->compatibilities = NULL;
    }

    if (analysis->suggested_weights) {
        nimcp_free(analysis->suggested_weights);
        analysis->suggested_weights = NULL;
    }

    analysis->num_compatibilities = 0;
    analysis->num_suggestions = 0;
}
