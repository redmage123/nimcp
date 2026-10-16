/**
 * @file nimcp_qmc_gpu.cu
 * @brief GPU-Accelerated Quantum Monte Carlo CUDA Implementation
 *
 * WHAT: CUDA kernels for Monte Carlo sampling and MCTS
 * WHY:  Massive parallelism for stochastic algorithms
 * HOW:  cuRAND, parallel reductions, atomic operations
 *
 * RNG ARCHITECTURE:
 *   This module maintains its own cuRAND state for Monte Carlo simulations.
 *   The RNG pattern follows the central GPU statistics module:
 *   - See: gpu/statistics/nimcp_statistics_gpu.h for stats_gpu_rng_create/destroy
 *   - See: gpu/common/nimcp_device_utils.cuh for shared device RNG functions
 *
 *   Module-local RNG state is used here because:
 *   1. Monte Carlo requires independent, reproducible sequences per simulation
 *   2. Thread-local curandState arrays are performance-critical
 *   3. Seed management is tied to simulation reproducibility
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

#include "utils/memory/nimcp_memory.h"
#include <cuda_runtime.h>
#include <curand_kernel.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/quantum/nimcp_qmc_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"

#define LOG_MODULE "QMC_GPU"

#define QMC_BLOCK_SIZE 256
#define QMC_GRID_SIZE(n) (((n) + QMC_BLOCK_SIZE - 1) / QMC_BLOCK_SIZE)

//=============================================================================
// Internal Structures
//=============================================================================

struct qmc_gpu_rng_struct {
    curandState* states;
    uint32_t num_generators;
    uint64_t seed;
};

struct qmc_gpu_mcts_struct {
    qmcts_gpu_config_t config;
    uint32_t max_nodes;
    uint32_t num_nodes;

    /* Node data on GPU */
    uint32_t* visit_counts;
    float* total_values;
    float* ucb1_scores;
    uint32_t* parent_indices;
    uint32_t* num_children;
    uint32_t* first_child_indices;
    uint32_t* actions;  /* Action that led to this node */
};

struct qmc_gpu_sat_struct {
    qmc_sat_gpu_config_t config;

    /* CNF on GPU */
    int32_t* clauses;           /* Flat array of literals */
    uint32_t* clause_offsets;   /* Start offset of each clause */
    uint32_t* clause_sizes;     /* Size of each clause */
    uint32_t total_literals;

    /* Working memory */
    bool* assignments;          /* Current assignments */
    uint32_t* clause_sat;       /* Clause satisfaction flags */
};

//=============================================================================
// Configuration Defaults
//=============================================================================

qmc_gpu_config_t qmc_gpu_default_config(void)
{
    qmc_gpu_config_t config;
    config.num_samples = 10000;
    config.threads_per_block = QMC_BLOCK_SIZE;
    config.max_blocks = 0;  /* Auto */
    config.use_stratified = false;
    config.use_sobol = false;
    config.seed = 0;  /* Use time */
    return config;
}

qmcts_gpu_config_t qmcts_gpu_default_config(void)
{
    qmcts_gpu_config_t config;
    config.num_iterations = 1000;
    config.num_rollouts = 100;
    config.max_depth = 50;
    config.exploration_constant = 1.414f;  /* sqrt(2) */
    config.batch_size = 256;
    config.virtual_loss = true;
    config.root_parallelization = false;
    return config;
}

qmc_sat_gpu_config_t qmc_sat_gpu_default_config(uint32_t num_vars, uint32_t num_clauses)
{
    qmc_sat_gpu_config_t config;
    config.num_variables = num_vars;
    config.num_clauses = num_clauses;
    config.max_literals = 10;
    config.mcts_iterations = 5000;
    config.random_samples = 10000;
    config.timeout_ms = 0.0f;
    return config;
}

//=============================================================================
// RNG Kernels
//=============================================================================

static __global__ void kernel_init_curand(curandState* states, uint64_t seed, uint32_t n)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    curand_init(seed, idx, 0, &states[idx]);
}

static __global__ void kernel_generate_uniform(
    curandState* states,
    float* output,
    uint32_t n)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    output[idx] = curand_uniform(&states[idx % blockDim.x]);
}

__global__ void kernel_generate_normal(
    curandState* states,
    float* output,
    float mean,
    float stddev,
    uint32_t n)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    output[idx] = curand_normal(&states[idx % blockDim.x]) * stddev + mean;
}

__global__ void kernel_generate_stratified(
    curandState* states,
    float* output,
    uint32_t num_strata,
    uint32_t n)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    uint32_t stratum = idx % num_strata;
    float stratum_size = 1.0f / (float)num_strata;
    float base = stratum * stratum_size;

    output[idx] = base + curand_uniform(&states[idx % blockDim.x]) * stratum_size;
}

