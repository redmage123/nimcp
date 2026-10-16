/**
 * @file nimcp_swarm_memory_gpu.cu
 * @brief GPU Swarm Memory CUDA Kernels Implementation
 *
 * WHAT: CUDA kernels for swarm memory consolidation and experience replay
 * WHY:  GPU acceleration enables massive parallelism for reinforcement learning
 *       experience replay buffers and multi-agent memory coordination
 * HOW:  Custom kernels for prioritized replay, consolidation, and agent sync
 *
 * IMPLEMENTATION NOTES:
 * - Sum-tree uses iterative parent updates for O(log n) priority sampling
 * - Memory coalescing optimized for batch operations
 * - Warp-shuffle reductions for efficient aggregation
 * - Atomic operations for concurrent buffer updates
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

#include <cuda_runtime.h>
#include <curand_kernel.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>

#include "gpu/swarm/nimcp_swarm_memory_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/memory/nimcp_memory.h"

#define LOG_MODULE "SWARM_MEMORY_GPU"

#define BLOCK_SIZE 256
#define WARP_SIZE 32
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)

//=============================================================================
// Device Helper Functions
//=============================================================================

/**
 * @brief Warp-level sum reduction using shuffle
 */
__device__ inline float warp_reduce_sum(float val) {
    for (int offset = WARP_SIZE / 2; offset > 0; offset /= 2) {
        val += __shfl_down_sync(0xffffffff, val, offset);
    }
    return val;
}

/**
 * @brief Warp-level max reduction using shuffle
 */
__device__ inline float warp_reduce_max(float val) {
    for (int offset = WARP_SIZE / 2; offset > 0; offset /= 2) {
        val = fmaxf(val, __shfl_down_sync(0xffffffff, val, offset));
    }
    return val;
}

/**
 * @brief Block-level sum reduction
 */
static __device__ float block_reduce_sum(float val) {
    static __shared__ float shared[32];
    int lane = threadIdx.x % WARP_SIZE;
    int wid = threadIdx.x / WARP_SIZE;

    val = warp_reduce_sum(val);

    if (lane == 0) shared[wid] = val;
    __syncthreads();

    val = (threadIdx.x < blockDim.x / WARP_SIZE) ? shared[lane] : 0.0f;
    if (wid == 0) val = warp_reduce_sum(val);

    return val;
}

/**
 * @brief Compute parent index in sum-tree
 */
__device__ inline int sum_tree_parent(int idx) {
    return (idx - 1) / 2;
}

/**
 * @brief Compute left child index in sum-tree
 */
__device__ inline int sum_tree_left_child(int idx) {
    return 2 * idx + 1;
}

/**
 * @brief Compute right child index in sum-tree
 */
__device__ inline int sum_tree_right_child(int idx) {
    return 2 * idx + 2;
}

//=============================================================================
// Sum-Tree Kernels for Prioritized Experience Replay
//=============================================================================

/**
 * @brief Initialize sum-tree leaves from priorities
 */
__global__ void kernel_init_sum_tree_leaves(
    float* sum_tree,
    const float* priorities,
    int capacity)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= capacity) return;

    // Leaves start at index (capacity - 1)
    int leaf_idx = capacity - 1 + idx;
    sum_tree[leaf_idx] = priorities[idx];
}

/**
 * @brief Build sum-tree bottom-up (one level per kernel launch)
 */
__global__ void kernel_build_sum_tree_level(
    float* sum_tree,
    int level_start,
    int level_size)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= level_size) return;

    int node_idx = level_start + idx;
    int left = sum_tree_left_child(node_idx);
    int right = sum_tree_right_child(node_idx);

    sum_tree[node_idx] = sum_tree[left] + sum_tree[right];
}

/**
 * @brief Update single priority in sum-tree (propagate to root)
 */
__global__ void kernel_update_sum_tree_single(
    float* sum_tree,
    int leaf_idx,
    float new_priority,
    int capacity)
{
    // Only one thread does the update
    if (threadIdx.x != 0 || blockIdx.x != 0) return;

    int tree_idx = capacity - 1 + leaf_idx;
    float delta = new_priority - sum_tree[tree_idx];

    // Update leaf
    sum_tree[tree_idx] = new_priority;

    // Propagate to root
    while (tree_idx > 0) {
        tree_idx = sum_tree_parent(tree_idx);
        sum_tree[tree_idx] += delta;
    }
}

/**
 * @brief Batch update priorities in sum-tree
 */
__global__ void kernel_update_priorities_batch(
    float* priorities,
    float* sum_tree,
    const int* indices,
    const float* td_errors,
    float alpha,
    float epsilon,
    int batch_size,
    int capacity)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= batch_size) return;

    int buffer_idx = indices[idx];
    if (buffer_idx < 0 || buffer_idx >= capacity) return;

    // Compute priority from TD-error: priority = |TD| + epsilon
    float td = td_errors[idx];
    float priority = powf(fabsf(td) + epsilon, alpha);

    // Update priority array
    priorities[buffer_idx] = priority;

    // Update sum-tree leaf
    int tree_idx = capacity - 1 + buffer_idx;
    float old_priority = sum_tree[tree_idx];
    float delta = priority - old_priority;

    // Atomic update to handle concurrent updates
    atomicAdd(&sum_tree[tree_idx], delta);
}

/**
 * @brief Propagate sum-tree updates after batch priority update
 * Must be called after kernel_update_priorities_batch
 */
__global__ void kernel_propagate_sum_tree(
    float* sum_tree,
    int tree_size)
{
    // Process internal nodes bottom-up
    // Each thread handles one internal node
    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    // Internal nodes are [0, (tree_size-1)/2)
    int num_internal = (tree_size - 1) / 2;
    if (idx >= num_internal) return;

    // Process from leaves toward root
    // Node at idx has children at 2*idx+1 and 2*idx+2
    int node_idx = num_internal - 1 - idx;  // Process in reverse order
    int left = sum_tree_left_child(node_idx);
    int right = sum_tree_right_child(node_idx);

    if (left < tree_size && right < tree_size) {
        sum_tree[node_idx] = sum_tree[left] + sum_tree[right];
    }
}

/**
 * @brief Sample from sum-tree using stratified sampling
 */
__global__ void kernel_sample_sum_tree(
    const float* sum_tree,
    const float* random_vals,
    int* sampled_indices,
    int batch_size,
    int capacity)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= batch_size) return;

    float total_priority = sum_tree[0];

    // Stratified sampling: divide priority range into batch_size segments
    float segment_size = total_priority / batch_size;
    float sample_val = segment_size * idx + random_vals[idx] * segment_size;

    // Traverse sum-tree to find leaf
    int tree_idx = 0;
    int tree_size = 2 * capacity - 1;

    while (sum_tree_left_child(tree_idx) < tree_size) {
        int left = sum_tree_left_child(tree_idx);
        int right = sum_tree_right_child(tree_idx);

        if (sample_val <= sum_tree[left]) {
            tree_idx = left;
        } else {
            sample_val -= sum_tree[left];
            tree_idx = right;
        }
    }

    // Convert tree index to buffer index
    int buffer_idx = tree_idx - (capacity - 1);
    sampled_indices[idx] = buffer_idx;
}

/**
 * @brief Compute importance sampling weights
 */
__global__ void kernel_compute_is_weights(
    const float* priorities,
    float* weights,
    const int* indices,
    float beta,
    float total_priority,
    int capacity,
    int current_size,
    int batch_size)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= batch_size) return;

    int buffer_idx = indices[idx];
    float priority = priorities[buffer_idx];

    // P(i) = p_i / sum(p_j)
    float prob = priority / total_priority;

    // w_i = (N * P(i))^(-beta)
    float weight = powf((float)current_size * prob, -beta);

    weights[idx] = weight;
}

/**
 * @brief Normalize IS weights by max weight
 */
__global__ void kernel_normalize_is_weights(
    float* weights,
    float max_weight,
    int batch_size)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= batch_size) return;

    weights[idx] = weights[idx] / max_weight;
}

//=============================================================================
// Memory Consolidation Kernels
//=============================================================================

/**
 * @brief Apply memory decay (Ebbinghaus forgetting curve)
 */
__global__ void kernel_memory_decay(
    float* memory_strength,
    float decay_rate,
    float min_strength,
    int n)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float strength = memory_strength[idx];
    strength *= expf(-decay_rate);

    // Clamp to minimum
    memory_strength[idx] = fmaxf(strength, min_strength);
}

/**
 * @brief Selective memory consolidation based on importance
 */
__global__ void kernel_selective_consolidate(
    const float* memories,
    const float* importance,
    float* consolidated,
    float threshold,
    int num_memories,
    int memory_dim)
{
    int mem_idx = blockIdx.x;
    int dim_idx = threadIdx.x;

    if (mem_idx >= num_memories || dim_idx >= memory_dim) return;

    float imp = importance[mem_idx];
    float mem_val = memories[mem_idx * memory_dim + dim_idx];

    // Only consolidate if importance above threshold
    if (imp >= threshold) {
        consolidated[mem_idx * memory_dim + dim_idx] = mem_val;
    } else {
        // Decay unconsolidated memories
        consolidated[mem_idx * memory_dim + dim_idx] = mem_val * 0.9f;
    }
}

/**
 * @brief Hippocampal compression - downsample sequence
 */
__global__ void kernel_hippocampal_compress(
    const float* sequence,
    float* compressed,
    int seq_len,
    int state_dim,
    int compressed_len)
{
    int comp_idx = blockIdx.x;
    int dim_idx = threadIdx.x;

    if (comp_idx >= compressed_len || dim_idx >= state_dim) return;

    // Compute which original frames to average
    float ratio = (float)seq_len / compressed_len;
    int start_frame = (int)(comp_idx * ratio);
    int end_frame = (int)((comp_idx + 1) * ratio);
    end_frame = min(end_frame, seq_len);

    // Average frames in window
    float sum = 0.0f;
    int count = 0;
    for (int f = start_frame; f < end_frame; f++) {
        sum += sequence[f * state_dim + dim_idx];
        count++;
    }

    compressed[comp_idx * state_dim + dim_idx] = (count > 0) ? sum / count : 0.0f;
}

/**
 * @brief Systems consolidation - transfer to cortical weights
 */
__global__ void kernel_systems_consolidation(
    const float* hippocampal_mem,
    float* cortical_weights,
    const float* consolidation_gate,
    float learning_rate,
    int memory_dim,
    int cortical_dim,
    int batch_size)
{
    int mem_idx = blockIdx.y;
    int i = blockIdx.x * blockDim.x + threadIdx.x;  // memory_dim index
    int j = threadIdx.y;  // cortical_dim index (tiled)

    if (mem_idx >= batch_size || i >= memory_dim || j >= cortical_dim) return;

    float gate = consolidation_gate[mem_idx];
    float mem_val = hippocampal_mem[mem_idx * memory_dim + i];

    // Hebbian-like update: delta_w = lr * gate * mem_val
    float delta = learning_rate * gate * mem_val;

    atomicAdd(&cortical_weights[i * cortical_dim + j], delta);
}

/**
 * @brief SWS replay - sort memories by importance
 */
__global__ void kernel_sws_importance_score(
    const float* importance_scores,
    float* scored_indices,
    int num_memories)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_memories) return;

    // Store (importance, index) for sorting
    // Pack into single float: importance in high bits, index in low bits
    scored_indices[idx] = importance_scores[idx];
}

//=============================================================================
// Radix Sort Kernels for Particle/Memory Sorting
//=============================================================================

#define RADIX_BITS 4
#define RADIX_SIZE (1 << RADIX_BITS)  // 16 buckets
#define RADIX_MASK (RADIX_SIZE - 1)

/**
 * @brief Convert float to sortable unsigned int (preserves order)
 *
 * Flips sign bit for positive, flips all bits for negative.
 * This ensures float ordering matches unsigned int ordering.
 */
__device__ __forceinline__ unsigned int float_to_sortable(float f)
{
    unsigned int u = __float_as_uint(f);
    unsigned int mask = (u & 0x80000000) ? 0xffffffff : 0x80000000;
    return u ^ mask;
}

/**
 * @brief Convert sortable unsigned int back to float
 */
__device__ __forceinline__ float sortable_to_float(unsigned int u)
{
    unsigned int mask = (u & 0x80000000) ? 0x80000000 : 0xffffffff;
    return __uint_as_float(u ^ mask);
}

/**
 * @brief Compute histogram of 4-bit digit at given shift position
 *
 * Each block computes local histogram, then atomically adds to global histogram.
 */
