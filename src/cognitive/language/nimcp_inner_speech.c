/**
 * @file nimcp_inner_speech.c
 * @brief Inner speech loop — iterative self-refinement via generate/re-encode
 *
 * WHAT: Brain generates text from output, re-encodes it, blends, repeats
 * WHY:  Internal rehearsal refines outputs before commitment (Vygotsky)
 * HOW:  Loop: generate -> encode -> blend (0.6*orig + 0.4*encoded) -> converge
 */

#include "cognitive/language/nimcp_inner_speech.h"
#include "cognitive/language/nimcp_w12_language_kg_events.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_MODULE "INNER_SPEECH"

/* Forward declarations for native language system */
struct nimcp_native_language;
typedef struct nimcp_native_language nimcp_native_language_t;

extern int nimcp_language_generate(nimcp_native_language_t* lang,
    const float* brain_embedding, uint32_t embed_dim,
    char* output_text, uint32_t max_text_length);

extern int nimcp_language_encode(nimcp_native_language_t* lang,
    const char* text, float* embedding, uint32_t max_dim);

struct nimcp_inner_speech {
    nimcp_inner_speech_config_t config;
    nimcp_native_language_t* language;

    /* Iteration buffers: one per iteration for debugging/history */
    float* iter_buffers[INNER_SPEECH_MAX_ITER];
    char* iter_texts[INNER_SPEECH_MAX_ITER];
    float convergence_history[INNER_SPEECH_MAX_ITER];
    uint32_t last_iterations;
    uint32_t allocated_iters;
    uint32_t buffer_dim;
};

/* ---- helpers ---- */

