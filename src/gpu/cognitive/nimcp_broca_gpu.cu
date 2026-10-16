/**
 * @file nimcp_broca_gpu.cu
 * @brief GPU CUDA Kernels for Broca's Region Language Production
 *
 * WHAT: CUDA kernels for GPU-accelerated Broca's region operations
 * WHY:  GPU acceleration for parallel lexical, phonological, and motor processing
 * HOW:  Custom kernels for batch word lookup, phoneme encoding, motor planning
 *
 * BIOLOGICAL BASIS:
 * =================
 * Broca's region (BA44/45) processes language production in parallel:
 * - Lexical access: Multiple word candidates activated simultaneously
 * - Phonological encoding: Phoneme sequences for coarticulation
 * - Motor planning: Coordinated articulator commands
 *
 * GPU ACCELERATION RATIONALE:
 * ===========================
 * Language production benefits from massive parallelism:
 * - Lexicon search: O(1) parallel lookup vs O(n) sequential
 * - Phoneme encoding: All words processed simultaneously
 * - Motor commands: All articulators planned in parallel
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

#include <cuda_runtime.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "gpu/cognitive/nimcp_broca_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/memory/nimcp_memory.h"

#define LOG_MODULE "BROCA_GPU"

#define BLOCK_SIZE 256
#define WARP_SIZE 32
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)

// Number of articulators for motor planning
#define NUM_ARTICULATORS 6

// Phoneme-to-articulator mapping constants
#define ARTICULATOR_JAW      0
#define ARTICULATOR_LIPS     1
#define ARTICULATOR_TONGUE   2
#define ARTICULATOR_VELUM    3
#define ARTICULATOR_LARYNX   4
#define ARTICULATOR_LUNGS    5

//=============================================================================
// Internal GPU Broca Context
//=============================================================================

struct broca_gpu_context {
    nimcp_gpu_context_t* gpu_ctx;
    broca_gpu_config_t config;

    // GPU lexicon
    broca_gpu_lexical_entry_t* d_lexicon;
    uint32_t lexicon_size;
    uint32_t lexicon_capacity;

    // GPU working memory
    uint32_t* d_wm_word_ids;
    float* d_wm_activations;
    uint32_t wm_count;

    // Temporary buffers
    uint32_t* d_temp_word_ids;
    uint8_t* d_temp_phonemes;
    broca_gpu_motor_command_t* d_temp_commands;
    broca_gpu_lookup_result_t* d_temp_results;
    uint32_t* d_temp_boundaries;
    uint32_t* d_temp_counts;

    // CUDA stream for async operations
    cudaStream_t stream;

    // Statistics
    broca_gpu_stats_t stats;
};

//=============================================================================
// CUDA Kernels: Lexical Lookup
//=============================================================================

/**
 * @brief Parallel lexical lookup kernel
 *
 * Each thread looks up one word_id in the lexicon
 */
__global__ void kernel_batch_lexical_lookup(
    const broca_gpu_lexical_entry_t* __restrict__ lexicon,
    uint32_t lexicon_size,
    const uint32_t* __restrict__ word_ids,
    uint32_t query_count,
    broca_gpu_lookup_result_t* __restrict__ results
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= query_count) return;

    uint32_t target_id = word_ids[idx];
    results[idx].word_id = target_id;
    results[idx].found = false;
    results[idx].phoneme_count = 0;

    // Linear search (can be optimized with hash table)
    for (uint32_t i = 0; i < lexicon_size; i++) {
        if (lexicon[i].word_id == target_id) {
            results[idx].found = true;
            results[idx].phoneme_count = lexicon[i].phoneme_count;
            results[idx].frequency = lexicon[i].frequency;

            // Copy phonemes
            for (uint32_t p = 0; p < 16; p++) {
                results[idx].phonemes[p] = lexicon[i].phonemes[p];
            }
            break;
        }
    }
}

/**
 * @brief Find entries with highest activation (parallel reduction)
 */
__global__ void kernel_find_max_activation(
    const broca_gpu_lexical_entry_t* __restrict__ lexicon,
    uint32_t lexicon_size,
    float* __restrict__ max_activations,
    uint32_t* __restrict__ max_indices
) {
    extern __shared__ float shared_data[];
    float* s_max = shared_data;
    uint32_t* s_idx = (uint32_t*)&s_max[blockDim.x];

    uint32_t tid = threadIdx.x;
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;

    // Initialize
    if (i < lexicon_size) {
        s_max[tid] = lexicon[i].activation;
        s_idx[tid] = i;
    } else {
        s_max[tid] = -1.0f;
        s_idx[tid] = 0;
    }
    __syncthreads();

    // Reduction
    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s && s_max[tid + s] > s_max[tid]) {
            s_max[tid] = s_max[tid + s];
            s_idx[tid] = s_idx[tid + s];
        }
        __syncthreads();
    }

    // Write result
    if (tid == 0) {
        max_activations[blockIdx.x] = s_max[0];
        max_indices[blockIdx.x] = s_idx[0];
    }
}

