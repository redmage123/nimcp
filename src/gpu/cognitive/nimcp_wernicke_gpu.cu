/**
 * @file nimcp_wernicke_gpu.cu
 * @brief GPU CUDA Kernels for Wernicke's Region Language Comprehension
 *
 * WHAT: CUDA kernels for GPU-accelerated Wernicke's region operations
 * WHY:  GPU acceleration for parallel phoneme recognition, lexical access, semantic spreading
 * HOW:  Custom kernels for batch phoneme matching, cohort word recognition, spreading activation
 *
 * BIOLOGICAL BASIS:
 * =================
 * Wernicke's region (posterior STG, BA22) processes language comprehension:
 * - Phoneme recognition: Parallel pattern matching against phoneme templates
 * - Lexical access: Cohort-based parallel word recognition
 * - Semantic activation: Spreading activation across concept network
 * - Context integration: Parallel context embedding updates
 *
 * GPU ACCELERATION RATIONALE:
 * ===========================
 * Language comprehension benefits from massive parallelism:
 * - Phoneme recognition: All templates matched simultaneously
 * - Cohort narrowing: All word candidates updated in parallel
 * - Semantic spreading: Thousands of concepts activated together
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#ifdef NIMCP_ENABLE_CUDA

#include <cuda_runtime.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#include "gpu/cognitive/nimcp_wernicke_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/memory/nimcp_memory.h"

#define LOG_MODULE "WERNICKE_GPU"

#define BLOCK_SIZE 256
#define WARP_SIZE 32
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)

/* P3-W1: Named constants for spectral feature dimensions (was magic number 82) */
#define WERNICKE_SPECTRAL_FEATURE_DIM  82  /* 40 mel + 13 mfcc + 13 delta + 13 delta_delta + 3 */

/* P3-W2: Named constant for working memory refresh amount (was magic number 0.1f) */
#define WERNICKE_WM_REFRESH_AMOUNT  0.1f

/* P3-W3: Named constant for activation threshold in spreading (was magic number 0.01f) */
#define WERNICKE_ACTIVATION_THRESHOLD  0.01f

//=============================================================================
// Internal GPU Wernicke Context
//=============================================================================

struct wernicke_gpu_context {
    nimcp_gpu_context_t* gpu_ctx;
    wernicke_gpu_config_t config;

    // GPU phoneme embeddings [num_phonemes x embed_dim]
    float* d_phoneme_embeddings;
    uint32_t num_phonemes;
    uint32_t phoneme_embed_dim;

    // GPU lexicon
    wernicke_gpu_lexical_entry_t* d_lexicon;
    uint32_t lexicon_size;
    uint32_t lexicon_capacity;

    // GPU cohort state
    float* d_cohort_activations;     // [lexicon_capacity]
    uint8_t* d_cohort_matched;       // [lexicon_capacity] phonemes matched
    uint32_t cohort_phoneme_pos;     // Current phoneme position in recognition

    // GPU semantic network
    float* d_concept_activations;    // [max_concepts]
    float* d_concept_embeddings;     // [max_concepts x semantic_embed_dim]
    uint32_t* d_adjacency_matrix;    // [max_concepts x max_neighbors]
    float* d_adjacency_weights;      // [max_concepts x max_neighbors]
    uint32_t num_concepts;
    uint32_t max_neighbors;

    // GPU working memory (phonological loop)
    uint8_t* d_wm_phonemes;
    float* d_wm_activations;
    uint32_t wm_count;

    // Temporary buffers
    float* d_temp_posteriors;        // [max_frames x num_phonemes]
    float* d_temp_activations;       // [max_concepts]
    uint32_t* d_temp_indices;        // [max_concepts]

    // CUDA stream for async operations
    cudaStream_t stream;

    // Statistics
    wernicke_gpu_stats_t stats;
};

//=============================================================================
// CUDA Kernels: Phoneme Recognition
//=============================================================================

/**
 * @brief Compute similarity between spectral frame and phoneme embeddings
 *
 * Each thread computes similarity for one (frame, phoneme) pair
 *
 * @param frame_stride Stride between frames in float units (may differ from
 *                     feature_dim due to struct padding)
 */
__global__ void kernel_compute_phoneme_similarity(
    const float* __restrict__ spectral_features,  // [num_frames x frame_stride]
    uint32_t feature_dim,
    uint32_t frame_stride,
    const float* __restrict__ phoneme_embeddings, // [num_phonemes x embed_dim]
    uint32_t num_phonemes,
    uint32_t embed_dim,
    uint32_t num_frames,
    float* __restrict__ similarities              // [num_frames x num_phonemes]
) {
    uint32_t frame_idx = blockIdx.x;
    uint32_t phoneme_idx = threadIdx.x;

    if (frame_idx >= num_frames || phoneme_idx >= num_phonemes) return;

    // Compute dot product similarity
    float sum = 0.0f;
    uint32_t min_dim = (feature_dim < embed_dim) ? feature_dim : embed_dim;

    for (uint32_t d = 0; d < min_dim; d++) {
        float feat = spectral_features[frame_idx * frame_stride + d];
        float embed = phoneme_embeddings[phoneme_idx * embed_dim + d];
        sum += feat * embed;
    }

    similarities[frame_idx * num_phonemes + phoneme_idx] = sum;
}

/**
 * @brief Softmax over phoneme similarities
 */
__global__ void kernel_softmax_phonemes(
    float* __restrict__ similarities,
    uint32_t num_phonemes,
    uint32_t num_frames
) {
    uint32_t frame_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (frame_idx >= num_frames) return;

    // Find max for numerical stability
    float max_val = -FLT_MAX;
    for (uint32_t p = 0; p < num_phonemes; p++) {
        float val = similarities[frame_idx * num_phonemes + p];
        if (val > max_val) max_val = val;
    }

    // Compute exp and sum
    float sum = 0.0f;
    for (uint32_t p = 0; p < num_phonemes; p++) {
        float exp_val = expf(similarities[frame_idx * num_phonemes + p] - max_val);
        similarities[frame_idx * num_phonemes + p] = exp_val;
        sum += exp_val;
    }

    // Normalize
    for (uint32_t p = 0; p < num_phonemes; p++) {
        similarities[frame_idx * num_phonemes + p] /= sum;
    }
}

/**
 * @brief Find argmax phoneme per frame
 */
__global__ void kernel_argmax_phonemes(
    const float* __restrict__ posteriors,
    uint32_t num_phonemes,
    uint32_t num_frames,
    uint8_t* __restrict__ phoneme_ids,
    float* __restrict__ confidences
) {
    uint32_t frame_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (frame_idx >= num_frames) return;

    float max_val = -FLT_MAX;
    uint8_t max_idx = 0;

    for (uint32_t p = 0; p < num_phonemes; p++) {
        float val = posteriors[frame_idx * num_phonemes + p];
        if (val > max_val) {
            max_val = val;
            max_idx = (uint8_t)p;
        }
    }

    phoneme_ids[frame_idx] = max_idx;
    confidences[frame_idx] = max_val;
}

//=============================================================================
// CUDA Kernels: Word Recognition (Cohort Model)
//=============================================================================

/**
 * @brief Initialize cohort from first phoneme
 *
 * Each thread checks one lexical entry
 */
__global__ void kernel_init_cohort(
    const wernicke_gpu_lexical_entry_t* __restrict__ lexicon,
    uint32_t lexicon_size,
    uint8_t first_phoneme,
    float phoneme_confidence,
    float* __restrict__ cohort_activations,
    uint8_t* __restrict__ cohort_matched
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= lexicon_size) return;

    // Check if first phoneme matches
    if (lexicon[idx].phoneme_count > 0 && lexicon[idx].phonemes[0] == first_phoneme) {
        cohort_activations[idx] = lexicon[idx].frequency * phoneme_confidence;
        cohort_matched[idx] = 1;
    } else {
        cohort_activations[idx] = 0.0f;
        cohort_matched[idx] = 0;
    }
}

/**
 * @brief Update cohort with new phoneme evidence
 */
__global__ void kernel_update_cohort(
    const wernicke_gpu_lexical_entry_t* __restrict__ lexicon,
    uint32_t lexicon_size,
    uint8_t new_phoneme,
    float phoneme_confidence,
    uint32_t phoneme_position,
    float* __restrict__ cohort_activations,
    uint8_t* __restrict__ cohort_matched
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= lexicon_size) return;

    // Skip entries not in cohort
    if (cohort_activations[idx] <= 0.0f) return;

    // Check if phoneme at position matches
    if (phoneme_position < lexicon[idx].phoneme_count &&
        lexicon[idx].phonemes[phoneme_position] == new_phoneme) {
        // Match - boost activation
        cohort_activations[idx] *= (1.0f + phoneme_confidence);
        cohort_matched[idx] = (uint8_t)(phoneme_position + 1);
    } else {
        // Mismatch - remove from cohort
        cohort_activations[idx] = 0.0f;
        cohort_matched[idx] = 0;
    }
}

/**
 * @brief Apply decay to cohort activations
 */
__global__ void kernel_decay_cohort(
    float* __restrict__ cohort_activations,
    uint32_t lexicon_size,
    float decay_rate
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= lexicon_size) return;

    cohort_activations[idx] *= decay_rate;
}

/**
 * @brief Collect active cohort members (both partial and complete matches)
 */