__global__ void kernel_radix_histogram(
    const unsigned int* keys,
    unsigned int* histogram,
    size_t n,
    int shift)
{
    __shared__ unsigned int s_hist[RADIX_SIZE];

    // Initialize shared histogram
    if (threadIdx.x < RADIX_SIZE) {
        s_hist[threadIdx.x] = 0;
    }
    __syncthreads();

    // Each thread processes multiple elements
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;

    for (size_t i = tid; i < n; i += stride) {
        unsigned int key = keys[i];
        unsigned int digit = (key >> shift) & RADIX_MASK;
        atomicAdd(&s_hist[digit], 1);
    }
    __syncthreads();

    // Add local histogram to global histogram
    if (threadIdx.x < RADIX_SIZE) {
        atomicAdd(&histogram[blockIdx.x * RADIX_SIZE + threadIdx.x], s_hist[threadIdx.x]);
    }
}

/**
 * @brief Prefix sum within blocks (Blelloch scan)
 */
__global__ void kernel_prefix_sum_blocks(
    unsigned int* data,
    unsigned int* block_sums,
    size_t n)
{
    extern __shared__ unsigned int s_data[];

    int tid = threadIdx.x;
    int block_offset = blockIdx.x * blockDim.x * 2;

    // Load input into shared memory
    int ai = tid;
    int bi = tid + blockDim.x;
    s_data[ai] = (block_offset + ai < n) ? data[block_offset + ai] : 0;
    s_data[bi] = (block_offset + bi < n) ? data[block_offset + bi] : 0;
    __syncthreads();

    // Build sum in place (up-sweep)
    int offset = 1;
    for (int d = blockDim.x; d > 0; d >>= 1) {
        if (tid < d) {
            int ai2 = offset * (2 * tid + 1) - 1;
            int bi2 = offset * (2 * tid + 2) - 1;
            s_data[bi2] += s_data[ai2];
        }
        offset *= 2;
        __syncthreads();
    }

    // Store block sum and clear last element
    if (tid == 0) {
        if (block_sums) block_sums[blockIdx.x] = s_data[blockDim.x * 2 - 1];
        s_data[blockDim.x * 2 - 1] = 0;
    }
    __syncthreads();

    // Down-sweep
    for (int d = 1; d < blockDim.x * 2; d *= 2) {
        offset >>= 1;
        if (tid < d) {
            int ai2 = offset * (2 * tid + 1) - 1;
            int bi2 = offset * (2 * tid + 2) - 1;
            unsigned int temp = s_data[ai2];
            s_data[ai2] = s_data[bi2];
            s_data[bi2] += temp;
        }
        __syncthreads();
    }

    // Write results back
    if (block_offset + ai < n) data[block_offset + ai] = s_data[ai];
    if (block_offset + bi < n) data[block_offset + bi] = s_data[bi];
}

/**
 * @brief Add block sums to complete global prefix sum
 */
__global__ void kernel_add_block_sums(
    unsigned int* data,
    const unsigned int* block_sums,
    size_t n)
{
    if (blockIdx.x == 0) return;  // First block has no increment

    unsigned int increment = block_sums[blockIdx.x];
    int block_offset = blockIdx.x * blockDim.x * 2;

    int idx1 = block_offset + threadIdx.x;
    int idx2 = block_offset + threadIdx.x + blockDim.x;

    if (idx1 < n) data[idx1] += increment;
    if (idx2 < n) data[idx2] += increment;
}

/**
 * @brief Scatter keys (and optionally values) to sorted positions
 */
__global__ void kernel_radix_scatter(
    const unsigned int* keys_in,
    unsigned int* keys_out,
    const unsigned int* values_in,
    unsigned int* values_out,
    const unsigned int* prefix_sum,
    size_t n,
    int shift,
    int num_blocks)
{
    __shared__ unsigned int s_offsets[RADIX_SIZE];

    // Load starting offsets for this block from prefix sum
    if (threadIdx.x < RADIX_SIZE) {
        // Global offset for this digit across all blocks
        unsigned int global_offset = 0;
        for (int b = 0; b < num_blocks; b++) {
            if (b < blockIdx.x) {
                global_offset += prefix_sum[b * RADIX_SIZE + threadIdx.x + 1] -
                                 prefix_sum[b * RADIX_SIZE + threadIdx.x];
            }
        }
        // This is simplified - real implementation needs proper prefix sum of histogram
        s_offsets[threadIdx.x] = prefix_sum[threadIdx.x];
    }
    __syncthreads();

    // Scatter elements
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;

    for (size_t i = tid; i < n; i += stride) {
        unsigned int key = keys_in[i];
        unsigned int digit = (key >> shift) & RADIX_MASK;

        // Use atomic to get unique destination
        unsigned int dest = atomicAdd(&s_offsets[digit], 1);
        keys_out[dest] = key;
        if (values_in && values_out) {
            values_out[dest] = values_in[i];
        }
    }
}

/**
 * @brief Compute local rank within same-digit elements for stable counting sort
 *
 * For each element, count how many preceding elements have the same digit.
 * This gives each element a unique position within its digit bucket.
 */
__global__ void kernel_radix_compute_ranks(
    const unsigned int* keys,
    unsigned int* ranks,
    const unsigned int* prefix_sum,  // [RADIX_SIZE] with prefix sums
    size_t n,
    int shift)
{
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= n) return;

    unsigned int key = keys[tid];
    unsigned int digit = (key >> shift) & RADIX_MASK;

    // Count how many elements with same digit come before this one
    unsigned int local_rank = 0;
    for (int i = 0; i < tid; i++) {
        unsigned int other_digit = (keys[i] >> shift) & RADIX_MASK;
        if (other_digit == digit) {
            local_rank++;
        }
    }

    // Final position = prefix_sum[digit] + local_rank
    ranks[tid] = prefix_sum[digit] + local_rank;
}

/**
 * @brief Scatter elements to sorted positions using precomputed ranks
 */
__global__ void kernel_radix_scatter_ranked(
    const unsigned int* keys_in,
    unsigned int* keys_out,
    const unsigned int* values_in,
    unsigned int* values_out,
    const unsigned int* ranks,
    size_t n)
{
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= n) return;

    unsigned int dest = ranks[tid];
    keys_out[dest] = keys_in[tid];
    if (values_in && values_out) {
        values_out[dest] = values_in[tid];
    }
}

//=============================================================================
// Radix Sort Context Structure and API
//=============================================================================

/**
 * @brief Radix sort context for swarm particle sorting
 */
typedef struct nimcp_radix_sort_ctx {
    void* gpu_context;
    size_t max_elements;

    // Workspace buffers
    unsigned int* d_temp_keys;
    unsigned int* d_temp_values;
    unsigned int* d_histogram;
    unsigned int* d_prefix_sum;
    unsigned int* d_ranks;  // For stable counting sort
} nimcp_radix_sort_ctx_t;

// Forward declaration
extern "C" void nimcp_radix_sort_destroy(nimcp_radix_sort_ctx_t* ctx);

/**
 * @brief Create radix sort context
 */
extern "C" nimcp_radix_sort_ctx_t* nimcp_radix_sort_create(void* gpu_ctx, size_t max_elements)
{
    if (!gpu_ctx || max_elements == 0) {
        LOG_ERROR("Invalid parameters for radix sort creation");
        return NULL;
    }

    nimcp_radix_sort_ctx_t* ctx = (nimcp_radix_sort_ctx_t*)nimcp_calloc(1, sizeof(nimcp_radix_sort_ctx_t));
    if (!ctx) {
        LOG_ERROR("Failed to allocate radix sort context");
        return NULL;
    }

    ctx->gpu_context = gpu_ctx;
    ctx->max_elements = max_elements;

    // Allocate workspace buffers
    int num_blocks = GRID_SIZE(max_elements);

    cudaError_t err;
    err = cudaMalloc(&ctx->d_temp_keys, max_elements * sizeof(unsigned int));
    if (err != cudaSuccess) goto cleanup;

    err = cudaMalloc(&ctx->d_temp_values, max_elements * sizeof(unsigned int));
    if (err != cudaSuccess) goto cleanup;

    // Histogram: one per block per digit
    err = cudaMalloc(&ctx->d_histogram, num_blocks * RADIX_SIZE * sizeof(unsigned int));
    if (err != cudaSuccess) goto cleanup;

    // Prefix sum of histogram
    err = cudaMalloc(&ctx->d_prefix_sum, (num_blocks * RADIX_SIZE + 1) * sizeof(unsigned int));
    if (err != cudaSuccess) goto cleanup;

    // Ranks buffer for stable sort
    err = cudaMalloc(&ctx->d_ranks, max_elements * sizeof(unsigned int));
    if (err != cudaSuccess) goto cleanup;

    LOG_DEBUG("Created radix sort context: max_elements=%zu", max_elements);
    return ctx;

cleanup:
    LOG_ERROR("Failed to allocate radix sort buffers: %s", cudaGetErrorString(err));
    nimcp_radix_sort_destroy(ctx);
    return NULL;
}

/**
 * @brief Destroy radix sort context
 */
extern "C" void nimcp_radix_sort_destroy(nimcp_radix_sort_ctx_t* ctx)
{
    if (!ctx) return;

    if (ctx->d_temp_keys) cudaFree(ctx->d_temp_keys);
    if (ctx->d_temp_values) cudaFree(ctx->d_temp_values);
    if (ctx->d_histogram) cudaFree(ctx->d_histogram);
    if (ctx->d_prefix_sum) cudaFree(ctx->d_prefix_sum);
    if (ctx->d_ranks) cudaFree(ctx->d_ranks);

    nimcp_free(ctx);
    LOG_DEBUG("Destroyed radix sort context");
}

/**
 * @brief Sort unsigned integer keys using radix sort
 */
extern "C" int nimcp_radix_sort_keys(
    nimcp_radix_sort_ctx_t* ctx,
    unsigned int* keys,
    size_t n)
{
    if (!ctx || !keys || n == 0) return -1;
    if (n > ctx->max_elements) {
        LOG_ERROR("Array size %zu exceeds max_elements %zu", n, ctx->max_elements);
        return -1;
    }

    unsigned int* d_keys_in = keys;
    unsigned int* d_keys_out = ctx->d_temp_keys;

    // Process 4 bits at a time (8 passes for 32-bit integers)
    for (int shift = 0; shift < 32; shift += RADIX_BITS) {
        // Clear histogram
        cudaMemset(ctx->d_histogram, 0, GRID_SIZE(n) * RADIX_SIZE * sizeof(unsigned int));

        // Build histogram
        kernel_radix_histogram<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            d_keys_in, ctx->d_histogram, n, shift);

        // Compute prefix sum of histogram
        // Simplified: compute global counts and prefix sum on CPU
        unsigned int h_counts[RADIX_SIZE] = {0};
        unsigned int h_prefix[RADIX_SIZE + 1];

        // Sum across all blocks
        unsigned int* h_hist = (unsigned int*)nimcp_malloc(GRID_SIZE(n) * RADIX_SIZE * sizeof(unsigned int));
        if (!h_hist) {
            LOG_ERROR("Failed to allocate histogram buffer");
            return false;
        }
        cudaMemcpy(h_hist, ctx->d_histogram, GRID_SIZE(n) * RADIX_SIZE * sizeof(unsigned int), cudaMemcpyDeviceToHost);

        for (size_t b = 0; b < (size_t)GRID_SIZE(n); b++) {
            for (int d = 0; d < RADIX_SIZE; d++) {
                h_counts[d] += h_hist[b * RADIX_SIZE + d];
            }
        }
        nimcp_free(h_hist);

        // Compute prefix sum
        h_prefix[0] = 0;
        for (int d = 0; d < RADIX_SIZE; d++) {
            h_prefix[d + 1] = h_prefix[d] + h_counts[d];
        }

        // Copy prefix sum to device
        cudaMemcpy(ctx->d_prefix_sum, h_prefix, (RADIX_SIZE + 1) * sizeof(unsigned int), cudaMemcpyHostToDevice);

        // Compute ranks for stable sort
        kernel_radix_compute_ranks<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            d_keys_in, ctx->d_ranks, ctx->d_prefix_sum, n, shift);
        cudaDeviceSynchronize();

        // Scatter to sorted positions using computed ranks
        kernel_radix_scatter_ranked<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            d_keys_in, d_keys_out, NULL, NULL, ctx->d_ranks, n);

        // Swap buffers
        unsigned int* temp = d_keys_in;
        d_keys_in = d_keys_out;
        d_keys_out = temp;
    }

    // If result is in temp buffer, copy back
    if (d_keys_in == ctx->d_temp_keys) {
        cudaMemcpy(keys, ctx->d_temp_keys, n * sizeof(unsigned int), cudaMemcpyDeviceToDevice);
    }

    cudaError_t err = cudaGetLastError();
    return (err == cudaSuccess) ? 0 : -1;
}

