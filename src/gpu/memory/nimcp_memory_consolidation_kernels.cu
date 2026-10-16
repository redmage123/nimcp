/**
 * @file nimcp_memory_consolidation_kernels.cu
 * @brief GPU-accelerated Memory Consolidation CUDA Kernels
 *
 * WHAT: CUDA kernels for memory consolidation operations
 * WHY:  GPU acceleration for hippocampal replay, systems consolidation,
 *       and memory engram operations enabling massive parallelism
 * HOW:  Custom kernels for replay, consolidation, and similarity search
 *
 * BIOLOGICAL BASIS:
 * - Sharp-wave ripples in hippocampus (Wilson & McNaughton, 1994)
 * - Complementary learning systems (McClelland et al., 1995)
 * - Systems consolidation and memory transfer
 * - Ebbinghaus forgetting curve
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

// Include CUDA headers FIRST (before any extern "C" blocks from our headers)
#include <cuda_runtime.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <curand_kernel.h>

// Now include our headers (which have extern "C" blocks)
#include "gpu/memory/nimcp_memory_consolidation_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/common/nimcp_device_utils.cuh"  // For nimcp_device_lcg_* RNG functions
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/memory/nimcp_memory.h"

#define LOG_MODULE "MEMORY_GPU"

//-----------------------------------------------------------------------------
// Helper Macros - Using self-healing recovery versions
//-----------------------------------------------------------------------------

#define CUDA_CHECK(call) NIMCP_CUDA_RECOVER(call, GPU_ERROR_CUDA_RUNTIME)
// CUDA_CHECK_VOID uses warning only since void functions can't propagate errors
#define CUDA_CHECK_VOID(call) NIMCP_CUDA_CHECK_WARN(call)

#define BLOCK_SIZE 256
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)
#define WARP_SIZE 32

//=============================================================================
// Default Parameter Functions
//=============================================================================

nimcp_replay_params_t nimcp_replay_params_default(void)
{
    nimcp_replay_params_t params;
    params.replay_strength = 0.8f;
    params.tau_decay = 20.0f;           // 20ms decay time constant
    params.noise_stddev = 0.1f;
    params.compressed_replay = true;
    params.compression_factor = 15.0f;  // 15x compressed replay (middle of 10-20x)
    return params;
}

nimcp_consolidation_params_t nimcp_consolidation_params_default(void)
{
    nimcp_consolidation_params_t params;
    params.transfer_rate = 0.01f;
    params.semantic_threshold = 0.7f;
    params.forgetting_rate = 0.001f;
    params.similarity_threshold = 0.5f;
    params.consolidation_rate_sws = 0.1f;    // 10x faster during slow-wave sleep
    params.consolidation_rate_awake = 0.01f;
    return params;
}

nimcp_engram_update_params_t nimcp_engram_update_params_default(void)
{
    nimcp_engram_update_params_t params;
    params.learning_rate = 0.01f;
    params.weight_decay = 0.0001f;
    params.momentum = 0.9f;
    params.max_weight = 1.0f;
    params.min_weight = -1.0f;
    params.use_hebbian = true;
    return params;
}

//=============================================================================
// Lifecycle Functions - Engram Batch
//=============================================================================

nimcp_gpu_engram_batch_t* nimcp_gpu_engram_batch_create(
    nimcp_gpu_context_t* ctx,
    size_t batch_size,
    size_t max_neurons)
{
    if (!ctx || batch_size == 0 || max_neurons == 0) {
        LOG_ERROR("Invalid parameters for engram batch creation");
        return NULL;
    }

    if (max_neurons > MEMORY_GPU_MAX_ENGRAM_NEURONS) {
        LOG_WARN("max_neurons %zu exceeds limit %d, clamping",
                 max_neurons, MEMORY_GPU_MAX_ENGRAM_NEURONS);
        max_neurons = MEMORY_GPU_MAX_ENGRAM_NEURONS;
    }

    nimcp_gpu_engram_batch_t* batch =
        (nimcp_gpu_engram_batch_t*)nimcp_calloc(1, sizeof(nimcp_gpu_engram_batch_t));
    if (!batch) {
        LOG_ERROR("Failed to allocate engram batch structure");
        return NULL;
    }

    batch->batch_size = batch_size;
    batch->max_neurons = max_neurons;

    // 2D tensors: [batch, max_neurons]
    size_t dims_2d[2] = {batch_size, max_neurons};
    // 1D tensors: [batch]
    size_t dims_1d[1] = {batch_size};

    batch->neuron_ids = nimcp_gpu_tensor_create(ctx, dims_2d, 2, NIMCP_GPU_PRECISION_UINT32);
    batch->activations = nimcp_gpu_tensor_create(ctx, dims_2d, 2, NIMCP_GPU_PRECISION_FP32);
    batch->consolidation_strength = nimcp_gpu_tensor_create(ctx, dims_1d, 1, NIMCP_GPU_PRECISION_FP32);
    batch->hippocampal_dependency = nimcp_gpu_tensor_create(ctx, dims_1d, 1, NIMCP_GPU_PRECISION_FP32);
    batch->emotional_salience = nimcp_gpu_tensor_create(ctx, dims_1d, 1, NIMCP_GPU_PRECISION_FP32);
    batch->memory_age = nimcp_gpu_tensor_create(ctx, dims_1d, 1, NIMCP_GPU_PRECISION_FP32);

    if (!batch->neuron_ids || !batch->activations || !batch->consolidation_strength ||
        !batch->hippocampal_dependency || !batch->emotional_salience || !batch->memory_age) {
        LOG_ERROR("Failed to allocate engram batch tensors");
        nimcp_gpu_engram_batch_destroy(batch);
        return NULL;
    }

    LOG_DEBUG("Created engram batch: size=%zu, max_neurons=%zu", batch_size, max_neurons);
    return batch;
}

void nimcp_gpu_engram_batch_destroy(nimcp_gpu_engram_batch_t* batch)
{
    if (!batch) return;

    if (batch->neuron_ids) nimcp_gpu_tensor_destroy(batch->neuron_ids);
    if (batch->activations) nimcp_gpu_tensor_destroy(batch->activations);
    if (batch->consolidation_strength) nimcp_gpu_tensor_destroy(batch->consolidation_strength);
    if (batch->hippocampal_dependency) nimcp_gpu_tensor_destroy(batch->hippocampal_dependency);
    if (batch->emotional_salience) nimcp_gpu_tensor_destroy(batch->emotional_salience);
    if (batch->memory_age) nimcp_gpu_tensor_destroy(batch->memory_age);

    nimcp_free(batch);
}

//=============================================================================
// Lifecycle Functions - Cortical Batch
//=============================================================================

nimcp_gpu_cortical_batch_t* nimcp_gpu_cortical_batch_create(
    nimcp_gpu_context_t* ctx,
    size_t batch_size,
    size_t feature_dim,
    size_t max_neighbors)
{
    if (!ctx || batch_size == 0 || feature_dim == 0) {
        LOG_ERROR("Invalid parameters for cortical batch creation");
        return NULL;
    }

    if (feature_dim > MEMORY_GPU_MAX_FEATURE_DIM) {
        LOG_WARN("feature_dim %zu exceeds limit %d, clamping",
                 feature_dim, MEMORY_GPU_MAX_FEATURE_DIM);
        feature_dim = MEMORY_GPU_MAX_FEATURE_DIM;
    }

    if (max_neighbors > MEMORY_GPU_MAX_NEIGHBORS) {
        LOG_WARN("max_neighbors %zu exceeds limit %d, clamping",
                 max_neighbors, MEMORY_GPU_MAX_NEIGHBORS);
        max_neighbors = MEMORY_GPU_MAX_NEIGHBORS;
    }

    nimcp_gpu_cortical_batch_t* batch =
        (nimcp_gpu_cortical_batch_t*)nimcp_calloc(1, sizeof(nimcp_gpu_cortical_batch_t));
    if (!batch) {
        LOG_ERROR("Failed to allocate cortical batch structure");
        return NULL;
    }

    batch->batch_size = batch_size;
    batch->feature_dim = feature_dim;
    batch->max_neighbors = max_neighbors;

    // 2D tensors
    size_t dims_features[2] = {batch_size, feature_dim};
    size_t dims_neighbors[2] = {batch_size, max_neighbors};
    // 1D tensors
    size_t dims_1d[1] = {batch_size};

    batch->features = nimcp_gpu_tensor_create(ctx, dims_features, 2, NIMCP_GPU_PRECISION_FP32);
    batch->consolidation_strength = nimcp_gpu_tensor_create(ctx, dims_1d, 1, NIMCP_GPU_PRECISION_FP32);
    batch->hippocampal_dependency = nimcp_gpu_tensor_create(ctx, dims_1d, 1, NIMCP_GPU_PRECISION_FP32);
    batch->node_type = nimcp_gpu_tensor_create(ctx, dims_1d, 1, NIMCP_GPU_PRECISION_INT32);
    batch->neighbor_indices = nimcp_gpu_tensor_create(ctx, dims_neighbors, 2, NIMCP_GPU_PRECISION_INT32);
    batch->neighbor_weights = nimcp_gpu_tensor_create(ctx, dims_neighbors, 2, NIMCP_GPU_PRECISION_FP32);

    if (!batch->features || !batch->consolidation_strength || !batch->hippocampal_dependency ||
        !batch->node_type || !batch->neighbor_indices || !batch->neighbor_weights) {
        LOG_ERROR("Failed to allocate cortical batch tensors");
        nimcp_gpu_cortical_batch_destroy(batch);
        return NULL;
    }

    LOG_DEBUG("Created cortical batch: size=%zu, feature_dim=%zu, max_neighbors=%zu",
              batch_size, feature_dim, max_neighbors);
    return batch;
}

void nimcp_gpu_cortical_batch_destroy(nimcp_gpu_cortical_batch_t* batch)
{
    if (!batch) return;

    if (batch->features) nimcp_gpu_tensor_destroy(batch->features);
    if (batch->consolidation_strength) nimcp_gpu_tensor_destroy(batch->consolidation_strength);
    if (batch->hippocampal_dependency) nimcp_gpu_tensor_destroy(batch->hippocampal_dependency);
    if (batch->node_type) nimcp_gpu_tensor_destroy(batch->node_type);
    if (batch->neighbor_indices) nimcp_gpu_tensor_destroy(batch->neighbor_indices);
    if (batch->neighbor_weights) nimcp_gpu_tensor_destroy(batch->neighbor_weights);

    nimcp_free(batch);
}

//=============================================================================
// Lifecycle Functions - Replay Batch
//=============================================================================

nimcp_gpu_replay_batch_t* nimcp_gpu_replay_batch_create(
    nimcp_gpu_context_t* ctx,
    size_t batch_size)
{
    if (!ctx || batch_size == 0) {
        LOG_ERROR("Invalid parameters for replay batch creation");
        return NULL;
    }

    nimcp_gpu_replay_batch_t* batch =
        (nimcp_gpu_replay_batch_t*)nimcp_calloc(1, sizeof(nimcp_gpu_replay_batch_t));
    if (!batch) {
        LOG_ERROR("Failed to allocate replay batch structure");
        return NULL;
    }

    batch->batch_size = batch_size;

    size_t dims[1] = {batch_size};

    batch->engram_indices = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_INT32);
    batch->priorities = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    batch->emotional_salience = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    batch->is_completed = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_INT32);

    if (!batch->engram_indices || !batch->priorities ||
        !batch->emotional_salience || !batch->is_completed) {
        LOG_ERROR("Failed to allocate replay batch tensors");
        nimcp_gpu_replay_batch_destroy(batch);
        return NULL;
    }

    LOG_DEBUG("Created replay batch: size=%zu", batch_size);
    return batch;
}

void nimcp_gpu_replay_batch_destroy(nimcp_gpu_replay_batch_t* batch)
{
    if (!batch) return;

    if (batch->engram_indices) nimcp_gpu_tensor_destroy(batch->engram_indices);
    if (batch->priorities) nimcp_gpu_tensor_destroy(batch->priorities);
    if (batch->emotional_salience) nimcp_gpu_tensor_destroy(batch->emotional_salience);
    if (batch->is_completed) nimcp_gpu_tensor_destroy(batch->is_completed);

    nimcp_free(batch);
}

//=============================================================================
// Device Helper Functions
//=============================================================================

__device__ inline float device_sigmoid(float x)
{
    return 1.0f / (1.0f + expf(-x));
}

__device__ inline float device_tanh(float x)
{
    return tanhf(x);
}

__device__ inline float device_clamp(float x, float min_val, float max_val)
{
    return fmaxf(min_val, fminf(max_val, x));
}

/**
 * @brief Generate random number using LCG
 *
 * NOTE: Uses centralized RNG functions from nimcp_device_utils.cuh
 * For production Monte Carlo simulations, prefer cuRAND via:
 *   - stats_gpu_rng_create() (see gpu/statistics/nimcp_statistics_gpu.h)
 *   - nimcp_stats_gpu_sample_normal/uniform for batch sampling
 *
 * The LCG is suitable for simple stochastic noise in replay operations.
 */