//=============================================================================
// RNG API Implementation
//=============================================================================

qmc_gpu_rng_t qmc_gpu_rng_create(
    nimcp_gpu_context_t* ctx,
    uint32_t num_generators,
    uint64_t seed)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    (void)ctx;

    if (num_generators == 0) {
        num_generators = QMC_BLOCK_SIZE;
    }

    qmc_gpu_rng_t rng = (qmc_gpu_rng_t)nimcp_calloc(1, sizeof(struct qmc_gpu_rng_struct));
    if (!rng) return NULL;

    rng->num_generators = num_generators;
    rng->seed = (seed == 0) ? (uint64_t)time(NULL) : seed;

    cudaError_t err = cudaMalloc(&rng->states, num_generators * sizeof(curandState));
    if (err != cudaSuccess) {
        nimcp_free(rng);
        return NULL;
    }

    kernel_init_curand<<<QMC_GRID_SIZE(num_generators), QMC_BLOCK_SIZE>>>(
        rng->states, rng->seed, num_generators);

    err = cudaGetLastError();
    if (err != cudaSuccess) {
        cudaFree(rng->states);
        nimcp_free(rng);
        return NULL;
    }

    LOG_DEBUG("Created GPU RNG with %u generators, seed=%lu", num_generators, rng->seed);
    return rng;
}

void qmc_gpu_rng_destroy(qmc_gpu_rng_t rng)
{
    if (!rng) return;

    cudaFree(rng->states);
    nimcp_free(rng);
}

bool qmc_gpu_rng_reseed(qmc_gpu_rng_t rng, uint64_t seed)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!rng) return false;

    rng->seed = (seed == 0) ? (uint64_t)time(NULL) : seed;

    kernel_init_curand<<<QMC_GRID_SIZE(rng->num_generators), QMC_BLOCK_SIZE>>>(
        rng->states, rng->seed, rng->num_generators);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Monte Carlo Sampling Implementation
//=============================================================================

bool qmc_gpu_sample_uniform(
    nimcp_gpu_context_t* ctx,
    qmc_gpu_rng_t rng,
    nimcp_gpu_tensor_t* output)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    (void)ctx;
    if (!rng || !output || !output->data) return false;

    uint32_t n = output->numel;

    kernel_generate_uniform<<<QMC_GRID_SIZE(n), QMC_BLOCK_SIZE>>>(
        rng->states, (float*)output->data, n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool qmc_gpu_sample_normal(
    nimcp_gpu_context_t* ctx,
    qmc_gpu_rng_t rng,
    nimcp_gpu_tensor_t* output,
    float mean,
    float stddev)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    (void)ctx;
    if (!rng || !output || !output->data) return false;

    uint32_t n = output->numel;

    kernel_generate_normal<<<QMC_GRID_SIZE(n), QMC_BLOCK_SIZE>>>(
        rng->states, (float*)output->data, mean, stddev, n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool qmc_gpu_sample_stratified(
    nimcp_gpu_context_t* ctx,
    qmc_gpu_rng_t rng,
    nimcp_gpu_tensor_t* output,
    uint32_t num_strata)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    (void)ctx;
    if (!rng || !output || !output->data || num_strata == 0) return false;

    uint32_t n = output->numel;

    kernel_generate_stratified<<<QMC_GRID_SIZE(n), QMC_BLOCK_SIZE>>>(
        rng->states, (float*)output->data, num_strata, n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Categorical Sampling Kernel
//=============================================================================

__global__ void kernel_cumsum(const float* input, float* output, uint32_t n)
{
    extern __shared__ float sdata[];

    uint32_t tid = threadIdx.x;
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    sdata[tid] = (idx < n) ? input[idx] : 0.0f;
    __syncthreads();

    /* Inclusive scan */
    for (uint32_t stride = 1; stride < blockDim.x; stride *= 2) {
        float temp = 0.0f;
        if (tid >= stride) {
            temp = sdata[tid - stride];
        }
        __syncthreads();
        if (tid >= stride) {
            sdata[tid] += temp;
        }
        __syncthreads();
    }

    if (idx < n) {
        output[idx] = sdata[tid];
    }
}

__global__ void kernel_sample_categorical(
    curandState* states,
    const float* cumsum_probs,
    float total_prob,
    uint32_t* output,
    uint32_t num_categories,
    uint32_t num_samples)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_samples) return;

    float r = curand_uniform(&states[idx % blockDim.x]) * total_prob;

    /* Binary search for category */
    uint32_t lo = 0, hi = num_categories;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        if (cumsum_probs[mid] < r) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }

    output[idx] = lo;
}