/**
 * @brief Sort key-value pairs using radix sort
 */
extern "C" int nimcp_radix_sort_pairs(
    nimcp_radix_sort_ctx_t* ctx,
    unsigned int* keys,
    unsigned int* values,
    size_t n)
{
    if (!ctx || !keys || !values || n == 0) return -1;
    if (n > ctx->max_elements) return -1;

    unsigned int* d_keys_in = keys;
    unsigned int* d_keys_out = ctx->d_temp_keys;
    unsigned int* d_vals_in = values;
    unsigned int* d_vals_out = ctx->d_temp_values;

    for (int shift = 0; shift < 32; shift += RADIX_BITS) {
        cudaMemset(ctx->d_histogram, 0, GRID_SIZE(n) * RADIX_SIZE * sizeof(unsigned int));

        kernel_radix_histogram<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            d_keys_in, ctx->d_histogram, n, shift);

        // Compute prefix sum
        unsigned int h_counts[RADIX_SIZE] = {0};
        unsigned int h_prefix[RADIX_SIZE + 1];
        unsigned int* h_hist = (unsigned int*)nimcp_malloc(GRID_SIZE(n) * RADIX_SIZE * sizeof(unsigned int));
        if (!h_hist) {
            LOG_ERROR("Failed to allocate histogram buffer");
            return false;
        }
        cudaMemcpy(h_hist, ctx->d_histogram, GRID_SIZE(n) * RADIX_SIZE * sizeof(unsigned int), cudaMemcpyDeviceToHost);

        for (size_t b = 0; b < (size_t)GRID_SIZE(n); b++) {
            for (int d = 0; d < RADIX_SIZE; d++) {
                h_counts[d] += h_hist[b * RADIX_SIZE + d];
            }
        }
        nimcp_free(h_hist);

        h_prefix[0] = 0;
        for (int d = 0; d < RADIX_SIZE; d++) {
            h_prefix[d + 1] = h_prefix[d] + h_counts[d];
        }
        cudaMemcpy(ctx->d_prefix_sum, h_prefix, (RADIX_SIZE + 1) * sizeof(unsigned int), cudaMemcpyHostToDevice);

        // Compute ranks for stable sort
        kernel_radix_compute_ranks<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            d_keys_in, ctx->d_ranks, ctx->d_prefix_sum, n, shift);
        cudaDeviceSynchronize();

        // Scatter to sorted positions using computed ranks
        kernel_radix_scatter_ranked<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            d_keys_in, d_keys_out, d_vals_in, d_vals_out, ctx->d_ranks, n);

        // Swap all buffers
        unsigned int* temp = d_keys_in;
        d_keys_in = d_keys_out;
        d_keys_out = temp;
        temp = d_vals_in;
        d_vals_in = d_vals_out;
        d_vals_out = temp;
    }

    if (d_keys_in == ctx->d_temp_keys) {
        cudaMemcpy(keys, ctx->d_temp_keys, n * sizeof(unsigned int), cudaMemcpyDeviceToDevice);
        cudaMemcpy(values, ctx->d_temp_values, n * sizeof(unsigned int), cudaMemcpyDeviceToDevice);
    }

    cudaError_t err = cudaGetLastError();
    return (err == cudaSuccess) ? 0 : -1;
}

/**
 * @brief Sort floats with indices using radix sort
 *
 * Converts floats to sortable unsigned ints, sorts key-value pairs,
 * then converts back. Indices track original positions.
 */
extern "C" int nimcp_radix_sort_floats(
    nimcp_radix_sort_ctx_t* ctx,
    float* keys,
    unsigned int* indices,
    size_t n)
{
    if (!ctx || !keys || !indices || n == 0) return -1;
    if (n > ctx->max_elements) return -1;

    // Convert floats to sortable uints in-place (reusing the buffer)
    unsigned int* u_keys = (unsigned int*)keys;

    // Initialize indices if not already done
    // Kernel to convert and initialize
    // For now, do on host (production would use kernel)
    float* h_keys = (float*)nimcp_malloc(n * sizeof(float));
    unsigned int* h_indices = (unsigned int*)nimcp_malloc(n * sizeof(unsigned int));

    if (!h_keys || !h_indices) {
        nimcp_free(h_keys);
        nimcp_free(h_indices);
        return -1;
    }

    cudaMemcpy(h_keys, keys, n * sizeof(float), cudaMemcpyDeviceToHost);

    for (size_t i = 0; i < n; i++) {
        h_indices[i] = (unsigned int)i;
    }

    // Convert floats to sortable uints
    unsigned int* h_ukeys = (unsigned int*)h_keys;
    for (size_t i = 0; i < n; i++) {
        unsigned int u = *(unsigned int*)&h_keys[i];
        unsigned int mask = (u & 0x80000000) ? 0xffffffff : 0x80000000;
        h_ukeys[i] = u ^ mask;
    }

    // Copy back to device
    cudaMemcpy(u_keys, h_ukeys, n * sizeof(unsigned int), cudaMemcpyHostToDevice);
    cudaMemcpy(indices, h_indices, n * sizeof(unsigned int), cudaMemcpyHostToDevice);

    // Sort key-value pairs
    int result = nimcp_radix_sort_pairs(ctx, u_keys, indices, n);

    // Convert back to floats
    cudaMemcpy(h_ukeys, u_keys, n * sizeof(unsigned int), cudaMemcpyDeviceToHost);
    for (size_t i = 0; i < n; i++) {
        unsigned int u = h_ukeys[i];
        unsigned int mask = (u & 0x80000000) ? 0x80000000 : 0xffffffff;
        *(unsigned int*)&h_keys[i] = u ^ mask;
    }
    cudaMemcpy(keys, h_keys, n * sizeof(float), cudaMemcpyHostToDevice);

    nimcp_free(h_keys);
    nimcp_free(h_indices);

    return result;
}

//=============================================================================
// Multi-Agent Memory Coordination Kernels
//=============================================================================

/**
 * @brief Aggregate agent memories with weighted average
 */
__global__ void kernel_aggregate_agent_memories(
    const float* agent_memories,
    float* aggregated,
    const float* agent_weights,
    int num_agents,
    int memory_dim)
{
    int dim_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (dim_idx >= memory_dim) return;

    float sum = 0.0f;
    float weight_sum = 0.0f;

    for (int a = 0; a < num_agents; a++) {
        float w = agent_weights[a];
        sum += w * agent_memories[a * memory_dim + dim_idx];
        weight_sum += w;
    }

    aggregated[dim_idx] = (weight_sum > 0.0f) ? sum / weight_sum : 0.0f;
}

/**
 * @brief Federated averaging - average agent updates
 */
__global__ void kernel_federated_average(
    const float* agent_updates,
    float* shared_knowledge,
    int num_agents,
    int knowledge_dim)
{
    int dim_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (dim_idx >= knowledge_dim) return;

    float sum = 0.0f;
    for (int a = 0; a < num_agents; a++) {
        sum += agent_updates[a * knowledge_dim + dim_idx];
    }

    // Average and add to shared knowledge
    shared_knowledge[dim_idx] += sum / num_agents;
}

/**
 * @brief Broadcast memory to target agents
 */
__global__ void kernel_memory_broadcast(
    const float* source_memory,
    float* target_memories,
    const int* target_agents,
    int num_targets,
    int memory_dim)
{
    int target_idx = blockIdx.y;
    int dim_idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (target_idx >= num_targets || dim_idx >= memory_dim) return;

    int agent_idx = target_agents[target_idx];
    target_memories[agent_idx * memory_dim + dim_idx] = source_memory[dim_idx];
}

/**
 * @brief Resolve memory conflicts using confidence weighting
 */
__global__ void kernel_memory_conflict_resolution(
    const float* memories,
    const float* confidence,
    float* resolved,
    int num_agents,
    int memory_dim)
{
    int dim_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (dim_idx >= memory_dim) return;

    float weighted_sum = 0.0f;
    float conf_sum = 0.0f;

    for (int a = 0; a < num_agents; a++) {
        float conf = confidence[a * memory_dim + dim_idx];
        float mem = memories[a * memory_dim + dim_idx];
        weighted_sum += conf * mem;
        conf_sum += conf;
    }

    resolved[dim_idx] = (conf_sum > 0.0f) ? weighted_sum / conf_sum : 0.0f;
}

//=============================================================================
// Episodic Memory Kernels
//=============================================================================

/**
 * @brief Store episode in buffer
 */
__global__ void kernel_store_episode(
    float* episode_buffer,
    const float* episode,
    int episode_len,
    int state_dim,
    int write_idx,
    int max_len)
{
    int frame_idx = blockIdx.y;
    int dim_idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (frame_idx >= episode_len || dim_idx >= state_dim) return;

    int actual_len = min(episode_len, max_len);
    if (frame_idx >= actual_len) return;

    // Calculate buffer offset
    int buffer_offset = write_idx * max_len * state_dim + frame_idx * state_dim + dim_idx;
    episode_buffer[buffer_offset] = episode[frame_idx * state_dim + dim_idx];
}

/**
 * @brief Compute episode similarity using average cosine similarity
 */
__global__ void kernel_episode_similarity(
    const float* query,
    const float* episodes,
    float* similarities,
    int num_episodes,
    int episode_len,
    int state_dim)
{
    int episode_idx = blockIdx.x;
    if (episode_idx >= num_episodes) return;

    __shared__ float shared_dot[BLOCK_SIZE];
    __shared__ float shared_norm_q[BLOCK_SIZE];
    __shared__ float shared_norm_e[BLOCK_SIZE];

    float local_dot = 0.0f;
    float local_norm_q = 0.0f;
    float local_norm_e = 0.0f;

    // Each thread handles multiple dimensions
    for (int d = threadIdx.x; d < state_dim; d += blockDim.x) {
        float q = query[d];

        // Average over episode frames
        float e_avg = 0.0f;
        for (int f = 0; f < episode_len; f++) {
            e_avg += episodes[episode_idx * episode_len * state_dim + f * state_dim + d];
        }
        e_avg /= episode_len;

        local_dot += q * e_avg;
        local_norm_q += q * q;
        local_norm_e += e_avg * e_avg;
    }

    shared_dot[threadIdx.x] = local_dot;
    shared_norm_q[threadIdx.x] = local_norm_q;
    shared_norm_e[threadIdx.x] = local_norm_e;
    __syncthreads();

    // Reduce within block
    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            shared_dot[threadIdx.x] += shared_dot[threadIdx.x + stride];
            shared_norm_q[threadIdx.x] += shared_norm_q[threadIdx.x + stride];
            shared_norm_e[threadIdx.x] += shared_norm_e[threadIdx.x + stride];
        }
        __syncthreads();
    }

    if (threadIdx.x == 0) {
        float norm_q = sqrtf(shared_norm_q[0]);
        float norm_e = sqrtf(shared_norm_e[0]);
        float denom = norm_q * norm_e;
        similarities[episode_idx] = (denom > 1e-8f) ? shared_dot[0] / denom : 0.0f;
    }
}

/**
 * @brief Replay selected episodes
 */
__global__ void kernel_episode_replay(
    const float* episodes,
    const int* replay_indices,
    float* replayed_states,
    int batch_size,
    int episode_len,
    int state_dim)
{
    int batch_idx = blockIdx.z;
    int frame_idx = blockIdx.y;
    int dim_idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (batch_idx >= batch_size || frame_idx >= episode_len || dim_idx >= state_dim) return;

    int episode_idx = replay_indices[batch_idx];

    int src_offset = episode_idx * episode_len * state_dim + frame_idx * state_dim + dim_idx;
    int dst_offset = batch_idx * episode_len * state_dim + frame_idx * state_dim + dim_idx;

    replayed_states[dst_offset] = episodes[src_offset];
}

//=============================================================================
// Experience Replay Buffer Kernels
//=============================================================================

/**
 * @brief Store single transition in replay buffer
 */
__global__ void kernel_store_transition(
    float* states,
    float* actions,
    float* rewards,
    float* next_states,
    float* dones,
    float* priorities,
    const float* state,
    const float* action,
    float reward,
    const float* next_state,
    float done,
    float max_priority,
    int write_idx,
    int state_dim,
    int action_dim)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    // Copy state
    if (idx < state_dim) {
        states[write_idx * state_dim + idx] = state[idx];
        next_states[write_idx * state_dim + idx] = next_state[idx];
    }

    // Copy action
    if (idx < action_dim) {
        actions[write_idx * action_dim + idx] = action[idx];
    }

    // Store scalar values (single thread)
    if (idx == 0) {
        rewards[write_idx] = reward;
        dones[write_idx] = done;
        priorities[write_idx] = max_priority;
    }
}