__device__ inline float device_random(uint32_t* state)
{
    return nimcp_device_lcg_uniform(state);
}

/**
 * @brief Box-Muller transform for Gaussian noise
 *
 * NOTE: Uses centralized RNG functions from nimcp_device_utils.cuh
 */
__device__ inline float device_gaussian(uint32_t* state, float stddev)
{
    return nimcp_device_lcg_gaussian(state, stddev);
}

//=============================================================================
// Hippocampal Replay Kernels
//=============================================================================

/**
 * @brief Kernel for parallel hippocampal replay with stochastic noise
 *
 * Simulates sharp-wave ripple events with compressed replay.
 * Each thread processes one replay event.
 */
__global__ void kernel_hippocampal_replay(
    const float* __restrict__ engram_activations,  // [n_engrams, max_neurons]
    const int32_t* __restrict__ replay_indices,    // [n_replay]
    const float* __restrict__ priorities,          // [n_replay]
    const float* __restrict__ emotional_salience,  // [n_replay]
    float* __restrict__ output,                    // [n_replay, max_neurons]
    float replay_strength,
    float tau_decay,
    float noise_stddev,
    float compression_factor,
    size_t n_replay,
    size_t max_neurons,
    size_t n_engrams,
    uint32_t seed)
{
    size_t replay_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (replay_idx >= n_replay) return;

    // Get engram index for this replay event
    int32_t engram_idx = replay_indices[replay_idx];
    if (engram_idx < 0 || (size_t)engram_idx >= n_engrams) return;

    // Get modulating factors
    float priority = priorities[replay_idx];
    float emotion = emotional_salience[replay_idx];

    // Modulate replay strength by priority and emotional salience
    float modulated_strength = replay_strength * priority * (1.0f + 0.5f * emotion);
    modulated_strength = device_clamp(modulated_strength, 0.0f, 1.0f);

    // Initialize random state for this thread
    uint32_t rng_state = seed + replay_idx * 1237u;

    // Process each neuron in the engram
    for (size_t n = 0; n < max_neurons; n++) {
        size_t engram_offset = (size_t)engram_idx * max_neurons + n;
        size_t output_offset = replay_idx * max_neurons + n;

        // Get original activation
        float activation = engram_activations[engram_offset];

        // Apply compression (faster dynamics)
        float compressed = activation * compression_factor;

        // Apply decay based on tau
        float decay_factor = expf(-1.0f / tau_decay);
        float replayed = compressed * decay_factor * modulated_strength;

        // Add stochastic noise for pattern separation
        if (noise_stddev > 0.0f) {
            replayed += device_gaussian(&rng_state, noise_stddev);
        }

        // Apply activation bounds
        replayed = device_clamp(replayed, -1.0f, 1.0f);

        output[output_offset] = replayed;
    }
}