/**
 * @brief Update lexical activations kernel
 */
__global__ void kernel_update_activations(
    broca_gpu_lexical_entry_t* __restrict__ lexicon,
    uint32_t lexicon_size,
    const uint32_t* __restrict__ boost_word_ids,
    uint32_t boost_count,
    float boost_amount,
    float decay_rate
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= lexicon_size) return;

    // Apply decay
    lexicon[idx].activation *= decay_rate;

    // Check if this entry should be boosted
    uint32_t my_word_id = lexicon[idx].word_id;
    for (uint32_t i = 0; i < boost_count; i++) {
        if (boost_word_ids[i] == my_word_id) {
            lexicon[idx].activation += boost_amount;
            if (lexicon[idx].activation > 1.0f) {
                lexicon[idx].activation = 1.0f;
            }
            break;
        }
    }
}

//=============================================================================
// CUDA Kernels: Phoneme Encoding
//=============================================================================

/**
 * @brief Parallel phoneme encoding kernel
 *
 * Each thread processes one word, writing phonemes to output buffer
 * Uses prefix sum (scan) for proper output placement
 */
__global__ void kernel_encode_phonemes(
    const broca_gpu_lexical_entry_t* __restrict__ lexicon,
    uint32_t lexicon_size,
    const uint32_t* __restrict__ word_ids,
    uint32_t word_count,
    uint8_t* __restrict__ phoneme_buffer,
    uint32_t* __restrict__ word_offsets,
    uint32_t* __restrict__ phoneme_counts
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= word_count) return;

    uint32_t target_id = word_ids[idx];
    uint32_t offset = (idx > 0) ? word_offsets[idx - 1] : 0;
    uint32_t count = 0;

    // Find word in lexicon and copy phonemes
    for (uint32_t i = 0; i < lexicon_size; i++) {
        if (lexicon[i].word_id == target_id) {
            count = lexicon[i].phoneme_count;
            for (uint32_t p = 0; p < count; p++) {
                phoneme_buffer[offset + p] = lexicon[i].phonemes[p];
            }
            break;
        }
    }

    phoneme_counts[idx] = count;
}

/**
 * @brief Compute phoneme offsets (prefix sum)
 */
__global__ void kernel_compute_offsets(
    const broca_gpu_lexical_entry_t* __restrict__ lexicon,
    uint32_t lexicon_size,
    const uint32_t* __restrict__ word_ids,
    uint32_t word_count,
    uint32_t* __restrict__ offsets
) {
    // Simple sequential scan (can be parallelized with proper prefix sum)
    if (threadIdx.x == 0 && blockIdx.x == 0) {
        uint32_t running_sum = 0;
        for (uint32_t w = 0; w < word_count; w++) {
            uint32_t target_id = word_ids[w];
            uint32_t count = 0;

            for (uint32_t i = 0; i < lexicon_size; i++) {
                if (lexicon[i].word_id == target_id) {
                    count = lexicon[i].phoneme_count;
                    break;
                }
            }

            running_sum += count;
            offsets[w] = running_sum;
        }
    }
}

/**
 * @brief Apply coarticulation effects kernel
 */
__global__ void kernel_apply_coarticulation(
    uint8_t* __restrict__ phonemes,
    uint32_t phoneme_count,
    float strength
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= phoneme_count || idx == 0) return;

    // Simple coarticulation: blend with neighbors
    // This is a simplified model - real coarticulation is more complex
    uint8_t prev = phonemes[idx - 1];
    uint8_t curr = phonemes[idx];

    // Blend phoneme features (placeholder - real impl would use phonetic features)
    // For now, we just modify based on neighbor relationship
    if (strength > 0.5f && prev != curr) {
        // Could modify phoneme features here
        // Currently a no-op placeholder for extensibility
    }
}

//=============================================================================
// CUDA Kernels: Motor Command Generation
//=============================================================================

/**
 * @brief Generate motor commands for phonemes
 *
 * Each thread generates commands for one phoneme
 * Produces NUM_ARTICULATORS commands per phoneme
 */
