/**
 * @file nimcp_embedding_relevance_gpu.h
 * @brief GPU-accelerated batch relevance recomputation for CPU-staged embeddings
 *
 * WHAT: Batch-transfer embedding vectors from pinned CPU to GPU, compute
 *       cosine similarity with context vector, scatter results to relevance cache
 * WHY:  2048D dot products on 100K+ embeddings — GPU is 50-100x faster than CPU
 * HOW:  Pinned H2D async transfer → shared-memory dot product kernel → scatter
 *
 * ARCHITECTURE:
 *   CPU (pinned)              GPU
 *   +-----------------+       +------------------+
 *   | embedding_pool  | H2D   | staging_buffer   |
 *   | [cap × 2048]    | ====> | [batch × 2048]   |
 *   +-----------------+       +------------------+
 *                                     |
 *                              dot product kernel
 *                              (shared mem context)
 *                                     |
 *                              +------------------+
 *                              | relevance_cache  |
 *                              | [cap × 1 float]  |
 *                              +------------------+
 *                                     | D2H
 *                              scatter to synapse_t
 */

#ifndef NIMCP_EMBEDDING_RELEVANCE_GPU_H
#define NIMCP_EMBEDDING_RELEVANCE_GPU_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"
#include "gpu/context/nimcp_gpu_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief GPU-side buffers for embedding relevance computation
 */
typedef struct nimcp_embedding_gpu_state {
    void* d_staging_buffer;      /**< Device buffer: batch_size × dim floats */
    void* d_context;             /**< Device buffer: dim floats (context vector) */
    void* d_relevance_out;       /**< Device buffer: batch_size floats (batch results) */
    void* d_relevance_cache;     /**< Device buffer: capacity floats (full cache) */
    uint32_t batch_size;         /**< Max embeddings per batch transfer */
    uint32_t capacity;           /**< Total embedding slots */
    uint16_t dim;                /**< Embedding dimension */
    bool initialized;
} nimcp_embedding_gpu_state_t;

/**
 * @brief Initialize GPU buffers for embedding relevance computation
 *
 * @param ctx GPU context
 * @param state Output state struct (caller-allocated)
 * @param capacity Total number of embedding slots in CPU pool
 * @param dim Embedding dimension (e.g. 2048)
 * @param batch_size Max embeddings per GPU batch (e.g. 4096)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_embedding_gpu_init(
    nimcp_gpu_context_t* ctx,
    nimcp_embedding_gpu_state_t* state,
    uint32_t capacity,
    uint16_t dim,
    uint32_t batch_size);

/**
 * @brief Destroy GPU buffers
 */
NIMCP_EXPORT void nimcp_embedding_gpu_destroy(
    nimcp_embedding_gpu_state_t* state);

/**
 * @brief Batch-recompute relevance for all embeddings against a context vector
 *
 * Transfers embeddings from pinned CPU pool to GPU in batches,
 * computes cosine similarity with context, writes results to
 * relevance_out array (caller scatters to synapse_t fields).
 *
 * @param ctx GPU context (uses stream pool for async transfers)
 * @param state GPU state (initialized via nimcp_embedding_gpu_init)
 * @param h_embedding_pool Pinned CPU embedding pool (capacity × dim floats)
 * @param h_context_embedding Context vector on host (dim floats)
 * @param active_indices Array of active embedding pool indices to process
 * @param num_active Number of active indices
 * @param h_relevance_out Output: relevance values [num_active floats], host memory
 * @return Number of embeddings processed, 0 on error
 */
NIMCP_EXPORT uint32_t nimcp_embedding_gpu_batch_relevance(
    nimcp_gpu_context_t* ctx,
    nimcp_embedding_gpu_state_t* state,
    const float* h_embedding_pool,
    const float* h_context_embedding,
    const uint32_t* active_indices,
    uint32_t num_active,
    float* h_relevance_out);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMBEDDING_RELEVANCE_GPU_H */