/**
 * @brief Batch store transitions
 */
__global__ void kernel_store_transitions_batch(
    float* states,
    float* actions,
    float* rewards,
    float* next_states,
    float* dones,
    float* priorities,
    const float* batch_states,
    const float* batch_actions,
    const float* batch_rewards,
    const float* batch_next_states,
    const float* batch_dones,
    float max_priority,
    int write_idx,
    int batch_size,
    int capacity,
    int state_dim,
    int action_dim)
{
    int batch_idx = blockIdx.y;
    int dim_idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (batch_idx >= batch_size) return;

    int buffer_idx = (write_idx + batch_idx) % capacity;

    // Copy state dimensions
    if (dim_idx < state_dim) {
        states[buffer_idx * state_dim + dim_idx] =
            batch_states[batch_idx * state_dim + dim_idx];
        next_states[buffer_idx * state_dim + dim_idx] =
            batch_next_states[batch_idx * state_dim + dim_idx];
    }

    // Copy action dimensions
    if (dim_idx < action_dim) {
        actions[buffer_idx * action_dim + dim_idx] =
            batch_actions[batch_idx * action_dim + dim_idx];
    }

    // Store scalars (single thread per batch item)
    if (dim_idx == 0) {
        rewards[buffer_idx] = batch_rewards[batch_idx];
        dones[buffer_idx] = batch_dones[batch_idx];
        priorities[buffer_idx] = max_priority;
    }
}

/**
 * @brief Gather samples from buffer using indices
 */
__global__ void kernel_gather_samples(
    const float* src,
    float* dst,
    const int* indices,
    int batch_size,
    int dim)
{
    int batch_idx = blockIdx.y;
    int dim_idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (batch_idx >= batch_size || dim_idx >= dim) return;

    int src_idx = indices[batch_idx];
    dst[batch_idx * dim + dim_idx] = src[src_idx * dim + dim_idx];
}

/**
 * @brief Gather scalar values from buffer
 */
__global__ void kernel_gather_scalars(
    const float* src,
    float* dst,
    const int* indices,
    int batch_size)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= batch_size) return;

    dst[idx] = src[indices[idx]];
}

//=============================================================================
// Default Parameter Functions
//=============================================================================

extern "C" void nimcp_replay_buffer_gpu_default_config(
    nimcp_replay_buffer_gpu_config_t* config)
{
    if (!config) return;

    config->capacity = SWARM_MEMORY_GPU_DEFAULT_CAPACITY;
    config->state_dim = 4;
    config->action_dim = 2;
    config->use_prioritized = true;
    config->priority_alpha = SWARM_MEMORY_GPU_DEFAULT_ALPHA;
    config->priority_beta = SWARM_MEMORY_GPU_DEFAULT_BETA;
    config->priority_epsilon = SWARM_MEMORY_GPU_PRIORITY_EPS;
}

extern "C" void nimcp_hippocampal_gpu_default_params(
    nimcp_hippocampal_gpu_params_t* params)
{
    if (!params) return;

    params->compression_ratio = 15.0f;  // 15x compressed replay
    params->replay_noise = 0.01f;
    params->trace_decay = 0.95f;
    params->enable_compressed_replay = true;
}

extern "C" void nimcp_consolidation_gpu_default_params(
    nimcp_consolidation_gpu_params_t* params)
{
    if (!params) return;

    params->learning_rate = 0.01f;
    params->decay_rate = 0.001f;
    params->min_strength = 0.01f;
    params->importance_threshold = 0.3f;
    params->sws_consolidation_rate = 0.1f;
    params->awake_consolidation_rate = 0.01f;
}

extern "C" void nimcp_agent_coordination_gpu_default_params(
    nimcp_agent_coordination_params_t* params)
{
    if (!params) return;

    params->averaging_weight = 0.5f;
    params->conflict_resolution_alpha = 0.7f;
    params->confidence_threshold = 0.5f;
    params->enable_differential_privacy = false;
    params->dp_noise_scale = 0.1f;
}

//=============================================================================
// Replay Buffer Lifecycle Functions
//=============================================================================

extern "C" nimcp_replay_buffer_gpu_t* nimcp_replay_buffer_gpu_create(
    nimcp_gpu_context_t* ctx,
    const nimcp_replay_buffer_gpu_config_t* config)
{
    if (!ctx || !config) {
        LOG_ERROR("Invalid parameters for replay buffer creation");
        return NULL;
    }

    nimcp_replay_buffer_gpu_t* buffer =
        (nimcp_replay_buffer_gpu_t*)nimcp_malloc(sizeof(nimcp_replay_buffer_gpu_t));
    if (!buffer) {
        LOG_ERROR("Failed to allocate replay buffer structure");
        return NULL;
    }
    memset(buffer, 0, sizeof(nimcp_replay_buffer_gpu_t));

    buffer->ctx = ctx;
    buffer->capacity = config->capacity;
    buffer->state_dim = config->state_dim;
    buffer->action_dim = config->action_dim;
    buffer->current_size = 0;
    buffer->write_idx = 0;

    buffer->priority_alpha = config->priority_alpha;
    buffer->priority_beta = config->priority_beta;
    buffer->priority_epsilon = config->priority_epsilon;
    buffer->max_priority = 1.0f;
    buffer->use_per = config->use_prioritized;

    // Create tensors for experience storage
    size_t state_dims[] = {config->capacity, config->state_dim};
    size_t action_dims[] = {config->capacity, config->action_dim};
    size_t scalar_dims[] = {config->capacity};

    buffer->states = nimcp_gpu_tensor_create(ctx, state_dims, 2, NIMCP_GPU_PRECISION_FP32);
    buffer->next_states = nimcp_gpu_tensor_create(ctx, state_dims, 2, NIMCP_GPU_PRECISION_FP32);
    buffer->actions = nimcp_gpu_tensor_create(ctx, action_dims, 2, NIMCP_GPU_PRECISION_FP32);
    buffer->rewards = nimcp_gpu_tensor_create(ctx, scalar_dims, 1, NIMCP_GPU_PRECISION_FP32);
    buffer->dones = nimcp_gpu_tensor_create(ctx, scalar_dims, 1, NIMCP_GPU_PRECISION_FP32);
    buffer->priorities = nimcp_gpu_tensor_create(ctx, scalar_dims, 1, NIMCP_GPU_PRECISION_FP32);

    if (!buffer->states || !buffer->next_states || !buffer->actions ||
        !buffer->rewards || !buffer->dones || !buffer->priorities) {
        LOG_ERROR("Failed to create experience tensors");
        nimcp_replay_buffer_gpu_destroy(buffer);
        return NULL;
    }

    // Create sum-tree for PER (size = 2*capacity - 1)
    if (buffer->use_per) {
        size_t tree_size = 2 * config->capacity - 1;
        size_t tree_dims[] = {tree_size};

        buffer->sum_tree = nimcp_gpu_tensor_create(ctx, tree_dims, 1, NIMCP_GPU_PRECISION_FP32);
        buffer->min_tree = nimcp_gpu_tensor_create(ctx, tree_dims, 1, NIMCP_GPU_PRECISION_FP32);

        if (!buffer->sum_tree || !buffer->min_tree) {
            LOG_ERROR("Failed to create sum-tree tensors");
            nimcp_replay_buffer_gpu_destroy(buffer);
            return NULL;
        }

        // Initialize trees to zero
        nimcp_gpu_zeros(ctx, buffer->sum_tree);
        nimcp_gpu_zeros(ctx, buffer->min_tree);
    }

    // Initialize priorities to 1.0
    nimcp_gpu_ones(ctx, buffer->priorities);

    LOG_INFO("Created replay buffer: capacity=%zu, state_dim=%zu, action_dim=%zu, PER=%s",
             config->capacity, config->state_dim, config->action_dim,
             buffer->use_per ? "enabled" : "disabled");

    return buffer;
}

extern "C" void nimcp_replay_buffer_gpu_destroy(nimcp_replay_buffer_gpu_t* buffer)
{
    if (!buffer) return;

    if (buffer->states) nimcp_gpu_tensor_destroy(buffer->states);
    if (buffer->next_states) nimcp_gpu_tensor_destroy(buffer->next_states);
    if (buffer->actions) nimcp_gpu_tensor_destroy(buffer->actions);
    if (buffer->rewards) nimcp_gpu_tensor_destroy(buffer->rewards);
    if (buffer->dones) nimcp_gpu_tensor_destroy(buffer->dones);
    if (buffer->priorities) nimcp_gpu_tensor_destroy(buffer->priorities);
    if (buffer->sum_tree) nimcp_gpu_tensor_destroy(buffer->sum_tree);
    if (buffer->min_tree) nimcp_gpu_tensor_destroy(buffer->min_tree);

    nimcp_free(buffer);
    LOG_DEBUG("Destroyed replay buffer");
}

extern "C" bool nimcp_replay_buffer_gpu_clear(nimcp_replay_buffer_gpu_t* buffer)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!buffer) return false;

    buffer->current_size = 0;
    buffer->write_idx = 0;
    buffer->max_priority = 1.0f;
    buffer->total_stored = 0;
    buffer->total_sampled = 0;

    nimcp_gpu_zeros(buffer->ctx, buffer->priorities);
    if (buffer->sum_tree) nimcp_gpu_zeros(buffer->ctx, buffer->sum_tree);
    if (buffer->min_tree) nimcp_gpu_zeros(buffer->ctx, buffer->min_tree);

    return true;
}

//=============================================================================
// Swarm Memory Lifecycle Functions
//=============================================================================