bool qmc_gpu_sample_categorical(
    nimcp_gpu_context_t* ctx,
    qmc_gpu_rng_t rng,
    const nimcp_gpu_tensor_t* probabilities,
    uint32_t num_categories,
    nimcp_gpu_tensor_t* output,
    uint32_t num_samples)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    (void)ctx;
    if (!rng || !probabilities || !output) return false;

    /* Compute cumulative sum */
    float* d_cumsum;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_cumsum, num_categories * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);

    kernel_cumsum<<<QMC_GRID_SIZE(num_categories), QMC_BLOCK_SIZE,
                    QMC_BLOCK_SIZE * sizeof(float)>>>(
        (const float*)probabilities->data, d_cumsum, num_categories);

    /* Get total probability */
    float total_prob;
    NIMCP_CUDA_RECOVER(cudaMemcpy(&total_prob, &d_cumsum[num_categories - 1], sizeof(float),
               cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);

    /* Sample */
    kernel_sample_categorical<<<QMC_GRID_SIZE(num_samples), QMC_BLOCK_SIZE>>>(
        rng->states, d_cumsum, total_prob, (uint32_t*)output->data,
        num_categories, num_samples);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    cudaFree(d_cumsum);
    return true;
}

//=============================================================================
// Monte Carlo Integration Kernels
//=============================================================================

static __global__ void kernel_reduce_sum(const float* input, float* output, uint32_t n)
{
    extern __shared__ float sdata[];

    uint32_t tid = threadIdx.x;
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    sdata[tid] = (idx < n) ? input[idx] : 0.0f;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            sdata[tid] += sdata[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        atomicAdd(output, sdata[0]);
    }
}

static __global__ void kernel_reduce_sum_sq(const float* input, float mean,
                                      float* output, uint32_t n)
{
    extern __shared__ float sdata[];

    uint32_t tid = threadIdx.x;
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    float val = (idx < n) ? (input[idx] - mean) : 0.0f;
    sdata[tid] = val * val;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            sdata[tid] += sdata[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        atomicAdd(output, sdata[0]);
    }
}

bool qmc_gpu_integrate(
    nimcp_gpu_context_t* ctx,
    qmc_gpu_rng_t rng,
    const nimcp_gpu_tensor_t* values,
    uint32_t num_samples,
    float domain_volume,
    qmc_gpu_integration_result_t* result)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    (void)ctx;
    (void)rng;
    if (!values || !result || num_samples == 0) return false;

    cudaEvent_t start, stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);
    cudaEventRecord(start);

    /* Compute sum */
    float* d_sum;
    cudaMalloc(&d_sum, sizeof(float));
    cudaMemset(d_sum, 0, sizeof(float));

    kernel_reduce_sum<<<QMC_GRID_SIZE(num_samples), QMC_BLOCK_SIZE,
                        QMC_BLOCK_SIZE * sizeof(float)>>>(
        (const float*)values->data, d_sum, num_samples);

    float h_sum;
    cudaMemcpy(&h_sum, d_sum, sizeof(float), cudaMemcpyDeviceToHost);

    float mean = h_sum / (float)num_samples;
    result->estimate = mean * domain_volume;

    /* Compute variance */
    cudaMemset(d_sum, 0, sizeof(float));

    kernel_reduce_sum_sq<<<QMC_GRID_SIZE(num_samples), QMC_BLOCK_SIZE,
                           QMC_BLOCK_SIZE * sizeof(float)>>>(
        (const float*)values->data, mean, d_sum, num_samples);

    float h_var;
    cudaMemcpy(&h_var, d_sum, sizeof(float), cudaMemcpyDeviceToHost);

    result->variance = h_var / (float)(num_samples - 1);
    result->std_error = sqrtf(result->variance / num_samples) * domain_volume;
    result->num_samples = num_samples;

    cudaEventRecord(stop);
    cudaEventSynchronize(stop);
    cudaEventElapsedTime(&result->time_ms, start, stop);

    cudaFree(d_sum);
    cudaEventDestroy(start);
    cudaEventDestroy(stop);

    return true;
}

//=============================================================================
// MCTS Kernels
//=============================================================================

__global__ void kernel_compute_ucb1(
    const uint32_t* visit_counts,
    const float* total_values,
    const uint32_t* parent_indices,
    float* ucb1_scores,
    float exploration_constant,
    uint32_t parent_visits,
    uint32_t num_nodes)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_nodes) return;

    uint32_t visits = visit_counts[idx];
    if (visits == 0) {
        ucb1_scores[idx] = FLT_MAX;  /* Unvisited nodes have infinite UCB1 */
        return;
    }

    float mean_value = total_values[idx] / (float)visits;
    float exploration = exploration_constant * sqrtf(logf((float)parent_visits + 1.0f) / (float)visits);

    ucb1_scores[idx] = mean_value + exploration;
}

