/**
 * @file nimcp_embedding_relevance_kernels.cu
 * @brief GPU kernels for batch embedding relevance computation
 *
 * WHAT: CUDA kernels to compute cosine similarity between embedding vectors
 *       and a context vector, producing per-synapse relevance scores
 * WHY:  2048D dot products on 100K+ embeddings — GPU parallelism gives 50-100x speedup
 * HOW:  Context vector in shared memory, one thread per embedding, batch H2D transfers
 */

#ifdef NIMCP_ENABLE_CUDA

// CUDA headers MUST be outside extern "C" to avoid C++ operator linkage errors
#include <cuda_runtime.h>
#include <math.h>
#include <float.h>
#include <stdio.h>
#include <string.h>

// Project headers that may transitively include CUDA headers
#include "gpu/embedding/nimcp_embedding_relevance_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"

extern "C" {
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
}

#define BLOCK_SIZE 256
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)

// Max shared memory for context vector: 2048 floats = 8 KB (well within 48 KB limit)
#define MAX_SHARED_DIM 2048

static bool check_cuda(cudaError_t err, const char* op) {
    if (err != cudaSuccess) {
        LOG_ERROR("CUDA error in %s: %s", op, cudaGetErrorString(err));
        return false;
    }
    return true;
}


//=============================================================================
// Kernel: Batch dot product with shared-memory context
//=============================================================================

/**
 * Each thread computes the dot product of one embedding with the context vector.
 * Context vector is loaded into shared memory once per block.
 *
 * Input:  d_embeddings[batch_size × dim] — batch of embedding vectors (contiguous)
 * Input:  d_context[dim] — context vector (global, loaded to shared)
 * Output: d_relevance[batch_size] — relevance scores mapped to [0, 1]
 */
__global__ void kernel_batch_relevance(
    const float* __restrict__ d_embeddings,
    const float* __restrict__ d_context,
    float* __restrict__ d_relevance,
    uint32_t batch_size,
    uint16_t dim)
{
    // Load context vector into shared memory (cooperative load across block)
    extern __shared__ float s_context[];

    for (uint32_t i = threadIdx.x; i < dim; i += blockDim.x) {
        s_context[i] = d_context[i];
    }
    __syncthreads();

    uint32_t tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= batch_size) return;

    // Dot product: embedding[tid] · context
    const float* emb = &d_embeddings[(size_t)tid * dim];
    float dot = 0.0f;
    for (uint16_t i = 0; i < dim; i++) {
        dot += emb[i] * s_context[i];
    }

    // Context norm (could be precomputed, but dim iterations is cheap vs transfer)
    // Embeddings are already unit-normalized, so cosine = dot / ||context||
    // We compute ||context|| once per block via shared memory reduction,
    // but for simplicity and correctness, we just store raw dot and let the
    // host normalize. Actually, let's compute it here since all threads need it.

    // Shared memory reduction for context norm (only computed once)
    __shared__ float s_ctx_norm;
    if (threadIdx.x == 0) {
        float norm_sq = 0.0f;
        for (uint16_t i = 0; i < dim; i++) {
            norm_sq += s_context[i] * s_context[i];
        }
        s_ctx_norm = sqrtf(norm_sq);
        if (s_ctx_norm < 1e-6f) s_ctx_norm = 1.0f;  // Avoid div-by-zero
    }
    __syncthreads();

    // Cosine similarity → [0, 1]
    float similarity = dot / s_ctx_norm;
    d_relevance[tid] = (similarity + 1.0f) * 0.5f;
}


//=============================================================================
// Kernel: Gather embeddings from pool by index
//=============================================================================

/**
 * Gathers embedding vectors from the full pool into a contiguous batch buffer.
 * This runs on GPU after the full pool has been transferred (for large pools),
 * or can be skipped if we do CPU-side gather into pinned staging buffer.
 *
 * For our use case, we do CPU-side gather (cheaper for pinned→pinned copy),
 * so this kernel is provided but not used in the primary path.
 */
__global__ void kernel_gather_embeddings(
    const float* __restrict__ d_pool,
    const uint32_t* __restrict__ d_indices,
    float* __restrict__ d_batch,
    uint32_t num_indices,
    uint16_t dim)
{
    uint32_t tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= num_indices) return;

    uint32_t pool_idx = d_indices[tid];
    const float* src = &d_pool[(size_t)pool_idx * dim];
    float* dst = &d_batch[(size_t)tid * dim];

    for (uint16_t i = 0; i < dim; i++) {
        dst[i] = src[i];
    }
}


//=============================================================================
// Host API: Init / Destroy
//=============================================================================

extern "C"
bool nimcp_embedding_gpu_init(
    nimcp_gpu_context_t* ctx,
    nimcp_embedding_gpu_state_t* state,
    uint32_t capacity,
    uint16_t dim,
    uint32_t batch_size)
{
    if (!ctx || !state || capacity == 0 || dim == 0) return false;

    memset(state, 0, sizeof(*state));
    state->batch_size = batch_size;
    state->capacity = capacity;
    state->dim = dim;

    // Staging buffer: batch_size × dim floats
    size_t staging_bytes = (size_t)batch_size * dim * sizeof(float);
    cudaError_t err = cudaMalloc(&state->d_staging_buffer, staging_bytes);
    if (!check_cuda(err, "malloc staging")) return false;

    // Context vector: dim floats
    err = cudaMalloc(&state->d_context, dim * sizeof(float));
    if (!check_cuda(err, "malloc context")) {
        cudaFree(state->d_staging_buffer);
        return false;
    }

    // Batch relevance output: batch_size floats
    err = cudaMalloc(&state->d_relevance_out, batch_size * sizeof(float));
    if (!check_cuda(err, "malloc relevance_out")) {
        cudaFree(state->d_staging_buffer);
        cudaFree(state->d_context);
        return false;
    }

    // Full relevance cache: capacity floats (always resident)
    err = cudaMalloc(&state->d_relevance_cache, capacity * sizeof(float));
    if (!check_cuda(err, "malloc relevance_cache")) {
        cudaFree(state->d_staging_buffer);
        cudaFree(state->d_context);
        cudaFree(state->d_relevance_out);
        return false;
    }

    // Zero-init the cache
    cudaMemset(state->d_relevance_cache, 0, capacity * sizeof(float));

    state->initialized = true;
    LOG_INFO("Embedding GPU state initialized: %u capacity, %uD, batch=%u (staging=%.1f MB, cache=%.1f MB)",
             capacity, dim, batch_size,
             (float)staging_bytes / (1024.0f * 1024.0f),
             (float)(capacity * sizeof(float)) / (1024.0f * 1024.0f));
    return true;
}


