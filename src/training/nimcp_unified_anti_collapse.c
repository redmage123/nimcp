/**
 * @file nimcp_unified_anti_collapse.c
 * @brief Unified anti-collapse infrastructure — diversity loss + gradient normalization
 *
 * WHAT: Single implementation of anti-mode-collapse logic shared by all training paths
 * WHY:  Previously copy-pasted across CNN, SNN, LNN, and Adaptive trainers
 * HOW:  Ring buffer of recent outputs, cosine similarity penalty, gradient norm/clip
 *
 * @author NIMCP Development Team
 * @date 2026-03-11
 */

#include "training/nimcp_unified_training.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <math.h>
#include <string.h>

//=============================================================================
// Anti-Collapse Lifecycle
//=============================================================================

int nimcp_anti_collapse_init(nimcp_anti_collapse_state_t* state,
                             const nimcp_anti_collapse_config_t* config) {
    if (!state) return -1;

    memset(state, 0, sizeof(nimcp_anti_collapse_state_t));

    if (config) {
        state->config = *config;
    } else {
        /* Defaults */
        state->config.diversity_loss_weight = NIMCP_UTM_DEFAULT_DIVERSITY_WEIGHT;
        state->config.diversity_buffer_size = NIMCP_UTM_DEFAULT_DIVERSITY_BUFFER;
        state->config.use_gradient_normalization = true;
        state->config.gradient_target_norm = NIMCP_UTM_DEFAULT_GRADIENT_TARGET;
        state->config.gradient_clip_value = 5.0f;
        state->config.adaptive_gradient_target = true;
    }

    if (state->config.diversity_buffer_size == 0) {
        state->config.diversity_buffer_size = NIMCP_UTM_DEFAULT_DIVERSITY_BUFFER;
    }

    /* Adaptive gradient target EMA state */
    state->ema_gradient_norm = 0.0f;
    state->ema_alpha = 0.01f;

    /* Buffer is lazily allocated on first use (when output_dim is known) */
    return 0;
}

void nimcp_anti_collapse_destroy(nimcp_anti_collapse_state_t* state) {
    if (!state) return;
    nimcp_free(state->diversity_buffer);
    state->diversity_buffer = NULL;
    state->buffer_pos = 0;
    state->buffer_count = 0;
    state->output_dim = 0;
}

//=============================================================================
// Diversity Loss
//=============================================================================

float nimcp_anti_collapse_diversity_loss(nimcp_anti_collapse_state_t* state,
                                          const float* output,
                                          float* grad_output,
                                          uint32_t dim) {
    if (!state || !output || dim == 0) return 0.0f;
    if (state->config.diversity_loss_weight <= 0.0f) return 0.0f;

    uint32_t buf_size = state->config.diversity_buffer_size;

    /* Lazy-init or resize buffer */
    if (!state->diversity_buffer || state->output_dim != dim) {
        nimcp_free(state->diversity_buffer);
        state->diversity_buffer = (float*)nimcp_calloc(
            (size_t)buf_size * dim, sizeof(float));
        if (!state->diversity_buffer) {
            NIMCP_LOGGING_WARN("anti_collapse: Failed to allocate diversity buffer");
            return 0.0f;
        }
        state->buffer_pos = 0;
        state->buffer_count = 0;
        state->output_dim = dim;
    }

    float diversity_loss = 0.0f;
    float weight = state->config.diversity_loss_weight;

    /* Compute diversity gradient if buffer has entries */
    if (state->buffer_count > 0 && grad_output) {
        float avg_sim = 0.0f;
        uint32_t count = state->buffer_count;

        for (uint32_t b = 0; b < count; b++) {
            const float* prev = &state->diversity_buffer[(size_t)b * dim];

            /* Cosine similarity: dot / (||cur|| * ||prev||) */
            float dot = 0.0f, norm_cur_sq = 0.0f, norm_prev_sq = 0.0f;
            for (uint32_t j = 0; j < dim; j++) {
                dot += output[j] * prev[j];
                norm_cur_sq += output[j] * output[j];
                norm_prev_sq += prev[j] * prev[j];
            }

            float norm_cur = sqrtf(norm_cur_sq);
            float norm_prev = sqrtf(norm_prev_sq);
            float denom = norm_cur * norm_prev;

            if (denom > 1e-8f) {
                float sim = dot / denom;
                avg_sim += sim;

                /* Gradient of cosine similarity w.r.t. output */
                float inv_nc = 1.0f / (norm_cur + 1e-8f);
                float inv_np = 1.0f / (norm_prev + 1e-8f);
                for (uint32_t j = 0; j < dim; j++) {
                    float d_sim = (prev[j] * inv_nc * inv_np
                                   - sim * output[j] * inv_nc * inv_nc)
                                  / (float)count;
                    grad_output[j] += weight * d_sim;
                }
            }
        }

        diversity_loss = (count > 0) ? (avg_sim / (float)count) * weight : 0.0f;
    }

    /* Store current output in ring buffer */
    memcpy(&state->diversity_buffer[(size_t)state->buffer_pos * dim],
           output, (size_t)dim * sizeof(float));
    state->buffer_pos = (state->buffer_pos + 1) % buf_size;
    if (state->buffer_count < buf_size) {
        state->buffer_count++;
    }

    return diversity_loss;
}