__global__ void kernel_backpropagate(
    uint32_t* visit_counts,
    float* total_values,
    const uint32_t* parent_indices,
    const uint32_t* leaf_indices,
    const float* values,
    uint32_t num_leaves,
    uint32_t max_depth)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_leaves) return;

    uint32_t node = leaf_indices[idx];
    float value = values[idx];

    /* Traverse to root */
    for (uint32_t d = 0; d < max_depth && node != UINT32_MAX; d++) {
        atomicAdd(&visit_counts[node], 1);
        atomicAdd(&total_values[node], value);
        node = parent_indices[node];
    }
}

//=============================================================================
// MCTS API Implementation
//=============================================================================

qmc_gpu_mcts_t qmcts_gpu_create(
    nimcp_gpu_context_t* ctx,
    const qmcts_gpu_config_t* config,
    uint32_t max_nodes)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    (void)ctx;
    if (!config || max_nodes == 0) return NULL;

    qmc_gpu_mcts_t mcts = (qmc_gpu_mcts_t)nimcp_calloc(1, sizeof(struct qmc_gpu_mcts_struct));
    if (!mcts) return NULL;

    mcts->config = *config;
    mcts->max_nodes = max_nodes;
    mcts->num_nodes = 1;  /* Root node */

    /* Allocate GPU memory */
    cudaMalloc(&mcts->visit_counts, max_nodes * sizeof(uint32_t));
    cudaMalloc(&mcts->total_values, max_nodes * sizeof(float));
    cudaMalloc(&mcts->ucb1_scores, max_nodes * sizeof(float));
    cudaMalloc(&mcts->parent_indices, max_nodes * sizeof(uint32_t));
    cudaMalloc(&mcts->num_children, max_nodes * sizeof(uint32_t));
    cudaMalloc(&mcts->first_child_indices, max_nodes * sizeof(uint32_t));
    cudaMalloc(&mcts->actions, max_nodes * sizeof(uint32_t));

    if (!mcts->visit_counts || !mcts->total_values || !mcts->ucb1_scores) {
        qmcts_gpu_destroy(mcts);
        return NULL;
    }

    qmcts_gpu_reset(mcts);

    LOG_DEBUG("Created GPU MCTS with max %u nodes", max_nodes);
    return mcts;
}

void qmcts_gpu_destroy(qmc_gpu_mcts_t mcts)
{
    if (!mcts) return;

    cudaFree(mcts->visit_counts);
    cudaFree(mcts->total_values);
    cudaFree(mcts->ucb1_scores);
    cudaFree(mcts->parent_indices);
    cudaFree(mcts->num_children);
    cudaFree(mcts->first_child_indices);
    cudaFree(mcts->actions);

    nimcp_free(mcts);
}

bool qmcts_gpu_reset(qmc_gpu_mcts_t mcts)
{
    if (!mcts) return false;

    cudaMemset(mcts->visit_counts, 0, mcts->max_nodes * sizeof(uint32_t));
    cudaMemset(mcts->total_values, 0, mcts->max_nodes * sizeof(float));
    cudaMemset(mcts->ucb1_scores, 0, mcts->max_nodes * sizeof(float));
    cudaMemset(mcts->parent_indices, 0xFF, mcts->max_nodes * sizeof(uint32_t));  /* UINT32_MAX */
    cudaMemset(mcts->num_children, 0, mcts->max_nodes * sizeof(uint32_t));
    cudaMemset(mcts->first_child_indices, 0, mcts->max_nodes * sizeof(uint32_t));
    cudaMemset(mcts->actions, 0, mcts->max_nodes * sizeof(uint32_t));

    mcts->num_nodes = 1;

    return true;
}