__global__ void kernel_check_recognition(
    const wernicke_gpu_lexical_entry_t* __restrict__ lexicon,
    uint32_t lexicon_size,
    const float* __restrict__ cohort_activations,
    const uint8_t* __restrict__ cohort_matched,
    wernicke_gpu_word_candidate_t* __restrict__ candidates,
    uint32_t* __restrict__ candidate_count,
    uint32_t max_candidates
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= lexicon_size) return;

    // Include all active cohort members (both partial and complete)
    if (cohort_activations[idx] > 0.0f && cohort_matched[idx] > 0) {
        /* P1-W1: Guard against division by zero when phoneme_count is 0 */
        if (lexicon[idx].phoneme_count == 0) return;

        // Atomically add to candidates
        uint32_t slot = atomicAdd(candidate_count, 1);
        if (slot < max_candidates) {
            candidates[slot].word_id = lexicon[idx].word_id;
            // Normalize probability: matched_proportion * frequency
            float match_ratio = (float)cohort_matched[idx] / (float)lexicon[idx].phoneme_count;
            candidates[slot].cohort_probability = match_ratio * lexicon[idx].frequency;
            candidates[slot].uniqueness_point = match_ratio;
            candidates[slot].matched_phonemes = cohort_matched[idx];
            candidates[slot].recognition_complete =
                (cohort_matched[idx] == lexicon[idx].phoneme_count);
        }
    }
}

//=============================================================================
// CUDA Kernels: Semantic Spreading Activation
//=============================================================================

/**
 * @brief Initialize activations from seed concepts
 */
__global__ void kernel_init_activations(
    float* __restrict__ activations,
    uint32_t num_concepts,
    const uint32_t* __restrict__ seed_concepts,
    const float* __restrict__ seed_activations,
    uint32_t num_seeds
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_concepts) return;

    // Check if this concept is a seed
    float activation = 0.0f;
    for (uint32_t s = 0; s < num_seeds; s++) {
        if (seed_concepts[s] == idx) {
            activation = seed_activations[s];
            break;
        }
    }
    activations[idx] = activation;
}

/**
 * @brief Single spreading activation step
 */
__global__ void kernel_spreading_step(
    const float* __restrict__ current_activations,
    float* __restrict__ new_activations,
    uint32_t num_concepts,
    const uint32_t* __restrict__ adjacency_matrix,  // [num_concepts x max_neighbors]
    const float* __restrict__ adjacency_weights,
    uint32_t max_neighbors,
    float decay
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_concepts) return;

    // Keep current activation with decay
    float activation = current_activations[idx] * decay;

    // Add spreading from neighbors
    for (uint32_t n = 0; n < max_neighbors; n++) {
        uint32_t neighbor_idx = adjacency_matrix[idx * max_neighbors + n];
        if (neighbor_idx == 0xFFFFFFFF) break;  // End marker

        float weight = adjacency_weights[idx * max_neighbors + n];
        activation += current_activations[neighbor_idx] * weight * (1.0f - decay);
    }

    // Clamp to [0, 1]
    if (activation > 1.0f) activation = 1.0f;
    if (activation < 0.0f) activation = 0.0f;

    new_activations[idx] = activation;
}

/**
 * @brief Compute semantic similarity between two concepts
 */
__global__ void kernel_semantic_similarity(
    const float* __restrict__ embeddings,
    uint32_t embed_dim,
    uint32_t concept_a,
    uint32_t concept_b,
    float* __restrict__ similarity
) {
    // Single block reduction for dot product and norms
    extern __shared__ float shared[];
    float* s_dot = shared;
    float* s_norm_a = &shared[blockDim.x];
    float* s_norm_b = &shared[2 * blockDim.x];

    uint32_t tid = threadIdx.x;
    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;

    // Each thread handles multiple dimensions
    for (uint32_t d = tid; d < embed_dim; d += blockDim.x) {
        float a = embeddings[concept_a * embed_dim + d];
        float b = embeddings[concept_b * embed_dim + d];
        dot += a * b;
        norm_a += a * a;
        norm_b += b * b;
    }

    s_dot[tid] = dot;
    s_norm_a[tid] = norm_a;
    s_norm_b[tid] = norm_b;
    __syncthreads();

    // Reduction
    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            s_dot[tid] += s_dot[tid + s];
            s_norm_a[tid] += s_norm_a[tid + s];
            s_norm_b[tid] += s_norm_b[tid + s];
        }
        __syncthreads();
    }

    // Write result
    if (tid == 0) {
        float denom = sqrtf(s_norm_a[0]) * sqrtf(s_norm_b[0]);
        *similarity = (denom > 1e-8f) ? (s_dot[0] / denom) : 0.0f;
    }
}

//=============================================================================
// CUDA Kernels: Working Memory
//=============================================================================

/**
 * @brief Apply decay to working memory
 */
__global__ void kernel_wm_decay(
    float* __restrict__ activations,
    uint32_t count,
    float decay_factor,
    float threshold,
    uint8_t* __restrict__ remove_flags
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= count) return;

    activations[idx] *= decay_factor;
    remove_flags[idx] = (activations[idx] < threshold) ? 1 : 0;
}

/**
 * @brief Rehearse working memory (refresh activations)
 */
__global__ void kernel_wm_rehearse(
    float* __restrict__ activations,
    uint32_t count,
    float refresh_amount
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= count) return;

    activations[idx] += refresh_amount;
    if (activations[idx] > 1.0f) activations[idx] = 1.0f;
}

//=============================================================================
// Configuration
//=============================================================================

extern "C" wernicke_gpu_config_t wernicke_gpu_default_config(void) {
    wernicke_gpu_config_t config;
    memset(&config, 0, sizeof(config));

    // Phoneme recognition
    config.num_phoneme_categories = WERNICKE_GPU_DEFAULT_NUM_PHONEMES;
    config.phoneme_embedding_dim = WERNICKE_GPU_MAX_PHONEME_DIM;
    config.max_spectral_frames = 1000;

    // Lexical access
    config.max_lexicon_size = 50000;
    config.max_cohort_size = 1000;
    config.word_embedding_dim = WERNICKE_GPU_MAX_WORD_DIM;
    config.max_phonemes_per_word = 16;

    // Semantic activation
    config.max_concepts = 100000;
    config.semantic_embedding_dim = WERNICKE_GPU_MAX_SEMANTIC_DIM;
    config.spreading_iterations = 5;
    config.spreading_decay = 0.9f;

    // Attention
    config.enable_attention = false;
    config.attention_heads = 8;

    // Working memory
    config.working_memory_slots = 7;  // Miller's law
    config.wm_decay_rate = 0.95f;

    // Transfer
    config.enable_async_transfer = true;

    // Batch
    config.max_batch_size = 32;

    return config;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

extern "C" wernicke_gpu_context_t* wernicke_gpu_create(
    nimcp_gpu_context_t* gpu_ctx,
    const wernicke_gpu_config_t* config
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!gpu_ctx) {
        LOG_ERROR("NULL GPU context");
        return NULL;
    }

    wernicke_gpu_context_t* ctx =
        (wernicke_gpu_context_t*)nimcp_calloc(1, sizeof(wernicke_gpu_context_t));
    if (!ctx) {
        LOG_ERROR("Failed to allocate Wernicke GPU context");
        return NULL;
    }

    ctx->gpu_ctx = gpu_ctx;
    ctx->config = config ? *config : wernicke_gpu_default_config();

    // Create CUDA stream
    cudaError_t err = cudaStreamCreate(&ctx->stream);
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to create CUDA stream: %s", cudaGetErrorString(err));
        nimcp_free(ctx);
        return NULL;
    }

    // Allocate GPU buffers

    // Phoneme embeddings
    size_t phoneme_embed_size = ctx->config.num_phoneme_categories *
                                 ctx->config.phoneme_embedding_dim * sizeof(float);
    err = cudaMalloc(&ctx->d_phoneme_embeddings, phoneme_embed_size);
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to allocate phoneme embeddings: %s", cudaGetErrorString(err));
        cudaStreamDestroy(ctx->stream);
        nimcp_free(ctx);
        return NULL;
    }

    // Lexicon
    size_t lexicon_size = ctx->config.max_lexicon_size * sizeof(wernicke_gpu_lexical_entry_t);
    err = cudaMalloc(&ctx->d_lexicon, lexicon_size);
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to allocate lexicon: %s", cudaGetErrorString(err));
        cudaFree(ctx->d_phoneme_embeddings);
        cudaStreamDestroy(ctx->stream);
        nimcp_free(ctx);
        return NULL;
    }
    ctx->lexicon_capacity = ctx->config.max_lexicon_size;

    // Declare posteriors_size before any gotos (C++ requirement)
    size_t posteriors_size = ctx->config.max_spectral_frames *
                             ctx->config.num_phoneme_categories * sizeof(float);

    // Cohort state
    err = cudaMalloc(&ctx->d_cohort_activations,
                     ctx->config.max_lexicon_size * sizeof(float));
    if (err != cudaSuccess) goto cleanup_lexicon;

    err = cudaMalloc(&ctx->d_cohort_matched,
                     ctx->config.max_lexicon_size * sizeof(uint8_t));
    if (err != cudaSuccess) goto cleanup_cohort_act;

    // Semantic network
    err = cudaMalloc(&ctx->d_concept_activations,
                     ctx->config.max_concepts * sizeof(float));
    if (err != cudaSuccess) goto cleanup_cohort;

    // Working memory
    err = cudaMalloc(&ctx->d_wm_phonemes,
                     ctx->config.working_memory_slots * sizeof(uint8_t));
    if (err != cudaSuccess) goto cleanup_concepts;

    err = cudaMalloc(&ctx->d_wm_activations,
                     ctx->config.working_memory_slots * sizeof(float));
    if (err != cudaSuccess) goto cleanup_wm_phonemes;

    // Temporary buffers
    err = cudaMalloc(&ctx->d_temp_posteriors, posteriors_size);
    if (err != cudaSuccess) goto cleanup_wm_act;

    LOG_INFO("Created Wernicke GPU context (lexicon: %u, concepts: %u)",
             ctx->config.max_lexicon_size, ctx->config.max_concepts);

    return ctx;

// Cleanup on error
cleanup_wm_act:
    cudaFree(ctx->d_wm_activations);
cleanup_wm_phonemes:
    cudaFree(ctx->d_wm_phonemes);
cleanup_concepts:
    cudaFree(ctx->d_concept_activations);
cleanup_cohort:
    cudaFree(ctx->d_cohort_matched);
cleanup_cohort_act:
    cudaFree(ctx->d_cohort_activations);