static float cosine_similarity(const float* a, const float* b, uint32_t dim) {
    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;

    for (uint32_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    float denom = sqrtf(norm_a) * sqrtf(norm_b);
    if (denom < 1e-12f) return 0.0f;
    return dot / denom;
}

/* ---- public API ---- */

nimcp_inner_speech_config_t nimcp_inner_speech_config_default(void) {
    nimcp_inner_speech_config_t cfg = {0};
    cfg.max_iterations = 3;
    cfg.convergence_threshold = 0.95f;
    cfg.refinement_lr = 0.0001f;
    cfg.blend_original = 0.6f;
    cfg.blend_encoded = 0.4f;
    return cfg;
}

nimcp_inner_speech_t* nimcp_inner_speech_create(const nimcp_inner_speech_config_t* config) {
    if (!config) {
        LOG_ERROR("[%s] NULL config", LOG_MODULE);
        return NULL;
    }

    nimcp_inner_speech_t* handle = nimcp_calloc(1, sizeof(nimcp_inner_speech_t));
    if (!handle) {
        LOG_ERROR("[%s] Failed to allocate inner speech handle", LOG_MODULE);
        return NULL;
    }

    handle->config = *config;

    /* Clamp max_iterations */
    if (handle->config.max_iterations == 0) {
        handle->config.max_iterations = 1;
    }
    if (handle->config.max_iterations > INNER_SPEECH_MAX_ITER) {
        handle->config.max_iterations = INNER_SPEECH_MAX_ITER;
    }

    handle->buffer_dim = INNER_SPEECH_BUFFER_DIM;
    handle->allocated_iters = handle->config.max_iterations;

    /* Allocate per-iteration buffers */
    for (uint32_t i = 0; i < handle->allocated_iters; i++) {
        handle->iter_buffers[i] = nimcp_calloc(INNER_SPEECH_BUFFER_DIM, sizeof(float));
        if (!handle->iter_buffers[i]) {
            LOG_ERROR("[%s] Failed to allocate iteration buffer %u", LOG_MODULE, i);
            nimcp_inner_speech_destroy(handle);
            return NULL;
        }

        handle->iter_texts[i] = nimcp_calloc(INNER_SPEECH_MAX_TEXT, sizeof(char));
        if (!handle->iter_texts[i]) {
            LOG_ERROR("[%s] Failed to allocate iteration text %u", LOG_MODULE, i);
            nimcp_inner_speech_destroy(handle);
            return NULL;
        }
    }

    handle->last_iterations = 0;
    handle->language = NULL;  /* Set externally or via a setter */

    LOG_INFO("[%s] Created: max_iter=%u, threshold=%.3f, blend=%.2f/%.2f",
             LOG_MODULE, handle->config.max_iterations,
             handle->config.convergence_threshold,
             handle->config.blend_original, handle->config.blend_encoded);

    return handle;
}

void nimcp_inner_speech_destroy(nimcp_inner_speech_t* handle) {
    if (!handle) return;

    for (uint32_t i = 0; i < handle->allocated_iters; i++) {
        if (handle->iter_buffers[i]) nimcp_free(handle->iter_buffers[i]);
        if (handle->iter_texts[i]) nimcp_free(handle->iter_texts[i]);
    }

    nimcp_free(handle);
    LOG_DEBUG("[%s] Destroyed", LOG_MODULE);
}

int nimcp_inner_speech_refine(nimcp_inner_speech_t* handle,
    const float* brain_output, uint32_t output_dim,
    float* refined_output, char* refined_text, uint32_t max_text)
{
    if (!handle || !brain_output || !refined_output) {
        LOG_ERROR("[%s] NULL argument to refine", LOG_MODULE);
        return -1;
    }

    /* Handle edge case: max_text=0 means no text output requested */
    if (refined_text && max_text == 0) {
        refined_text = NULL; /* Treat as if no text output requested */
    }

    if (output_dim == 0 || output_dim > INNER_SPEECH_BUFFER_DIM) {
        LOG_ERROR("[%s] Invalid output_dim=%u (max %u)", LOG_MODULE,
                  output_dim, INNER_SPEECH_BUFFER_DIM);
        return -1;
    }

    /* Early exit if no language system — pass through unchanged */
    if (!handle->language) {
        LOG_DEBUG("[%s] No language system attached, pass-through", LOG_MODULE);
        memcpy(refined_output, brain_output, output_dim * sizeof(float));
        if (refined_text) refined_text[0] = '\0';
        return 0;
    }

    /* Seed iteration 0 with original brain output */
    memcpy(handle->iter_buffers[0], brain_output, output_dim * sizeof(float));
    handle->last_iterations = 0;

    float* current = handle->iter_buffers[0];
    float blend_orig = handle->config.blend_original;
    float blend_enc = handle->config.blend_encoded;

    for (uint32_t iter = 0; iter < handle->config.max_iterations; iter++) {
        handle->last_iterations = iter + 1;
        char* text_buf = handle->iter_texts[iter];

        /* Step 1: Generate text from current embedding */
        int gen_ret = nimcp_language_generate(handle->language,
            current, output_dim, text_buf, INNER_SPEECH_MAX_TEXT);
        if (gen_ret != 0) {
            LOG_WARN("[%s] Generate failed at iter %u, using previous", LOG_MODULE, iter);
            break;
        }

        /* Step 2: Encode text back to embedding */
        float* next_buf;
        if (iter + 1 < handle->allocated_iters) {
            next_buf = handle->iter_buffers[iter + 1];
        } else {
            next_buf = handle->iter_buffers[iter];  /* Overwrite last */
        }

        float encoded[INNER_SPEECH_BUFFER_DIM];
        memset(encoded, 0, sizeof(encoded));

        int enc_ret = nimcp_language_encode(handle->language,
            text_buf, encoded, output_dim);
        if (enc_ret != 0) {
            LOG_WARN("[%s] Encode failed at iter %u, stopping", LOG_MODULE, iter);
            break;
        }

        /* Step 3: Blend original and encoded */
        for (uint32_t d = 0; d < output_dim; d++) {
            next_buf[d] = blend_orig * current[d] + blend_enc * encoded[d];
        }

        /* Step 4: Check convergence (cosine similarity between iterations) */
        float sim = cosine_similarity(current, next_buf, output_dim);
        handle->convergence_history[iter] = sim;

        LOG_DEBUG("[%s] Iter %u: cosine_sim=%.4f (threshold=%.4f)",
                  LOG_MODULE, iter, sim, handle->config.convergence_threshold);

        current = next_buf;

        if (sim >= handle->config.convergence_threshold) {
            LOG_DEBUG("[%s] Converged at iter %u (sim=%.4f)",
                      LOG_MODULE, iter, sim);
            break;
        }
    }

    /* Copy final refined output */
    memcpy(refined_output, current, output_dim * sizeof(float));

    /* Copy last generated text (skip if no text output requested) */
    if (refined_text && max_text > 0) {
        uint32_t last_text_idx = handle->last_iterations > 0 ? handle->last_iterations - 1 : 0;
        if (last_text_idx < handle->allocated_iters && handle->iter_texts[last_text_idx]) {
            uint32_t copy_len = max_text < INNER_SPEECH_MAX_TEXT ? max_text : INNER_SPEECH_MAX_TEXT;
            strncpy(refined_text, handle->iter_texts[last_text_idx], copy_len - 1);
            refined_text[copy_len - 1] = '\0';
        } else {
            refined_text[0] = '\0';
        }
    }

    LOG_INFO("[%s] Refined in %u iterations", LOG_MODULE, handle->last_iterations);

    /* W12 KG emit: one event per completed refinement cycle (not per
     * inner iteration). Fires at utterance-level granularity, so no
     * extra rate limiting needed. */
    {
        uint32_t iters = handle->last_iterations;
        float last_cos = 0.0f;
        if (iters > 0 && iters <= INNER_SPEECH_MAX_ITER) {
            last_cos = handle->convergence_history[iters - 1];
        }
        w12_emit_inner_speech_refine_auto(iters, last_cos);
    }
    return 0;
}

uint32_t nimcp_inner_speech_get_iterations(const nimcp_inner_speech_t* handle) {
    if (!handle) return 0;
    return handle->last_iterations;
}