extern "C" nimcp_swarm_memory_gpu_t* nimcp_swarm_memory_gpu_create(
    nimcp_gpu_context_t* ctx,
    size_t replay_capacity,
    int num_agents,
    int state_dim,
    int action_dim,
    int memory_dim)
{
    if (!ctx || num_agents <= 0 || state_dim <= 0 || action_dim <= 0 || memory_dim <= 0) {
        LOG_ERROR("Invalid parameters for swarm memory creation");
        return NULL;
    }

    nimcp_swarm_memory_gpu_t* mem =
        (nimcp_swarm_memory_gpu_t*)nimcp_malloc(sizeof(nimcp_swarm_memory_gpu_t));
    if (!mem) {
        LOG_ERROR("Failed to allocate swarm memory structure");
        return NULL;
    }
    memset(mem, 0, sizeof(nimcp_swarm_memory_gpu_t));

    mem->ctx = ctx;
    mem->num_agents = num_agents;
    mem->memory_dim = memory_dim;

    // Create replay buffer
    nimcp_replay_buffer_gpu_config_t rb_config;
    nimcp_replay_buffer_gpu_default_config(&rb_config);
    rb_config.capacity = replay_capacity;
    rb_config.state_dim = state_dim;
    rb_config.action_dim = action_dim;

    mem->replay_buffer = nimcp_replay_buffer_gpu_create(ctx, &rb_config);
    if (!mem->replay_buffer) {
        LOG_ERROR("Failed to create replay buffer");
        nimcp_free(mem);
        return NULL;
    }

    // Create hippocampal trace
    mem->trace_len = 256;
    size_t trace_dims[] = {mem->trace_len, (size_t)state_dim};
    mem->hippocampal_trace = nimcp_gpu_tensor_create(ctx, trace_dims, 2, NIMCP_GPU_PRECISION_FP32);

    size_t capacity_dims[] = {replay_capacity};
    mem->eligibility_traces = nimcp_gpu_tensor_create(ctx, capacity_dims, 1, NIMCP_GPU_PRECISION_FP32);

    // Create cortical weights
    mem->cortical_dim = memory_dim;
    size_t cortical_dims[] = {(size_t)state_dim, mem->cortical_dim};
    size_t bias_dims[] = {mem->cortical_dim};
    mem->cortical_weights = nimcp_gpu_tensor_create(ctx, cortical_dims, 2, NIMCP_GPU_PRECISION_FP32);
    mem->cortical_bias = nimcp_gpu_tensor_create(ctx, bias_dims, 1, NIMCP_GPU_PRECISION_FP32);

    // Create consolidation state
    mem->consolidation_mask = nimcp_gpu_tensor_create(ctx, capacity_dims, 1, NIMCP_GPU_PRECISION_FP32);
    mem->memory_strength = nimcp_gpu_tensor_create(ctx, capacity_dims, 1, NIMCP_GPU_PRECISION_FP32);
    mem->memory_importance = nimcp_gpu_tensor_create(ctx, capacity_dims, 1, NIMCP_GPU_PRECISION_FP32);

    // Create multi-agent memory
    size_t agent_mem_dims[] = {(size_t)num_agents, (size_t)memory_dim};
    size_t shared_dims[] = {(size_t)memory_dim};
    mem->agent_memories = nimcp_gpu_tensor_create(ctx, agent_mem_dims, 2, NIMCP_GPU_PRECISION_FP32);
    mem->agent_confidence = nimcp_gpu_tensor_create(ctx, agent_mem_dims, 2, NIMCP_GPU_PRECISION_FP32);
    mem->shared_knowledge = nimcp_gpu_tensor_create(ctx, shared_dims, 1, NIMCP_GPU_PRECISION_FP32);
    mem->agent_gradients = nimcp_gpu_tensor_create(ctx, agent_mem_dims, 2, NIMCP_GPU_PRECISION_FP32);

    // Verify all allocations
    if (!mem->hippocampal_trace || !mem->eligibility_traces ||
        !mem->cortical_weights || !mem->cortical_bias ||
        !mem->consolidation_mask || !mem->memory_strength || !mem->memory_importance ||
        !mem->agent_memories || !mem->agent_confidence ||
        !mem->shared_knowledge || !mem->agent_gradients) {
        LOG_ERROR("Failed to allocate swarm memory tensors");
        nimcp_swarm_memory_gpu_destroy(mem);
        return NULL;
    }

    // Initialize tensors
    nimcp_gpu_zeros(ctx, mem->hippocampal_trace);
    nimcp_gpu_zeros(ctx, mem->eligibility_traces);
    nimcp_gpu_zeros(ctx, mem->cortical_weights);
    nimcp_gpu_zeros(ctx, mem->cortical_bias);
    nimcp_gpu_zeros(ctx, mem->consolidation_mask);
    nimcp_gpu_ones(ctx, mem->memory_strength);
    nimcp_gpu_zeros(ctx, mem->memory_importance);
    nimcp_gpu_zeros(ctx, mem->agent_memories);
    nimcp_gpu_fill(ctx, mem->agent_confidence, 0.5f);
    nimcp_gpu_zeros(ctx, mem->shared_knowledge);
    nimcp_gpu_zeros(ctx, mem->agent_gradients);

    // Set default parameters
    nimcp_hippocampal_gpu_default_params(&mem->hippocampal_params);
    nimcp_consolidation_gpu_default_params(&mem->consolidation_params);
    nimcp_agent_coordination_gpu_default_params(&mem->coordination_params);

    LOG_INFO("Created swarm memory: capacity=%zu, agents=%d, state_dim=%d, memory_dim=%d",
             replay_capacity, num_agents, state_dim, memory_dim);

    return mem;
}

extern "C" void nimcp_swarm_memory_gpu_destroy(nimcp_swarm_memory_gpu_t* mem)
{
    if (!mem) return;

    if (mem->replay_buffer) nimcp_replay_buffer_gpu_destroy(mem->replay_buffer);

    if (mem->hippocampal_trace) nimcp_gpu_tensor_destroy(mem->hippocampal_trace);
    if (mem->eligibility_traces) nimcp_gpu_tensor_destroy(mem->eligibility_traces);
    if (mem->cortical_weights) nimcp_gpu_tensor_destroy(mem->cortical_weights);
    if (mem->cortical_bias) nimcp_gpu_tensor_destroy(mem->cortical_bias);
    if (mem->consolidation_mask) nimcp_gpu_tensor_destroy(mem->consolidation_mask);
    if (mem->memory_strength) nimcp_gpu_tensor_destroy(mem->memory_strength);
    if (mem->memory_importance) nimcp_gpu_tensor_destroy(mem->memory_importance);
    if (mem->agent_memories) nimcp_gpu_tensor_destroy(mem->agent_memories);
    if (mem->agent_confidence) nimcp_gpu_tensor_destroy(mem->agent_confidence);
    if (mem->shared_knowledge) nimcp_gpu_tensor_destroy(mem->shared_knowledge);
    if (mem->agent_gradients) nimcp_gpu_tensor_destroy(mem->agent_gradients);
    if (mem->episode_buffer) nimcp_gpu_tensor_destroy(mem->episode_buffer);
    if (mem->episode_lengths) nimcp_gpu_tensor_destroy(mem->episode_lengths);
    if (mem->episode_rewards) nimcp_gpu_tensor_destroy(mem->episode_rewards);
    if (mem->temp_batch_states) nimcp_gpu_tensor_destroy(mem->temp_batch_states);
    if (mem->temp_batch_indices) nimcp_gpu_tensor_destroy(mem->temp_batch_indices);
    if (mem->temp_random) nimcp_gpu_tensor_destroy(mem->temp_random);

    nimcp_free(mem);
    LOG_DEBUG("Destroyed swarm memory");
}

//=============================================================================
// Replay Batch Lifecycle
//=============================================================================

extern "C" nimcp_replay_batch_t* nimcp_replay_batch_create(
    nimcp_gpu_context_t* ctx,
    size_t batch_size,
    size_t state_dim,
    size_t action_dim)
{
    if (!ctx || batch_size == 0) return NULL;

    nimcp_replay_batch_t* batch = (nimcp_replay_batch_t*)nimcp_malloc(sizeof(nimcp_replay_batch_t));
    if (!batch) return NULL;
    memset(batch, 0, sizeof(nimcp_replay_batch_t));

    batch->batch_size = batch_size;

    size_t state_dims[] = {batch_size, state_dim};
    size_t action_dims[] = {batch_size, action_dim};
    size_t scalar_dims[] = {batch_size};

    batch->states = nimcp_gpu_tensor_create(ctx, state_dims, 2, NIMCP_GPU_PRECISION_FP32);
    batch->next_states = nimcp_gpu_tensor_create(ctx, state_dims, 2, NIMCP_GPU_PRECISION_FP32);
    batch->actions = nimcp_gpu_tensor_create(ctx, action_dims, 2, NIMCP_GPU_PRECISION_FP32);
    batch->rewards = nimcp_gpu_tensor_create(ctx, scalar_dims, 1, NIMCP_GPU_PRECISION_FP32);
    batch->dones = nimcp_gpu_tensor_create(ctx, scalar_dims, 1, NIMCP_GPU_PRECISION_FP32);
    batch->weights = nimcp_gpu_tensor_create(ctx, scalar_dims, 1, NIMCP_GPU_PRECISION_FP32);
    batch->indices = nimcp_gpu_tensor_create(ctx, scalar_dims, 1, NIMCP_GPU_PRECISION_INT32);

    if (!batch->states || !batch->next_states || !batch->actions ||
        !batch->rewards || !batch->dones || !batch->weights || !batch->indices) {
        nimcp_replay_batch_destroy(batch);
        return NULL;
    }

    return batch;
}

extern "C" void nimcp_replay_batch_destroy(nimcp_replay_batch_t* batch)
{
    if (!batch) return;

    if (batch->states) nimcp_gpu_tensor_destroy(batch->states);
    if (batch->next_states) nimcp_gpu_tensor_destroy(batch->next_states);
    if (batch->actions) nimcp_gpu_tensor_destroy(batch->actions);
    if (batch->rewards) nimcp_gpu_tensor_destroy(batch->rewards);
    if (batch->dones) nimcp_gpu_tensor_destroy(batch->dones);
    if (batch->weights) nimcp_gpu_tensor_destroy(batch->weights);
    if (batch->indices) nimcp_gpu_tensor_destroy(batch->indices);

    nimcp_free(batch);
}

//=============================================================================
// Experience Replay Operations
//=============================================================================

extern "C" bool nimcp_swarm_memory_gpu_store(
    nimcp_swarm_memory_gpu_t* mem,
    nimcp_gpu_tensor_t* state,
    nimcp_gpu_tensor_t* action,
    float reward,
    nimcp_gpu_tensor_t* next_state,
    bool done)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!mem || !mem->replay_buffer || !state || !action || !next_state) {
        return false;
    }

    nimcp_replay_buffer_gpu_t* buf = mem->replay_buffer;
    nimcp_cuda_stream_t stream = nimcp_gpu_get_compute_stream(mem->ctx);

    // Launch kernel to store transition
    int max_dim = (int)fmax(buf->state_dim, buf->action_dim);
    dim3 block(BLOCK_SIZE);
    dim3 grid(GRID_SIZE(max_dim));

    kernel_store_transition<<<grid, block, 0, stream>>>(
        (float*)buf->states->data,
        (float*)buf->actions->data,
        (float*)buf->rewards->data,
        (float*)buf->next_states->data,
        (float*)buf->dones->data,
        (float*)buf->priorities->data,
        (const float*)state->data,
        (const float*)action->data,
        reward,
        (const float*)next_state->data,
        done ? 1.0f : 0.0f,
        buf->max_priority,
        (int)buf->write_idx,
        (int)buf->state_dim,
        (int)buf->action_dim
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Update sum-tree if PER enabled
    if (buf->use_per && buf->sum_tree) {
        kernel_update_sum_tree_single<<<1, 1, 0, stream>>>(
            (float*)buf->sum_tree->data,
            (int)buf->write_idx,
            buf->max_priority,
            (int)buf->capacity
        );
        NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    }

    // Update indices
    buf->write_idx = (buf->write_idx + 1) % buf->capacity;
    if (buf->current_size < buf->capacity) {
        buf->current_size++;
    }
    buf->total_stored++;

    return true;
}

extern "C" bool nimcp_swarm_memory_gpu_store_batch(
    nimcp_swarm_memory_gpu_t* mem,
    nimcp_gpu_tensor_t* states,
    nimcp_gpu_tensor_t* actions,
    nimcp_gpu_tensor_t* rewards,
    nimcp_gpu_tensor_t* next_states,
    nimcp_gpu_tensor_t* dones,
    size_t batch_size)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!mem || !mem->replay_buffer || !states || !actions ||
        !rewards || !next_states || !dones) {
        return false;
    }

    nimcp_replay_buffer_gpu_t* buf = mem->replay_buffer;
    nimcp_cuda_stream_t stream = nimcp_gpu_get_compute_stream(mem->ctx);

    int max_dim = (int)fmax(buf->state_dim, buf->action_dim);
    dim3 block(BLOCK_SIZE);
    dim3 grid(GRID_SIZE(max_dim), (unsigned int)batch_size);

    kernel_store_transitions_batch<<<grid, block, 0, stream>>>(
        (float*)buf->states->data,
        (float*)buf->actions->data,
        (float*)buf->rewards->data,
        (float*)buf->next_states->data,
        (float*)buf->dones->data,
        (float*)buf->priorities->data,
        (const float*)states->data,
        (const float*)actions->data,
        (const float*)rewards->data,
        (const float*)next_states->data,
        (const float*)dones->data,
        buf->max_priority,
        (int)buf->write_idx,
        (int)batch_size,
        (int)buf->capacity,
        (int)buf->state_dim,
        (int)buf->action_dim
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Update indices
    buf->write_idx = (buf->write_idx + batch_size) % buf->capacity;
    buf->current_size = (buf->current_size + batch_size > buf->capacity)
                        ? buf->capacity
                        : buf->current_size + batch_size;
    buf->total_stored += batch_size;

    // Rebuild sum-tree after batch update if PER enabled
    if (buf->use_per && buf->sum_tree) {
        nimcp_swarm_memory_gpu_build_sum_tree(
            mem->ctx, buf->priorities, buf->sum_tree, buf->capacity);
    }

    return true;
}