cleanup_lexicon:
    cudaFree(ctx->d_lexicon);
    cudaFree(ctx->d_phoneme_embeddings);
    cudaStreamDestroy(ctx->stream);
    nimcp_free(ctx);
    return NULL;
}

extern "C" void wernicke_gpu_destroy(wernicke_gpu_context_t* ctx) {
    if (!ctx) return;

    // Free GPU buffers
    if (ctx->d_temp_posteriors) cudaFree(ctx->d_temp_posteriors);
    if (ctx->d_temp_activations) cudaFree(ctx->d_temp_activations);
    if (ctx->d_temp_indices) cudaFree(ctx->d_temp_indices);
    if (ctx->d_wm_activations) cudaFree(ctx->d_wm_activations);
    if (ctx->d_wm_phonemes) cudaFree(ctx->d_wm_phonemes);
    if (ctx->d_adjacency_weights) cudaFree(ctx->d_adjacency_weights);
    if (ctx->d_adjacency_matrix) cudaFree(ctx->d_adjacency_matrix);
    if (ctx->d_concept_embeddings) cudaFree(ctx->d_concept_embeddings);
    if (ctx->d_concept_activations) cudaFree(ctx->d_concept_activations);
    if (ctx->d_cohort_matched) cudaFree(ctx->d_cohort_matched);
    if (ctx->d_cohort_activations) cudaFree(ctx->d_cohort_activations);
    if (ctx->d_lexicon) cudaFree(ctx->d_lexicon);
    if (ctx->d_phoneme_embeddings) cudaFree(ctx->d_phoneme_embeddings);

    if (ctx->stream) cudaStreamDestroy(ctx->stream);

    nimcp_free(ctx);
    LOG_DEBUG("Destroyed Wernicke GPU context");
}

extern "C" bool wernicke_gpu_synchronize(wernicke_gpu_context_t* ctx) {
    if (!ctx) return false;
    cudaError_t err = cudaStreamSynchronize(ctx->stream);
    return (err == cudaSuccess);
}

//=============================================================================
// Phoneme Embedding Management
//=============================================================================

extern "C" bool wernicke_gpu_upload_phoneme_embeddings(
    wernicke_gpu_context_t* ctx,
    const float* embeddings,
    uint32_t num_phonemes,
    uint32_t embed_dim
) {
    if (!ctx || !embeddings) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }
    if (num_phonemes > ctx->config.num_phoneme_categories ||
        embed_dim > ctx->config.phoneme_embedding_dim) {
        LOG_ERROR("Phoneme embeddings exceed configured limits");
        return false;
    }

    size_t size = num_phonemes * embed_dim * sizeof(float);
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(ctx->d_phoneme_embeddings, embeddings, size,
                               cudaMemcpyHostToDevice, ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    ctx->num_phonemes = num_phonemes;
    ctx->phoneme_embed_dim = embed_dim;

    return true;
}

//=============================================================================
// Phoneme Recognition
//=============================================================================