bool qmcts_gpu_compute_ucb1(
    nimcp_gpu_context_t* ctx,
    qmc_gpu_mcts_t mcts,
    float exploration_constant)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    (void)ctx;
    if (!mcts) return false;

    /* Get root visits for UCB1 calculation */
    uint32_t root_visits;
    NIMCP_CUDA_RECOVER(cudaMemcpy(&root_visits, &mcts->visit_counts[0], sizeof(uint32_t), cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);

    kernel_compute_ucb1<<<QMC_GRID_SIZE(mcts->num_nodes), QMC_BLOCK_SIZE>>>(
        mcts->visit_counts,
        mcts->total_values,
        mcts->parent_indices,
        mcts->ucb1_scores,
        exploration_constant,
        root_visits,
        mcts->num_nodes);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool qmcts_gpu_backpropagate(
    nimcp_gpu_context_t* ctx,
    qmc_gpu_mcts_t mcts,
    const uint32_t* leaf_indices,
    const float* values,
    uint32_t num_leaves)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    (void)ctx;
    if (!mcts || !leaf_indices || !values || num_leaves == 0) return false;

    /* Copy to GPU */
    uint32_t* d_leaves;
    float* d_values;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_leaves, num_leaves * sizeof(uint32_t)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_values, num_leaves * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMemcpy(d_leaves, leaf_indices, num_leaves * sizeof(uint32_t), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpy(d_values, values, num_leaves * sizeof(float), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);

    kernel_backpropagate<<<QMC_GRID_SIZE(num_leaves), QMC_BLOCK_SIZE>>>(
        mcts->visit_counts,
        mcts->total_values,
        mcts->parent_indices,
        d_leaves,
        d_values,
        num_leaves,
        mcts->config.max_depth);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    cudaFree(d_leaves);
    cudaFree(d_values);

    return true;
}

//=============================================================================
// SAT Solver Kernels
//=============================================================================

__global__ void kernel_evaluate_assignment(
    const int32_t* clauses,
    const uint32_t* clause_offsets,
    const uint32_t* clause_sizes,
    const bool* assignment,
    uint32_t* clause_sat,
    uint32_t num_clauses)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_clauses) return;

    uint32_t offset = clause_offsets[idx];
    uint32_t size = clause_sizes[idx];

    bool satisfied = false;
    for (uint32_t i = 0; i < size && !satisfied; i++) {
        int32_t lit = clauses[offset + i];
        uint32_t var = (lit > 0) ? (lit - 1) : (-lit - 1);
        bool val = assignment[var];

        if ((lit > 0 && val) || (lit < 0 && !val)) {
            satisfied = true;
        }
    }

    clause_sat[idx] = satisfied ? 1 : 0;
}

__global__ void kernel_evaluate_batch(
    const int32_t* clauses,
    const uint32_t* clause_offsets,
    const uint32_t* clause_sizes,
    const bool* assignments,
    uint32_t* scores,
    uint32_t num_clauses,
    uint32_t num_variables,
    uint32_t num_assignments)
{
    uint32_t assign_idx = blockIdx.x;
    uint32_t clause_idx = threadIdx.x;

    if (assign_idx >= num_assignments) return;

    extern __shared__ uint32_t shared_count[];
    shared_count[threadIdx.x] = 0;

    const bool* assignment = &assignments[assign_idx * num_variables];

    /* Each thread checks one clause at a time */
    for (uint32_t c = clause_idx; c < num_clauses; c += blockDim.x) {
        uint32_t offset = clause_offsets[c];
        uint32_t size = clause_sizes[c];

        bool satisfied = false;
        for (uint32_t i = 0; i < size && !satisfied; i++) {
            int32_t lit = clauses[offset + i];
            uint32_t var = (lit > 0) ? (lit - 1) : (-lit - 1);
            bool val = assignment[var];

            if ((lit > 0 && val) || (lit < 0 && !val)) {
                satisfied = true;
            }
        }

        if (satisfied) {
            shared_count[threadIdx.x]++;
        }
    }

    __syncthreads();

    /* Reduce within block */
    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (clause_idx < s) {
            shared_count[clause_idx] += shared_count[clause_idx + s];
        }
        __syncthreads();
    }

    if (clause_idx == 0) {
        scores[assign_idx] = shared_count[0];
    }
}

__global__ void kernel_generate_random_assignments(
    curandState* states,
    bool* assignments,
    uint32_t num_variables,
    uint32_t num_assignments)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_assignments * num_variables) return;

    assignments[idx] = curand_uniform(&states[idx % blockDim.x]) > 0.5f;
}

//=============================================================================
// SAT Solver API Implementation
//=============================================================================

qmc_gpu_sat_t qmc_sat_gpu_create(
    nimcp_gpu_context_t* ctx,
    const qmc_sat_gpu_config_t* config)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    (void)ctx;
    if (!config) return NULL;

    qmc_gpu_sat_t sat = (qmc_gpu_sat_t)nimcp_calloc(1, sizeof(struct qmc_gpu_sat_struct));
    if (!sat) return NULL;

    sat->config = *config;

    /* Allocate maximum possible size */
    uint32_t max_literals = config->num_clauses * config->max_literals;

    cudaMalloc(&sat->clauses, max_literals * sizeof(int32_t));
    cudaMalloc(&sat->clause_offsets, config->num_clauses * sizeof(uint32_t));
    cudaMalloc(&sat->clause_sizes, config->num_clauses * sizeof(uint32_t));
    cudaMalloc(&sat->assignments, config->num_variables * sizeof(bool));
    cudaMalloc(&sat->clause_sat, config->num_clauses * sizeof(uint32_t));

    if (!sat->clauses || !sat->clause_offsets || !sat->assignments) {
        qmc_sat_gpu_destroy(sat);
        return NULL;
    }

    LOG_DEBUG("Created GPU SAT solver: %u vars, %u clauses",
              config->num_variables, config->num_clauses);
    return sat;
}