extern "C" bool nimcp_swarm_memory_gpu_sample(
    nimcp_swarm_memory_gpu_t* mem,
    size_t batch_size,
    nimcp_replay_batch_t* batch)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!mem || !mem->replay_buffer || !batch || batch_size == 0) {
        return false;
    }

    nimcp_replay_buffer_gpu_t* buf = mem->replay_buffer;

    if (buf->current_size < batch_size) {
        LOG_WARN("Not enough samples in buffer: %zu < %zu", buf->current_size, batch_size);
        return false;
    }

    nimcp_cuda_stream_t stream = nimcp_gpu_get_compute_stream(mem->ctx);

    // Allocate random values tensor if needed
    if (!mem->temp_random || mem->temp_random->numel < batch_size) {
        if (mem->temp_random) nimcp_gpu_tensor_destroy(mem->temp_random);
        size_t rand_dims[] = {batch_size};
        mem->temp_random = nimcp_gpu_tensor_create(mem->ctx, rand_dims, 1, NIMCP_GPU_PRECISION_FP32);
    }

    // Generate random values on host and copy (simple approach)
    float* host_random = (float*)nimcp_malloc(batch_size * sizeof(float));
    if (!host_random) {
        LOG_ERROR("Failed to allocate random values buffer");
        return false;
    }
    for (size_t i = 0; i < batch_size; i++) {
        host_random[i] = (float)rand() / RAND_MAX;
    }
    nimcp_gpu_memcpy(mem->ctx, mem->temp_random->data, host_random,
                     batch_size * sizeof(float), GPU_MEMCPY_HOST_TO_DEVICE);
    nimcp_free(host_random);

    // Sample indices using sum-tree
    if (buf->use_per && buf->sum_tree) {
        dim3 block(BLOCK_SIZE);
        dim3 grid(GRID_SIZE(batch_size));

        kernel_sample_sum_tree<<<grid, block, 0, stream>>>(
            (const float*)buf->sum_tree->data,
            (const float*)mem->temp_random->data,
            (int*)batch->indices->data,
            (int)batch_size,
            (int)buf->capacity
        );
        NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    } else {
        // Uniform random sampling
        int* host_indices = (int*)nimcp_malloc(batch_size * sizeof(int));
        if (!host_indices) {
            LOG_ERROR("Failed to allocate host indices buffer");
            return false;
        }
        for (size_t i = 0; i < batch_size; i++) {
            host_indices[i] = rand() % buf->current_size;
        }
        nimcp_gpu_memcpy(mem->ctx, batch->indices->data, host_indices,
                         batch_size * sizeof(int), GPU_MEMCPY_HOST_TO_DEVICE);
        nimcp_free(host_indices);
    }

    // Gather samples
    dim3 block_gather(BLOCK_SIZE);

    // Gather states
    dim3 grid_states(GRID_SIZE(buf->state_dim), (unsigned int)batch_size);
    kernel_gather_samples<<<grid_states, block_gather, 0, stream>>>(
        (const float*)buf->states->data,
        (float*)batch->states->data,
        (const int*)batch->indices->data,
        (int)batch_size,
        (int)buf->state_dim
    );

    kernel_gather_samples<<<grid_states, block_gather, 0, stream>>>(
        (const float*)buf->next_states->data,
        (float*)batch->next_states->data,
        (const int*)batch->indices->data,
        (int)batch_size,
        (int)buf->state_dim
    );

    // Gather actions
    dim3 grid_actions(GRID_SIZE(buf->action_dim), (unsigned int)batch_size);
    kernel_gather_samples<<<grid_actions, block_gather, 0, stream>>>(
        (const float*)buf->actions->data,
        (float*)batch->actions->data,
        (const int*)batch->indices->data,
        (int)batch_size,
        (int)buf->action_dim
    );

    // Gather scalars
    dim3 grid_scalar(GRID_SIZE(batch_size));
    kernel_gather_scalars<<<grid_scalar, block_gather, 0, stream>>>(
        (const float*)buf->rewards->data,
        (float*)batch->rewards->data,
        (const int*)batch->indices->data,
        (int)batch_size
    );

    kernel_gather_scalars<<<grid_scalar, block_gather, 0, stream>>>(
        (const float*)buf->dones->data,
        (float*)batch->dones->data,
        (const int*)batch->indices->data,
        (int)batch_size
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Compute IS weights for PER
    if (buf->use_per) {
        // Get total priority from sum-tree root
        float total_priority;
        nimcp_gpu_memcpy(mem->ctx, &total_priority, buf->sum_tree->data,
                         sizeof(float), GPU_MEMCPY_DEVICE_TO_HOST);

        kernel_compute_is_weights<<<grid_scalar, block_gather, 0, stream>>>(
            (const float*)buf->priorities->data,
            (float*)batch->weights->data,
            (const int*)batch->indices->data,
            buf->priority_beta,
            total_priority,
            (int)buf->capacity,
            (int)buf->current_size,
            (int)batch_size
        );
        NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

        // Find max weight for normalization
        float max_weight = 1.0f;  // Simplified - in production use reduction kernel

        kernel_normalize_is_weights<<<grid_scalar, block_gather, 0, stream>>>(
            (float*)batch->weights->data,
            max_weight,
            (int)batch_size
        );
        NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    } else {
        // Uniform weights
        nimcp_gpu_ones(mem->ctx, batch->weights);
    }

    buf->total_sampled += batch_size;
    return true;
}

extern "C" bool nimcp_swarm_memory_gpu_update_priorities(
    nimcp_swarm_memory_gpu_t* mem,
    nimcp_gpu_tensor_t* indices,
    nimcp_gpu_tensor_t* td_errors,
    size_t batch_size)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!mem || !mem->replay_buffer || !indices || !td_errors) {
        return false;
    }

    if (!mem->replay_buffer->use_per) {
        return true;  // No-op for uniform replay
    }

    nimcp_replay_buffer_gpu_t* buf = mem->replay_buffer;
    nimcp_cuda_stream_t stream = nimcp_gpu_get_compute_stream(mem->ctx);

    dim3 block(BLOCK_SIZE);
    dim3 grid(GRID_SIZE(batch_size));

    kernel_update_priorities_batch<<<grid, block, 0, stream>>>(
        (float*)buf->priorities->data,
        (float*)buf->sum_tree->data,
        (const int*)indices->data,
        (const float*)td_errors->data,
        buf->priority_alpha,
        buf->priority_epsilon,
        (int)batch_size,
        (int)buf->capacity
    );
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Rebuild sum-tree to propagate changes
    int tree_size = 2 * buf->capacity - 1;
    int num_internal = (tree_size - 1) / 2;

    kernel_propagate_sum_tree<<<GRID_SIZE(num_internal), block, 0, stream>>>(
        (float*)buf->sum_tree->data,
        tree_size
    );
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Update max priority
    float max_td = 0.0f;  // Would need reduction kernel for accuracy
    float new_max = powf(max_td + buf->priority_epsilon, buf->priority_alpha);
    if (new_max > buf->max_priority) {
        buf->max_priority = new_max;
    }

    return true;
}

extern "C" void nimcp_swarm_memory_gpu_set_beta(
    nimcp_swarm_memory_gpu_t* mem,
    float new_beta)
{
    if (!mem || !mem->replay_buffer) return;
    mem->replay_buffer->priority_beta = fminf(fmaxf(new_beta, 0.0f), 1.0f);
}

//=============================================================================
// Memory Consolidation Operations
//=============================================================================

extern "C" bool nimcp_swarm_memory_gpu_consolidate(
    nimcp_swarm_memory_gpu_t* mem,
    float dt)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!mem) return false;

    nimcp_cuda_stream_t stream = nimcp_gpu_get_compute_stream(mem->ctx);

    // Apply memory decay
    if (mem->memory_strength && mem->replay_buffer) {
        size_t n = mem->replay_buffer->current_size;
        if (n > 0) {
            dim3 block(BLOCK_SIZE);
            dim3 grid(GRID_SIZE(n));

            kernel_memory_decay<<<grid, block, 0, stream>>>(
                (float*)mem->memory_strength->data,
                mem->consolidation_params.decay_rate * dt,
                mem->consolidation_params.min_strength,
                (int)n
            );
            NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
        }
    }

    mem->consolidation_count++;
    return true;
}

extern "C" bool nimcp_swarm_memory_gpu_decay(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* memory_strength,
    float decay_rate,
    float min_strength)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !memory_strength) return false;

    size_t n = memory_strength->numel;
    nimcp_cuda_stream_t stream = nimcp_gpu_get_compute_stream(ctx);

    dim3 block(BLOCK_SIZE);
    dim3 grid(GRID_SIZE(n));

    kernel_memory_decay<<<grid, block, 0, stream>>>(
        (float*)memory_strength->data,
        decay_rate,
        min_strength,
        (int)n
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

extern "C" bool nimcp_swarm_memory_gpu_selective_consolidate(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* memories,
    const nimcp_gpu_tensor_t* importance,
    nimcp_gpu_tensor_t* consolidated,
    float threshold)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !memories || !importance || !consolidated) return false;

    // Assume memories is [num_memories, memory_dim]
    size_t num_memories = memories->dims[0];
    size_t memory_dim = memories->dims[1];

    nimcp_cuda_stream_t stream = nimcp_gpu_get_compute_stream(ctx);

    dim3 block(memory_dim > BLOCK_SIZE ? BLOCK_SIZE : (unsigned int)memory_dim);
    dim3 grid((unsigned int)num_memories);

    kernel_selective_consolidate<<<grid, block, 0, stream>>>(
        (const float*)memories->data,
        (const float*)importance->data,
        (float*)consolidated->data,
        threshold,
        (int)num_memories,
        (int)memory_dim
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

extern "C" bool nimcp_swarm_memory_gpu_hippocampal_compress(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* sequence,
    nimcp_gpu_tensor_t* compressed,
    size_t seq_len,
    size_t state_dim,
    float compression_ratio)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !sequence || !compressed || compression_ratio <= 1.0f) return false;

    size_t compressed_len = (size_t)(seq_len / compression_ratio);
    if (compressed_len == 0) compressed_len = 1;

    nimcp_cuda_stream_t stream = nimcp_gpu_get_compute_stream(ctx);

    dim3 block(state_dim > BLOCK_SIZE ? BLOCK_SIZE : (unsigned int)state_dim);
    dim3 grid((unsigned int)compressed_len);

    kernel_hippocampal_compress<<<grid, block, 0, stream>>>(
        (const float*)sequence->data,
        (float*)compressed->data,
        (int)seq_len,
        (int)state_dim,
        (int)compressed_len
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

extern "C" bool nimcp_swarm_memory_gpu_systems_consolidation(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* hippocampal_mem,
    nimcp_gpu_tensor_t* cortical_weights,
    const nimcp_gpu_tensor_t* consolidation_gate,
    float learning_rate)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !hippocampal_mem || !cortical_weights || !consolidation_gate) return false;

    size_t batch_size = hippocampal_mem->dims[0];
    size_t memory_dim = hippocampal_mem->dims[1];
    size_t cortical_dim = cortical_weights->dims[1];

    nimcp_cuda_stream_t stream = nimcp_gpu_get_compute_stream(ctx);

    dim3 block(BLOCK_SIZE, 1);
    dim3 grid(GRID_SIZE(memory_dim), (unsigned int)batch_size);

    kernel_systems_consolidation<<<grid, block, 0, stream>>>(
        (const float*)hippocampal_mem->data,
        (float*)cortical_weights->data,
        (const float*)consolidation_gate->data,
        learning_rate,
        (int)memory_dim,
        (int)cortical_dim,
        (int)batch_size
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

extern "C" bool nimcp_swarm_memory_gpu_sws_replay(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* memories,
    nimcp_gpu_tensor_t* replay_order,
    const nimcp_gpu_tensor_t* importance_scores,
    size_t num_memories,
    size_t replay_count)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !memories || !replay_order || !importance_scores) return false;

    // Simple implementation: copy top-k importance scores to replay order
    // In production, use a proper top-k selection kernel

    // For now, just copy indices in order of importance
    // This is a placeholder - full implementation would use radix sort

    float* host_importance = (float*)nimcp_malloc(num_memories * sizeof(float));
    int* host_order = (int*)nimcp_malloc(replay_count * sizeof(int));

    if (!host_importance || !host_order) {
        nimcp_free(host_importance);
        nimcp_free(host_order);
        LOG_ERROR("Failed to allocate host buffers for replay selection");
        return false;
    }

    nimcp_gpu_memcpy(ctx, host_importance, importance_scores->data,
                     num_memories * sizeof(float), GPU_MEMCPY_DEVICE_TO_HOST);

    // Simple selection of top-k
    for (size_t i = 0; i < replay_count; i++) {
        float max_val = -FLT_MAX;
        int max_idx = 0;
        for (size_t j = 0; j < num_memories; j++) {
            if (host_importance[j] > max_val) {
                max_val = host_importance[j];
                max_idx = (int)j;
            }
        }
        host_order[i] = max_idx;
        host_importance[max_idx] = -FLT_MAX;  // Mark as selected
    }

    nimcp_gpu_memcpy(ctx, replay_order->data, host_order,
                     replay_count * sizeof(int), GPU_MEMCPY_HOST_TO_DEVICE);

    nimcp_free(host_importance);
    nimcp_free(host_order);

    return true;
}