__global__ void kernel_generate_motor_commands(
    const uint8_t* __restrict__ phonemes,
    uint32_t phoneme_count,
    broca_gpu_motor_command_t* __restrict__ commands,
    float base_timestamp,
    float phoneme_duration_ms
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= phoneme_count) return;

    uint8_t phoneme = phonemes[idx];
    float timestamp = base_timestamp + idx * phoneme_duration_ms;
    uint32_t cmd_base = idx * NUM_ARTICULATORS;

    // Generate command for each articulator
    // Positions based on phoneme (simplified phonetic model)
    for (uint32_t a = 0; a < NUM_ARTICULATORS; a++) {
        broca_gpu_motor_command_t* cmd = &commands[cmd_base + a];
        cmd->articulator = (uint8_t)a;
        cmd->phoneme = phoneme;
        cmd->timestamp_ms = timestamp;

        // Simple phoneme-to-position mapping
        // Real implementation would use proper articulatory phonetics
        float phoneme_norm = (float)phoneme / 255.0f;

        switch (a) {
            case ARTICULATOR_JAW:
                // Jaw opening varies with vowel height
                cmd->position = 0.3f + 0.4f * phoneme_norm;
                cmd->velocity = 0.5f;
                break;
            case ARTICULATOR_LIPS:
                // Lip rounding
                cmd->position = 0.2f + 0.6f * phoneme_norm;
                cmd->velocity = 0.6f;
                break;
            case ARTICULATOR_TONGUE:
                // Tongue position
                cmd->position = 0.4f + 0.3f * phoneme_norm;
                cmd->velocity = 0.7f;
                break;
            case ARTICULATOR_VELUM:
                // Velum (nasalization)
                cmd->position = phoneme_norm > 0.5f ? 0.8f : 0.2f;
                cmd->velocity = 0.4f;
                break;
            case ARTICULATOR_LARYNX:
                // Voicing
                cmd->position = 0.5f;
                cmd->velocity = 0.3f;
                break;
            case ARTICULATOR_LUNGS:
                // Airflow
                cmd->position = 0.7f;
                cmd->velocity = 0.2f;
                break;
        }
    }
}

/**
 * @brief Adjust motor command timing
 */
__global__ void kernel_adjust_timing(
    broca_gpu_motor_command_t* __restrict__ commands,
    uint32_t command_count,
    float rate_multiplier
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= command_count) return;

    commands[idx].timestamp_ms *= rate_multiplier;
}

//=============================================================================
// CUDA Kernels: Working Memory
//=============================================================================

/**
 * @brief Apply decay to working memory activations
 */
__global__ void kernel_wm_decay(
    float* __restrict__ activations,
    uint32_t count,
    float decay_factor
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= count) return;

    activations[idx] *= decay_factor;
}

//=============================================================================
// API Implementation: Configuration
//=============================================================================

extern "C" broca_gpu_config_t broca_gpu_default_config(void) {
    broca_gpu_config_t config;
    config.max_lexicon_size = 10000;
    config.max_batch_size = 256;
    config.max_phonemes_per_word = 16;
    config.max_articulators = NUM_ARTICULATORS;
    config.working_memory_slots = 16;
    config.enable_coarticulation = true;
    config.enable_async_transfer = true;
    config.activation_decay_rate = 0.95f;
    return config;
}

//=============================================================================
// API Implementation: Lifecycle
//=============================================================================