bool nimcp_gpu_hippocampal_replay(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_engram_batch_t* engrams,
    const nimcp_gpu_replay_batch_t* replay_events,
    nimcp_gpu_tensor_t* output,
    const nimcp_replay_params_t* params)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !engrams || !replay_events || !output || !params) {
        LOG_ERROR("NULL parameter in hippocampal replay");
        return false;
    }

    if (!engrams->activations || !replay_events->engram_indices ||
        !replay_events->priorities || !replay_events->emotional_salience) {
        LOG_ERROR("Missing required tensors for replay");
        return false;
    }

    size_t n_replay = replay_events->batch_size;
    size_t max_neurons = engrams->max_neurons;
    size_t n_engrams = engrams->batch_size;

    // Verify output dimensions
    if (output->numel < n_replay * max_neurons) {
        LOG_ERROR("Output tensor too small for replay results");
        return false;
    }

    // Generate seed from current time
    uint32_t seed = (uint32_t)clock();

    kernel_hippocampal_replay<<<GRID_SIZE(n_replay), BLOCK_SIZE>>>(
        (const float*)engrams->activations->data,
        (const int32_t*)replay_events->engram_indices->data,
        (const float*)replay_events->priorities->data,
        (const float*)replay_events->emotional_salience->data,
        (float*)output->data,
        params->replay_strength,
        params->tau_decay,
        params->noise_stddev,
        params->compressed_replay ? params->compression_factor : 1.0f,
        n_replay,
        max_neurons,
        n_engrams,
        seed);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Pattern Completion Kernels
//=============================================================================

/**
 * @brief Compute overlap score between cue and stored engram
 *
 * Uses cosine similarity with masking for partial cues
 */
__global__ void kernel_pattern_overlap(
    const float* __restrict__ engram_activations,  // [n_engrams, max_neurons]
    const float* __restrict__ cue_patterns,        // [n_queries, max_neurons]
    const float* __restrict__ cue_masks,           // [n_queries, max_neurons]
    float* __restrict__ match_scores,              // [n_queries, n_engrams]
    size_t n_queries,
    size_t n_engrams,
    size_t max_neurons)
{
    // 2D grid: x = query, y = engram
    size_t query_idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t engram_idx = blockIdx.y * blockDim.y + threadIdx.y;

    if (query_idx >= n_queries || engram_idx >= n_engrams) return;

    float dot_product = 0.0f;
    float cue_norm = 0.0f;
    float engram_norm = 0.0f;
    size_t valid_count = 0;

    // Compute masked cosine similarity
    for (size_t n = 0; n < max_neurons; n++) {
        size_t cue_offset = query_idx * max_neurons + n;
        size_t engram_offset = engram_idx * max_neurons + n;

        float mask = cue_masks[cue_offset];
        if (mask > 0.5f) {  // Only consider valid (unmasked) positions
            float c = cue_patterns[cue_offset];
            float e = engram_activations[engram_offset];

            dot_product += c * e;
            cue_norm += c * c;
            engram_norm += e * e;
            valid_count++;
        }
    }

    float similarity = 0.0f;
    if (valid_count > 0 && cue_norm > 0.0f && engram_norm > 0.0f) {
        similarity = dot_product / (sqrtf(cue_norm) * sqrtf(engram_norm));
    }

    match_scores[query_idx * n_engrams + engram_idx] = similarity;
}

/**
 * @brief Complete pattern from best matching engram
 */