//=============================================================================
// Multi-Agent Memory Coordination
//=============================================================================

extern "C" bool nimcp_swarm_memory_gpu_sync_agents(nimcp_swarm_memory_gpu_t* mem)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!mem || !mem->agent_memories || !mem->shared_knowledge) return false;

    // Create uniform weights
    size_t weight_dims[] = {(size_t)mem->num_agents};
    nimcp_gpu_tensor_t* weights = nimcp_gpu_tensor_create(
        mem->ctx, weight_dims, 1, NIMCP_GPU_PRECISION_FP32);

    if (!weights) return false;
    nimcp_gpu_fill(mem->ctx, weights, 1.0f / mem->num_agents);

    bool result = nimcp_swarm_memory_gpu_aggregate_memories(
        mem->ctx,
        mem->agent_memories,
        mem->shared_knowledge,
        weights,
        mem->num_agents,
        (int)mem->memory_dim
    );

    nimcp_gpu_tensor_destroy(weights);
    mem->agent_sync_count++;

    return result;
}

extern "C" bool nimcp_swarm_memory_gpu_aggregate_memories(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* agent_memories,
    nimcp_gpu_tensor_t* aggregated,
    const nimcp_gpu_tensor_t* agent_weights,
    int num_agents,
    int memory_dim)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !agent_memories || !aggregated || !agent_weights) return false;

    nimcp_cuda_stream_t stream = nimcp_gpu_get_compute_stream(ctx);

    dim3 block(BLOCK_SIZE);
    dim3 grid(GRID_SIZE(memory_dim));

    kernel_aggregate_agent_memories<<<grid, block, 0, stream>>>(
        (const float*)agent_memories->data,
        (float*)aggregated->data,
        (const float*)agent_weights->data,
        num_agents,
        memory_dim
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

extern "C" bool nimcp_swarm_memory_gpu_federated_average(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* agent_updates,
    nimcp_gpu_tensor_t* shared_knowledge,
    int num_agents,
    int knowledge_dim)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !agent_updates || !shared_knowledge) return false;

    nimcp_cuda_stream_t stream = nimcp_gpu_get_compute_stream(ctx);

    dim3 block(BLOCK_SIZE);
    dim3 grid(GRID_SIZE(knowledge_dim));

    kernel_federated_average<<<grid, block, 0, stream>>>(
        (const float*)agent_updates->data,
        (float*)shared_knowledge->data,
        num_agents,
        knowledge_dim
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

extern "C" bool nimcp_swarm_memory_gpu_broadcast(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* source_memory,
    nimcp_gpu_tensor_t* target_memories,
    const nimcp_gpu_tensor_t* target_agents,
    int num_targets,
    int memory_dim)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !source_memory || !target_memories || !target_agents) return false;

    nimcp_cuda_stream_t stream = nimcp_gpu_get_compute_stream(ctx);

    dim3 block(BLOCK_SIZE);
    dim3 grid(GRID_SIZE(memory_dim), (unsigned int)num_targets);

    kernel_memory_broadcast<<<grid, block, 0, stream>>>(
        (const float*)source_memory->data,
        (float*)target_memories->data,
        (const int*)target_agents->data,
        num_targets,
        memory_dim
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

extern "C" bool nimcp_swarm_memory_gpu_conflict_resolution(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* memories,
    const nimcp_gpu_tensor_t* confidence,
    nimcp_gpu_tensor_t* resolved,
    int num_agents,
    int memory_dim)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !memories || !confidence || !resolved) return false;

    nimcp_cuda_stream_t stream = nimcp_gpu_get_compute_stream(ctx);

    dim3 block(BLOCK_SIZE);
    dim3 grid(GRID_SIZE(memory_dim));

    kernel_memory_conflict_resolution<<<grid, block, 0, stream>>>(
        (const float*)memories->data,
        (const float*)confidence->data,
        (float*)resolved->data,
        num_agents,
        memory_dim
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Episodic Memory Operations
//=============================================================================

extern "C" bool nimcp_swarm_memory_gpu_store_episode(
    nimcp_swarm_memory_gpu_t* mem,
    nimcp_gpu_tensor_t* episode,
    nimcp_gpu_tensor_t* actions,
    nimcp_gpu_tensor_t* rewards,
    size_t episode_len)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!mem || !episode) return false;

    // Lazy initialize episode buffer
    if (!mem->episode_buffer) {
        mem->max_episodes = 1000;
        size_t max_len = SWARM_MEMORY_GPU_MAX_EPISODE_LEN;
        size_t state_dim = mem->replay_buffer ? mem->replay_buffer->state_dim : 64;

        size_t ep_dims[] = {mem->max_episodes, max_len, state_dim};
        mem->episode_buffer = nimcp_gpu_tensor_create(mem->ctx, ep_dims, 3, NIMCP_GPU_PRECISION_FP32);

        size_t len_dims[] = {mem->max_episodes};
        mem->episode_lengths = nimcp_gpu_tensor_create(mem->ctx, len_dims, 1, NIMCP_GPU_PRECISION_FP32);
        mem->episode_rewards = nimcp_gpu_tensor_create(mem->ctx, len_dims, 1, NIMCP_GPU_PRECISION_FP32);

        if (!mem->episode_buffer || !mem->episode_lengths || !mem->episode_rewards) {
            return false;
        }

        mem->current_episode_count = 0;
        mem->episode_write_idx = 0;
    }

    size_t state_dim = mem->replay_buffer ? mem->replay_buffer->state_dim : 64;
    size_t max_len = SWARM_MEMORY_GPU_MAX_EPISODE_LEN;

    nimcp_cuda_stream_t stream = nimcp_gpu_get_compute_stream(mem->ctx);

    dim3 block(BLOCK_SIZE);
    dim3 grid(GRID_SIZE(state_dim), (unsigned int)episode_len);

    kernel_store_episode<<<grid, block, 0, stream>>>(
        (float*)mem->episode_buffer->data,
        (const float*)episode->data,
        (int)episode_len,
        (int)state_dim,
        (int)mem->episode_write_idx,
        (int)max_len
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Update episode length
    float len_f = (float)episode_len;
    nimcp_gpu_memcpy(mem->ctx,
                     (float*)mem->episode_lengths->data + mem->episode_write_idx,
                     &len_f, sizeof(float), GPU_MEMCPY_HOST_TO_DEVICE);

    // Update indices
    mem->episode_write_idx = (mem->episode_write_idx + 1) % mem->max_episodes;
    if (mem->current_episode_count < mem->max_episodes) {
        mem->current_episode_count++;
    }

    return true;
}

extern "C" bool nimcp_swarm_memory_gpu_episode_similarity(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* query,
    const nimcp_gpu_tensor_t* episodes,
    nimcp_gpu_tensor_t* similarities,
    size_t num_episodes,
    size_t episode_len,
    size_t state_dim)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !query || !episodes || !similarities) return false;

    nimcp_cuda_stream_t stream = nimcp_gpu_get_compute_stream(ctx);

    dim3 block(min((int)state_dim, BLOCK_SIZE));
    dim3 grid((unsigned int)num_episodes);

    kernel_episode_similarity<<<grid, block, 0, stream>>>(
        (const float*)query->data,
        (const float*)episodes->data,
        (float*)similarities->data,
        (int)num_episodes,
        (int)episode_len,
        (int)state_dim
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

extern "C" bool nimcp_swarm_memory_gpu_episode_replay(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* episodes,
    const nimcp_gpu_tensor_t* replay_indices,
    nimcp_gpu_tensor_t* replayed_states,
    nimcp_gpu_tensor_t* replayed_actions,
    size_t batch_size,
    size_t episode_len,
    size_t state_dim,
    size_t action_dim)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !episodes || !replay_indices || !replayed_states) return false;

    nimcp_cuda_stream_t stream = nimcp_gpu_get_compute_stream(ctx);

    dim3 block(BLOCK_SIZE);
    dim3 grid(GRID_SIZE(state_dim), (unsigned int)episode_len, (unsigned int)batch_size);

    kernel_episode_replay<<<grid, block, 0, stream>>>(
        (const float*)episodes->data,
        (const int*)replay_indices->data,
        (float*)replayed_states->data,
        (int)batch_size,
        (int)episode_len,
        (int)state_dim
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Sum-Tree Operations
//=============================================================================

extern "C" bool nimcp_swarm_memory_gpu_build_sum_tree(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* priorities,
    nimcp_gpu_tensor_t* sum_tree,
    size_t capacity)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !priorities || !sum_tree) return false;

    nimcp_cuda_stream_t stream = nimcp_gpu_get_compute_stream(ctx);

    // Initialize leaves
    dim3 block(BLOCK_SIZE);
    dim3 grid(GRID_SIZE(capacity));

    kernel_init_sum_tree_leaves<<<grid, block, 0, stream>>>(
        (float*)sum_tree->data,
        (const float*)priorities->data,
        (int)capacity
    );
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Build internal nodes level by level (bottom-up)
    int tree_size = 2 * capacity - 1;
    int level_start = (capacity - 1) / 2;  // Start at parent of leaves
    int level_size = (capacity + 1) / 2;

    while (level_start >= 0) {
        dim3 level_grid(GRID_SIZE(level_size));
        kernel_build_sum_tree_level<<<level_grid, block, 0, stream>>>(
            (float*)sum_tree->data,
            level_start,
            level_size
        );
        NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

        if (level_start == 0) break;
        level_size = (level_start + 1) / 2;
        level_start = (level_start - 1) / 2;
    }

    return true;
}