//=============================================================================
// Gradient Normalization / Clipping
//=============================================================================

float nimcp_anti_collapse_normalize_gradients(
    nimcp_anti_collapse_state_t* state,
    float** gradients, const size_t* sizes, uint32_t num_arrays) {

    if (!state || !gradients || !sizes || num_arrays == 0) return 1.0f;
    const nimcp_anti_collapse_config_t* config = &state->config;

    /* Compute global gradient norm across all arrays */
    double global_norm_sq = 0.0;
    for (uint32_t a = 0; a < num_arrays; a++) {
        if (!gradients[a]) continue;
        for (size_t j = 0; j < sizes[a]; j++) {
            double g = (double)gradients[a][j];
            global_norm_sq += g * g;
        }
    }
    float global_norm = (float)sqrt(global_norm_sq);

    if (global_norm <= 1e-8f) return 1.0f;

    /* Update EMA of gradient norms for adaptive target */
    if (config->adaptive_gradient_target) {
        if (state->ema_gradient_norm <= 0.0f) {
            /* Bootstrap: first step sets EMA directly.
             * Guard against NaN/Inf poisoning the EMA permanently. */
            state->ema_gradient_norm = isfinite(global_norm) ? global_norm : 1.0f;
        } else if (!isfinite(global_norm)) {
            /* Skip EMA update on NaN/Inf gradient norms */
        } else {
            state->ema_gradient_norm = state->ema_alpha * global_norm
                                     + (1.0f - state->ema_alpha) * state->ema_gradient_norm;
        }
        /* Clamp EMA to reasonable range */
        if (state->ema_gradient_norm < 1.0f) state->ema_gradient_norm = 1.0f;
        if (state->ema_gradient_norm > 1e6f) state->ema_gradient_norm = 1e6f;
    }

    float scale = 1.0f;
    if (config->use_gradient_normalization) {
        /* Determine target norm: adaptive (sqrt of EMA) or fixed */
        float target = config->gradient_target_norm;
        if (config->adaptive_gradient_target && target <= 0.0f) {
            target = sqrtf(state->ema_gradient_norm);
        }
        if (target <= 0.0f) target = 1.0f;  /* Safety fallback */
        scale = target / global_norm;
    } else if (global_norm > config->gradient_clip_value) {
        /* Legacy clipping: only scale down when above threshold */
        scale = config->gradient_clip_value / global_norm;
    }

    if (scale != 1.0f) {
        for (uint32_t a = 0; a < num_arrays; a++) {
            if (!gradients[a]) continue;
            for (size_t j = 0; j < sizes[a]; j++) {
                gradients[a][j] *= scale;
            }
        }
    }

    return scale;
}