__global__ void kernel_pattern_complete(
    const float* __restrict__ engram_activations,  // [n_engrams, max_neurons]
    const float* __restrict__ cue_patterns,        // [n_queries, max_neurons]
    const float* __restrict__ cue_masks,           // [n_queries, max_neurons]
    const float* __restrict__ match_scores,        // [n_queries, n_engrams]
    float* __restrict__ completed_patterns,        // [n_queries, max_neurons]
    float completion_threshold,
    size_t n_queries,
    size_t n_engrams,
    size_t max_neurons)
{
    size_t query_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (query_idx >= n_queries) return;

    // Find best matching engram for this query
    float best_score = -FLT_MAX;
    int32_t best_engram = -1;

    for (size_t e = 0; e < n_engrams; e++) {
        float score = match_scores[query_idx * n_engrams + e];
        if (score > best_score) {
            best_score = score;
            best_engram = (int32_t)e;
        }
    }

    // Complete pattern if threshold is met
    for (size_t n = 0; n < max_neurons; n++) {
        size_t output_offset = query_idx * max_neurons + n;
        size_t cue_offset = query_idx * max_neurons + n;

        float mask = cue_masks[cue_offset];
        float cue_val = cue_patterns[cue_offset];

        if (best_engram >= 0 && best_score >= completion_threshold) {
            size_t engram_offset = (size_t)best_engram * max_neurons + n;
            float engram_val = engram_activations[engram_offset];

            // Use cue where available, engram for completion
            if (mask > 0.5f) {
                // Blend cue with engram (weighted by confidence)
                completed_patterns[output_offset] = 0.7f * cue_val + 0.3f * engram_val;
            } else {
                // Use engram for missing parts
                completed_patterns[output_offset] = engram_val * best_score;
            }
        } else {
            // No good match - return cue as-is
            completed_patterns[output_offset] = cue_val;
        }
    }
}

bool nimcp_gpu_pattern_completion(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_engram_batch_t* engrams,
    const nimcp_gpu_tensor_t* cue_patterns,
    const nimcp_gpu_tensor_t* cue_masks,
    nimcp_gpu_tensor_t* completed_patterns,
    nimcp_gpu_tensor_t* match_scores,
    float completion_threshold)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !engrams || !cue_patterns || !cue_masks ||
        !completed_patterns || !match_scores) {
        LOG_ERROR("NULL parameter in pattern completion");
        return false;
    }

    size_t n_engrams = engrams->batch_size;
    size_t max_neurons = engrams->max_neurons;

    // Infer n_queries from cue_patterns dimensions
    size_t n_queries = cue_patterns->numel / max_neurons;

    // Phase 1: Compute overlap scores
    dim3 block_2d(16, 16);
    dim3 grid_2d((n_queries + 15) / 16, (n_engrams + 15) / 16);

    kernel_pattern_overlap<<<grid_2d, block_2d>>>(
        (const float*)engrams->activations->data,
        (const float*)cue_patterns->data,
        (const float*)cue_masks->data,
        (float*)match_scores->data,
        n_queries,
        n_engrams,
        max_neurons);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Phase 2: Complete patterns
    kernel_pattern_complete<<<GRID_SIZE(n_queries), BLOCK_SIZE>>>(
        (const float*)engrams->activations->data,
        (const float*)cue_patterns->data,
        (const float*)cue_masks->data,
        (const float*)match_scores->data,
        (float*)completed_patterns->data,
        completion_threshold,
        n_queries,
        n_engrams,
        max_neurons);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Systems Consolidation Kernels
//=============================================================================

/**
 * @brief Extract semantic features from engram activations
 *
 * Projects engram neuron activations to lower-dimensional semantic space
 */
__global__ void kernel_extract_features(
    const float* __restrict__ engram_activations,  // [batch, max_neurons]
    float* __restrict__ features,                  // [batch, feature_dim]
    size_t batch_size,
    size_t max_neurons,
    size_t feature_dim)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t batch_idx = idx / feature_dim;
    size_t feat_idx = idx % feature_dim;

    if (batch_idx >= batch_size) return;

    // Simple pooling: average over groups of neurons
    size_t neurons_per_feature = (max_neurons + feature_dim - 1) / feature_dim;
    size_t start_neuron = feat_idx * neurons_per_feature;
    size_t end_neuron = min(start_neuron + neurons_per_feature, max_neurons);

    float sum = 0.0f;
    size_t count = 0;

    for (size_t n = start_neuron; n < end_neuron; n++) {
        sum += engram_activations[batch_idx * max_neurons + n];
        count++;
    }

    float feature = (count > 0) ? (sum / (float)count) : 0.0f;
    features[batch_idx * feature_dim + feat_idx] = device_tanh(feature);
}

/**
 * @brief Transfer engram features to cortical representations
 *
 * Implements gradual hippocampus-to-cortex memory transfer
 */
__global__ void kernel_systems_consolidation(
    const float* __restrict__ engram_features,     // [batch, feature_dim]
    float* __restrict__ cortical_features,         // [batch, feature_dim]
    float* __restrict__ cortical_strength,         // [batch]
    float* __restrict__ cortical_hip_dependency,   // [batch]
    const float* __restrict__ engram_strength,     // [batch]
    const float* __restrict__ engram_emotion,      // [batch]
    float transfer_rate,
    float replay_strength,
    size_t batch_size,
    size_t feature_dim)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= batch_size) return;

    // Modulate transfer by emotional salience
    float emotion = engram_emotion[idx];
    float modulated_rate = transfer_rate * replay_strength * (1.0f + 0.5f * emotion);
    modulated_rate = device_clamp(modulated_rate, 0.0f, 1.0f);

    // Update cortical features (running average with engram)
    for (size_t f = 0; f < feature_dim; f++) {
        size_t offset = idx * feature_dim + f;
        float engram_feat = engram_features[offset];
        float cortical_feat = cortical_features[offset];

        // Exponential moving average
        cortical_features[offset] = (1.0f - modulated_rate) * cortical_feat +
                                    modulated_rate * engram_feat;
    }

    // Update consolidation strength
    float old_strength = cortical_strength[idx];
    float engram_str = engram_strength[idx];
    cortical_strength[idx] = old_strength + modulated_rate * (engram_str - old_strength);

    // Decrease hippocampal dependency as consolidation proceeds
    float old_dependency = cortical_hip_dependency[idx];
    float new_dependency = old_dependency * (1.0f - modulated_rate * 0.1f);
    cortical_hip_dependency[idx] = device_clamp(new_dependency, 0.0f, 1.0f);
}

