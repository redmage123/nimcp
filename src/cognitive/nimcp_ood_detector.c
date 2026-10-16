/**
 * @file nimcp_ood_detector.c
 * @brief Out-of-Distribution (OOD) detection system
 *
 * Combines 4 signals into a single OOD score:
 *   1. Memory distance   — query persistent store for similar past inputs
 *   2. Energy score       — high energy from output layer = uncertain
 *   3. Network disagreement — adaptive vs SNN output divergence
 *   4. VAE reconstruction error — poor reconstruction = novel input
 *
 * Combined: ood_score = w1*mem_dist + w2*energy + w3*disagree + w4*recon
 * If ood_score > threshold -> flag as OOD.
 */

#include "cognitive/nimcp_ood_detector.h"
#include "memory/nimcp_memory_store.h"
#include "utils/memory/nimcp_memory.h"

#include <math.h>
#include <string.h>

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct nimcp_ood_detector {
    nimcp_ood_config_t config;
    nimcp_ood_stats_t stats;
};

/* ============================================================================
 * Configuration
 * ============================================================================ */

nimcp_ood_config_t nimcp_ood_config_default(void)
{
    nimcp_ood_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.memory_distance_weight  = 0.3f;
    cfg.energy_score_weight     = 0.25f;
    cfg.disagreement_weight     = 0.25f;
    cfg.reconstruction_weight   = 0.2f;
    cfg.ood_threshold           = 0.7f;
    cfg.confidence_reduction    = 0.5f;
    cfg.feature_dim             = 0;
    cfg.enable_memory_check     = true;
    cfg.enable_energy_score     = true;
    cfg.enable_disagreement     = true;
    cfg.enable_reconstruction   = true;
    cfg.enable_bloom_precheck   = true;
    return cfg;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

nimcp_ood_detector_t* nimcp_ood_detector_create(const nimcp_ood_config_t* config)
{
    nimcp_ood_detector_t* det = (nimcp_ood_detector_t*)nimcp_calloc(1, sizeof(nimcp_ood_detector_t));
    if (!det) {
        return NULL;
    }
    if (config) {
        det->config = *config;
    } else {
        det->config = nimcp_ood_config_default();
    }
    memset(&det->stats, 0, sizeof(det->stats));
    return det;
}

void nimcp_ood_detector_destroy(nimcp_ood_detector_t* detector)
{
    if (detector) {
        nimcp_free(detector);
    }
}

/* ============================================================================
 * Signal Computation Functions
 * ============================================================================ */

float nimcp_ood_energy_score(const float* logits, uint32_t dim)
{
    if (!logits || dim == 0) {
        return 0.0f;
    }

    /* Find max for numerical stability (log-sum-exp trick) */
    float max_val = logits[0];
    for (uint32_t i = 1; i < dim; i++) {
        if (logits[i] > max_val) {
            max_val = logits[i];
        }
    }

    float sum_exp = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        sum_exp += expf(logits[i] - max_val);
    }

    if (sum_exp <= 0.0f) {
        return 100.0f;  /* Very high energy = very uncertain */
    }

    /* Negative log-sum-exp: in-distribution = low energy, OOD = high energy */
    return -(max_val + logf(sum_exp));
}

float nimcp_ood_disagreement_score(const float* output_a, const float* output_b, uint32_t dim)
{
    if (!output_a || !output_b || dim == 0) {
        return 0.0f;
    }

    /* Cosine similarity: dot(a,b) / (||a|| * ||b||)
     * Disagreement = 1 - cosine_similarity */
    float dot = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;

    for (uint32_t i = 0; i < dim; i++) {
        dot    += output_a[i] * output_b[i];
        norm_a += output_a[i] * output_a[i];
        norm_b += output_b[i] * output_b[i];
    }

    float denom = sqrtf(norm_a) * sqrtf(norm_b);
    if (denom < 1e-12f) {
        return 1.0f;  /* Zero vectors = maximum disagreement */
    }

    float cosine = dot / denom;
    /* Clamp to [-1, 1] for numerical safety */
    if (cosine > 1.0f) cosine = 1.0f;
    if (cosine < -1.0f) cosine = -1.0f;

    /* Map from [-1, 1] to [0, 1]: disagreement = (1 - cosine) / 2
     * Identical vectors → 0, orthogonal → 0.5, opposite → 1.0 */
    return (1.0f - cosine) * 0.5f;
}

float nimcp_ood_reconstruction_error(const float* input, const float* reconstruction, uint32_t dim)
{
    if (!input || !reconstruction || dim == 0) {
        return 0.0f;
    }

    /* Mean squared error */
    float mse = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        float diff = input[i] - reconstruction[i];
        mse += diff * diff;
    }
    mse /= (float)dim;

    return mse;
}

/* ============================================================================
 * Core Detection
 * ============================================================================ */