extern "C" bool wernicke_gpu_recognize_phonemes(
    wernicke_gpu_context_t* ctx,
    const wernicke_gpu_spectral_frame_t* frames,
    uint32_t num_frames,
    wernicke_gpu_phoneme_result_t* results
) {
    if (!ctx || !frames || !results || num_frames == 0) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }
    if (ctx->num_phonemes == 0) {
        LOG_ERROR("No phoneme embeddings uploaded");
        return false;
    }

    // Upload spectral features
    // Feature dim = 40 (mel) + 13 (mfcc) + 13 (delta) + 13 (delta_delta) + 3 = 82
    const uint32_t feature_dim = WERNICKE_SPECTRAL_FEATURE_DIM;
    // Frame stride accounts for struct padding (sizeof(struct) / sizeof(float))
    const uint32_t frame_stride = (uint32_t)(sizeof(wernicke_gpu_spectral_frame_t) / sizeof(float));
    size_t frames_size = num_frames * sizeof(wernicke_gpu_spectral_frame_t);

    /* P1-W2: Use goto cleanup pattern to prevent cascading GPU memory leaks */
    float* d_spectral = NULL;
    uint8_t* d_phoneme_ids = NULL;
    float* d_confidences = NULL;

    cudaError_t alloc_err = cudaMalloc(&d_spectral, frames_size);
    if (alloc_err != cudaSuccess) {
        LOG_ERROR("Failed to allocate d_spectral: %s", cudaGetErrorString(alloc_err));
        return false;
    }
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(d_spectral, frames, frames_size,
                               cudaMemcpyHostToDevice, ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    // Allocate outputs
    alloc_err = cudaMalloc(&d_phoneme_ids, num_frames * sizeof(uint8_t));
    if (alloc_err != cudaSuccess) {
        LOG_ERROR("Failed to allocate d_phoneme_ids: %s", cudaGetErrorString(alloc_err));
        cudaFree(d_spectral);
        return false;
    }
    alloc_err = cudaMalloc(&d_confidences, num_frames * sizeof(float));
    if (alloc_err != cudaSuccess) {
        LOG_ERROR("Failed to allocate d_confidences: %s", cudaGetErrorString(alloc_err));
        cudaFree(d_phoneme_ids);
        cudaFree(d_spectral);
        return false;
    }

    // Compute similarities
    dim3 grid(num_frames, 1, 1);
    /* P1-14: Clamp block size to CUDA max threads per block (1024) */
    uint32_t phoneme_block = (ctx->num_phonemes > 1024) ? 1024 : ctx->num_phonemes;
    dim3 block(phoneme_block, 1, 1);

    kernel_compute_phoneme_similarity<<<grid, block, 0, ctx->stream>>>(
        d_spectral,
        feature_dim,
        frame_stride,
        ctx->d_phoneme_embeddings,
        ctx->num_phonemes,
        ctx->phoneme_embed_dim,
        num_frames,
        ctx->d_temp_posteriors
    );
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Softmax
    kernel_softmax_phonemes<<<GRID_SIZE(num_frames), BLOCK_SIZE, 0, ctx->stream>>>(
        ctx->d_temp_posteriors,
        ctx->num_phonemes,
        num_frames
    );
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Argmax
    kernel_argmax_phonemes<<<GRID_SIZE(num_frames), BLOCK_SIZE, 0, ctx->stream>>>(
        ctx->d_temp_posteriors,
        ctx->num_phonemes,
        num_frames,
        d_phoneme_ids,
        d_confidences
    );
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Copy results back
    uint8_t* h_phoneme_ids = (uint8_t*)nimcp_malloc(num_frames);
    float* h_confidences = (float*)nimcp_malloc(num_frames * sizeof(float));
    /* P1-18: NULL check after malloc to prevent NULL dereference */
    if (!h_phoneme_ids || !h_confidences) {
        nimcp_free(h_phoneme_ids);
        nimcp_free(h_confidences);
        cudaFree(d_spectral);
        cudaFree(d_phoneme_ids);
        cudaFree(d_confidences);
        LOG_ERROR("Failed to allocate host buffers for phoneme results");
        return false;
    }

    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(h_phoneme_ids, d_phoneme_ids, num_frames,
                               cudaMemcpyDeviceToHost, ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(h_confidences, d_confidences, num_frames * sizeof(float),
                               cudaMemcpyDeviceToHost, ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    // Populate results
    for (uint32_t i = 0; i < num_frames; i++) {
        results[i].phoneme_id = h_phoneme_ids[i];
        results[i].confidence = h_confidences[i];
        results[i].posterior = NULL;  // Optional, not filled
    }

    // Cleanup
    nimcp_free(h_phoneme_ids);
    nimcp_free(h_confidences);
    cudaFree(d_spectral);
    cudaFree(d_phoneme_ids);
    cudaFree(d_confidences);

    ctx->stats.phoneme_recognitions++;

    return true;
}

extern "C" bool wernicke_gpu_compute_posteriors(
    wernicke_gpu_context_t* ctx,
    const wernicke_gpu_spectral_frame_t* frames,
    uint32_t num_frames,
    float* posteriors
) {
    if (!ctx || !frames || !posteriors || num_frames == 0) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }
    if (ctx->num_phonemes == 0) return false;

    bool success = false;
    float* d_spectral = NULL;

    const uint32_t feature_dim = WERNICKE_SPECTRAL_FEATURE_DIM;
    const uint32_t frame_stride = (uint32_t)(sizeof(wernicke_gpu_spectral_frame_t) / sizeof(float));
    size_t frames_size = num_frames * sizeof(wernicke_gpu_spectral_frame_t);

    if (cudaMalloc(&d_spectral, frames_size) != cudaSuccess) {
        goto cleanup;
    }
    if (cudaMemcpyAsync(d_spectral, frames, frames_size,
                        cudaMemcpyHostToDevice, ctx->stream) != cudaSuccess) {
        goto cleanup;
    }

    {
        // Compute similarities
        dim3 grid(num_frames, 1, 1);
        /* P1-14: Clamp block size to CUDA max threads per block (1024) */
        uint32_t phoneme_block = (ctx->num_phonemes > 1024) ? 1024 : ctx->num_phonemes;
        dim3 block(phoneme_block, 1, 1);

        kernel_compute_phoneme_similarity<<<grid, block, 0, ctx->stream>>>(
            d_spectral,
            feature_dim,
            frame_stride,
            ctx->d_phoneme_embeddings,
            ctx->num_phonemes,
            ctx->phoneme_embed_dim,
            num_frames,
            ctx->d_temp_posteriors
        );
        if (cudaGetLastError() != cudaSuccess) goto cleanup;

        // Softmax
        kernel_softmax_phonemes<<<GRID_SIZE(num_frames), BLOCK_SIZE, 0, ctx->stream>>>(
            ctx->d_temp_posteriors,
            ctx->num_phonemes,
            num_frames
        );
        if (cudaGetLastError() != cudaSuccess) goto cleanup;

        // Copy back
        size_t post_size = num_frames * ctx->num_phonemes * sizeof(float);
        if (cudaMemcpyAsync(posteriors, ctx->d_temp_posteriors, post_size,
                            cudaMemcpyDeviceToHost, ctx->stream) != cudaSuccess) {
            goto cleanup;
        }
        if (cudaStreamSynchronize(ctx->stream) != cudaSuccess) {
            goto cleanup;
        }
    }

    success = true;

cleanup:
    if (d_spectral) cudaFree(d_spectral);
    return success;
}

//=============================================================================
// Lexicon Management
//=============================================================================

extern "C" bool wernicke_gpu_upload_lexicon(
    wernicke_gpu_context_t* ctx,
    const wernicke_gpu_lexical_entry_t* entries,
    uint32_t count
) {
    if (!ctx || !entries) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }
    if (count > ctx->lexicon_capacity) {
        LOG_ERROR("Lexicon count %u exceeds capacity %u", count, ctx->lexicon_capacity);
        return false;
    }

    size_t size = count * sizeof(wernicke_gpu_lexical_entry_t);
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(ctx->d_lexicon, entries, size,
                               cudaMemcpyHostToDevice, ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    ctx->lexicon_size = count;
    LOG_INFO("Uploaded lexicon with %u entries", count);

    return true;
}

extern "C" bool wernicke_gpu_clear_lexicon(wernicke_gpu_context_t* ctx) {
    if (!ctx) return false;
    ctx->lexicon_size = 0;
    return true;
}

extern "C" uint32_t wernicke_gpu_get_lexicon_size(const wernicke_gpu_context_t* ctx) {
    return ctx ? ctx->lexicon_size : 0;
}

//=============================================================================
// Word Recognition (Cohort Model)
//=============================================================================

extern "C" bool wernicke_gpu_recognize_words(
    wernicke_gpu_context_t* ctx,
    const uint8_t* phonemes,
    uint32_t num_phonemes,
    wernicke_gpu_word_candidate_t* candidates,
    uint32_t max_candidates,
    uint32_t* num_candidates
) {
    if (!ctx || !phonemes || !candidates || !num_candidates || num_phonemes == 0) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }
    if (ctx->lexicon_size == 0) {
        *num_candidates = 0;
        return true;
    }

    // Initialize cohort with first phoneme
    kernel_init_cohort<<<GRID_SIZE(ctx->lexicon_size), BLOCK_SIZE, 0, ctx->stream>>>(
        ctx->d_lexicon,
        ctx->lexicon_size,
        phonemes[0],
        1.0f,
        ctx->d_cohort_activations,
        ctx->d_cohort_matched
    );
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Update cohort with remaining phonemes
    for (uint32_t p = 1; p < num_phonemes; p++) {
        kernel_update_cohort<<<GRID_SIZE(ctx->lexicon_size), BLOCK_SIZE, 0, ctx->stream>>>(
            ctx->d_lexicon,
            ctx->lexicon_size,
            phonemes[p],
            1.0f,
            p,
            ctx->d_cohort_activations,
            ctx->d_cohort_matched
        );
        NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    }

    /* P2-W1: Use goto cleanup to prevent GPU memory leaks on cascading failures */
    wernicke_gpu_word_candidate_t* d_candidates = NULL;
    uint32_t* d_count = NULL;

    cudaError_t rw_err = cudaMalloc(&d_candidates, max_candidates * sizeof(wernicke_gpu_word_candidate_t));
    if (rw_err != cudaSuccess) {
        LOG_ERROR("Failed to allocate d_candidates: %s", cudaGetErrorString(rw_err));
        return false;
    }
    rw_err = cudaMalloc(&d_count, sizeof(uint32_t));
    if (rw_err != cudaSuccess) {
        LOG_ERROR("Failed to allocate d_count: %s", cudaGetErrorString(rw_err));
        cudaFree(d_candidates);
        return false;
    }
    NIMCP_CUDA_RECOVER(cudaMemsetAsync(d_count, 0, sizeof(uint32_t), ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    kernel_check_recognition<<<GRID_SIZE(ctx->lexicon_size), BLOCK_SIZE, 0, ctx->stream>>>(
        ctx->d_lexicon,
        ctx->lexicon_size,
        ctx->d_cohort_activations,
        ctx->d_cohort_matched,
        d_candidates,
        d_count,
        max_candidates
    );
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Copy results
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(num_candidates, d_count, sizeof(uint32_t),
                               cudaMemcpyDeviceToHost, ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    uint32_t actual_count = *num_candidates;
    if (actual_count > max_candidates) actual_count = max_candidates;

    if (actual_count > 0) {
        NIMCP_CUDA_RECOVER(cudaMemcpyAsync(candidates, d_candidates,
                                   actual_count * sizeof(wernicke_gpu_word_candidate_t),
                                   cudaMemcpyDeviceToHost, ctx->stream), GPU_ERROR_CUDA_RUNTIME);
        NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    }

    cudaFree(d_candidates);
    cudaFree(d_count);

    ctx->stats.word_recognitions++;

    return true;
}

extern "C" bool wernicke_gpu_update_cohort(
    wernicke_gpu_context_t* ctx,
    uint8_t new_phoneme,
    float phoneme_confidence
) {
    if (!ctx) return false;
    if (ctx->lexicon_size == 0) return true;

    if (ctx->cohort_phoneme_pos == 0) {
        // First phoneme - initialize cohort
        kernel_init_cohort<<<GRID_SIZE(ctx->lexicon_size), BLOCK_SIZE, 0, ctx->stream>>>(
            ctx->d_lexicon,
            ctx->lexicon_size,
            new_phoneme,
            phoneme_confidence,
            ctx->d_cohort_activations,
            ctx->d_cohort_matched
        );
    } else {
        // Update existing cohort
        kernel_update_cohort<<<GRID_SIZE(ctx->lexicon_size), BLOCK_SIZE, 0, ctx->stream>>>(
            ctx->d_lexicon,
            ctx->lexicon_size,
            new_phoneme,
            phoneme_confidence,
            ctx->cohort_phoneme_pos,
            ctx->d_cohort_activations,
            ctx->d_cohort_matched
        );
    }

    ctx->cohort_phoneme_pos++;
    return true;
}

extern "C" bool wernicke_gpu_get_cohort(
    wernicke_gpu_context_t* ctx,
    wernicke_gpu_word_candidate_t* candidates,
    uint32_t max_candidates,
    uint32_t* num_candidates
) {
    if (!ctx || !candidates || !num_candidates) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }
    if (ctx->lexicon_size == 0) {
        *num_candidates = 0;
        return true;
    }

    /* P2-W2: Use explicit error handling to prevent GPU memory leaks on cascading failures */
    wernicke_gpu_word_candidate_t* d_candidates = NULL;
    uint32_t* d_count = NULL;

    cudaError_t gc_err = cudaMalloc(&d_candidates, max_candidates * sizeof(wernicke_gpu_word_candidate_t));
    if (gc_err != cudaSuccess) {
        LOG_ERROR("Failed to allocate d_candidates: %s", cudaGetErrorString(gc_err));
        return false;
    }
    gc_err = cudaMalloc(&d_count, sizeof(uint32_t));
    if (gc_err != cudaSuccess) {
        LOG_ERROR("Failed to allocate d_count: %s", cudaGetErrorString(gc_err));
        cudaFree(d_candidates);
        return false;
    }
    NIMCP_CUDA_RECOVER(cudaMemsetAsync(d_count, 0, sizeof(uint32_t), ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    kernel_check_recognition<<<GRID_SIZE(ctx->lexicon_size), BLOCK_SIZE, 0, ctx->stream>>>(
        ctx->d_lexicon,
        ctx->lexicon_size,
        ctx->d_cohort_activations,
        ctx->d_cohort_matched,
        d_candidates,
        d_count,
        max_candidates
    );
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(num_candidates, d_count, sizeof(uint32_t),
                               cudaMemcpyDeviceToHost, ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    uint32_t actual = *num_candidates;
    if (actual > max_candidates) actual = max_candidates;
    *num_candidates = actual;

    if (actual > 0) {
        NIMCP_CUDA_RECOVER(cudaMemcpyAsync(candidates, d_candidates,
                                   actual * sizeof(wernicke_gpu_word_candidate_t),
                                   cudaMemcpyDeviceToHost, ctx->stream), GPU_ERROR_CUDA_RUNTIME);
        NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    }

    cudaFree(d_candidates);
    cudaFree(d_count);

    return true;
}

extern "C" bool wernicke_gpu_reset_cohort(wernicke_gpu_context_t* ctx) {
    if (!ctx) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    NIMCP_CUDA_RECOVER(cudaMemsetAsync(ctx->d_cohort_activations, 0,
                               ctx->lexicon_size * sizeof(float), ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemsetAsync(ctx->d_cohort_matched, 0,
                               ctx->lexicon_size * sizeof(uint8_t), ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    ctx->cohort_phoneme_pos = 0;

    return true;
}

//=============================================================================
// Semantic Network
//=============================================================================

extern "C" bool wernicke_gpu_upload_semantic_network(
    wernicke_gpu_context_t* ctx,
    const wernicke_gpu_concept_t* concepts,
    uint32_t num_concepts,
    const uint32_t* adjacency_matrix,
    const float* weights
) {
    if (!ctx || !concepts) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }
    if (num_concepts > ctx->config.max_concepts) {
        LOG_ERROR("Concepts %u exceed max %u", num_concepts, ctx->config.max_concepts);
        return false;
    }

    // We need to determine max_neighbors from the concepts
    uint32_t max_neighbors = 0;
    for (uint32_t i = 0; i < num_concepts; i++) {
        if (concepts[i].num_neighbors > max_neighbors) {
            max_neighbors = concepts[i].num_neighbors;
        }
    }

    // Allocate adjacency structures if needed
    if (!ctx->d_adjacency_matrix || ctx->max_neighbors < max_neighbors) {
        if (ctx->d_adjacency_matrix) cudaFree(ctx->d_adjacency_matrix);
        if (ctx->d_adjacency_weights) cudaFree(ctx->d_adjacency_weights);
        ctx->d_adjacency_matrix = NULL;
        ctx->d_adjacency_weights = NULL;

        size_t adj_size = num_concepts * max_neighbors;
        if (cudaMalloc(&ctx->d_adjacency_matrix, adj_size * sizeof(uint32_t)) != cudaSuccess) {
            LOG_ERROR("Failed to allocate adjacency matrix on GPU");
            return false;
        }
        if (cudaMalloc(&ctx->d_adjacency_weights, adj_size * sizeof(float)) != cudaSuccess) {
            cudaFree(ctx->d_adjacency_matrix);
            ctx->d_adjacency_matrix = NULL;
            LOG_ERROR("Failed to allocate adjacency weights on GPU");
            return false;
        }
        ctx->max_neighbors = max_neighbors;
    }

    // Upload if provided
    if (adjacency_matrix && weights) {
        size_t adj_size = num_concepts * max_neighbors;
        if (cudaMemcpyAsync(ctx->d_adjacency_matrix, adjacency_matrix,
                            adj_size * sizeof(uint32_t),
                            cudaMemcpyHostToDevice, ctx->stream) != cudaSuccess) {
            LOG_ERROR("Failed to upload adjacency matrix");
            return false;
        }
        if (cudaMemcpyAsync(ctx->d_adjacency_weights, weights,
                            adj_size * sizeof(float),
                            cudaMemcpyHostToDevice, ctx->stream) != cudaSuccess) {
            LOG_ERROR("Failed to upload adjacency weights");
            return false;
        }
    }

    ctx->num_concepts = num_concepts;
    LOG_INFO("Uploaded semantic network with %u concepts", num_concepts);

    return true;
}

extern "C" bool wernicke_gpu_spread_activation(
    wernicke_gpu_context_t* ctx,
    const uint32_t* seed_concepts,
    const float* seed_activations,
    uint32_t num_seeds,
    wernicke_gpu_activation_result_t* results,
    uint32_t max_results,
    uint32_t* num_results
) {
    if (!ctx || !seed_concepts || !seed_activations) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // If no semantic network uploaded, seed concepts directly into activations buffer
    if (ctx->num_concepts == 0) {
        // Determine the range of concept IDs we need to cover
        uint32_t max_concept_id = 0;
        for (uint32_t i = 0; i < num_seeds; i++) {
            if (seed_concepts[i] > max_concept_id)
                max_concept_id = seed_concepts[i];
        }
        uint32_t needed = max_concept_id + 1;
        if (needed <= ctx->config.max_concepts) {
            // Zero the activations buffer
            NIMCP_CUDA_RECOVER(cudaMemsetAsync(ctx->d_concept_activations, 0,
                needed * sizeof(float), ctx->stream), GPU_ERROR_CUDA_RUNTIME);

            // Set seed activations
            float* h_acts = (float*)nimcp_calloc(needed, sizeof(float));
            /* P2: NULL check after calloc */
            if (!h_acts) {
                LOG_ERROR("Failed to allocate seed activations buffer");
                if (num_results) *num_results = 0;
                return false;
            }
            for (uint32_t i = 0; i < num_seeds; i++) {
                if (seed_concepts[i] < needed) {
                    h_acts[seed_concepts[i]] = seed_activations[i];
                }
            }
            NIMCP_CUDA_RECOVER(cudaMemcpyAsync(ctx->d_concept_activations, h_acts,
                needed * sizeof(float), cudaMemcpyHostToDevice, ctx->stream),
                GPU_ERROR_CUDA_RUNTIME);
            NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);
            nimcp_free(h_acts);
            ctx->num_concepts = needed;
        }

        if (results && num_results) {
            uint32_t result_count = (num_seeds < max_results) ? num_seeds : max_results;
            for (uint32_t i = 0; i < result_count; i++) {
                results[i].concept_id = seed_concepts[i];
                results[i].activation = seed_activations[i];
                results[i].spreading_contribution = 0.0f;
            }
            *num_results = result_count;
        } else if (num_results) {
            *num_results = 0;
        }

        ctx->stats.spreading_activations++;
        return true;
    }

    // Upload seeds
    uint32_t* d_seeds = NULL;
    float* d_seed_acts = NULL;
    float* d_new_activations = NULL;

    if (cudaMalloc(&d_seeds, num_seeds * sizeof(uint32_t)) != cudaSuccess) {
        if (num_results) *num_results = 0;
        return false;
    }
    if (cudaMalloc(&d_seed_acts, num_seeds * sizeof(float)) != cudaSuccess) {
        cudaFree(d_seeds);
        if (num_results) *num_results = 0;
        return false;
    }
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(d_seeds, seed_concepts, num_seeds * sizeof(uint32_t),
                               cudaMemcpyHostToDevice, ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(d_seed_acts, seed_activations, num_seeds * sizeof(float),
                               cudaMemcpyHostToDevice, ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    // Initialize activations
    kernel_init_activations<<<GRID_SIZE(ctx->num_concepts), BLOCK_SIZE, 0, ctx->stream>>>(
        ctx->d_concept_activations,
        ctx->num_concepts,
        d_seeds,
        d_seed_acts,
        num_seeds
    );
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Allocate second buffer for ping-pong
    if (cudaMalloc(&d_new_activations, ctx->num_concepts * sizeof(float)) != cudaSuccess) {
        cudaFree(d_seeds);
        cudaFree(d_seed_acts);
        if (num_results) *num_results = 0;
        return false;
    }

    // Spreading iterations
    float* current = ctx->d_concept_activations;
    float* next = d_new_activations;

    for (uint32_t iter = 0; iter < ctx->config.spreading_iterations; iter++) {
        kernel_spreading_step<<<GRID_SIZE(ctx->num_concepts), BLOCK_SIZE, 0, ctx->stream>>>(
            current,
            next,
            ctx->num_concepts,
            ctx->d_adjacency_matrix,
            ctx->d_adjacency_weights,
            ctx->max_neighbors,
            ctx->config.spreading_decay
        );
        NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

        // Swap
        float* tmp = current;
        current = next;
        next = tmp;
    }

    // Make sure concept_activations points to final result
    if (current != ctx->d_concept_activations) {
        NIMCP_CUDA_RECOVER(cudaMemcpyAsync(ctx->d_concept_activations, current,
            ctx->num_concepts * sizeof(float), cudaMemcpyDeviceToDevice, ctx->stream),
            GPU_ERROR_CUDA_RUNTIME);
    }

    // Copy final activations to host and sort
    float* h_activations = (float*)nimcp_malloc(ctx->num_concepts * sizeof(float));
    /* P2: NULL check after malloc */
    if (!h_activations) {
        LOG_ERROR("Failed to allocate host activations buffer");
        cudaFree(d_new_activations);
        cudaFree(d_seed_acts);
        cudaFree(d_seeds);
        if (num_results) *num_results = 0;
        return false;
    }
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(h_activations, current, ctx->num_concepts * sizeof(float),
                               cudaMemcpyDeviceToHost, ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    if (results && num_results) {
        // Find top activated (simple CPU sort for now)
        typedef struct { uint32_t id; float activation; } act_pair_t;
        act_pair_t* pairs = (act_pair_t*)nimcp_malloc(ctx->num_concepts * sizeof(act_pair_t));
        /* P2: NULL check after malloc */
        if (!pairs) {
            LOG_ERROR("Failed to allocate pairs buffer");
            nimcp_free(h_activations);
            cudaFree(d_new_activations);
            cudaFree(d_seed_acts);
            cudaFree(d_seeds);
            *num_results = 0;
            return false;
        }

        uint32_t count = 0;
        for (uint32_t i = 0; i < ctx->num_concepts; i++) {
            if (h_activations[i] > WERNICKE_ACTIVATION_THRESHOLD) {
                pairs[count].id = i;
                pairs[count].activation = h_activations[i];
                count++;
            }
        }

        /* P3: TODO: Replace with partial sort or heap-based top-k for O(n log k) */
        /* Simple selection sort for top results (for demonstration) */
        for (uint32_t i = 0; i < count && i < max_results; i++) {
            for (uint32_t j = i + 1; j < count; j++) {
                if (pairs[j].activation > pairs[i].activation) {
                    act_pair_t tmp = pairs[i];
                    pairs[i] = pairs[j];
                    pairs[j] = tmp;
                }
            }
        }

        // Fill results
        uint32_t result_count = (count < max_results) ? count : max_results;
        for (uint32_t i = 0; i < result_count; i++) {
            results[i].concept_id = pairs[i].id;
            results[i].activation = pairs[i].activation;
            results[i].spreading_contribution = pairs[i].activation;  // Simplified
        }
        *num_results = result_count;

        nimcp_free(pairs);
    } else if (num_results) {
        *num_results = 0;
    }

    // Cleanup
    nimcp_free(h_activations);
    cudaFree(d_new_activations);
    cudaFree(d_seed_acts);
    cudaFree(d_seeds);

    ctx->stats.spreading_activations++;

    return true;
}

extern "C" bool wernicke_gpu_get_top_activated(
    wernicke_gpu_context_t* ctx,
    uint32_t top_k,
    wernicke_gpu_activation_result_t* results,
    uint32_t* actual_count
) {
    if (!ctx || !results || !actual_count) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }
    if (ctx->num_concepts == 0) {
        *actual_count = 0;
        return true;
    }

    // Copy activations to host
    float* h_activations = (float*)nimcp_malloc(ctx->num_concepts * sizeof(float));
    /* P2: NULL check after malloc */
    if (!h_activations) {
        LOG_ERROR("Failed to allocate host activations buffer");
        *actual_count = 0;
        return false;
    }
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(h_activations, ctx->d_concept_activations,
                               ctx->num_concepts * sizeof(float),
                               cudaMemcpyDeviceToHost, ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    // Find top-k
    typedef struct { uint32_t id; float activation; } act_pair_t;
    act_pair_t* top = (act_pair_t*)nimcp_calloc(top_k, sizeof(act_pair_t));
    /* P2: NULL check after calloc */
    if (!top) {
        LOG_ERROR("Failed to allocate top-k buffer");
        nimcp_free(h_activations);
        *actual_count = 0;
        return false;
    }

    for (uint32_t i = 0; i < ctx->num_concepts; i++) {
        float act = h_activations[i];
        if (act > 0.0f) {
            // Insert into sorted top-k
            for (uint32_t j = 0; j < top_k; j++) {
                if (act > top[j].activation) {
                    // Shift down
                    for (uint32_t k = top_k - 1; k > j; k--) {
                        top[k] = top[k - 1];
                    }
                    top[j].id = i;
                    top[j].activation = act;
                    break;
                }
            }
        }
    }

    // Count non-zero
    uint32_t count = 0;
    for (uint32_t i = 0; i < top_k && top[i].activation > 0.0f; i++) {
        results[i].concept_id = top[i].id;
        results[i].activation = top[i].activation;
        results[i].spreading_contribution = 0.0f;
        count++;
    }
    *actual_count = count;

    nimcp_free(top);
    nimcp_free(h_activations);

    return true;
}

extern "C" float wernicke_gpu_semantic_similarity(
    wernicke_gpu_context_t* ctx,
    uint32_t concept_a,
    uint32_t concept_b
) {
    if (!ctx) return 0.0f;

    // Same concept always has similarity 1.0
    if (concept_a == concept_b) return 1.0f;

    // Without embeddings, return 0 for different concepts
    if (!ctx->d_concept_embeddings) return 0.0f;
    if (concept_a >= ctx->num_concepts || concept_b >= ctx->num_concepts) return 0.0f;

    float* d_similarity;
    float h_similarity = 0.0f;

    cudaError_t err = cudaMalloc(&d_similarity, sizeof(float));
    if (err != cudaSuccess) return 0.0f;

    size_t shared_size = 3 * BLOCK_SIZE * sizeof(float);
    kernel_semantic_similarity<<<1, BLOCK_SIZE, shared_size, ctx->stream>>>(
        ctx->d_concept_embeddings,
        ctx->config.semantic_embedding_dim,
        concept_a,
        concept_b,
        d_similarity
    );

    /* P2-W3: Stream sync required before synchronous cudaMemcpy when kernel
     * launched on a non-default stream, otherwise we may read stale data */
    cudaStreamSynchronize(ctx->stream);
    cudaMemcpy(&h_similarity, d_similarity, sizeof(float), cudaMemcpyDeviceToHost);
    cudaFree(d_similarity);

    return h_similarity;
}

//=============================================================================
// Working Memory
//=============================================================================

extern "C" bool wernicke_gpu_wm_push(
    wernicke_gpu_context_t* ctx,
    const uint8_t* phonemes,
    uint32_t count
) {
    if (!ctx || !phonemes) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    /* P2-W4: Guard against uint32_t underflow when wm_count > working_memory_slots */
    if (ctx->wm_count >= ctx->config.working_memory_slots) return true; /* WM full */
    uint32_t available = ctx->config.working_memory_slots - ctx->wm_count;
    uint32_t to_push = (count < available) ? count : available;

    if (to_push == 0) return true;  // WM full

    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(ctx->d_wm_phonemes + ctx->wm_count,
                               phonemes, to_push,
                               cudaMemcpyHostToDevice, ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    // Initialize activations to 1.0
    float* h_ones = (float*)nimcp_malloc(to_push * sizeof(float));
    /* P2: NULL check after malloc */
    if (!h_ones) {
        LOG_ERROR("Failed to allocate working memory activation buffer");
        return false;
    }
    for (uint32_t i = 0; i < to_push; i++) h_ones[i] = 1.0f;

    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(ctx->d_wm_activations + ctx->wm_count,
                               h_ones, to_push * sizeof(float),
                               cudaMemcpyHostToDevice, ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    nimcp_free(h_ones);
    ctx->wm_count += to_push;
    ctx->stats.wm_operations++;

    return true;
}

extern "C" bool wernicke_gpu_wm_get_contents(
    wernicke_gpu_context_t* ctx,
    uint8_t* phonemes,
    float* activations,
    uint32_t max_count,
    uint32_t* actual_count
) {
    if (!ctx || !phonemes || !actual_count) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    uint32_t to_get = (ctx->wm_count < max_count) ? ctx->wm_count : max_count;
    *actual_count = to_get;

    if (to_get == 0) return true;

    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(phonemes, ctx->d_wm_phonemes, to_get,
                               cudaMemcpyDeviceToHost, ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    if (activations) {
        NIMCP_CUDA_RECOVER(cudaMemcpyAsync(activations, ctx->d_wm_activations,
                                   to_get * sizeof(float),
                                   cudaMemcpyDeviceToHost, ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    }

    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    return true;
}

extern "C" bool wernicke_gpu_wm_rehearse(wernicke_gpu_context_t* ctx) {
    if (!ctx || ctx->wm_count == 0) return true;

    kernel_wm_rehearse<<<GRID_SIZE(ctx->wm_count), BLOCK_SIZE, 0, ctx->stream>>>(
        ctx->d_wm_activations,
        ctx->wm_count,
        WERNICKE_WM_REFRESH_AMOUNT
    );
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    ctx->stats.wm_operations++;
    return true;
}

extern "C" bool wernicke_gpu_wm_apply_decay(
    wernicke_gpu_context_t* ctx,
    float decay_factor,
    float threshold
) {
    if (!ctx || ctx->wm_count == 0) return true;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    bool success = false;
    uint8_t* d_remove = NULL;

    if (cudaMalloc(&d_remove, ctx->wm_count * sizeof(uint8_t)) != cudaSuccess) {
        goto cleanup;
    }

    kernel_wm_decay<<<GRID_SIZE(ctx->wm_count), BLOCK_SIZE, 0, ctx->stream>>>(
        ctx->d_wm_activations,
        ctx->wm_count,
        decay_factor,
        threshold,
        d_remove
    );
    if (cudaGetLastError() != cudaSuccess) goto cleanup;

    // For simplicity, we don't compact here - just mark as decayed
    // A proper implementation would compact the arrays

    ctx->stats.wm_operations++;
    success = true;

cleanup:
    if (d_remove) cudaFree(d_remove);
    return success;
}

extern "C" bool wernicke_gpu_wm_clear(wernicke_gpu_context_t* ctx) {
    if (!ctx) return false;
    ctx->wm_count = 0;
    return true;
}

//=============================================================================
// Full Comprehension Pipeline
//=============================================================================

extern "C" bool wernicke_gpu_comprehend(
    wernicke_gpu_context_t* ctx,
    const wernicke_gpu_spectral_frame_t* frames,
    uint32_t num_frames,
    wernicke_gpu_word_candidate_t* word_candidates,
    uint32_t max_word_candidates,
    uint32_t* num_word_candidates,
    wernicke_gpu_activation_result_t* semantic_activations,
    uint32_t max_semantic_activations,
    uint32_t* num_semantic_activations
) {
    if (!ctx || !frames) return false;

    // Step 1: Recognize phonemes
    wernicke_gpu_phoneme_result_t* phonemes =
        (wernicke_gpu_phoneme_result_t*)nimcp_malloc(num_frames * sizeof(wernicke_gpu_phoneme_result_t));
    /* P2: NULL check after malloc */
    if (!phonemes) {
        LOG_ERROR("Failed to allocate phoneme results buffer");
        return false;
    }

    if (!wernicke_gpu_recognize_phonemes(ctx, frames, num_frames, phonemes)) {
        nimcp_free(phonemes);
        return false;
    }

    // Step 2: Extract phoneme sequence
    uint8_t* phoneme_seq = (uint8_t*)nimcp_malloc(num_frames);
    /* P2: NULL check after malloc */
    if (!phoneme_seq) {
        LOG_ERROR("Failed to allocate phoneme sequence buffer");
        nimcp_free(phonemes);
        return false;
    }
    for (uint32_t i = 0; i < num_frames; i++) {
        phoneme_seq[i] = phonemes[i].phoneme_id;
    }

    // Step 3: Word recognition
    // Try phoneme sequence for word recognition using the cohort model.
    // If no exact cohort matches are found, use the phoneme posteriors
    // to find words with the best-matching initial phonemes (soft matching).
    if (word_candidates && num_word_candidates) {
        *num_word_candidates = 0;

        // Try full sequence first
        wernicke_gpu_recognize_words(ctx, phoneme_seq, num_frames,
                                      word_candidates, max_word_candidates,
                                      num_word_candidates);

        // If no matches, try each individual phoneme from the sequence
        if (*num_word_candidates == 0) {
            for (uint32_t i = 0; i < num_frames && *num_word_candidates == 0; i++) {
                wernicke_gpu_recognize_words(ctx, &phoneme_seq[i], 1,
                                              word_candidates, max_word_candidates,
                                              num_word_candidates);
            }
        }

        // If still no matches, use top phoneme posteriors to find words
        // This models the biological reality that word recognition uses
        // the full posterior distribution, not just the argmax phoneme
        if (*num_word_candidates == 0 && ctx->lexicon_size > 0) {
            // Get posteriors for first frame
            /* P1-W4: NULL check after calloc for posteriors */
            float* posteriors = (float*)nimcp_calloc(ctx->num_phonemes, sizeof(float));
            if (!posteriors) {
                LOG_ERROR("Failed to allocate posteriors buffer");
                goto skip_soft_matching;
            }
            wernicke_gpu_compute_posteriors(ctx, &frames[0], 1, posteriors);

            // For each lexicon word, score by posterior of its first phoneme
            float best_score = 0.0f;
            uint32_t best_word_idx = 0;
            bool found = false;

            // Read lexicon back from GPU
            /* P1-W3: NULL check after malloc for h_lexicon */
            wernicke_gpu_lexical_entry_t* h_lexicon =
                (wernicke_gpu_lexical_entry_t*)nimcp_malloc(ctx->lexicon_size * sizeof(wernicke_gpu_lexical_entry_t));
            if (!h_lexicon) {
                nimcp_free(posteriors);
                goto skip_soft_matching;
            }
            cudaMemcpy(h_lexicon, ctx->d_lexicon,
                       ctx->lexicon_size * sizeof(wernicke_gpu_lexical_entry_t),
                       cudaMemcpyDeviceToHost);

            for (uint32_t w = 0; w < ctx->lexicon_size; w++) {
                uint8_t first_ph = h_lexicon[w].phonemes[0];
                if (first_ph < ctx->num_phonemes) {
                    float score = posteriors[first_ph] * h_lexicon[w].frequency;
                    if (score > best_score || !found) {
                        // Add as candidate
                        if (*num_word_candidates < max_word_candidates) {
                            uint32_t idx = (*num_word_candidates)++;
                            word_candidates[idx].word_id = h_lexicon[w].word_id;
                            word_candidates[idx].cohort_probability = score;
                            word_candidates[idx].uniqueness_point = 0.0f;
                            word_candidates[idx].matched_phonemes = 0;
                            word_candidates[idx].recognition_complete = false;
                            found = true;
                            if (score > best_score) best_score = score;
                        }
                    }
                }
            }

            nimcp_free(h_lexicon);
            nimcp_free(posteriors);
        }
        skip_soft_matching: ; /* P1-W3/W4: goto target for allocation failures */
    }

    // Step 4: Semantic activation (if words were recognized)
    if (semantic_activations && num_semantic_activations) {
        *num_semantic_activations = 0;

        if (num_word_candidates && *num_word_candidates > 0 && ctx->num_concepts > 0) {
            // Use recognized word concepts as seeds
            uint32_t* seed_concepts = (uint32_t*)nimcp_malloc(*num_word_candidates * sizeof(uint32_t));
            float* seed_activations_arr = (float*)nimcp_malloc(*num_word_candidates * sizeof(float));
            /* P2: NULL check after malloc */
            if (!seed_concepts || !seed_activations_arr) {
                nimcp_free(seed_concepts);
                nimcp_free(seed_activations_arr);
                LOG_ERROR("Failed to allocate seed buffers for semantic activation");
                /* Continue without semantic activation rather than failing entirely */
            } else {

            // Map word_id to concept_id (simplified - assumes word_id == concept_id)
            for (uint32_t i = 0; i < *num_word_candidates; i++) {
                seed_concepts[i] = word_candidates[i].word_id % ctx->num_concepts;
                seed_activations_arr[i] = word_candidates[i].cohort_probability;
            }

            wernicke_gpu_spread_activation(ctx,
                                           seed_concepts, seed_activations_arr,
                                           *num_word_candidates,
                                           semantic_activations,
                                           max_semantic_activations,
                                           num_semantic_activations);

            nimcp_free(seed_activations_arr);
            nimcp_free(seed_concepts);
            } /* end else (successful malloc) */
        }
    }

    nimcp_free(phoneme_seq);
    nimcp_free(phonemes);

    return true;
}

//=============================================================================
// Statistics
//=============================================================================

extern "C" bool wernicke_gpu_get_stats(
    const wernicke_gpu_context_t* ctx,
    wernicke_gpu_stats_t* stats
) {
    if (!ctx || !stats) return false;
    *stats = ctx->stats;

    // Add dynamic stats
    stats->current_cohort_size = ctx->cohort_phoneme_pos;
    stats->active_concepts = ctx->num_concepts;

    // Estimate GPU memory
    stats->gpu_memory_used =
        ctx->config.num_phoneme_categories * ctx->config.phoneme_embedding_dim * sizeof(float) +
        ctx->lexicon_size * sizeof(wernicke_gpu_lexical_entry_t) +
        ctx->config.max_lexicon_size * (sizeof(float) + sizeof(uint8_t)) +
        ctx->num_concepts * sizeof(float);

    return true;
}

extern "C" void wernicke_gpu_reset_stats(wernicke_gpu_context_t* ctx) {
    if (ctx) {
        memset(&ctx->stats, 0, sizeof(wernicke_gpu_stats_t));
    }
}

//=============================================================================
// CPU Fallback Implementations
//=============================================================================

extern "C" bool wernicke_cpu_recognize_phonemes(
    const wernicke_gpu_spectral_frame_t* frames,
    uint32_t num_frames,
    const float* phoneme_embeddings,
    uint32_t num_phonemes,
    uint32_t embed_dim,
    wernicke_gpu_phoneme_result_t* results
) {
    if (!frames || !phoneme_embeddings || !results) return false;

    const uint32_t feature_dim = WERNICKE_SPECTRAL_FEATURE_DIM;  // Match GPU

    float* similarities = (float*)nimcp_malloc(num_phonemes * sizeof(float));
    if (!similarities) return false;

    for (uint32_t f = 0; f < num_frames; f++) {
        // Get frame features
        const float* features = (const float*)&frames[f];

        // Compute dot product similarities for all phonemes
        for (uint32_t p = 0; p < num_phonemes; p++) {
            float sim = 0.0f;
            uint32_t min_dim = (feature_dim < embed_dim) ? feature_dim : embed_dim;

            for (uint32_t d = 0; d < min_dim; d++) {
                sim += features[d] * phoneme_embeddings[p * embed_dim + d];
            }
            similarities[p] = sim;
        }

        // Apply softmax (same as GPU kernel)
        float max_val = -FLT_MAX;
        for (uint32_t p = 0; p < num_phonemes; p++) {
            if (similarities[p] > max_val) max_val = similarities[p];
        }

        float sum = 0.0f;
        for (uint32_t p = 0; p < num_phonemes; p++) {
            similarities[p] = expf(similarities[p] - max_val);
            sum += similarities[p];
        }
        for (uint32_t p = 0; p < num_phonemes; p++) {
            similarities[p] /= sum;
        }

        // Find argmax
        float best_conf = -FLT_MAX;
        uint8_t best_phoneme = 0;
        for (uint32_t p = 0; p < num_phonemes; p++) {
            if (similarities[p] > best_conf) {
                best_conf = similarities[p];
                best_phoneme = (uint8_t)p;
            }
        }

        results[f].phoneme_id = best_phoneme;
        results[f].confidence = best_conf;
        results[f].posterior = NULL;
    }

    nimcp_free(similarities);
    return true;
}

extern "C" bool wernicke_cpu_recognize_words(
    const wernicke_gpu_lexical_entry_t* lexicon,
    uint32_t lexicon_size,
    const uint8_t* phonemes,
    uint32_t num_phonemes,
    wernicke_gpu_word_candidate_t* candidates,
    uint32_t max_candidates,
    uint32_t* num_candidates
) {
    if (!lexicon || !phonemes || !candidates || !num_candidates) return false;

    *num_candidates = 0;

    for (uint32_t w = 0; w < lexicon_size && *num_candidates < max_candidates; w++) {
        const wernicke_gpu_lexical_entry_t* entry = &lexicon[w];

        // Check prefix match (cohort model)
        uint32_t match_count = 0;
        for (uint32_t p = 0; p < num_phonemes && p < entry->phoneme_count; p++) {
            if (entry->phonemes[p] != phonemes[p]) break;
            match_count++;
        }

        // Must match all input phonemes (prefix must match)
        if (match_count == num_phonemes) {
            uint32_t idx = (*num_candidates)++;
            candidates[idx].word_id = entry->word_id;
            float match_ratio = (float)match_count / (float)entry->phoneme_count;
            candidates[idx].cohort_probability = match_ratio * entry->frequency;
            candidates[idx].uniqueness_point = match_ratio;
            candidates[idx].matched_phonemes = (uint8_t)match_count;
            candidates[idx].recognition_complete =
                (match_count == entry->phoneme_count);
        }
    }

    return true;
}

extern "C" bool wernicke_cpu_spread_activation(
    const float* adjacency_weights,
    uint32_t num_concepts,
    uint32_t max_neighbors,
    const uint32_t* seed_concepts,
    const float* seed_activations,
    uint32_t num_seeds,
    uint32_t iterations,
    float decay,
    float* output_activations
) {
    if (!adjacency_weights || !seed_concepts || !seed_activations || !output_activations) {
        return false;
    }

    float* current = (float*)nimcp_calloc(num_concepts, sizeof(float));
    float* next = (float*)nimcp_calloc(num_concepts, sizeof(float));
    if (!current || !next) {
        nimcp_free(current);
        nimcp_free(next);
        return false;
    }

    // Initialize seeds
    for (uint32_t s = 0; s < num_seeds; s++) {
        if (seed_concepts[s] < num_concepts) {
            current[seed_concepts[s]] = seed_activations[s];
        }
    }

    // Iterate spreading
    for (uint32_t iter = 0; iter < iterations; iter++) {
        memset(next, 0, num_concepts * sizeof(float));

        for (uint32_t c = 0; c < num_concepts; c++) {
            // Decayed self-activation
            next[c] = current[c] * decay;

            // Spreading from neighbors would require adjacency list
            // Simplified: just decay for now
        }

        // Swap
        float* tmp = current;
        current = next;
        next = tmp;
    }

    // Copy result
    memcpy(output_activations, current, num_concepts * sizeof(float));

    nimcp_free(next);
    nimcp_free(current);

    return true;
}

#else /* !NIMCP_ENABLE_CUDA */

//=============================================================================
// Stub implementations when CUDA is not enabled
//=============================================================================

#include "gpu/cognitive/nimcp_wernicke_gpu.h"
#include <stdlib.h>
#include <string.h>

/* P3-W1: Named constant for spectral feature dimensions (stub section) */
#define WERNICKE_SPECTRAL_FEATURE_DIM  82

wernicke_gpu_config_t wernicke_gpu_default_config(void) {
    wernicke_gpu_config_t config;
    memset(&config, 0, sizeof(config));
    return config;
}

wernicke_gpu_context_t* wernicke_gpu_create(
    nimcp_gpu_context_t* gpu_ctx,
    const wernicke_gpu_config_t* config
) {
    (void)gpu_ctx;
    (void)config;
    return NULL;
}

void wernicke_gpu_destroy(wernicke_gpu_context_t* ctx) {
    (void)ctx;
}

bool wernicke_gpu_synchronize(wernicke_gpu_context_t* ctx) {
    (void)ctx;
    return false;
}

bool wernicke_gpu_upload_phoneme_embeddings(
    wernicke_gpu_context_t* ctx,
    const float* embeddings,
    uint32_t num_phonemes,
    uint32_t embed_dim
) {
    (void)ctx; (void)embeddings; (void)num_phonemes; (void)embed_dim;
    return false;
}

bool wernicke_gpu_recognize_phonemes(
    wernicke_gpu_context_t* ctx,
    const wernicke_gpu_spectral_frame_t* frames,
    uint32_t num_frames,
    wernicke_gpu_phoneme_result_t* results
) {
    (void)ctx; (void)frames; (void)num_frames; (void)results;
    return false;
}

bool wernicke_gpu_compute_posteriors(
    wernicke_gpu_context_t* ctx,
    const wernicke_gpu_spectral_frame_t* frames,
    uint32_t num_frames,
    float* posteriors
) {
    (void)ctx; (void)frames; (void)num_frames; (void)posteriors;
    return false;
}

bool wernicke_gpu_upload_lexicon(
    wernicke_gpu_context_t* ctx,
    const wernicke_gpu_lexical_entry_t* entries,
    uint32_t count
) {
    (void)ctx; (void)entries; (void)count;
    return false;
}

bool wernicke_gpu_clear_lexicon(wernicke_gpu_context_t* ctx) {
    (void)ctx;
    return false;
}

uint32_t wernicke_gpu_get_lexicon_size(const wernicke_gpu_context_t* ctx) {
    (void)ctx;
    return 0;
}

bool wernicke_gpu_recognize_words(
    wernicke_gpu_context_t* ctx,
    const uint8_t* phonemes,
    uint32_t num_phonemes,
    wernicke_gpu_word_candidate_t* candidates,
    uint32_t max_candidates,
    uint32_t* num_candidates
) {
    (void)ctx; (void)phonemes; (void)num_phonemes;
    (void)candidates; (void)max_candidates; (void)num_candidates;
    return false;
}

bool wernicke_gpu_update_cohort(
    wernicke_gpu_context_t* ctx,
    uint8_t new_phoneme,
    float phoneme_confidence
) {
    (void)ctx; (void)new_phoneme; (void)phoneme_confidence;
    return false;
}

bool wernicke_gpu_get_cohort(
    wernicke_gpu_context_t* ctx,
    wernicke_gpu_word_candidate_t* candidates,
    uint32_t max_candidates,
    uint32_t* num_candidates
) {
    (void)ctx; (void)candidates; (void)max_candidates; (void)num_candidates;
    return false;
}

bool wernicke_gpu_reset_cohort(wernicke_gpu_context_t* ctx) {
    (void)ctx;
    return false;
}

bool wernicke_gpu_upload_semantic_network(
    wernicke_gpu_context_t* ctx,
    const wernicke_gpu_concept_t* concepts,
    uint32_t num_concepts,
    const uint32_t* adjacency_matrix,
    const float* weights
) {
    (void)ctx; (void)concepts; (void)num_concepts;
    (void)adjacency_matrix; (void)weights;
    return false;
}

bool wernicke_gpu_spread_activation(
    wernicke_gpu_context_t* ctx,
    const uint32_t* seed_concepts,
    const float* seed_activations,
    uint32_t num_seeds,
    wernicke_gpu_activation_result_t* results,
    uint32_t max_results,
    uint32_t* num_results
) {
    (void)ctx; (void)seed_concepts; (void)seed_activations; (void)num_seeds;
    (void)results; (void)max_results; (void)num_results;
    return false;
}

bool wernicke_gpu_get_top_activated(
    wernicke_gpu_context_t* ctx,
    uint32_t top_k,
    wernicke_gpu_activation_result_t* results,
    uint32_t* actual_count
) {
    (void)ctx; (void)top_k; (void)results; (void)actual_count;
    return false;
}

float wernicke_gpu_semantic_similarity(
    wernicke_gpu_context_t* ctx,
    uint32_t concept_a,
    uint32_t concept_b
) {
    (void)ctx; (void)concept_a; (void)concept_b;
    return 0.0f;
}

bool wernicke_gpu_wm_push(
    wernicke_gpu_context_t* ctx,
    const uint8_t* phonemes,
    uint32_t count
) {
    (void)ctx; (void)phonemes; (void)count;
    return false;
}

bool wernicke_gpu_wm_get_contents(
    wernicke_gpu_context_t* ctx,
    uint8_t* phonemes,
    float* activations,
    uint32_t max_count,
    uint32_t* actual_count
) {
    (void)ctx; (void)phonemes; (void)activations; (void)max_count; (void)actual_count;
    return false;
}

bool wernicke_gpu_wm_rehearse(wernicke_gpu_context_t* ctx) {
    (void)ctx;
    return false;
}

bool wernicke_gpu_wm_apply_decay(
    wernicke_gpu_context_t* ctx,
    float decay_factor,
    float threshold
) {
    (void)ctx; (void)decay_factor; (void)threshold;
    return false;
}

bool wernicke_gpu_wm_clear(wernicke_gpu_context_t* ctx) {
    (void)ctx;
    return false;
}

bool wernicke_gpu_comprehend(
    wernicke_gpu_context_t* ctx,
    const wernicke_gpu_spectral_frame_t* frames,
    uint32_t num_frames,
    wernicke_gpu_word_candidate_t* word_candidates,
    uint32_t max_word_candidates,
    uint32_t* num_word_candidates,
    wernicke_gpu_activation_result_t* semantic_activations,
    uint32_t max_semantic_activations,
    uint32_t* num_semantic_activations
) {
    (void)ctx; (void)frames; (void)num_frames;
    (void)word_candidates; (void)max_word_candidates; (void)num_word_candidates;
    (void)semantic_activations; (void)max_semantic_activations; (void)num_semantic_activations;
    return false;
}

bool wernicke_gpu_get_stats(
    const wernicke_gpu_context_t* ctx,
    wernicke_gpu_stats_t* stats
) {
    (void)ctx; (void)stats;
    return false;
}

void wernicke_gpu_reset_stats(wernicke_gpu_context_t* ctx) {
    (void)ctx;
}

// CPU fallback implementations are always available
bool wernicke_cpu_recognize_phonemes(
    const wernicke_gpu_spectral_frame_t* frames,
    uint32_t num_frames,
    const float* phoneme_embeddings,
    uint32_t num_phonemes,
    uint32_t embed_dim,
    wernicke_gpu_phoneme_result_t* results
) {
    if (!frames || !phoneme_embeddings || !results) return false;

    const uint32_t feature_dim = WERNICKE_SPECTRAL_FEATURE_DIM;

    for (uint32_t f = 0; f < num_frames; f++) {
        const float* features = (const float*)&frames[f];
        float max_sim = -1e30f;
        uint8_t best_phoneme = 0;

        for (uint32_t p = 0; p < num_phonemes; p++) {
            float sim = 0.0f;
            uint32_t min_dim = (feature_dim < embed_dim) ? feature_dim : embed_dim;

            for (uint32_t d = 0; d < min_dim; d++) {
                sim += features[d] * phoneme_embeddings[p * embed_dim + d];
            }

            if (sim > max_sim) {
                max_sim = sim;
                best_phoneme = (uint8_t)p;
            }
        }

        results[f].phoneme_id = best_phoneme;
        results[f].confidence = max_sim;
        results[f].posterior = NULL;
    }

    return true;
}

bool wernicke_cpu_recognize_words(
    const wernicke_gpu_lexical_entry_t* lexicon,
    uint32_t lexicon_size,
    const uint8_t* phonemes,
    uint32_t num_phonemes,
    wernicke_gpu_word_candidate_t* candidates,
    uint32_t max_candidates,
    uint32_t* num_candidates
) {
    if (!lexicon || !phonemes || !candidates || !num_candidates) return false;

    *num_candidates = 0;

    for (uint32_t w = 0; w < lexicon_size && *num_candidates < max_candidates; w++) {
        const wernicke_gpu_lexical_entry_t* entry = &lexicon[w];

        if (entry->phoneme_count != num_phonemes) continue;

        bool match = true;
        for (uint32_t p = 0; p < num_phonemes; p++) {
            if (entry->phonemes[p] != phonemes[p]) {
                match = false;
                break;
            }
        }

        if (match) {
            uint32_t idx = (*num_candidates)++;
            candidates[idx].word_id = entry->word_id;
            candidates[idx].cohort_probability = entry->frequency;
            candidates[idx].uniqueness_point = 1.0f;
            candidates[idx].matched_phonemes = (uint8_t)num_phonemes;
            candidates[idx].recognition_complete = true;
        }
    }

    return true;
}

bool wernicke_cpu_spread_activation(
    const float* adjacency_weights,
    uint32_t num_concepts,
    uint32_t max_neighbors,
    const uint32_t* seed_concepts,
    const float* seed_activations,
    uint32_t num_seeds,
    uint32_t iterations,
    float decay,
    float* output_activations
) {
    (void)max_neighbors;
    if (!adjacency_weights || !seed_concepts || !seed_activations || !output_activations) {
        return false;
    }

    // Initialize to zero
    memset(output_activations, 0, num_concepts * sizeof(float));

    // Set seeds
    for (uint32_t s = 0; s < num_seeds; s++) {
        if (seed_concepts[s] < num_concepts) {
            output_activations[seed_concepts[s]] = seed_activations[s];
        }
    }

    // Simple decay-only spreading for stub
    for (uint32_t iter = 0; iter < iterations; iter++) {
        for (uint32_t c = 0; c < num_concepts; c++) {
            output_activations[c] *= decay;
        }
    }

    return true;
}

#endif /* NIMCP_ENABLE_CUDA */