bool nimcp_gpu_systems_consolidation(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_engram_batch_t* engrams,
    nimcp_gpu_cortical_batch_t* cortical_nodes,
    float replay_strength,
    const nimcp_consolidation_params_t* params)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !engrams || !cortical_nodes || !params) {
        LOG_ERROR("NULL parameter in systems consolidation");
        return false;
    }

    size_t batch_size = engrams->batch_size;
    size_t max_neurons = engrams->max_neurons;
    size_t feature_dim = cortical_nodes->feature_dim;

    // Allocate temporary tensor for extracted features
    size_t feat_dims[2] = {batch_size, feature_dim};
    nimcp_gpu_tensor_t* engram_features =
        nimcp_gpu_tensor_create(cortical_nodes->features->ctx, feat_dims, 2, NIMCP_GPU_PRECISION_FP32);

    if (!engram_features) {
        LOG_ERROR("Failed to allocate temporary feature tensor");
        return false;
    }

    // Phase 1: Extract semantic features from engrams
    size_t total_features = batch_size * feature_dim;
    kernel_extract_features<<<GRID_SIZE(total_features), BLOCK_SIZE>>>(
        (const float*)engrams->activations->data,
        (float*)engram_features->data,
        batch_size,
        max_neurons,
        feature_dim);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Phase 2: Transfer to cortical representations
    kernel_systems_consolidation<<<GRID_SIZE(batch_size), BLOCK_SIZE>>>(
        (const float*)engram_features->data,
        (float*)cortical_nodes->features->data,
        (float*)cortical_nodes->consolidation_strength->data,
        (float*)cortical_nodes->hippocampal_dependency->data,
        (const float*)engrams->consolidation_strength->data,
        (const float*)engrams->emotional_salience->data,
        params->transfer_rate,
        replay_strength,
        batch_size,
        feature_dim);

    cudaError_t err = cudaGetLastError();
    nimcp_gpu_tensor_destroy(engram_features);

    if (err != cudaSuccess) {
        LOG_ERROR("CUDA error in systems consolidation: %s", cudaGetErrorString(err));
        return false;
    }

    return true;
}

//=============================================================================
// Consolidation Update Kernel
//=============================================================================

/**
 * @brief Update consolidation strength over time
 */
__global__ void kernel_consolidation_update(
    float* __restrict__ consolidation_strength,  // [batch]
    float* __restrict__ hippocampal_dependency,  // [batch]
    float time_delta_seconds,
    float consolidation_rate,
    size_t batch_size)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= batch_size) return;

    float old_strength = consolidation_strength[idx];
    float delta = consolidation_rate * time_delta_seconds;

    // Asymptotic approach to 1.0
    float new_strength = old_strength + delta * (1.0f - old_strength);
    consolidation_strength[idx] = device_clamp(new_strength, 0.0f, 1.0f);

    // Hippocampal dependency decreases as consolidation increases
    float old_dependency = hippocampal_dependency[idx];
    float new_dependency = old_dependency * expf(-delta);
    hippocampal_dependency[idx] = device_clamp(new_dependency, 0.0f, 1.0f);
}

bool nimcp_gpu_consolidation_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_cortical_batch_t* cortical_nodes,
    float time_delta_seconds,
    bool is_sleeping,
    const nimcp_consolidation_params_t* params)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !cortical_nodes || !params) {
        LOG_ERROR("NULL parameter in consolidation update");
        return false;
    }

    float rate = is_sleeping ? params->consolidation_rate_sws : params->consolidation_rate_awake;

    kernel_consolidation_update<<<GRID_SIZE(cortical_nodes->batch_size), BLOCK_SIZE>>>(
        (float*)cortical_nodes->consolidation_strength->data,
        (float*)cortical_nodes->hippocampal_dependency->data,
        time_delta_seconds,
        rate,
        cortical_nodes->batch_size);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Memory Decay Kernel
//=============================================================================

/**
 * @brief Apply Ebbinghaus-style forgetting curve
 *
 * Memory retention = (1 + t/S)^(-b) where t=time, S=stability, b=decay rate
 */
__global__ void kernel_memory_decay(
    float* __restrict__ consolidation_strength,    // [batch]
    const float* __restrict__ last_activation_times, // [batch]
    float current_time,
    float forgetting_rate,
    size_t batch_size)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= batch_size) return;

    float last_activation = last_activation_times[idx];
    float time_since_activation = current_time - last_activation;

    if (time_since_activation <= 0.0f) return;

    float old_strength = consolidation_strength[idx];

    // Ebbinghaus forgetting curve (power law)
    // Using strength as stability factor
    float stability = fmaxf(old_strength, 0.1f);  // Minimum stability
    float retention = powf(1.0f + time_since_activation / (stability * 1000.0f), -forgetting_rate);

    float new_strength = old_strength * retention;
    consolidation_strength[idx] = device_clamp(new_strength, 0.0f, 1.0f);
}

bool nimcp_gpu_memory_decay(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_cortical_batch_t* cortical_nodes,
    float time_delta_seconds,
    const nimcp_gpu_tensor_t* last_activation_times,
    float forgetting_rate)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !cortical_nodes || !last_activation_times) {
        LOG_ERROR("NULL parameter in memory decay");
        return false;
    }

    // Use time_delta_seconds as current time offset
    kernel_memory_decay<<<GRID_SIZE(cortical_nodes->batch_size), BLOCK_SIZE>>>(
        (float*)cortical_nodes->consolidation_strength->data,
        (const float*)last_activation_times->data,
        time_delta_seconds,
        forgetting_rate,
        cortical_nodes->batch_size);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Engram Update Kernels
//=============================================================================

/**
 * @brief Update engram weights with Hebbian learning rule
 */
__global__ void kernel_engram_update_hebbian(
    float* __restrict__ activations,        // [batch, max_neurons]
    const float* __restrict__ updates,      // [batch, max_neurons]
    float learning_rate,
    float weight_decay,
    float min_weight,
    float max_weight,
    size_t batch_size,
    size_t max_neurons)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= batch_size * max_neurons) return;

    float activation = activations[idx];
    float update = updates[idx];

    // Hebbian update: delta_w = lr * pre * post
    // Here pre = update (external signal), post = activation
    float hebbian_delta = learning_rate * update * activation;

    // Apply weight decay (L2 regularization)
    float decayed = activation * (1.0f - weight_decay);

    // Combine
    float new_activation = decayed + hebbian_delta;

    // Clamp to bounds
    activations[idx] = device_clamp(new_activation, min_weight, max_weight);
}

/**
 * @brief Update engram weights with gradient-based rule
 */
__global__ void kernel_engram_update_gradient(
    float* __restrict__ activations,        // [batch, max_neurons]
    const float* __restrict__ updates,      // [batch, max_neurons]
    float learning_rate,
    float weight_decay,
    float momentum,
    float* __restrict__ momentum_buffer,    // [batch, max_neurons] (optional)
    float min_weight,
    float max_weight,
    size_t batch_size,
    size_t max_neurons)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= batch_size * max_neurons) return;

    float activation = activations[idx];
    float gradient = updates[idx];

    float update;
    if (momentum_buffer != NULL && momentum > 0.0f) {
        // Momentum-based update
        float prev_momentum = momentum_buffer[idx];
        float new_momentum = momentum * prev_momentum - learning_rate * gradient;
        momentum_buffer[idx] = new_momentum;
        update = new_momentum;
    } else {
        // Simple gradient descent
        update = -learning_rate * gradient;
    }

    // Apply weight decay
    float decayed = activation * (1.0f - weight_decay);

    // Apply update
    float new_activation = decayed + update;

    activations[idx] = device_clamp(new_activation, min_weight, max_weight);
}