void qmc_sat_gpu_destroy(qmc_gpu_sat_t sat)
{
    if (!sat) return;

    cudaFree(sat->clauses);
    cudaFree(sat->clause_offsets);
    cudaFree(sat->clause_sizes);
    cudaFree(sat->assignments);
    cudaFree(sat->clause_sat);

    nimcp_free(sat);
}

bool qmc_sat_gpu_set_cnf(
    qmc_gpu_sat_t sat,
    const int32_t* clauses,
    const uint32_t* clause_sizes,
    uint32_t num_clauses)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!sat || !clauses || !clause_sizes) return false;

    /* Compute offsets */
    uint32_t* h_offsets = (uint32_t*)nimcp_malloc(num_clauses * sizeof(uint32_t));
    uint32_t offset = 0;
    for (uint32_t i = 0; i < num_clauses; i++) {
        h_offsets[i] = offset;
        offset += clause_sizes[i];
    }
    sat->total_literals = offset;

    /* Copy to GPU */
    NIMCP_CUDA_RECOVER(cudaMemcpy(sat->clauses, clauses, offset * sizeof(int32_t), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpy(sat->clause_offsets, h_offsets, num_clauses * sizeof(uint32_t), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpy(sat->clause_sizes, clause_sizes, num_clauses * sizeof(uint32_t), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);

    nimcp_free(h_offsets);
    return true;
}

bool qmc_sat_gpu_estimate_probability(
    nimcp_gpu_context_t* ctx,
    qmc_gpu_sat_t sat,
    qmc_gpu_rng_t rng,
    uint32_t num_samples,
    float* probability,
    float* variance)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    (void)ctx;
    if (!sat || !rng || !probability) return false;

    uint32_t num_vars = sat->config.num_variables;
    uint32_t num_clauses = sat->config.num_clauses;

    /* Allocate batch assignments and scores */
    bool* d_assignments;
    uint32_t* d_scores;
    cudaMalloc(&d_assignments, num_samples * num_vars * sizeof(bool));
    cudaMalloc(&d_scores, num_samples * sizeof(uint32_t));

    /* Generate random assignments */
    kernel_generate_random_assignments<<<QMC_GRID_SIZE(num_samples * num_vars), QMC_BLOCK_SIZE>>>(
        rng->states, d_assignments, num_vars, num_samples);

    /* Evaluate all assignments */
    dim3 grid(num_samples);
    dim3 block(min(num_clauses, (uint32_t)QMC_BLOCK_SIZE));
    size_t shared_size = block.x * sizeof(uint32_t);

    kernel_evaluate_batch<<<grid, block, shared_size>>>(
        sat->clauses, sat->clause_offsets, sat->clause_sizes,
        d_assignments, d_scores, num_clauses, num_vars, num_samples);

    /* Copy scores back and count SAT */
    uint32_t* h_scores = (uint32_t*)nimcp_malloc(num_samples * sizeof(uint32_t));
    cudaMemcpy(h_scores, d_scores, num_samples * sizeof(uint32_t), cudaMemcpyDeviceToHost);

    uint32_t sat_count = 0;
    for (uint32_t i = 0; i < num_samples; i++) {
        if (h_scores[i] == num_clauses) {
            sat_count++;
        }
    }

    *probability = (float)sat_count / (float)num_samples;

    if (variance) {
        *variance = (*probability) * (1.0f - *probability) / (float)num_samples;
    }

    cudaFree(d_assignments);
    cudaFree(d_scores);
    nimcp_free(h_scores);

    return true;
}

bool qmc_sat_gpu_solve_mcts(
    nimcp_gpu_context_t* ctx,
    qmc_gpu_sat_t sat,
    qmc_gpu_rng_t rng,
    qmc_sat_gpu_result_t* result)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!sat || !rng || !result) return false;

    cudaEvent_t start, stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);
    cudaEventRecord(start);

    uint32_t num_vars = sat->config.num_variables;
    uint32_t num_clauses = sat->config.num_clauses;
    uint32_t batch_size = 256;

    /* Best assignment found */
    bool* best_assignment = (bool*)nimcp_malloc(num_vars * sizeof(bool));
    uint32_t best_score = 0;

    /* MCTS-inspired search with GPU batch evaluation */
    for (uint32_t iter = 0; iter < sat->config.mcts_iterations / batch_size; iter++) {
        /* Generate batch of candidates */
        bool* d_assignments;
        uint32_t* d_scores;
        cudaMalloc(&d_assignments, batch_size * num_vars * sizeof(bool));
        cudaMalloc(&d_scores, batch_size * sizeof(uint32_t));

        kernel_generate_random_assignments<<<QMC_GRID_SIZE(batch_size * num_vars), QMC_BLOCK_SIZE>>>(
            rng->states, d_assignments, num_vars, batch_size);

        /* Evaluate batch */
        dim3 grid(batch_size);
        dim3 block(min(num_clauses, (uint32_t)QMC_BLOCK_SIZE));
        size_t shared_size = block.x * sizeof(uint32_t);

        kernel_evaluate_batch<<<grid, block, shared_size>>>(
            sat->clauses, sat->clause_offsets, sat->clause_sizes,
            d_assignments, d_scores, num_clauses, num_vars, batch_size);

        /* Find best in batch */
        uint32_t* h_scores = (uint32_t*)nimcp_malloc(batch_size * sizeof(uint32_t));
        cudaMemcpy(h_scores, d_scores, batch_size * sizeof(uint32_t), cudaMemcpyDeviceToHost);

        for (uint32_t i = 0; i < batch_size; i++) {
            if (h_scores[i] > best_score) {
                best_score = h_scores[i];

                /* Copy assignment */
                cudaMemcpy(best_assignment,
                          d_assignments + i * num_vars,
                          num_vars * sizeof(bool),
                          cudaMemcpyDeviceToHost);

                if (best_score == num_clauses) {
                    /* Found SAT! */
                    break;
                }
            }
        }

        cudaFree(d_assignments);
        cudaFree(d_scores);
        nimcp_free(h_scores);

        if (best_score == num_clauses) {
            break;
        }
    }

    cudaEventRecord(stop);
    cudaEventSynchronize(stop);

    /* Fill result */
    result->satisfiable = (best_score == num_clauses);
    result->assignment = best_assignment;
    result->num_variables = num_vars;
    result->clauses_satisfied = best_score;
    result->sat_probability = (float)best_score / (float)num_clauses;
    result->iterations_used = sat->config.mcts_iterations;

    cudaEventElapsedTime(&result->time_ms, start, stop);

    cudaEventDestroy(start);
    cudaEventDestroy(stop);

    return true;
}

