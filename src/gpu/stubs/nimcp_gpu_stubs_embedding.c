/**
 * @file nimcp_gpu_stubs_embedding.c
 * @brief CPU fallback stubs for embedding relevance GPU functions
 *
 * WHAT: Serial CPU implementations when CUDA is not available
 * WHY:  Graceful degradation on non-GPU systems
 */

#include "gpu/embedding/nimcp_embedding_relevance_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

bool nimcp_embedding_gpu_init(
    nimcp_gpu_context_t* ctx,
    nimcp_embedding_gpu_state_t* state,
    uint32_t capacity,
    uint16_t dim,
    uint32_t batch_size)
{
    (void)ctx;
    if (!state) return false;
    memset(state, 0, sizeof(*state));
    state->batch_size = batch_size;
    state->capacity = capacity;
    state->dim = dim;
    state->initialized = true;
    LOG_DEBUG("Embedding GPU state (CPU stub): %u capacity, %uD", capacity, dim);
    return true;
}

void nimcp_embedding_gpu_destroy(nimcp_embedding_gpu_state_t* state)
{
    if (!state) return;
    memset(state, 0, sizeof(*state));
}

uint32_t nimcp_embedding_gpu_batch_relevance(
    nimcp_gpu_context_t* ctx,
    nimcp_embedding_gpu_state_t* state,
    const float* h_embedding_pool,
    const float* h_context_embedding,
    const uint32_t* active_indices,
    uint32_t num_active,
    float* h_relevance_out)
{
    (void)ctx;
    if (!state || !state->initialized) return 0;
    if (!h_embedding_pool || !h_context_embedding || !active_indices || !h_relevance_out) return 0;

    uint16_t dim = state->dim;

    // Precompute context norm
    float ctx_norm_sq = 0.0f;
    for (uint16_t i = 0; i < dim; i++) {
        ctx_norm_sq += h_context_embedding[i] * h_context_embedding[i];
    }
    float ctx_norm = sqrtf(ctx_norm_sq);
    if (ctx_norm < 1e-6f) return 0;
    float inv_norm = 1.0f / ctx_norm;

    for (uint32_t n = 0; n < num_active; n++) {
        uint32_t pool_idx = active_indices[n];
        const float* emb = &h_embedding_pool[(size_t)pool_idx * dim];

        float dot = 0.0f;
        for (uint16_t i = 0; i < dim; i++) {
            dot += emb[i] * h_context_embedding[i];
        }

        h_relevance_out[n] = (dot * inv_norm + 1.0f) * 0.5f;
    }

    return num_active;
}