bool nimcp_gpu_engram_weight_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_engram_batch_t* engrams,
    const nimcp_gpu_tensor_t* updates,
    const nimcp_engram_update_params_t* params)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !engrams || !updates || !params) {
        LOG_ERROR("NULL parameter in engram weight update");
        return false;
    }

    size_t total_elements = engrams->batch_size * engrams->max_neurons;

    if (params->use_hebbian) {
        kernel_engram_update_hebbian<<<GRID_SIZE(total_elements), BLOCK_SIZE>>>(
            (float*)engrams->activations->data,
            (const float*)updates->data,
            params->learning_rate,
            params->weight_decay,
            params->min_weight,
            params->max_weight,
            engrams->batch_size,
            engrams->max_neurons);
    } else {
        // Gradient-based update (no momentum buffer in this version)
        kernel_engram_update_gradient<<<GRID_SIZE(total_elements), BLOCK_SIZE>>>(
            (float*)engrams->activations->data,
            (const float*)updates->data,
            params->learning_rate,
            params->weight_decay,
            params->momentum,
            NULL,  // No momentum buffer
            params->min_weight,
            params->max_weight,
            engrams->batch_size,
            engrams->max_neurons);
    }

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Engram Overlap Kernel
//=============================================================================

/**
 * @brief Compute pairwise overlap between engram batches
 *
 * Uses cosine similarity as overlap metric
 */
__global__ void kernel_engram_overlap(
    const float* __restrict__ activations_a,  // [n_a, max_neurons]
    const float* __restrict__ activations_b,  // [n_b, max_neurons]
    float* __restrict__ overlap_matrix,       // [n_a, n_b]
    size_t n_a,
    size_t n_b,
    size_t max_neurons)
{
    size_t a_idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t b_idx = blockIdx.y * blockDim.y + threadIdx.y;

    if (a_idx >= n_a || b_idx >= n_b) return;

    float dot_product = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;

    for (size_t n = 0; n < max_neurons; n++) {
        float a = activations_a[a_idx * max_neurons + n];
        float b = activations_b[b_idx * max_neurons + n];

        dot_product += a * b;
        norm_a += a * a;
        norm_b += b * b;
    }

    float similarity = 0.0f;
    if (norm_a > 0.0f && norm_b > 0.0f) {
        similarity = dot_product / (sqrtf(norm_a) * sqrtf(norm_b));
    }

    overlap_matrix[a_idx * n_b + b_idx] = similarity;
}

bool nimcp_gpu_engram_overlap(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_engram_batch_t* engrams_a,
    const nimcp_gpu_engram_batch_t* engrams_b,
    nimcp_gpu_tensor_t* overlap_matrix)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !engrams_a || !engrams_b || !overlap_matrix) {
        LOG_ERROR("NULL parameter in engram overlap");
        return false;
    }

    if (engrams_a->max_neurons != engrams_b->max_neurons) {
        LOG_ERROR("Engram batches must have same max_neurons");
        return false;
    }

    size_t n_a = engrams_a->batch_size;
    size_t n_b = engrams_b->batch_size;

    dim3 block_2d(16, 16);
    dim3 grid_2d((n_a + 15) / 16, (n_b + 15) / 16);

    kernel_engram_overlap<<<grid_2d, block_2d>>>(
        (const float*)engrams_a->activations->data,
        (const float*)engrams_b->activations->data,
        (float*)overlap_matrix->data,
        n_a,
        n_b,
        engrams_a->max_neurons);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Similarity Search Kernels
//=============================================================================

/**
 * @brief Compute cosine similarity between queries and cortical nodes
 */
__global__ void kernel_similarity_compute(
    const float* __restrict__ node_features,   // [n_nodes, feature_dim]
    const float* __restrict__ query_features,  // [n_queries, feature_dim]
    float* __restrict__ similarities,          // [n_queries, n_nodes]
    size_t n_queries,
    size_t n_nodes,
    size_t feature_dim)
{
    size_t q_idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t n_idx = blockIdx.y * blockDim.y + threadIdx.y;

    if (q_idx >= n_queries || n_idx >= n_nodes) return;

    float dot_product = 0.0f;
    float norm_q = 0.0f;
    float norm_n = 0.0f;

    for (size_t f = 0; f < feature_dim; f++) {
        float q = query_features[q_idx * feature_dim + f];
        float n = node_features[n_idx * feature_dim + f];

        dot_product += q * n;
        norm_q += q * q;
        norm_n += n * n;
    }

    float similarity = 0.0f;
    if (norm_q > 0.0f && norm_n > 0.0f) {
        similarity = dot_product / (sqrtf(norm_q) * sqrtf(norm_n));
    }

    similarities[q_idx * n_nodes + n_idx] = similarity;
}

/**
 * @brief Select top-k most similar nodes for each query
 *
 * Simple O(n*k) selection - could be optimized with radix select
 */
__global__ void kernel_top_k_select(
    const float* __restrict__ similarities,    // [n_queries, n_nodes]
    int32_t* __restrict__ result_indices,      // [n_queries, top_k]
    float* __restrict__ result_similarities,   // [n_queries, top_k]
    size_t n_queries,
    size_t n_nodes,
    size_t top_k)
{
    size_t q_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (q_idx >= n_queries) return;

    // Initialize results with invalid values
    for (size_t k = 0; k < top_k; k++) {
        result_indices[q_idx * top_k + k] = -1;
        result_similarities[q_idx * top_k + k] = -FLT_MAX;
    }

    // Simple selection (insertion sort style)
    for (size_t n = 0; n < n_nodes; n++) {
        float sim = similarities[q_idx * n_nodes + n];

        // Find insertion position
        size_t insert_pos = top_k;
        for (size_t k = 0; k < top_k; k++) {
            if (sim > result_similarities[q_idx * top_k + k]) {
                insert_pos = k;
                break;
            }
        }

        if (insert_pos < top_k) {
            // Shift elements down
            for (size_t k = top_k - 1; k > insert_pos; k--) {
                result_indices[q_idx * top_k + k] = result_indices[q_idx * top_k + k - 1];
                result_similarities[q_idx * top_k + k] = result_similarities[q_idx * top_k + k - 1];
            }
            // Insert new element
            result_indices[q_idx * top_k + insert_pos] = (int32_t)n;
            result_similarities[q_idx * top_k + insert_pos] = sim;
        }
    }
}