int nimcp_ood_detect(
    nimcp_ood_detector_t* detector,
    const float* features, uint32_t feature_dim,
    const float* output_logits, uint32_t output_dim,
    const float* secondary_output, uint32_t secondary_dim,
    const float* reconstruction, uint32_t recon_dim,
    void* memory_store,
    nimcp_ood_result_t* result)
{
    if (!detector || !result) {
        return -1;
    }

    /* Initialize result */
    memset(result, 0, sizeof(nimcp_ood_result_t));
    result->confidence_adjustment = 1.0f;

    if (!features || feature_dim == 0) {
        return -1;
    }

    const nimcp_ood_config_t* cfg = &detector->config;

    float memory_dist   = 0.0f;
    float energy_norm   = 0.0f;
    float disagreement  = 0.0f;
    float recon_norm    = 0.0f;
    float total_weight  = 0.0f;

    /* --- Signal 1: Memory distance --- */
    if (memory_store && cfg->enable_memory_check) {
        nimcp_memory_store_t* store = (nimcp_memory_store_t*)memory_store;
        bool bloom_miss = false;

        /* Optional bloom filter pre-check for fast rejection */
        if (cfg->enable_bloom_precheck) {
            if (!nimcp_memory_store_bloom_check(store, features, feature_dim)) {
                /* Bloom says definitely unseen */
                bloom_miss = true;
                memory_dist = 1.0f;
            }
        }

        if (!bloom_miss) {
            nimcp_memory_search_result_t* sr =
                nimcp_memory_store_engram_search_similar(
                    store, features, feature_dim, 1, 0.0f);

            if (sr && sr->count > 0) {
                memory_dist = sr->distances[0];
                /* Clamp to [0, 1] */
                if (memory_dist < 0.0f) memory_dist = 0.0f;
                if (memory_dist > 1.0f) memory_dist = 1.0f;
                result->nearest_memory_id = sr->ids[0];
            } else {
                memory_dist = 1.0f;  /* Completely novel */
            }

            if (sr) {
                nimcp_memory_search_result_destroy(sr);
            }
        }

        total_weight += cfg->memory_distance_weight;
    }

    /* --- Signal 2: Energy score --- */
    if (output_logits && output_dim > 0 && cfg->enable_energy_score) {
        float energy = nimcp_ood_energy_score(output_logits, output_dim);
        /* Normalize to [0, 1] via sigmoid-like mapping:
         * In-distribution: energy < 0 (confident) → energy_norm near 0
         * OOD: energy > 0 (uncertain) → energy_norm near 1 */
        energy_norm = 1.0f - 1.0f / (1.0f + energy);
        /* Clamp for safety */
        if (energy_norm < 0.0f) energy_norm = 0.0f;
        if (energy_norm > 1.0f) energy_norm = 1.0f;
        total_weight += cfg->energy_score_weight;
    }

    /* --- Signal 3: Network disagreement --- */
    if (output_logits && secondary_output && cfg->enable_disagreement) {
        uint32_t cmp_dim = output_dim < secondary_dim ? output_dim : secondary_dim;
        if (cmp_dim > 0) {
            disagreement = nimcp_ood_disagreement_score(
                output_logits, secondary_output, cmp_dim);
            total_weight += cfg->disagreement_weight;
        }
    }

    /* --- Signal 4: VAE reconstruction error --- */
    if (reconstruction && recon_dim > 0 && cfg->enable_reconstruction) {
        uint32_t cmp_dim = feature_dim < recon_dim ? feature_dim : recon_dim;
        float raw_error = nimcp_ood_reconstruction_error(features, reconstruction, cmp_dim);
        /* Scale so typical errors map to 0-1 range */
        recon_norm = fminf(raw_error * 10.0f, 1.0f);
        total_weight += cfg->reconstruction_weight;
    }

    /* --- Combine signals --- */
    float ood_score = 0.0f;
    if (total_weight > 1e-7f) {
        if (memory_store && cfg->enable_memory_check) {
            ood_score += cfg->memory_distance_weight * memory_dist;
        }
        if (output_logits && output_dim > 0 && cfg->enable_energy_score) {
            ood_score += cfg->energy_score_weight * energy_norm;
        }
        if (output_logits && secondary_output && cfg->enable_disagreement) {
            uint32_t cmp_dim = output_dim < secondary_dim ? output_dim : secondary_dim;
            if (cmp_dim > 0) {
                ood_score += cfg->disagreement_weight * disagreement;
            }
        }
        if (reconstruction && recon_dim > 0 && cfg->enable_reconstruction) {
            ood_score += cfg->reconstruction_weight * recon_norm;
        }
        /* Normalize by total active weight so score stays in [0, 1] */
        ood_score /= total_weight;
    }

    /* --- Fill result --- */
    result->ood_score             = ood_score;
    result->is_ood                = (ood_score > cfg->ood_threshold);
    result->memory_distance       = memory_dist;
    result->energy_score          = energy_norm;
    result->disagreement_score    = disagreement;
    result->reconstruction_error  = recon_norm;
    result->confidence_adjustment = result->is_ood ? cfg->confidence_reduction : 1.0f;

    return 0;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

int nimcp_ood_update_stats(nimcp_ood_detector_t* detector, const nimcp_ood_result_t* result)
{
    if (!detector || !result) {
        return -1;
    }

    nimcp_ood_stats_t* s = &detector->stats;
    s->total_checks++;

    if (result->is_ood) {
        s->ood_detected++;
    } else {
        s->in_distribution++;
    }

    /* Running average: avg = avg + (new - avg) / n */
    s->avg_ood_score += (result->ood_score - s->avg_ood_score) / (float)s->total_checks;

    if (result->ood_score > s->max_ood_score) {
        s->max_ood_score = result->ood_score;
    }

    s->ood_rate = (float)s->ood_detected / (float)s->total_checks;

    return 0;
}

int nimcp_ood_get_stats(const nimcp_ood_detector_t* detector, nimcp_ood_stats_t* stats)
{
    if (!detector || !stats) {
        return -1;
    }
    *stats = detector->stats;
    return 0;
}