extern "C"
void nimcp_embedding_gpu_destroy(nimcp_embedding_gpu_state_t* state)
{
    if (!state || !state->initialized) return;

    if (state->d_staging_buffer) cudaFree(state->d_staging_buffer);
    if (state->d_context) cudaFree(state->d_context);
    if (state->d_relevance_out) cudaFree(state->d_relevance_out);
    if (state->d_relevance_cache) cudaFree(state->d_relevance_cache);

    memset(state, 0, sizeof(*state));
    LOG_DEBUG("Embedding GPU state destroyed");
}


//=============================================================================
// Host API: Batch relevance computation
//=============================================================================

extern "C"
uint32_t nimcp_embedding_gpu_batch_relevance(
    nimcp_gpu_context_t* ctx,
    nimcp_embedding_gpu_state_t* state,
    const float* h_embedding_pool,
    const float* h_context_embedding,
    const uint32_t* active_indices,
    uint32_t num_active,
    float* h_relevance_out)
{
    if (!ctx || !state || !state->initialized) return 0;
    if (!h_embedding_pool || !h_context_embedding || !active_indices || !h_relevance_out) return 0;
    if (num_active == 0) return 0;

    uint16_t dim = state->dim;
    uint32_t batch_size = state->batch_size;
    cudaStream_t stream = (cudaStream_t)nimcp_gpu_get_pool_stream(ctx);

    // Upload context vector (small, synchronous is fine)
    cudaError_t err = cudaMemcpyAsync(
        state->d_context, h_context_embedding,
        dim * sizeof(float), cudaMemcpyHostToDevice, stream);
    if (!check_cuda(err, "upload context")) return 0;

    uint32_t total_processed = 0;

    // Process in batches
    for (uint32_t offset = 0; offset < num_active; offset += batch_size) {
        uint32_t batch_count = num_active - offset;
        if (batch_count > batch_size) batch_count = batch_size;

        // CPU-side gather: copy active embeddings into contiguous staging area.
        // Since h_embedding_pool is pinned, this memcpy is fast and the subsequent
        // H2D transfer can use DMA.
        // We need a pinned staging buffer on host. For simplicity, gather directly
        // into d_staging_buffer via individual H2D copies per embedding.
        // Better: use a temporary pinned host buffer for the gather.

        // Allocate temporary pinned gather buffer (reuse across batches)
        // For batch_count × dim floats
        size_t gather_bytes = (size_t)batch_count * dim * sizeof(float);

        // Gather on CPU (pinned source → pinned staging)
        // We can't allocate pinned memory per batch call, so we'll do per-embedding
        // H2D copies. With pinned source, each is DMA-capable.
        // Actually, since the pool IS pinned, we can gather into a stack/heap buffer
        // and do a single H2D. The gather buffer doesn't need to be pinned for
        // a synchronous memcpy — but for async, it does. Let's use the staging approach:

        // Strategy: gather into a malloc'd host buffer, then single H2D.
        // The source pool is pinned, so memcpy from it is fast.
        float* h_gather = (float*)nimcp_malloc(gather_bytes);
        if (!h_gather) {
            LOG_ERROR("Failed to allocate gather buffer: %zu bytes", gather_bytes);
            break;
        }

        for (uint32_t i = 0; i < batch_count; i++) {
            uint32_t pool_idx = active_indices[offset + i];
            const float* src = &h_embedding_pool[(size_t)pool_idx * dim];
            float* dst = &h_gather[(size_t)i * dim];
            memcpy(dst, src, dim * sizeof(float));
        }

        // H2D transfer of gathered batch
        err = cudaMemcpyAsync(
            state->d_staging_buffer, h_gather,
            gather_bytes, cudaMemcpyHostToDevice, stream);
        nimcp_free(h_gather);
        if (!check_cuda(err, "H2D staging")) break;

        // Launch relevance kernel
        size_t shared_mem = dim * sizeof(float);  // For context vector
        kernel_batch_relevance<<<GRID_SIZE(batch_count), BLOCK_SIZE, shared_mem, stream>>>(
            (const float*)state->d_staging_buffer,
            (const float*)state->d_context,
            (float*)state->d_relevance_out,
            batch_count,
            dim);

        err = cudaGetLastError();
        if (!check_cuda(err, "kernel launch")) break;

        // D2H transfer of relevance results
        err = cudaMemcpyAsync(
            &h_relevance_out[offset],
            state->d_relevance_out,
            batch_count * sizeof(float),
            cudaMemcpyDeviceToHost, stream);
        if (!check_cuda(err, "D2H relevance")) break;

        total_processed += batch_count;
    }

    // Sync the stream to ensure all results are available
    cudaStreamSynchronize(stream);

    return total_processed;
}

#endif /* NIMCP_ENABLE_CUDA */