void qmc_sat_gpu_result_free(qmc_sat_gpu_result_t* result)
{
    if (!result) return;
    nimcp_free(result->assignment);
    result->assignment = NULL;
}

//=============================================================================
// Utility Functions
//=============================================================================

bool qmc_gpu_is_available(void)
{
    int device_count;
    cudaError_t err = cudaGetDeviceCount(&device_count);
    return (err == cudaSuccess && device_count > 0);
}

const char* qmc_gpu_version(void)
{
    return "1.0.0-cuda";
}

void qmc_gpu_print_diagnostics(void)
{
    int device_count;
    cudaGetDeviceCount(&device_count);

    fprintf(stderr, "[QMC GPU] CUDA devices: %d\n", device_count);

    for (int i = 0; i < device_count; i++) {
        cudaDeviceProp prop;
        cudaGetDeviceProperties(&prop, i);
        fprintf(stderr, "[QMC GPU] Device %d: %s (SM %d.%d, %.2f GB)\n",
                i, prop.name, prop.major, prop.minor,
                prop.totalGlobalMem / (1024.0 * 1024.0 * 1024.0));
    }
}

#else // !NIMCP_ENABLE_CUDA

//=============================================================================
// CPU Fallback (when CUDA not available)
//=============================================================================

#include "gpu/quantum/nimcp_qmc_gpu.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

qmc_gpu_config_t qmc_gpu_default_config(void) {
    qmc_gpu_config_t config = {0};
    config.num_samples = 10000;
    config.threads_per_block = 256;
    return config;
}

qmcts_gpu_config_t qmcts_gpu_default_config(void) {
    qmcts_gpu_config_t config = {0};
    config.num_iterations = 1000;
    config.exploration_constant = 1.414f;
    return config;
}

qmc_sat_gpu_config_t qmc_sat_gpu_default_config(uint32_t num_vars, uint32_t num_clauses) {
    qmc_sat_gpu_config_t config = {0};
    config.num_variables = num_vars;
    config.num_clauses = num_clauses;
    return config;
}

qmc_gpu_rng_t qmc_gpu_rng_create(nimcp_gpu_context_t* ctx, uint32_t n, uint64_t seed) {
    (void)ctx; (void)n; (void)seed;
    return NULL;  /* GPU not available */
}

void qmc_gpu_rng_destroy(qmc_gpu_rng_t rng) { (void)rng; }
bool qmc_gpu_rng_reseed(qmc_gpu_rng_t rng, uint64_t seed) { (void)rng; (void)seed; return false; }

bool qmc_gpu_sample_uniform(nimcp_gpu_context_t* ctx, qmc_gpu_rng_t rng, nimcp_gpu_tensor_t* out) {
    (void)ctx; (void)rng; (void)out; return false;
}

bool qmc_gpu_sample_normal(nimcp_gpu_context_t* ctx, qmc_gpu_rng_t rng,
                           nimcp_gpu_tensor_t* out, float m, float s) {
    (void)ctx; (void)rng; (void)out; (void)m; (void)s; return false;
}

