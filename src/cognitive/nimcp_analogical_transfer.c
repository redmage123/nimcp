/**
 * @file nimcp_analogical_transfer.c
 * @brief Analogical Transfer Engine — structural similarity search for novel problems.
 *
 * WHAT: Stores problem-solution pairs and retrieves structurally similar solutions
 *       for novel problems via cosine similarity on feature vectors.
 * WHY:  "This is LIKE something I've seen before" — a core human reasoning ability.
 * HOW:  Circular buffer of patterns, cosine search, weighted blending.
 *
 * DEPENDENCIES: None (standalone cognitive module)
 * TRAINING_IMPACT: None (inference-only pattern matching)
 *
 * @author Claude Code
 * @date 2026-03
 */

#define LOG_MODULE "analogical_transfer"

#include "cognitive/nimcp_analogical_transfer.h"
#include "utils/memory/nimcp_memory_guards.h"
#include "utils/logging/nimcp_logging.h"
#include <string.h>
#include <math.h>

/* W13: analogy events emitted from the brain-side callers (nimcp_brain_part_core.c
 * and nimcp_brain_learning.c). This module is brain-pointerless by design
 * (pure helper) so we do not include the W13 header here to keep the module
 * dependency footprint unchanged. */

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct nimcp_analogical_transfer {
    nimcp_analogical_config_t config;
    nimcp_analogical_pattern_t* patterns;   /**< Pattern buffer */
    uint32_t buffer_count;                  /**< Number of patterns stored */
    uint32_t buffer_capacity;               /**< Max patterns (from config) */
};

/* ============================================================================
 * Helpers
 * ============================================================================ */

/**
 * @brief Compute cosine similarity between two vectors.
 *
 * Uses the minimum of the two dimensions to handle mismatched sizes.
 *
 * @return Cosine similarity in [-1, 1], or 0.0 on degenerate input.
 */
static float cosine_similarity(const float* a, uint32_t a_dim,
                                const float* b, uint32_t b_dim)
{
    uint32_t dim = (a_dim < b_dim) ? a_dim : b_dim;
    if (dim == 0) { return 0.0f; }

    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        dot    += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    float denom = sqrtf(norm_a) * sqrtf(norm_b);
    if (denom < 1e-12f) { return 0.0f; }

    return dot / denom;
}

/**
 * @brief Find the index of the pattern with the lowest success_score.
 */