bool nimcp_gpu_similarity_search(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_cortical_batch_t* cortical_nodes,
    const nimcp_gpu_tensor_t* query_features,
    size_t top_k,
    nimcp_gpu_tensor_t* result_indices,
    nimcp_gpu_tensor_t* result_similarities)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !cortical_nodes || !query_features ||
        !result_indices || !result_similarities) {
        LOG_ERROR("NULL parameter in similarity search");
        return false;
    }

    size_t n_nodes = cortical_nodes->batch_size;
    size_t feature_dim = cortical_nodes->feature_dim;
    size_t n_queries = query_features->numel / feature_dim;

    // Allocate temporary similarity matrix
    size_t sim_dims[2] = {n_queries, n_nodes};
    nimcp_gpu_tensor_t* similarities =
        nimcp_gpu_tensor_create(ctx, sim_dims, 2, NIMCP_GPU_PRECISION_FP32);

    if (!similarities) {
        LOG_ERROR("Failed to allocate similarity matrix");
        return false;
    }

    // Phase 1: Compute all pairwise similarities
    dim3 block_2d(16, 16);
    dim3 grid_2d((n_queries + 15) / 16, (n_nodes + 15) / 16);

    kernel_similarity_compute<<<grid_2d, block_2d>>>(
        (const float*)cortical_nodes->features->data,
        (const float*)query_features->data,
        (float*)similarities->data,
        n_queries,
        n_nodes,
        feature_dim);

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        // Attempt recovery for kernel launch error
        nimcp_gpu_try_recover(NULL, GPU_ERROR_KERNEL_LAUNCH, err, NULL);
        nimcp_gpu_tensor_destroy(similarities);
        LOG_ERROR("CUDA error in similarity compute: %s", cudaGetErrorString(err));
        return false;
    }

    // Phase 2: Select top-k for each query
    kernel_top_k_select<<<GRID_SIZE(n_queries), BLOCK_SIZE>>>(
        (const float*)similarities->data,
        (int32_t*)result_indices->data,
        (float*)result_similarities->data,
        n_queries,
        n_nodes,
        top_k);

    err = cudaGetLastError();
    nimcp_gpu_tensor_destroy(similarities);

    if (err != cudaSuccess) {
        // Attempt recovery for kernel launch error
        nimcp_gpu_try_recover(NULL, GPU_ERROR_KERNEL_LAUNCH, err, NULL);
        LOG_ERROR("CUDA error in top-k select: %s", cudaGetErrorString(err));
        return false;
    }

    return true;
}

//=============================================================================
// Build Similarity Graph Kernel
//=============================================================================

/**
 * @brief Build similarity graph by connecting nodes above threshold
 */
__global__ void kernel_build_similarity_graph(
    const float* __restrict__ features,           // [batch, feature_dim]
    int32_t* __restrict__ neighbor_indices,       // [batch, max_neighbors]
    float* __restrict__ neighbor_weights,         // [batch, max_neighbors]
    float similarity_threshold,
    size_t batch_size,
    size_t feature_dim,
    size_t max_neighbors)
{
    size_t node_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (node_idx >= batch_size) return;

    // Initialize neighbors as invalid
    for (size_t k = 0; k < max_neighbors; k++) {
        neighbor_indices[node_idx * max_neighbors + k] = -1;
        neighbor_weights[node_idx * max_neighbors + k] = 0.0f;
    }

    // Compute norm of this node's features
    float norm_i = 0.0f;
    for (size_t f = 0; f < feature_dim; f++) {
        float v = features[node_idx * feature_dim + f];
        norm_i += v * v;
    }
    norm_i = sqrtf(norm_i);

    if (norm_i < 1e-6f) return;

    size_t neighbor_count = 0;

    // Find neighbors (skip self)
    for (size_t j = 0; j < batch_size && neighbor_count < max_neighbors; j++) {
        if (j == node_idx) continue;

        // Compute similarity
        float dot_product = 0.0f;
        float norm_j = 0.0f;

        for (size_t f = 0; f < feature_dim; f++) {
            float vi = features[node_idx * feature_dim + f];
            float vj = features[j * feature_dim + f];
            dot_product += vi * vj;
            norm_j += vj * vj;
        }
        norm_j = sqrtf(norm_j);

        if (norm_j < 1e-6f) continue;

        float similarity = dot_product / (norm_i * norm_j);

        if (similarity >= similarity_threshold) {
            neighbor_indices[node_idx * max_neighbors + neighbor_count] = (int32_t)j;
            neighbor_weights[node_idx * max_neighbors + neighbor_count] = similarity;
            neighbor_count++;
        }
    }
}

bool nimcp_gpu_build_similarity_graph(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_cortical_batch_t* cortical_nodes,
    float similarity_threshold)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !cortical_nodes) {
        LOG_ERROR("NULL parameter in build similarity graph");
        return false;
    }

    kernel_build_similarity_graph<<<GRID_SIZE(cortical_nodes->batch_size), BLOCK_SIZE>>>(
        (const float*)cortical_nodes->features->data,
        (int32_t*)cortical_nodes->neighbor_indices->data,
        (float*)cortical_nodes->neighbor_weights->data,
        similarity_threshold,
        cortical_nodes->batch_size,
        cortical_nodes->feature_dim,
        cortical_nodes->max_neighbors);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Episodic-Semantic Transfer Kernels
//=============================================================================

/**
 * @brief Extract semantic features from episodic engrams
 */
bool nimcp_gpu_extract_semantic_features(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_engram_batch_t* engrams,
    nimcp_gpu_tensor_t* semantic_features,
    size_t feature_dim)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !engrams || !semantic_features) {
        LOG_ERROR("NULL parameter in extract semantic features");
        return false;
    }

    size_t batch_size = engrams->batch_size;
    size_t max_neurons = engrams->max_neurons;
    size_t total_features = batch_size * feature_dim;

    kernel_extract_features<<<GRID_SIZE(total_features), BLOCK_SIZE>>>(
        (const float*)engrams->activations->data,
        (float*)semantic_features->data,
        batch_size,
        max_neurons,
        feature_dim);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Check if cortical memories should transition to semantic type
 */
__global__ void kernel_check_semantic_transition(
    const float* __restrict__ consolidation_strength,  // [batch]
    const float* __restrict__ hippocampal_dependency,  // [batch]
    int32_t* __restrict__ should_transition,           // [batch]
    float semantic_threshold,
    size_t batch_size)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= batch_size) return;

    float strength = consolidation_strength[idx];
    float dependency = hippocampal_dependency[idx];

    // Transition to semantic when:
    // 1. Consolidation strength exceeds threshold
    // 2. Hippocampal dependency is low (< 0.3)
    bool should_trans = (strength >= semantic_threshold) && (dependency < 0.3f);

    should_transition[idx] = should_trans ? 1 : 0;
}

bool nimcp_gpu_check_semantic_transition(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_cortical_batch_t* cortical_nodes,
    float semantic_threshold,
    nimcp_gpu_tensor_t* should_transition)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !cortical_nodes || !should_transition) {
        LOG_ERROR("NULL parameter in check semantic transition");
        return false;
    }

    kernel_check_semantic_transition<<<GRID_SIZE(cortical_nodes->batch_size), BLOCK_SIZE>>>(
        (const float*)cortical_nodes->consolidation_strength->data,
        (const float*)cortical_nodes->hippocampal_dependency->data,
        (int32_t*)should_transition->data,
        semantic_threshold,
        cortical_nodes->batch_size);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