bool qmc_gpu_sample_categorical(nimcp_gpu_context_t* ctx, qmc_gpu_rng_t rng,
                                 const nimcp_gpu_tensor_t* p, uint32_t nc,
                                 nimcp_gpu_tensor_t* out, uint32_t ns) {
    (void)ctx; (void)rng; (void)p; (void)nc; (void)out; (void)ns; return false;
}

bool qmc_gpu_sample_stratified(nimcp_gpu_context_t* ctx, qmc_gpu_rng_t rng,
                                nimcp_gpu_tensor_t* out, uint32_t n) {
    (void)ctx; (void)rng; (void)out; (void)n; return false;
}

bool qmc_gpu_integrate(nimcp_gpu_context_t* ctx, qmc_gpu_rng_t rng,
                       const nimcp_gpu_tensor_t* v, uint32_t n, float dv,
                       qmc_gpu_integration_result_t* r) {
    (void)ctx; (void)rng; (void)v; (void)n; (void)dv; (void)r; return false;
}

bool qmc_gpu_integrate_importance(nimcp_gpu_context_t* ctx,
                                   const nimcp_gpu_tensor_t* v,
                                   const nimcp_gpu_tensor_t* w,
                                   uint32_t n, qmc_gpu_integration_result_t* r) {
    (void)ctx; (void)v; (void)w; (void)n; (void)r; return false;
}

qmc_gpu_mcts_t qmcts_gpu_create(nimcp_gpu_context_t* ctx, const qmcts_gpu_config_t* c, uint32_t n) {
    (void)ctx; (void)c; (void)n; return NULL;
}

void qmcts_gpu_destroy(qmc_gpu_mcts_t m) { (void)m; }
bool qmcts_gpu_reset(qmc_gpu_mcts_t m) { (void)m; return false; }

bool qmcts_gpu_compute_ucb1(nimcp_gpu_context_t* ctx, qmc_gpu_mcts_t m, float e) {
    (void)ctx; (void)m; (void)e; return false;
}

bool qmcts_gpu_batch_rollout(nimcp_gpu_context_t* ctx, qmc_gpu_mcts_t m, qmc_gpu_rng_t r,
                              const uint32_t* s, uint32_t n, nimcp_gpu_tensor_t* v) {
    (void)ctx; (void)m; (void)r; (void)s; (void)n; (void)v; return false;
}

bool qmcts_gpu_backpropagate(nimcp_gpu_context_t* ctx, qmc_gpu_mcts_t m,
                              const uint32_t* l, const float* v, uint32_t n) {
    (void)ctx; (void)m; (void)l; (void)v; (void)n; return false;
}

bool qmcts_gpu_search(nimcp_gpu_context_t* ctx, qmc_gpu_mcts_t m, qmc_gpu_rng_t r,
                       uint32_t n, uint32_t* a, float* v) {
    (void)ctx; (void)m; (void)r; (void)n; (void)a; (void)v; return false;
}

qmc_gpu_sat_t qmc_sat_gpu_create(nimcp_gpu_context_t* ctx, const qmc_sat_gpu_config_t* c) {
    (void)ctx; (void)c; return NULL;
}

void qmc_sat_gpu_destroy(qmc_gpu_sat_t s) { (void)s; }

bool qmc_sat_gpu_set_cnf(qmc_gpu_sat_t s, const int32_t* c, const uint32_t* z, uint32_t n) {
    (void)s; (void)c; (void)z; (void)n; return false;
}

bool qmc_sat_gpu_estimate_probability(nimcp_gpu_context_t* ctx, qmc_gpu_sat_t s,
                                       qmc_gpu_rng_t r, uint32_t n, float* p, float* v) {
    (void)ctx; (void)s; (void)r; (void)n; (void)p; (void)v; return false;
}

bool qmc_sat_gpu_solve_mcts(nimcp_gpu_context_t* ctx, qmc_gpu_sat_t s,
                             qmc_gpu_rng_t r, qmc_sat_gpu_result_t* res) {
    (void)ctx; (void)s; (void)r; (void)res; return false;
}

bool qmc_sat_gpu_evaluate_batch(nimcp_gpu_context_t* ctx, qmc_gpu_sat_t s,
                                 const nimcp_gpu_tensor_t* a, uint32_t n,
                                 nimcp_gpu_tensor_t* sc) {
    (void)ctx; (void)s; (void)a; (void)n; (void)sc; return false;
}

void qmc_sat_gpu_result_free(qmc_sat_gpu_result_t* r) { if (r) { nimcp_free(r->assignment); } }

bool qmc_gpu_is_available(void) { return false; }
const char* qmc_gpu_version(void) { return "1.0.0-cpu"; }
void qmc_gpu_print_diagnostics(void) { fprintf(stderr, "[QMC GPU] CUDA not available\n"); }

#endif // NIMCP_ENABLE_CUDA