static uint32_t find_worst_pattern(const nimcp_analogical_transfer_t* handle)
{
    if (handle->buffer_count == 0) return 0;

    uint32_t worst = 0;
    float worst_score = handle->patterns[0].success_score;

    for (uint32_t i = 1; i < handle->buffer_count; i++) {
        if (handle->patterns[i].success_score < worst_score) {
            worst_score = handle->patterns[i].success_score;
            worst = i;
        }
    }

    return worst;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

nimcp_analogical_config_t nimcp_analogical_config_default(void)
{
    nimcp_analogical_config_t cfg;
    cfg.max_analogies        = 100;
    cfg.similarity_threshold = 0.6f;
    cfg.transfer_weight      = 0.3f;
    return cfg;
}

nimcp_analogical_transfer_t* nimcp_analogical_create(const nimcp_analogical_config_t* config)
{
    nimcp_analogical_config_t cfg = config ? *config : nimcp_analogical_config_default();

    if (cfg.max_analogies == 0) {
        LOG_ERROR("max_analogies must be > 0");
        return NULL;
    }

    nimcp_analogical_transfer_t* handle = nimcp_calloc(1, sizeof(*handle));
    if (!handle) {
        LOG_ERROR("Failed to allocate analogical transfer engine");
        return NULL;
    }

    handle->patterns = nimcp_calloc(cfg.max_analogies, sizeof(nimcp_analogical_pattern_t));
    if (!handle->patterns) {
        LOG_ERROR("Failed to allocate pattern buffer (%u patterns)", cfg.max_analogies);
        nimcp_free(handle);
        return NULL;
    }

    handle->config          = cfg;
    handle->buffer_count    = 0;
    handle->buffer_capacity = cfg.max_analogies;

    LOG_INFO("Analogical transfer engine created: capacity=%u, threshold=%.2f, weight=%.2f",
             cfg.max_analogies, cfg.similarity_threshold, cfg.transfer_weight);

    return handle;
}

void nimcp_analogical_destroy(nimcp_analogical_transfer_t* handle)
{
    if (!handle) { return; }

    nimcp_free(handle->patterns);
    nimcp_free(handle);

    LOG_DEBUG("Analogical transfer engine destroyed");
}

/* ============================================================================
 * Pattern Storage
 * ============================================================================ */

int nimcp_analogical_store_pattern(nimcp_analogical_transfer_t* handle,
                                   const float* features, uint32_t feat_dim,
                                   const float* solution, uint32_t sol_dim,
                                   const char* label, float success_score)
{
    if (!handle)   { return -1; }
    if (!features) { return -1; }
    if (!solution) { return -1; }
    if (feat_dim == 0 || feat_dim > ANALOGICAL_MAX_FEATURES) { return -1; }
    if (sol_dim == 0 || sol_dim > ANALOGICAL_MAX_SOLUTION)   { return -1; }

    uint32_t slot;

    if (handle->buffer_count < handle->buffer_capacity) {
        /* Space available — use next slot */
        slot = handle->buffer_count;
        handle->buffer_count++;
    } else {
        /* Full — evict worst pattern */
        slot = find_worst_pattern(handle);
        LOG_DEBUG("Evicting pattern '%s' (score=%.3f) for '%s'",
                  handle->patterns[slot].label, handle->patterns[slot].success_score,
                  label ? label : "");
    }

    nimcp_analogical_pattern_t* p = &handle->patterns[slot];
    memset(p, 0, sizeof(*p));

    memcpy(p->features, features, feat_dim * sizeof(float));
    p->feat_dim = feat_dim;

    memcpy(p->solution, solution, sol_dim * sizeof(float));
    p->sol_dim = sol_dim;

    if (label) {
        strncpy(p->label, label, ANALOGICAL_MAX_LABEL - 1);
        p->label[ANALOGICAL_MAX_LABEL - 1] = '\0';
    }

    p->success_score = success_score;

    LOG_DEBUG("Stored pattern '%s': feat_dim=%u, sol_dim=%u, score=%.3f",
              p->label, feat_dim, sol_dim, success_score);

    return 0;
}

/* ============================================================================
 * Analogy Search
 * ============================================================================ */

float nimcp_analogical_find_analogy(nimcp_analogical_transfer_t* handle,
                                    const float* query_features, uint32_t feat_dim,
                                    float* best_solution_out, uint32_t* sol_dim,
                                    char* best_label_out)
{
    if (!handle)         { return 0.0f; }
    if (!query_features) { return 0.0f; }
    if (handle->buffer_count == 0) { return 0.0f; }

    float best_sim = -1.0f;
    int   best_idx = -1;

    for (uint32_t i = 0; i < handle->buffer_count; i++) {
        float sim = cosine_similarity(query_features, feat_dim,
                                       handle->patterns[i].features,
                                       handle->patterns[i].feat_dim);

        /* Weight similarity by success_score to prefer proven solutions */
        float weighted = sim * handle->patterns[i].success_score;

        if (weighted > best_sim) {
            best_sim = weighted;
            best_idx = (int)i;
        }
    }

    /* Check raw similarity (not weighted) against threshold */
    if (best_idx < 0) { return 0.0f; }

    float raw_sim = cosine_similarity(query_features, feat_dim,
                                       handle->patterns[best_idx].features,
                                       handle->patterns[best_idx].feat_dim);

    if (raw_sim < handle->config.similarity_threshold) { return 0.0f; }

    /* Copy best solution */
    const nimcp_analogical_pattern_t* best = &handle->patterns[best_idx];

    if (best_solution_out) {
        memcpy(best_solution_out, best->solution, best->sol_dim * sizeof(float));
    }
    if (sol_dim) {
        *sol_dim = best->sol_dim;
    }
    if (best_label_out) {
        strncpy(best_label_out, best->label, ANALOGICAL_MAX_LABEL - 1);
        best_label_out[ANALOGICAL_MAX_LABEL - 1] = '\0';
    }

    LOG_DEBUG("Found analogy '%s': similarity=%.3f, success=%.3f",
              best->label, raw_sim, best->success_score);

    return raw_sim;
}

/* ============================================================================
 * Transfer Application
 * ============================================================================ */

int nimcp_analogical_apply_transfer(nimcp_analogical_transfer_t* handle,
                                    const float* query_features, uint32_t feat_dim,
                                    const float* brain_output, uint32_t output_dim,
                                    float* blended_output)
{
    if (!handle)         { return 0; }
    if (!brain_output)   { return 0; }
    if (!blended_output) { return 0; }
    if (output_dim == 0) { return 0; }

    /* Default: copy brain output unmodified */
    memcpy(blended_output, brain_output, output_dim * sizeof(float));

    /* Search for analogy */
    float analogy_solution[ANALOGICAL_MAX_SOLUTION];
    uint32_t analogy_dim = 0;
    char analogy_label[ANALOGICAL_MAX_LABEL];

    float sim = nimcp_analogical_find_analogy(handle, query_features, feat_dim,
                                              analogy_solution, &analogy_dim,
                                              analogy_label);

    if (sim < handle->config.similarity_threshold) { return 0; }

    /* Blend: output = (1-w)*brain + w*analogy */
    float w = handle->config.transfer_weight;
    uint32_t blend_dim = (output_dim < analogy_dim) ? output_dim : analogy_dim;

    for (uint32_t i = 0; i < blend_dim; i++) {
        blended_output[i] = (1.0f - w) * brain_output[i] + w * analogy_solution[i];
    }

    LOG_DEBUG("Applied transfer from '%s': sim=%.3f, weight=%.2f, blend_dim=%u",
              analogy_label, sim, w, blend_dim);

    return 1;
}

/* ============================================================================
 * Outcome Recording
 * ============================================================================ */

int nimcp_analogical_record_outcome(nimcp_analogical_transfer_t* handle,
                                    const char* label, float success)
{
    if (!handle) { return -1; }
    if (!label)  { return -1; }

    for (uint32_t i = 0; i < handle->buffer_count; i++) {
        if (strncmp(handle->patterns[i].label, label, ANALOGICAL_MAX_LABEL) == 0) {
            /* Exponential moving average */
            handle->patterns[i].success_score =
                0.8f * handle->patterns[i].success_score + 0.2f * success;

            LOG_DEBUG("Updated outcome for '%s': new score=%.3f",
                      label, handle->patterns[i].success_score);
            return 0;
        }
    }

    LOG_WARN("Pattern '%s' not found for outcome update", label);
    return -1;
}

/* ============================================================================
 * Query
 * ============================================================================ */

uint32_t nimcp_analogical_get_pattern_count(const nimcp_analogical_transfer_t* handle)
{
    if (!handle) { return 0; }
    return handle->buffer_count;
}