extern "C" broca_gpu_context_t* broca_gpu_create(
    nimcp_gpu_context_t* gpu_ctx,
    const broca_gpu_config_t* config
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!gpu_ctx) {
        LOG_ERROR("NULL GPU context");
        return NULL;
    }

    broca_gpu_context_t* ctx = (broca_gpu_context_t*)nimcp_calloc(1, sizeof(broca_gpu_context_t));
    if (!ctx) {
        LOG_ERROR("Failed to allocate Broca GPU context");
        return NULL;
    }

    ctx->gpu_ctx = gpu_ctx;
    ctx->config = config ? *config : broca_gpu_default_config();

    // Create CUDA stream
    cudaError_t err = cudaStreamCreate(&ctx->stream);
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to create CUDA stream: %s", cudaGetErrorString(err));
        nimcp_free(ctx);
        return NULL;
    }

    // Allocate GPU lexicon
    err = cudaMalloc(&ctx->d_lexicon,
                     ctx->config.max_lexicon_size * sizeof(broca_gpu_lexical_entry_t));
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to allocate GPU lexicon: %s", cudaGetErrorString(err));
        cudaStreamDestroy(ctx->stream);
        nimcp_free(ctx);
        return NULL;
    }
    ctx->lexicon_capacity = ctx->config.max_lexicon_size;
    ctx->lexicon_size = 0;

    // Pre-calculate buffer sizes for goto-safe declarations
    uint32_t max_phonemes = ctx->config.max_batch_size * ctx->config.max_phonemes_per_word;
    uint32_t max_commands = max_phonemes * ctx->config.max_articulators;

    // Allocate GPU working memory
    err = cudaMalloc(&ctx->d_wm_word_ids,
                     ctx->config.working_memory_slots * sizeof(uint32_t));
    if (err != cudaSuccess) goto cleanup;

    err = cudaMalloc(&ctx->d_wm_activations,
                     ctx->config.working_memory_slots * sizeof(float));
    if (err != cudaSuccess) goto cleanup;

    // Allocate temporary buffers
    err = cudaMalloc(&ctx->d_temp_word_ids,
                     ctx->config.max_batch_size * sizeof(uint32_t));
    if (err != cudaSuccess) goto cleanup;

    err = cudaMalloc(&ctx->d_temp_phonemes, max_phonemes * sizeof(uint8_t));
    if (err != cudaSuccess) goto cleanup;

    err = cudaMalloc(&ctx->d_temp_commands,
                     max_commands * sizeof(broca_gpu_motor_command_t));
    if (err != cudaSuccess) goto cleanup;

    err = cudaMalloc(&ctx->d_temp_results,
                     ctx->config.max_batch_size * sizeof(broca_gpu_lookup_result_t));
    if (err != cudaSuccess) goto cleanup;

    err = cudaMalloc(&ctx->d_temp_boundaries,
                     ctx->config.max_batch_size * sizeof(uint32_t));
    if (err != cudaSuccess) goto cleanup;

    err = cudaMalloc(&ctx->d_temp_counts,
                     ctx->config.max_batch_size * sizeof(uint32_t));
    if (err != cudaSuccess) goto cleanup;

    LOG_INFO("GPU Broca context created (lexicon: %u, WM slots: %u)",
             ctx->config.max_lexicon_size, ctx->config.working_memory_slots);

    return ctx;

cleanup:
    LOG_ERROR("Failed to allocate GPU memory: %s", cudaGetErrorString(err));
    broca_gpu_destroy(ctx);
    return NULL;
}

extern "C" void broca_gpu_destroy(broca_gpu_context_t* ctx) {
    if (!ctx) return;

    if (ctx->stream) {
        cudaStreamSynchronize(ctx->stream);
        cudaStreamDestroy(ctx->stream);
    }

    cudaFree(ctx->d_lexicon);
    cudaFree(ctx->d_wm_word_ids);
    cudaFree(ctx->d_wm_activations);
    cudaFree(ctx->d_temp_word_ids);
    cudaFree(ctx->d_temp_phonemes);
    cudaFree(ctx->d_temp_commands);
    cudaFree(ctx->d_temp_results);
    cudaFree(ctx->d_temp_boundaries);
    cudaFree(ctx->d_temp_counts);

    nimcp_free(ctx);
    LOG_DEBUG("GPU Broca context destroyed");
}