#else // !NIMCP_ENABLE_CUDA

//=============================================================================
// CPU Fallback Stubs (when CUDA is not available)
//=============================================================================

#include "gpu/memory/nimcp_memory_consolidation_gpu.h"
#include <stdlib.h>

nimcp_replay_params_t nimcp_replay_params_default(void)
{
    nimcp_replay_params_t params = {0};
    params.replay_strength = 0.8f;
    params.tau_decay = 20.0f;
    params.noise_stddev = 0.1f;
    params.compressed_replay = true;
    params.compression_factor = 15.0f;
    return params;
}

nimcp_consolidation_params_t nimcp_consolidation_params_default(void)
{
    nimcp_consolidation_params_t params = {0};
    params.transfer_rate = 0.01f;
    params.semantic_threshold = 0.7f;
    params.forgetting_rate = 0.001f;
    params.similarity_threshold = 0.5f;
    params.consolidation_rate_sws = 0.1f;
    params.consolidation_rate_awake = 0.01f;
    return params;
}

nimcp_engram_update_params_t nimcp_engram_update_params_default(void)
{
    nimcp_engram_update_params_t params = {0};
    params.learning_rate = 0.01f;
    params.weight_decay = 0.0001f;
    params.momentum = 0.9f;
    params.max_weight = 1.0f;
    params.min_weight = -1.0f;
    params.use_hebbian = true;
    return params;
}

nimcp_gpu_engram_batch_t* nimcp_gpu_engram_batch_create(
    nimcp_gpu_context_t* ctx, size_t batch_size, size_t max_neurons)
{
    (void)ctx; (void)batch_size; (void)max_neurons;
    return NULL;  // CUDA not available
}

void nimcp_gpu_engram_batch_destroy(nimcp_gpu_engram_batch_t* batch)
{
    (void)batch;
}

nimcp_gpu_cortical_batch_t* nimcp_gpu_cortical_batch_create(
    nimcp_gpu_context_t* ctx, size_t batch_size, size_t feature_dim, size_t max_neighbors)
{
    (void)ctx; (void)batch_size; (void)feature_dim; (void)max_neighbors;
    return NULL;
}

void nimcp_gpu_cortical_batch_destroy(nimcp_gpu_cortical_batch_t* batch)
{
    (void)batch;
}

nimcp_gpu_replay_batch_t* nimcp_gpu_replay_batch_create(
    nimcp_gpu_context_t* ctx, size_t batch_size)
{
    (void)ctx; (void)batch_size;
    return NULL;
}

void nimcp_gpu_replay_batch_destroy(nimcp_gpu_replay_batch_t* batch)
{
    (void)batch;
}

bool nimcp_gpu_hippocampal_replay(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_engram_batch_t* engrams,
    const nimcp_gpu_replay_batch_t* replay_events,
    nimcp_gpu_tensor_t* output,
    const nimcp_replay_params_t* params)
{
    (void)ctx; (void)engrams; (void)replay_events; (void)output; (void)params;
    return false;
}

bool nimcp_gpu_pattern_completion(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_engram_batch_t* engrams,
    const nimcp_gpu_tensor_t* cue_patterns,
    const nimcp_gpu_tensor_t* cue_masks,
    nimcp_gpu_tensor_t* completed_patterns,
    nimcp_gpu_tensor_t* match_scores,
    float completion_threshold)
{
    (void)ctx; (void)engrams; (void)cue_patterns; (void)cue_masks;
    (void)completed_patterns; (void)match_scores; (void)completion_threshold;
    return false;
}

bool nimcp_gpu_systems_consolidation(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_engram_batch_t* engrams,
    nimcp_gpu_cortical_batch_t* cortical_nodes,
    float replay_strength,
    const nimcp_consolidation_params_t* params)
{
    (void)ctx; (void)engrams; (void)cortical_nodes; (void)replay_strength; (void)params;
    return false;
}

bool nimcp_gpu_consolidation_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_cortical_batch_t* cortical_nodes,
    float time_delta_seconds,
    bool is_sleeping,
    const nimcp_consolidation_params_t* params)
{
    (void)ctx; (void)cortical_nodes; (void)time_delta_seconds; (void)is_sleeping; (void)params;
    return false;
}

bool nimcp_gpu_memory_decay(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_cortical_batch_t* cortical_nodes,
    float time_delta_seconds,
    const nimcp_gpu_tensor_t* last_activation_times,
    float forgetting_rate)
{
    (void)ctx; (void)cortical_nodes; (void)time_delta_seconds;
    (void)last_activation_times; (void)forgetting_rate;
    return false;
}

bool nimcp_gpu_engram_weight_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_engram_batch_t* engrams,
    const nimcp_gpu_tensor_t* updates,
    const nimcp_engram_update_params_t* params)
{
    (void)ctx; (void)engrams; (void)updates; (void)params;
    return false;
}

bool nimcp_gpu_engram_overlap(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_engram_batch_t* engrams_a,
    const nimcp_gpu_engram_batch_t* engrams_b,
    nimcp_gpu_tensor_t* overlap_matrix)
{
    (void)ctx; (void)engrams_a; (void)engrams_b; (void)overlap_matrix;
    return false;
}

bool nimcp_gpu_similarity_search(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_cortical_batch_t* cortical_nodes,
    const nimcp_gpu_tensor_t* query_features,
    size_t top_k,
    nimcp_gpu_tensor_t* result_indices,
    nimcp_gpu_tensor_t* result_similarities)
{
    (void)ctx; (void)cortical_nodes; (void)query_features;
    (void)top_k; (void)result_indices; (void)result_similarities;
    return false;
}

bool nimcp_gpu_build_similarity_graph(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_cortical_batch_t* cortical_nodes,
    float similarity_threshold)
{
    (void)ctx; (void)cortical_nodes; (void)similarity_threshold;
    return false;
}

bool nimcp_gpu_extract_semantic_features(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_engram_batch_t* engrams,
    nimcp_gpu_tensor_t* semantic_features,
    size_t feature_dim)
{
    (void)ctx; (void)engrams; (void)semantic_features; (void)feature_dim;
    return false;
}

bool nimcp_gpu_check_semantic_transition(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_cortical_batch_t* cortical_nodes,
    float semantic_threshold,
    nimcp_gpu_tensor_t* should_transition)
{
    (void)ctx; (void)cortical_nodes; (void)semantic_threshold; (void)should_transition;
    return false;
}

#endif // NIMCP_ENABLE_CUDA