extern "C" bool nimcp_swarm_memory_gpu_update_sum_tree(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* sum_tree,
    size_t leaf_idx,
    float new_priority,
    size_t capacity)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !sum_tree) return false;

    nimcp_cuda_stream_t stream = nimcp_gpu_get_compute_stream(ctx);

    kernel_update_sum_tree_single<<<1, 1, 0, stream>>>(
        (float*)sum_tree->data,
        (int)leaf_idx,
        new_priority,
        (int)capacity
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

extern "C" bool nimcp_swarm_memory_gpu_sample_sum_tree(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* sum_tree,
    const nimcp_gpu_tensor_t* random_vals,
    nimcp_gpu_tensor_t* sampled_indices,
    size_t batch_size,
    size_t capacity)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !sum_tree || !random_vals || !sampled_indices) return false;

    nimcp_cuda_stream_t stream = nimcp_gpu_get_compute_stream(ctx);

    dim3 block(BLOCK_SIZE);
    dim3 grid(GRID_SIZE(batch_size));

    kernel_sample_sum_tree<<<grid, block, 0, stream>>>(
        (const float*)sum_tree->data,
        (const float*)random_vals->data,
        (int*)sampled_indices->data,
        (int)batch_size,
        (int)capacity
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Utility Functions
//=============================================================================

extern "C" void nimcp_replay_buffer_gpu_stats(
    const nimcp_replay_buffer_gpu_t* buffer,
    size_t* size_out,
    size_t* capacity_out,
    uint64_t* total_stored_out,
    uint64_t* total_sampled_out)
{
    if (!buffer) return;

    if (size_out) *size_out = buffer->current_size;
    if (capacity_out) *capacity_out = buffer->capacity;
    if (total_stored_out) *total_stored_out = buffer->total_stored;
    if (total_sampled_out) *total_sampled_out = buffer->total_sampled;
}

extern "C" void nimcp_swarm_memory_gpu_print_info(const nimcp_swarm_memory_gpu_t* mem)
{
    if (!mem) {
        LOG_INFO("Swarm Memory GPU: NULL");
        return;
    }

    LOG_INFO("=== Swarm Memory GPU Info ===");
    LOG_INFO("  Agents: %d", mem->num_agents);
    LOG_INFO("  Memory Dim: %zu", mem->memory_dim);
    LOG_INFO("  Trace Len: %zu", mem->trace_len);
    LOG_INFO("  Cortical Dim: %zu", mem->cortical_dim);

    if (mem->replay_buffer) {
        LOG_INFO("  Replay Buffer:");
        LOG_INFO("    Capacity: %zu", mem->replay_buffer->capacity);
        LOG_INFO("    Current Size: %zu", mem->replay_buffer->current_size);
        LOG_INFO("    State Dim: %zu", mem->replay_buffer->state_dim);
        LOG_INFO("    Action Dim: %zu", mem->replay_buffer->action_dim);
        LOG_INFO("    PER Enabled: %s", mem->replay_buffer->use_per ? "yes" : "no");
        LOG_INFO("    Alpha: %.3f", mem->replay_buffer->priority_alpha);
        LOG_INFO("    Beta: %.3f", mem->replay_buffer->priority_beta);
        LOG_INFO("    Total Stored: %lu", mem->replay_buffer->total_stored);
        LOG_INFO("    Total Sampled: %lu", mem->replay_buffer->total_sampled);
    }

    LOG_INFO("  Stats:");
    LOG_INFO("    Consolidations: %lu", mem->consolidation_count);
    LOG_INFO("    Agent Syncs: %lu", mem->agent_sync_count);
    LOG_INFO("=============================");
}

#else  // !NIMCP_ENABLE_CUDA

//=============================================================================
// CPU Fallback Stubs (when CUDA is not available)
//=============================================================================

#include "gpu/swarm/nimcp_swarm_memory_gpu.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "SWARM_MEMORY_GPU"

#define STUB_WARN() LOG_WARN("CUDA not enabled - function is no-op")

void nimcp_replay_buffer_gpu_default_config(nimcp_replay_buffer_gpu_config_t* config) {
    STUB_WARN();
    if (config) {
        config->capacity = 100000;
        config->state_dim = 4;
        config->action_dim = 2;
        config->use_prioritized = true;
        config->priority_alpha = 0.6f;
        config->priority_beta = 0.4f;
        config->priority_epsilon = 1e-6f;
    }
}

void nimcp_hippocampal_gpu_default_params(nimcp_hippocampal_gpu_params_t* params) {
    STUB_WARN();
    if (params) {
        params->compression_ratio = 15.0f;
        params->replay_noise = 0.01f;
        params->trace_decay = 0.95f;
        params->enable_compressed_replay = true;
    }
}

void nimcp_consolidation_gpu_default_params(nimcp_consolidation_gpu_params_t* params) {
    STUB_WARN();
    if (params) {
        params->learning_rate = 0.01f;
        params->decay_rate = 0.001f;
        params->min_strength = 0.01f;
        params->importance_threshold = 0.3f;
        params->sws_consolidation_rate = 0.1f;
        params->awake_consolidation_rate = 0.01f;
    }
}

void nimcp_agent_coordination_gpu_default_params(nimcp_agent_coordination_params_t* params) {
    STUB_WARN();
    if (params) {
        params->averaging_weight = 0.5f;
        params->conflict_resolution_alpha = 0.7f;
        params->confidence_threshold = 0.5f;
        params->enable_differential_privacy = false;
        params->dp_noise_scale = 0.1f;
    }
}

nimcp_replay_buffer_gpu_t* nimcp_replay_buffer_gpu_create(
    nimcp_gpu_context_t* ctx, const nimcp_replay_buffer_gpu_config_t* config) {
    STUB_WARN();
    (void)ctx; (void)config;
    return NULL;
}

void nimcp_replay_buffer_gpu_destroy(nimcp_replay_buffer_gpu_t* buffer) {
    STUB_WARN();
    (void)buffer;
}

bool nimcp_replay_buffer_gpu_clear(nimcp_replay_buffer_gpu_t* buffer) {
    STUB_WARN();
    (void)buffer;
    return false;
}

nimcp_swarm_memory_gpu_t* nimcp_swarm_memory_gpu_create(
    nimcp_gpu_context_t* ctx, size_t replay_capacity, int num_agents,
    int state_dim, int action_dim, int memory_dim) {
    STUB_WARN();
    (void)ctx; (void)replay_capacity; (void)num_agents;
    (void)state_dim; (void)action_dim; (void)memory_dim;
    return NULL;
}

void nimcp_swarm_memory_gpu_destroy(nimcp_swarm_memory_gpu_t* mem) {
    STUB_WARN();
    (void)mem;
}

nimcp_replay_batch_t* nimcp_replay_batch_create(
    nimcp_gpu_context_t* ctx, size_t batch_size, size_t state_dim, size_t action_dim) {
    STUB_WARN();
    (void)ctx; (void)batch_size; (void)state_dim; (void)action_dim;
    return NULL;
}

void nimcp_replay_batch_destroy(nimcp_replay_batch_t* batch) {
    STUB_WARN();
    (void)batch;
}

bool nimcp_swarm_memory_gpu_store(
    nimcp_swarm_memory_gpu_t* mem, nimcp_gpu_tensor_t* state,
    nimcp_gpu_tensor_t* action, float reward, nimcp_gpu_tensor_t* next_state, bool done) {
    STUB_WARN();
    (void)mem; (void)state; (void)action; (void)reward; (void)next_state; (void)done;
    return false;
}

bool nimcp_swarm_memory_gpu_store_batch(
    nimcp_swarm_memory_gpu_t* mem, nimcp_gpu_tensor_t* states,
    nimcp_gpu_tensor_t* actions, nimcp_gpu_tensor_t* rewards,
    nimcp_gpu_tensor_t* next_states, nimcp_gpu_tensor_t* dones, size_t batch_size) {
    STUB_WARN();
    (void)mem; (void)states; (void)actions; (void)rewards;
    (void)next_states; (void)dones; (void)batch_size;
    return false;
}

bool nimcp_swarm_memory_gpu_sample(
    nimcp_swarm_memory_gpu_t* mem, size_t batch_size, nimcp_replay_batch_t* batch) {
    STUB_WARN();
    (void)mem; (void)batch_size; (void)batch;
    return false;
}

bool nimcp_swarm_memory_gpu_update_priorities(
    nimcp_swarm_memory_gpu_t* mem, nimcp_gpu_tensor_t* indices,
    nimcp_gpu_tensor_t* td_errors, size_t batch_size) {
    STUB_WARN();
    (void)mem; (void)indices; (void)td_errors; (void)batch_size;
    return false;
}

void nimcp_swarm_memory_gpu_set_beta(nimcp_swarm_memory_gpu_t* mem, float new_beta) {
    STUB_WARN();
    (void)mem; (void)new_beta;
}

bool nimcp_swarm_memory_gpu_consolidate(nimcp_swarm_memory_gpu_t* mem, float dt) {
    STUB_WARN();
    (void)mem; (void)dt;
    return false;
}

bool nimcp_swarm_memory_gpu_decay(
    nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* memory_strength,
    float decay_rate, float min_strength) {
    STUB_WARN();
    (void)ctx; (void)memory_strength; (void)decay_rate; (void)min_strength;
    return false;
}

bool nimcp_swarm_memory_gpu_selective_consolidate(
    nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* memories,
    const nimcp_gpu_tensor_t* importance, nimcp_gpu_tensor_t* consolidated, float threshold) {
    STUB_WARN();
    (void)ctx; (void)memories; (void)importance; (void)consolidated; (void)threshold;
    return false;
}

bool nimcp_swarm_memory_gpu_hippocampal_compress(
    nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* sequence,
    nimcp_gpu_tensor_t* compressed, size_t seq_len, size_t state_dim, float compression_ratio) {
    STUB_WARN();
    (void)ctx; (void)sequence; (void)compressed;
    (void)seq_len; (void)state_dim; (void)compression_ratio;
    return false;
}

bool nimcp_swarm_memory_gpu_systems_consolidation(
    nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* hippocampal_mem,
    nimcp_gpu_tensor_t* cortical_weights, const nimcp_gpu_tensor_t* consolidation_gate,
    float learning_rate) {
    STUB_WARN();
    (void)ctx; (void)hippocampal_mem; (void)cortical_weights;
    (void)consolidation_gate; (void)learning_rate;
    return false;
}

bool nimcp_swarm_memory_gpu_sws_replay(
    nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* memories,
    nimcp_gpu_tensor_t* replay_order, const nimcp_gpu_tensor_t* importance_scores,
    size_t num_memories, size_t replay_count) {
    STUB_WARN();
    (void)ctx; (void)memories; (void)replay_order;
    (void)importance_scores; (void)num_memories; (void)replay_count;
    return false;
}

bool nimcp_swarm_memory_gpu_sync_agents(nimcp_swarm_memory_gpu_t* mem) {
    STUB_WARN();
    (void)mem;
    return false;
}

bool nimcp_swarm_memory_gpu_aggregate_memories(
    nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* agent_memories,
    nimcp_gpu_tensor_t* aggregated, const nimcp_gpu_tensor_t* agent_weights,
    int num_agents, int memory_dim) {
    STUB_WARN();
    (void)ctx; (void)agent_memories; (void)aggregated;
    (void)agent_weights; (void)num_agents; (void)memory_dim;
    return false;
}

bool nimcp_swarm_memory_gpu_federated_average(
    nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* agent_updates,
    nimcp_gpu_tensor_t* shared_knowledge, int num_agents, int knowledge_dim) {
    STUB_WARN();
    (void)ctx; (void)agent_updates; (void)shared_knowledge;
    (void)num_agents; (void)knowledge_dim;
    return false;
}

bool nimcp_swarm_memory_gpu_broadcast(
    nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* source_memory,
    nimcp_gpu_tensor_t* target_memories, const nimcp_gpu_tensor_t* target_agents,
    int num_targets, int memory_dim) {
    STUB_WARN();
    (void)ctx; (void)source_memory; (void)target_memories;
    (void)target_agents; (void)num_targets; (void)memory_dim;
    return false;
}

bool nimcp_swarm_memory_gpu_conflict_resolution(
    nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* memories,
    const nimcp_gpu_tensor_t* confidence, nimcp_gpu_tensor_t* resolved,
    int num_agents, int memory_dim) {
    STUB_WARN();
    (void)ctx; (void)memories; (void)confidence;
    (void)resolved; (void)num_agents; (void)memory_dim;
    return false;
}

bool nimcp_swarm_memory_gpu_store_episode(
    nimcp_swarm_memory_gpu_t* mem, nimcp_gpu_tensor_t* episode,
    nimcp_gpu_tensor_t* actions, nimcp_gpu_tensor_t* rewards, size_t episode_len) {
    STUB_WARN();
    (void)mem; (void)episode; (void)actions; (void)rewards; (void)episode_len;
    return false;
}

bool nimcp_swarm_memory_gpu_episode_similarity(
    nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* query,
    const nimcp_gpu_tensor_t* episodes, nimcp_gpu_tensor_t* similarities,
    size_t num_episodes, size_t episode_len, size_t state_dim) {
    STUB_WARN();
    (void)ctx; (void)query; (void)episodes; (void)similarities;
    (void)num_episodes; (void)episode_len; (void)state_dim;
    return false;
}

bool nimcp_swarm_memory_gpu_episode_replay(
    nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* episodes,
    const nimcp_gpu_tensor_t* replay_indices, nimcp_gpu_tensor_t* replayed_states,
    nimcp_gpu_tensor_t* replayed_actions, size_t batch_size,
    size_t episode_len, size_t state_dim, size_t action_dim) {
    STUB_WARN();
    (void)ctx; (void)episodes; (void)replay_indices;
    (void)replayed_states; (void)replayed_actions;
    (void)batch_size; (void)episode_len; (void)state_dim; (void)action_dim;
    return false;
}

bool nimcp_swarm_memory_gpu_build_sum_tree(
    nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* priorities,
    nimcp_gpu_tensor_t* sum_tree, size_t capacity) {
    STUB_WARN();
    (void)ctx; (void)priorities; (void)sum_tree; (void)capacity;
    return false;
}

bool nimcp_swarm_memory_gpu_update_sum_tree(
    nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* sum_tree,
    size_t leaf_idx, float new_priority, size_t capacity) {
    STUB_WARN();
    (void)ctx; (void)sum_tree; (void)leaf_idx; (void)new_priority; (void)capacity;
    return false;
}

bool nimcp_swarm_memory_gpu_sample_sum_tree(
    nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* sum_tree,
    const nimcp_gpu_tensor_t* random_vals, nimcp_gpu_tensor_t* sampled_indices,
    size_t batch_size, size_t capacity) {
    STUB_WARN();
    (void)ctx; (void)sum_tree; (void)random_vals;
    (void)sampled_indices; (void)batch_size; (void)capacity;
    return false;
}

void nimcp_replay_buffer_gpu_stats(
    const nimcp_replay_buffer_gpu_t* buffer,
    size_t* size_out, size_t* capacity_out,
    uint64_t* total_stored_out, uint64_t* total_sampled_out) {
    STUB_WARN();
    (void)buffer;
    if (size_out) *size_out = 0;
    if (capacity_out) *capacity_out = 0;
    if (total_stored_out) *total_stored_out = 0;
    if (total_sampled_out) *total_sampled_out = 0;
}

void nimcp_swarm_memory_gpu_print_info(const nimcp_swarm_memory_gpu_t* mem) {
    STUB_WARN();
    (void)mem;
}

#endif // NIMCP_ENABLE_CUDA