extern "C" bool broca_gpu_synchronize(broca_gpu_context_t* ctx) {
    if (!ctx) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

//=============================================================================
// API Implementation: Lexicon Management
//=============================================================================

extern "C" bool broca_gpu_upload_lexicon(
    broca_gpu_context_t* ctx,
    const broca_gpu_lexical_entry_t* entries,
    uint32_t count
) {
    if (!ctx || !entries) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }
    if (count > ctx->lexicon_capacity) {
        LOG_ERROR("Lexicon overflow: %u > %u", count, ctx->lexicon_capacity);
        return false;
    }

    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(ctx->d_lexicon, entries,
                               count * sizeof(broca_gpu_lexical_entry_t),
                               cudaMemcpyHostToDevice, ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    ctx->lexicon_size = count;
    LOG_DEBUG("Uploaded %u lexicon entries to GPU", count);
    return true;
}

extern "C" bool broca_gpu_clear_lexicon(broca_gpu_context_t* ctx) {
    if (!ctx) return false;
    ctx->lexicon_size = 0;
    return true;
}

extern "C" uint32_t broca_gpu_get_lexicon_size(const broca_gpu_context_t* ctx) {
    return ctx ? ctx->lexicon_size : 0;
}

//=============================================================================
// API Implementation: Lexical Lookup
//=============================================================================

extern "C" bool broca_gpu_batch_lexical_lookup(
    broca_gpu_context_t* ctx,
    const uint32_t* word_ids,
    uint32_t count,
    broca_gpu_lookup_result_t* results
) {
    if (!ctx || !word_ids || !results || count == 0) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }
    if (ctx->lexicon_size == 0) return false;  // Empty lexicon
    if (count > ctx->config.max_batch_size) {
        LOG_ERROR("Batch size overflow: %u > %u", count, ctx->config.max_batch_size);
        return false;
    }

    // Upload word IDs
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(ctx->d_temp_word_ids, word_ids,
                               count * sizeof(uint32_t),
                               cudaMemcpyHostToDevice, ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    // Launch lookup kernel
    kernel_batch_lexical_lookup<<<GRID_SIZE(count), BLOCK_SIZE, 0, ctx->stream>>>(
        ctx->d_lexicon, ctx->lexicon_size,
        ctx->d_temp_word_ids, count,
        ctx->d_temp_results
    );
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Download results
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(results, ctx->d_temp_results,
                               count * sizeof(broca_gpu_lookup_result_t),
                               cudaMemcpyDeviceToHost, ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    ctx->stats.lexical_lookups += count;
    return true;
}

extern "C" bool broca_gpu_update_activations(
    broca_gpu_context_t* ctx,
    const uint32_t* word_ids,
    uint32_t count,
    float boost_amount,
    float decay_rate
) {
    if (!ctx) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }
    if (ctx->lexicon_size == 0) return true;

    uint32_t* d_boost_ids = NULL;
    if (word_ids && count > 0) {
        NIMCP_CUDA_RECOVER(cudaMemcpyAsync(ctx->d_temp_word_ids, word_ids,
                                   count * sizeof(uint32_t),
                                   cudaMemcpyHostToDevice, ctx->stream), GPU_ERROR_CUDA_RUNTIME);
        d_boost_ids = ctx->d_temp_word_ids;
    }

    kernel_update_activations<<<GRID_SIZE(ctx->lexicon_size), BLOCK_SIZE, 0, ctx->stream>>>(
        ctx->d_lexicon, ctx->lexicon_size,
        d_boost_ids, count,
        boost_amount, decay_rate
    );
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

extern "C" bool broca_gpu_find_top_activated(
    broca_gpu_context_t* ctx,
    uint32_t top_n,
    broca_gpu_lexical_entry_t* results,
    uint32_t* actual_count
) {
    if (!ctx || !results || !actual_count) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Simplified: just return first top_n entries with highest activation
    // Real implementation would use parallel sorting/selection

    *actual_count = (top_n < ctx->lexicon_size) ? top_n : ctx->lexicon_size;

    NIMCP_CUDA_RECOVER(cudaMemcpy(results, ctx->d_lexicon,
                          *actual_count * sizeof(broca_gpu_lexical_entry_t),
                          cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);

    return true;
}

//=============================================================================
// API Implementation: Phoneme Encoding
//=============================================================================

extern "C" bool broca_gpu_encode_phonemes(
    broca_gpu_context_t* ctx,
    const uint32_t* word_ids,
    uint32_t word_count,
    uint8_t* phoneme_buffer,
    uint32_t buffer_size,
    uint32_t* phoneme_count,
    uint32_t* word_boundaries
) {
    if (!ctx || !word_ids || !phoneme_buffer || !phoneme_count) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }
    if (word_count == 0) {
        *phoneme_count = 0;
        return true;
    }
    if (word_count > ctx->config.max_batch_size) {
        LOG_ERROR("Word count overflow: %u > %u", word_count, ctx->config.max_batch_size);
        return false;
    }

    // Upload word IDs
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(ctx->d_temp_word_ids, word_ids,
                               word_count * sizeof(uint32_t),
                               cudaMemcpyHostToDevice, ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    // Compute offsets (prefix sum)
    kernel_compute_offsets<<<1, 1, 0, ctx->stream>>>(
        ctx->d_lexicon, ctx->lexicon_size,
        ctx->d_temp_word_ids, word_count,
        ctx->d_temp_boundaries
    );
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Encode phonemes
    kernel_encode_phonemes<<<GRID_SIZE(word_count), BLOCK_SIZE, 0, ctx->stream>>>(
        ctx->d_lexicon, ctx->lexicon_size,
        ctx->d_temp_word_ids, word_count,
        ctx->d_temp_phonemes,
        ctx->d_temp_boundaries,
        ctx->d_temp_counts
    );
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Download boundaries to get total count
    uint32_t* h_boundaries = (uint32_t*)nimcp_malloc(word_count * sizeof(uint32_t));
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(h_boundaries, ctx->d_temp_boundaries,
                               word_count * sizeof(uint32_t),
                               cudaMemcpyDeviceToHost, ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    uint32_t total_phonemes = h_boundaries[word_count - 1];
    if (total_phonemes > buffer_size) {
        LOG_ERROR("Phoneme buffer overflow: %u > %u", total_phonemes, buffer_size);
        nimcp_free(h_boundaries);
        return false;
    }

    // Download phonemes
    NIMCP_CUDA_RECOVER(cudaMemcpy(phoneme_buffer, ctx->d_temp_phonemes,
                          total_phonemes * sizeof(uint8_t),
                          cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);

    *phoneme_count = total_phonemes;

    if (word_boundaries) {
        memcpy(word_boundaries, h_boundaries, word_count * sizeof(uint32_t));
    }

    nimcp_free(h_boundaries);

    ctx->stats.phonemes_encoded += total_phonemes;
    return true;
}

extern "C" bool broca_gpu_apply_coarticulation(
    broca_gpu_context_t* ctx,
    uint8_t* phonemes,
    uint32_t phoneme_count,
    float coarticulation_strength
) {
    if (!ctx || !phonemes || phoneme_count == 0) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Upload phonemes
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(ctx->d_temp_phonemes, phonemes,
                               phoneme_count * sizeof(uint8_t),
                               cudaMemcpyHostToDevice, ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    // Apply coarticulation
    kernel_apply_coarticulation<<<GRID_SIZE(phoneme_count), BLOCK_SIZE, 0, ctx->stream>>>(
        ctx->d_temp_phonemes, phoneme_count, coarticulation_strength
    );
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Download results
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(phonemes, ctx->d_temp_phonemes,
                               phoneme_count * sizeof(uint8_t),
                               cudaMemcpyDeviceToHost, ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

//=============================================================================
// API Implementation: Motor Command Generation
//=============================================================================

extern "C" bool broca_gpu_generate_motor_commands(
    broca_gpu_context_t* ctx,
    const uint8_t* phonemes,
    uint32_t phoneme_count,
    broca_gpu_motor_command_t* commands,
    uint32_t max_commands,
    uint32_t* command_count,
    float base_timestamp
) {
    if (!ctx || !phonemes || !commands || !command_count) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }
    if (phoneme_count == 0) {
        *command_count = 0;
        return true;
    }

    uint32_t total_commands = phoneme_count * NUM_ARTICULATORS;
    if (total_commands > max_commands) {
        LOG_ERROR("Command buffer overflow: %u > %u", total_commands, max_commands);
        return false;
    }

    // Upload phonemes
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(ctx->d_temp_phonemes, phonemes,
                               phoneme_count * sizeof(uint8_t),
                               cudaMemcpyHostToDevice, ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    // Generate motor commands
    float phoneme_duration_ms = 80.0f; // Average phoneme duration
    kernel_generate_motor_commands<<<GRID_SIZE(phoneme_count), BLOCK_SIZE, 0, ctx->stream>>>(
        ctx->d_temp_phonemes, phoneme_count,
        ctx->d_temp_commands,
        base_timestamp, phoneme_duration_ms
    );
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Download commands
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(commands, ctx->d_temp_commands,
                               total_commands * sizeof(broca_gpu_motor_command_t),
                               cudaMemcpyDeviceToHost, ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    *command_count = total_commands;
    ctx->stats.motor_commands += total_commands;
    return true;
}

extern "C" bool broca_gpu_adjust_timing(
    broca_gpu_context_t* ctx,
    broca_gpu_motor_command_t* commands,
    uint32_t command_count,
    float rate_multiplier
) {
    if (!ctx || !commands || command_count == 0) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Upload commands
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(ctx->d_temp_commands, commands,
                               command_count * sizeof(broca_gpu_motor_command_t),
                               cudaMemcpyHostToDevice, ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    // Adjust timing
    kernel_adjust_timing<<<GRID_SIZE(command_count), BLOCK_SIZE, 0, ctx->stream>>>(
        ctx->d_temp_commands, command_count, rate_multiplier
    );
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Download results
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(commands, ctx->d_temp_commands,
                               command_count * sizeof(broca_gpu_motor_command_t),
                               cudaMemcpyDeviceToHost, ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

//=============================================================================
// API Implementation: Working Memory
//=============================================================================

extern "C" bool broca_gpu_wm_push(
    broca_gpu_context_t* ctx,
    const uint32_t* word_ids,
    uint32_t count,
    float initial_activation
) {
    if (!ctx || !word_ids || count == 0) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Calculate available space
    uint32_t available = ctx->config.working_memory_slots - ctx->wm_count;
    if (available == 0) {
        // WM is full - circular buffer: shift old entries and make room
        uint32_t shift = (count > ctx->config.working_memory_slots) ?
                         ctx->config.working_memory_slots : count;
        uint32_t keep = ctx->config.working_memory_slots - shift;

        if (keep > 0) {
            // Shift remaining entries to the start
            NIMCP_CUDA_RECOVER(cudaMemcpyAsync(ctx->d_wm_word_ids,
                                       ctx->d_wm_word_ids + shift,
                                       keep * sizeof(uint32_t),
                                       cudaMemcpyDeviceToDevice, ctx->stream), GPU_ERROR_CUDA_RUNTIME);
            NIMCP_CUDA_RECOVER(cudaMemcpyAsync(ctx->d_wm_activations,
                                       ctx->d_wm_activations + shift,
                                       keep * sizeof(float),
                                       cudaMemcpyDeviceToDevice, ctx->stream), GPU_ERROR_CUDA_RUNTIME);
        }
        ctx->wm_count = keep;
        available = shift;
    }

    // Limit count to available space
    uint32_t copy_count = (count > available) ? available : count;

    // Upload word IDs
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(ctx->d_wm_word_ids + ctx->wm_count, word_ids,
                               copy_count * sizeof(uint32_t),
                               cudaMemcpyHostToDevice, ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    // Set activations
    float* h_activations = (float*)nimcp_malloc(copy_count * sizeof(float));
    for (uint32_t i = 0; i < copy_count; i++) {
        h_activations[i] = initial_activation;
    }
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(ctx->d_wm_activations + ctx->wm_count, h_activations,
                               copy_count * sizeof(float),
                               cudaMemcpyHostToDevice, ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    nimcp_free(h_activations);

    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    ctx->wm_count += copy_count;
    ctx->stats.wm_operations++;
    return true;
}

extern "C" bool broca_gpu_wm_get_contents(
    broca_gpu_context_t* ctx,
    uint32_t* word_ids,
    float* activations,
    uint32_t max_count,
    uint32_t* actual_count
) {
    if (!ctx || !word_ids || !actual_count) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    *actual_count = (ctx->wm_count < max_count) ? ctx->wm_count : max_count;

    NIMCP_CUDA_RECOVER(cudaMemcpy(word_ids, ctx->d_wm_word_ids,
                          *actual_count * sizeof(uint32_t),
                          cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);

    if (activations) {
        NIMCP_CUDA_RECOVER(cudaMemcpy(activations, ctx->d_wm_activations,
                              *actual_count * sizeof(float),
                              cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);
    }

    return true;
}

extern "C" bool broca_gpu_wm_apply_decay(
    broca_gpu_context_t* ctx,
    float decay_factor,
    float threshold
) {
    if (!ctx) return false;  // Null context is an error
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }
    if (ctx->wm_count == 0) return true;  // Empty WM is OK

    kernel_wm_decay<<<GRID_SIZE(ctx->wm_count), BLOCK_SIZE, 0, ctx->stream>>>(
        ctx->d_wm_activations, ctx->wm_count, decay_factor
    );
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

extern "C" bool broca_gpu_wm_clear(broca_gpu_context_t* ctx) {
    if (!ctx) return false;
    ctx->wm_count = 0;
    return true;
}

//=============================================================================
// API Implementation: Full Pipeline
//=============================================================================

extern "C" bool broca_gpu_produce_utterance(
    broca_gpu_context_t* ctx,
    const uint32_t* word_ids,
    uint32_t word_count,
    broca_gpu_motor_command_t* commands,
    uint32_t max_commands,
    uint32_t* command_count,
    float base_timestamp
) {
    if (!ctx || !word_ids || !commands || !command_count) return false;

    // Step 1: Encode phonemes
    uint32_t max_phonemes = word_count * ctx->config.max_phonemes_per_word;
    uint8_t* phoneme_buffer = (uint8_t*)nimcp_malloc(max_phonemes);
    uint32_t phoneme_count = 0;

    if (!broca_gpu_encode_phonemes(ctx, word_ids, word_count,
                                   phoneme_buffer, max_phonemes,
                                   &phoneme_count, NULL)) {
        nimcp_free(phoneme_buffer);
        return false;
    }

    // Step 2: Apply coarticulation (if enabled)
    if (ctx->config.enable_coarticulation && phoneme_count > 1) {
        broca_gpu_apply_coarticulation(ctx, phoneme_buffer, phoneme_count, 0.5f);
    }

    // Step 3: Generate motor commands
    bool success = broca_gpu_generate_motor_commands(ctx, phoneme_buffer, phoneme_count,
                                                     commands, max_commands,
                                                     command_count, base_timestamp);

    nimcp_free(phoneme_buffer);
    return success;
}

//=============================================================================
// API Implementation: Statistics
//=============================================================================

extern "C" bool broca_gpu_get_stats(
    const broca_gpu_context_t* ctx,
    broca_gpu_stats_t* stats
) {
    if (!ctx || !stats) return false;
    *stats = ctx->stats;
    return true;
}

extern "C" void broca_gpu_reset_stats(broca_gpu_context_t* ctx) {
    if (ctx) {
        memset(&ctx->stats, 0, sizeof(broca_gpu_stats_t));
    }
}

//=============================================================================
// CPU Reference Implementations (for testing)
//=============================================================================

extern "C" bool broca_cpu_batch_lexical_lookup(
    const broca_gpu_lexical_entry_t* lexicon,
    uint32_t lexicon_size,
    const uint32_t* word_ids,
    uint32_t count,
    broca_gpu_lookup_result_t* results
) {
    if (!lexicon || !word_ids || !results) return false;

    for (uint32_t q = 0; q < count; q++) {
        uint32_t target_id = word_ids[q];
        results[q].word_id = target_id;
        results[q].found = false;
        results[q].phoneme_count = 0;

        for (uint32_t i = 0; i < lexicon_size; i++) {
            if (lexicon[i].word_id == target_id) {
                results[q].found = true;
                results[q].phoneme_count = lexicon[i].phoneme_count;
                results[q].frequency = lexicon[i].frequency;
                memcpy(results[q].phonemes, lexicon[i].phonemes, 16);
                break;
            }
        }
    }

    return true;
}

extern "C" bool broca_cpu_encode_phonemes(
    const broca_gpu_lexical_entry_t* lexicon,
    uint32_t lexicon_size,
    const uint32_t* word_ids,
    uint32_t word_count,
    uint8_t* phoneme_buffer,
    uint32_t buffer_size,
    uint32_t* phoneme_count,
    uint32_t* word_boundaries
) {
    if (!lexicon || !word_ids || !phoneme_buffer || !phoneme_count) return false;

    uint32_t offset = 0;
    for (uint32_t w = 0; w < word_count; w++) {
        uint32_t target_id = word_ids[w];

        for (uint32_t i = 0; i < lexicon_size; i++) {
            if (lexicon[i].word_id == target_id) {
                uint32_t count = lexicon[i].phoneme_count;
                if (offset + count > buffer_size) {
                    return false;
                }
                memcpy(phoneme_buffer + offset, lexicon[i].phonemes, count);
                offset += count;
                break;
            }
        }

        if (word_boundaries) {
            word_boundaries[w] = offset;
        }
    }

    *phoneme_count = offset;
    return true;
}

extern "C" bool broca_cpu_generate_motor_commands(
    const uint8_t* phonemes,
    uint32_t phoneme_count,
    broca_gpu_motor_command_t* commands,
    uint32_t max_commands,
    uint32_t* command_count,
    float base_timestamp,
    uint32_t num_articulators
) {
    if (!phonemes || !commands || !command_count) return false;

    uint32_t total = phoneme_count * num_articulators;
    if (total > max_commands) return false;

    float phoneme_duration_ms = 80.0f;

    for (uint32_t p = 0; p < phoneme_count; p++) {
        uint8_t phoneme = phonemes[p];
        float timestamp = base_timestamp + p * phoneme_duration_ms;

        for (uint32_t a = 0; a < num_articulators; a++) {
            broca_gpu_motor_command_t* cmd = &commands[p * num_articulators + a];
            cmd->articulator = (uint8_t)a;
            cmd->phoneme = phoneme;
            cmd->timestamp_ms = timestamp;

            float phoneme_norm = (float)phoneme / 255.0f;
            switch (a) {
                case 0: cmd->position = 0.3f + 0.4f * phoneme_norm; cmd->velocity = 0.5f; break;
                case 1: cmd->position = 0.2f + 0.6f * phoneme_norm; cmd->velocity = 0.6f; break;
                case 2: cmd->position = 0.4f + 0.3f * phoneme_norm; cmd->velocity = 0.7f; break;
                case 3: cmd->position = phoneme_norm > 0.5f ? 0.8f : 0.2f; cmd->velocity = 0.4f; break;
                case 4: cmd->position = 0.5f; cmd->velocity = 0.3f; break;
                case 5: cmd->position = 0.7f; cmd->velocity = 0.2f; break;
                default: cmd->position = 0.5f; cmd->velocity = 0.5f; break;
            }
        }
    }

    *command_count = total;
    return true;
}

#endif /* NIMCP_ENABLE_CUDA */
