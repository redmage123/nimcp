/**
 * @file nimcp_omni_gpu.cu
 * @brief GPU Omnidirectional Inference CUDA Kernels
 * @version 1.0.0
 * @date 2025-01-04
 *
 * WHAT: CUDA kernels for omnidirectional inference operations
 * WHY:  GPU acceleration for bidirectional prediction, Hopfield retrieval,
 *       predictive coding, and temporal replay
 * HOW:  Custom CUDA kernels optimized for each inference direction
 *
 * HOT PATHS ACCELERATED:
 * 1. Bidirectional Prediction: Forward/backward latent prediction
 * 2. Hopfield Attention: Softmax attention over stored patterns
 * 3. Predictive Error: Parallel error computation across hierarchy levels
 * 4. Replay Sampling: GPU-accelerated priority-weighted sampling
 *
 * @author NIMCP Development Team
 */

#ifdef NIMCP_ENABLE_CUDA

#include <cuda_runtime.h>
#include <curand_kernel.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>

#include "gpu/cognitive/nimcp_omni_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/memory/nimcp_memory.h"

#define LOG_MODULE "OMNI_GPU"

/* ============================================================================
 * CUDA Error Checking Macros
 * ============================================================================ */

#define CUDA_CHECK_INT(call, ret_val) do { \
    cudaError_t err = call; \
    if (err != cudaSuccess) { \
        LOG_ERROR("CUDA error at %s:%d: %s", __FILE__, __LINE__, \
                  cudaGetErrorString(err)); \
        return ret_val; \
    } \
} while(0)

/* ============================================================================
 * Constants
 * ============================================================================ */

#define BLOCK_SIZE 256
#define WARP_SIZE 32
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)

/* ============================================================================
 * Device Activation Functions
 * ============================================================================ */

__device__ inline float gelu_device(float x) {
    const float sqrt_2_pi = 0.7978845608028654f;
    const float coeff = 0.044715f;
    float x3 = x * x * x;
    float inner = sqrt_2_pi * (x + coeff * x3);
    return 0.5f * x * (1.0f + tanhf(inner));
}

__device__ inline float relu_device(float x) {
    return fmaxf(0.0f, x);
}

__device__ inline float sigmoid_device(float x) {
    return 1.0f / (1.0f + expf(-x));
}

/* ============================================================================
 * Bidirectional Prediction Kernels
 * ============================================================================ */

/**
 * @brief Single-layer forward kernel: output = GELU(W @ input + bias)
 */
__global__ void kernel_omni_layer_forward(
    float* __restrict__ output,
    const float* __restrict__ input,
    const float* __restrict__ weights,
    const float* __restrict__ bias,
    uint32_t batch_size,
    uint32_t in_dim,
    uint32_t out_dim)
{
    uint32_t tid = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t batch_idx = tid / out_dim;
    uint32_t out_idx = tid % out_dim;

    if (batch_idx >= batch_size || out_idx >= out_dim) return;

    double sum = bias[out_idx];
    const float* input_row = input + batch_idx * in_dim;
    const float* weight_row = weights + out_idx * in_dim;

    for (uint32_t j = 0; j < in_dim; j++) {
        sum += weight_row[j] * input_row[j];
    }

    output[batch_idx * out_dim + out_idx] = gelu_device((float)sum);
}

/**
 * @brief Precision-weighted prediction kernel
 */
__global__ void kernel_omni_precision_predict(
    float* __restrict__ output,
    const float* __restrict__ input,
    const float* __restrict__ weights,
    const float* __restrict__ bias,
    const float* __restrict__ precision,
    uint32_t batch_size,
    uint32_t in_dim,
    uint32_t out_dim)
{
    uint32_t tid = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t batch_idx = tid / out_dim;
    uint32_t out_idx = tid % out_dim;

    if (batch_idx >= batch_size || out_idx >= out_dim) return;

    double sum = bias[out_idx];
    const float* input_row = input + batch_idx * in_dim;
    const float* weight_row = weights + out_idx * in_dim;

    for (uint32_t j = 0; j < in_dim; j++) {
        sum += weight_row[j] * input_row[j] * precision[j];
    }

    output[batch_idx * out_dim + out_idx] = gelu_device((float)sum);
}

/* ============================================================================
 * Hopfield Attention Kernels
 * ============================================================================ */

/**
 * @brief Compute similarities between query and all patterns
 */
__global__ void kernel_hopfield_similarities(
    float* __restrict__ similarities,
    const float* __restrict__ query,
    const float* __restrict__ patterns,
    uint32_t pattern_count,
    uint32_t pattern_dim,
    float beta)
{
    uint32_t pattern_idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (pattern_idx >= pattern_count) return;

    const float* pattern = patterns + pattern_idx * pattern_dim;
    double dot = 0.0;
    double norm_p = 0.0;
    double norm_q = 0.0;

    for (uint32_t j = 0; j < pattern_dim; j++) {
        dot += pattern[j] * query[j];
        norm_p += pattern[j] * pattern[j];
        norm_q += query[j] * query[j];
    }

    float cosine = (float)(dot / (sqrt(norm_p) * sqrt(norm_q) + 1e-8));
    similarities[pattern_idx] = beta * cosine;
}

/**
 * @brief Softmax attention kernel with reduction
 */
__global__ void kernel_hopfield_softmax(
    float* __restrict__ attention,
    const float* __restrict__ similarities,
    uint32_t pattern_count)
{
    extern __shared__ float shared[];

    uint32_t tid = threadIdx.x;
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;

    /* Load and find max for numerical stability */
    float val = (i < pattern_count) ? similarities[i] : -FLT_MAX;
    shared[tid] = val;
    __syncthreads();

    /* Parallel reduction for max */
    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s && tid + s < blockDim.x) {
            shared[tid] = fmaxf(shared[tid], shared[tid + s]);
        }
        __syncthreads();
    }
    float max_val = shared[0];
    __syncthreads();

    /* Compute exp(x - max) */
    val = (i < pattern_count) ? expf(similarities[i] - max_val) : 0.0f;
    shared[tid] = val;
    __syncthreads();

    /* Parallel reduction for sum */
    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s && tid + s < blockDim.x) {
            shared[tid] += shared[tid + s];
        }
        __syncthreads();
    }
    float sum = shared[0];

    if (i < pattern_count) {
        attention[i] = val / (sum + 1e-8f);
    }
}

/**
 * @brief Weighted pattern retrieval
 */
__global__ void kernel_hopfield_retrieve(
    float* __restrict__ output,
    const float* __restrict__ patterns,
    const float* __restrict__ attention,
    uint32_t pattern_count,
    uint32_t pattern_dim)
{
    uint32_t dim_idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (dim_idx >= pattern_dim) return;

    double sum = 0.0;
    for (uint32_t p = 0; p < pattern_count; p++) {
        sum += attention[p] * patterns[p * pattern_dim + dim_idx];
    }
    output[dim_idx] = (float)sum;
}

/**
 * @brief Compute Hopfield energy
 */
__global__ void kernel_hopfield_energy(
    float* __restrict__ energy,
    const float* __restrict__ state,
    const float* __restrict__ patterns,
    uint32_t pattern_count,
    uint32_t pattern_dim,
    float beta)
{
    extern __shared__ float shared[];

    uint32_t tid = threadIdx.x;

    /* Compute log-sum-exp of similarities */
    float max_sim = -FLT_MAX;
    for (uint32_t p = tid; p < pattern_count; p += blockDim.x) {
        float dot = 0.0f;
        for (uint32_t j = 0; j < pattern_dim; j++) {
            dot += patterns[p * pattern_dim + j] * state[j];
        }
        max_sim = fmaxf(max_sim, beta * dot);
    }
    shared[tid] = max_sim;
    __syncthreads();

    /* Reduce to find global max */
    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            shared[tid] = fmaxf(shared[tid], shared[tid + s]);
        }
        __syncthreads();
    }
    float global_max = shared[0];
    __syncthreads();

    /* Compute sum of exp */
    float sum_exp = 0.0f;
    for (uint32_t p = tid; p < pattern_count; p += blockDim.x) {
        float dot = 0.0f;
        for (uint32_t j = 0; j < pattern_dim; j++) {
            dot += patterns[p * pattern_dim + j] * state[j];
        }
        sum_exp += expf(beta * dot - global_max);
    }
    shared[tid] = sum_exp;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            shared[tid] += shared[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        /* E = -log(sum_exp) + max + beta/2 * ||state||^2 */
        float norm_sq = 0.0f;
        for (uint32_t j = 0; j < pattern_dim; j++) {
            norm_sq += state[j] * state[j];
        }
        *energy = -logf(shared[0]) - global_max + 0.5f * beta * norm_sq;
    }
}

/* ============================================================================
 * Predictive Hierarchy Kernels
 * ============================================================================ */

/**
 * @brief Compute prediction errors: error = precision * (prediction - state)
 */
__global__ void kernel_hierarchy_compute_error(
    float* __restrict__ error,
    const float* __restrict__ prediction,
    const float* __restrict__ state,
    const float* __restrict__ precision,
    uint32_t dim)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (idx >= dim) return;

    float diff = prediction[idx] - state[idx];
    error[idx] = precision[idx] * diff;
}

/**
 * @brief Bottom-up forward pass through hierarchy level
 */
__global__ void kernel_hierarchy_forward_level(
    float* __restrict__ output,
    const float* __restrict__ input,
    const float* __restrict__ weights,
    const float* __restrict__ bias,
    uint32_t in_dim,
    uint32_t out_dim)
{
    uint32_t out_idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (out_idx >= out_dim) return;

    double sum = bias[out_idx];
    const float* weight_row = weights + out_idx * in_dim;

    for (uint32_t j = 0; j < in_dim; j++) {
        sum += weight_row[j] * input[j];
    }

    output[out_idx] = gelu_device((float)sum);
}

/**
 * @brief Top-down backward pass through hierarchy level
 */
__global__ void kernel_hierarchy_backward_level(
    float* __restrict__ prediction,
    const float* __restrict__ upper_state,
    const float* __restrict__ weights,
    const float* __restrict__ bias,
    uint32_t upper_dim,
    uint32_t lower_dim)
{
    uint32_t lower_idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (lower_idx >= lower_dim) return;

    double sum = bias[lower_idx];
    const float* weight_row = weights + lower_idx * upper_dim;

    for (uint32_t j = 0; j < upper_dim; j++) {
        sum += weight_row[j] * upper_state[j];
    }

    prediction[lower_idx] = (float)sum;
}

/**
 * @brief Update state from error: state += lr * error
 */
__global__ void kernel_hierarchy_update_state(
    float* __restrict__ state,
    const float* __restrict__ error,
    float learning_rate,
    uint32_t dim)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (idx >= dim) return;

    state[idx] -= learning_rate * error[idx];
}

/**
 * @brief Adaptive precision update from errors
 */
__global__ void kernel_hierarchy_update_precision(
    float* __restrict__ precision,
    const float* __restrict__ error,
    float decay,
    float min_precision,
    float max_precision,
    uint32_t dim)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (idx >= dim) return;

    /* Precision ~ 1 / variance, update as exponential moving average */
    float error_sq = error[idx] * error[idx];
    float new_prec = decay * precision[idx] + (1.0f - decay) / (error_sq + 1e-8f);
    precision[idx] = fminf(fmaxf(new_prec, min_precision), max_precision);
}

/**
 * @brief Compute free energy for one level
 */
__global__ void kernel_hierarchy_level_free_energy(
    float* __restrict__ level_fe,
    const float* __restrict__ error,
    const float* __restrict__ precision,
    uint32_t dim)
{
    extern __shared__ float shared[];

    uint32_t tid = threadIdx.x;
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    float val = 0.0f;
    if (idx < dim) {
        val = 0.5f * precision[idx] * error[idx] * error[idx];
    }
    shared[tid] = val;
    __syncthreads();

    /* Parallel reduction */
    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            shared[tid] += shared[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        atomicAdd(level_fe, shared[0]);
    }
}

/* ============================================================================
 * Temporal Replay Kernels
 * ============================================================================ */

/**
 * @brief Priority-based sampling indices
 */
__global__ void kernel_replay_priority_sample(
    uint32_t* __restrict__ indices,
    const float* __restrict__ priorities,
    const float* __restrict__ random_vals,
    uint32_t batch_size,
    uint32_t buffer_size)
{
    uint32_t batch_idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (batch_idx >= batch_size) return;

    /* Compute CDF and sample */
    float target = random_vals[batch_idx];
    float cumsum = 0.0f;

    for (uint32_t i = 0; i < buffer_size; i++) {
        cumsum += priorities[i];
        if (cumsum >= target) {
            indices[batch_idx] = i;
            return;
        }
    }

    indices[batch_idx] = buffer_size - 1;
}

/**
 * @brief Forward sweep: copy sequence in forward order
 */
__global__ void kernel_replay_forward_sweep(
    float* __restrict__ output,
    const float* __restrict__ sequences,
    uint32_t sequence_idx,
    uint32_t start,
    uint32_t length,
    uint32_t sequence_len,
    uint32_t state_dim)
{
    uint32_t tid = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t step = tid / state_dim;
    uint32_t dim = tid % state_dim;

    if (step >= length || dim >= state_dim) return;

    uint32_t src_pos = start + step;
    if (src_pos >= sequence_len) return;

    uint32_t src_offset = sequence_idx * sequence_len * state_dim +
                           src_pos * state_dim + dim;
    uint32_t dst_offset = step * state_dim + dim;

    output[dst_offset] = sequences[src_offset];
}

/**
 * @brief Backward sweep: copy sequence in reverse order
 */
__global__ void kernel_replay_backward_sweep(
    float* __restrict__ output,
    const float* __restrict__ sequences,
    uint32_t sequence_idx,
    uint32_t end,
    uint32_t length,
    uint32_t sequence_len,
    uint32_t state_dim)
{
    uint32_t tid = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t step = tid / state_dim;
    uint32_t dim = tid % state_dim;

    if (step >= length || dim >= state_dim) return;

    int src_pos = (int)end - (int)step;
    if (src_pos < 0) return;

    uint32_t src_offset = sequence_idx * sequence_len * state_dim +
                           src_pos * state_dim + dim;
    uint32_t dst_offset = step * state_dim + dim;

    output[dst_offset] = sequences[src_offset];
}

/**
 * @brief Update replay priorities
 */
__global__ void kernel_replay_update_priorities(
    float* __restrict__ priorities,
    const uint32_t* __restrict__ indices,
    const float* __restrict__ new_priorities,
    uint32_t count)
{
    uint32_t tid = blockIdx.x * blockDim.x + threadIdx.x;

    if (tid >= count) return;

    uint32_t idx = indices[tid];
    priorities[idx] = new_priorities[tid];
}

/* ============================================================================
 * Free Energy Aggregation Kernels
 * ============================================================================ */

/**
 * @brief Compute precision-weighted squared error
 */
__global__ void kernel_compute_free_energy(
    float* __restrict__ total_fe,
    const float* __restrict__ prediction_error,
    const float* __restrict__ precision,
    uint32_t dim)
{
    extern __shared__ float shared[];

    uint32_t tid = threadIdx.x;
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    float val = 0.0f;
    if (idx < dim) {
        float err = prediction_error[idx];
        val = 0.5f * precision[idx] * err * err;
    }
    shared[tid] = val;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            shared[tid] += shared[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        atomicAdd(total_fe, shared[0]);
    }
}

/**
 * @brief Compute FE gradient: grad = precision * error
 */
__global__ void kernel_compute_fe_gradient(
    float* __restrict__ gradient,
    const float* __restrict__ prediction_error,
    const float* __restrict__ precision,
    uint32_t dim)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (idx >= dim) return;

    gradient[idx] = precision[idx] * prediction_error[idx];
}

/* ============================================================================
 * Host API Implementation
 * ============================================================================ */

static const char* DIRECTION_STRINGS[] = {
    "FORWARD", "BACKWARD", "LATERAL", "HIER_UP", "HIER_DOWN",
    "MASKED", "ASSOCIATIVE"
};

static const char* HOPFIELD_MODE_STRINGS[] = {
    "SOFTMAX", "EXPONENTIAL", "POLYNOMIAL", "SPARSE_TOP_K"
};

static const char* REPLAY_MODE_STRINGS[] = {
    "FORWARD", "BACKWARD", "PRIORITY"
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

nimcp_omni_gpu_state_t* nimcp_omni_gpu_create(
    nimcp_gpu_context_t* ctx,
    uint32_t latent_dim,
    uint32_t hidden_dim,
    uint32_t num_patterns,
    uint32_t num_levels)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        LOG_ERROR("Invalid GPU context");
        return NULL;
    }

    nimcp_omni_gpu_state_t* state = (nimcp_omni_gpu_state_t*)nimcp_calloc(1, sizeof(*state));
    if (!state) {
        LOG_ERROR("Failed to allocate GPU state");
        return NULL;
    }

    state->ctx = ctx;

    /* Allocate bidirectional predictor */
    state->bidirectional = (nimcp_omni_gpu_bidirectional_t*)nimcp_calloc(1, sizeof(*state->bidirectional));
    if (!state->bidirectional) {
        nimcp_omni_gpu_destroy(state);
        return NULL;
    }
    state->bidirectional->ctx = ctx;
    state->bidirectional->input_dim = latent_dim;
    state->bidirectional->hidden_dim = hidden_dim;
    state->bidirectional->output_dim = latent_dim;

    /* Allocate Hopfield state */
    state->hopfield = (nimcp_omni_gpu_hopfield_t*)nimcp_calloc(1, sizeof(*state->hopfield));
    if (!state->hopfield) {
        nimcp_omni_gpu_destroy(state);
        return NULL;
    }
    state->hopfield->ctx = ctx;
    state->hopfield->pattern_dim = latent_dim;
    state->hopfield->capacity = num_patterns;
    state->hopfield->beta = 1.0f;
    state->hopfield->mode = NIMCP_HOPFIELD_GPU_SOFTMAX;

    /* Allocate hierarchy state */
    state->hierarchy = (nimcp_omni_gpu_hierarchy_t*)nimcp_calloc(1, sizeof(*state->hierarchy));
    if (!state->hierarchy) {
        nimcp_omni_gpu_destroy(state);
        return NULL;
    }
    state->hierarchy->ctx = ctx;
    state->hierarchy->num_levels = num_levels;

    /* Allocate replay state */
    state->replay = (nimcp_omni_gpu_replay_t*)nimcp_calloc(1, sizeof(*state->replay));
    if (!state->replay) {
        nimcp_omni_gpu_destroy(state);
        return NULL;
    }
    state->replay->ctx = ctx;
    state->replay->state_dim = latent_dim;

    state->initialized = true;
    LOG_INFO("Created omni GPU state: dim=%u, hidden=%u, patterns=%u, levels=%u",
             latent_dim, hidden_dim, num_patterns, num_levels);

    return state;
}

void nimcp_omni_gpu_destroy(nimcp_omni_gpu_state_t* state)
{
    if (!state) return;

    if (state->bidirectional) {
        if (state->bidirectional->forward_weights) {
            nimcp_gpu_tensor_destroy(state->bidirectional->forward_weights);
        }
        if (state->bidirectional->backward_weights) {
            nimcp_gpu_tensor_destroy(state->bidirectional->backward_weights);
        }
        if (state->bidirectional->lateral_weights) {
            nimcp_gpu_tensor_destroy(state->bidirectional->lateral_weights);
        }
        nimcp_free(state->bidirectional);
    }

    if (state->hopfield) {
        if (state->hopfield->patterns) {
            nimcp_gpu_tensor_destroy(state->hopfield->patterns);
        }
        if (state->hopfield->similarities) {
            nimcp_gpu_tensor_destroy(state->hopfield->similarities);
        }
        if (state->hopfield->attention) {
            nimcp_gpu_tensor_destroy(state->hopfield->attention);
        }
        nimcp_free(state->hopfield);
    }

    if (state->hierarchy) {
        for (uint32_t i = 0; i < state->hierarchy->num_levels; i++) {
            if (state->hierarchy->predictions && state->hierarchy->predictions[i]) {
                nimcp_gpu_tensor_destroy(state->hierarchy->predictions[i]);
            }
            if (state->hierarchy->errors && state->hierarchy->errors[i]) {
                nimcp_gpu_tensor_destroy(state->hierarchy->errors[i]);
            }
            if (state->hierarchy->precisions && state->hierarchy->precisions[i]) {
                nimcp_gpu_tensor_destroy(state->hierarchy->precisions[i]);
            }
            if (state->hierarchy->states && state->hierarchy->states[i]) {
                nimcp_gpu_tensor_destroy(state->hierarchy->states[i]);
            }
        }
        nimcp_free(state->hierarchy->predictions);
        nimcp_free(state->hierarchy->errors);
        nimcp_free(state->hierarchy->precisions);
        nimcp_free(state->hierarchy->states);
        nimcp_free(state->hierarchy->level_dims);
        nimcp_free(state->hierarchy);
    }

    if (state->replay) {
        if (state->replay->sequences) {
            nimcp_gpu_tensor_destroy(state->replay->sequences);
        }
        if (state->replay->priorities) {
            nimcp_gpu_tensor_destroy(state->replay->priorities);
        }
        nimcp_free(state->replay);
    }

    if (state->global_precision) {
        nimcp_gpu_tensor_destroy(state->global_precision);
    }
    if (state->free_energy) {
        nimcp_gpu_tensor_destroy(state->free_energy);
    }

    nimcp_free(state);
    LOG_DEBUG("Destroyed omni GPU state");
}

bool nimcp_omni_gpu_is_valid(const nimcp_omni_gpu_state_t* state)
{
    return state && state->initialized && state->ctx &&
           nimcp_gpu_context_is_valid(state->ctx);
}

/* ============================================================================
 * Bidirectional Prediction API
 * ============================================================================ */

bool nimcp_omni_gpu_predict(
    nimcp_omni_gpu_state_t* state,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    nimcp_omni_gpu_direction_t direction)
{
    if (!nimcp_omni_gpu_is_valid(state) || !input || !output) {
        return false;
    }
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    nimcp_gpu_tensor_t* weights = NULL;
    nimcp_gpu_tensor_t* bias = NULL;

    switch (direction) {
        case NIMCP_OMNI_GPU_DIR_FORWARD:
            weights = state->bidirectional->forward_weights;
            bias = state->bidirectional->forward_bias;
            break;
        case NIMCP_OMNI_GPU_DIR_BACKWARD:
            weights = state->bidirectional->backward_weights;
            bias = state->bidirectional->backward_bias;
            break;
        case NIMCP_OMNI_GPU_DIR_LATERAL:
            weights = state->bidirectional->lateral_weights;
            bias = state->bidirectional->lateral_bias;
            break;
        default:
            LOG_ERROR("Unsupported direction: %d", direction);
            return false;
    }

    if (!weights || !bias) {
        LOG_ERROR("Weights not initialized for direction %s",
                  DIRECTION_STRINGS[direction]);
        return false;
    }

    uint32_t batch_size = input->dims[0];
    uint32_t in_dim = state->bidirectional->input_dim;
    uint32_t out_dim = state->bidirectional->output_dim;

    uint32_t total = batch_size * out_dim;
    dim3 block(BLOCK_SIZE);
    dim3 grid(GRID_SIZE(total));

    cudaStream_t stream = nimcp_gpu_get_compute_stream(state->ctx);

    kernel_omni_layer_forward<<<grid, block, 0, stream>>>(
        (float*)output->data,
        (const float*)input->data,
        (const float*)weights->data,
        (const float*)bias->data,
        batch_size, in_dim, out_dim
    );
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    return true;
}

bool nimcp_omni_gpu_predict_multi(
    nimcp_omni_gpu_state_t* state,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t** outputs,
    const nimcp_omni_gpu_direction_t* directions,
    uint32_t num_directions)
{
    for (uint32_t i = 0; i < num_directions; i++) {
        if (!nimcp_omni_gpu_predict(state, input, outputs[i], directions[i])) {
            return false;
        }
    }
    return true;
}

bool nimcp_omni_gpu_predict_precision(
    nimcp_omni_gpu_state_t* state,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* precision,
    nimcp_gpu_tensor_t* output,
    nimcp_omni_gpu_direction_t direction)
{
    if (!nimcp_omni_gpu_is_valid(state) || !input || !precision || !output) {
        return false;
    }
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    nimcp_gpu_tensor_t* weights = NULL;
    nimcp_gpu_tensor_t* bias = NULL;

    switch (direction) {
        case NIMCP_OMNI_GPU_DIR_FORWARD:
            weights = state->bidirectional->forward_weights;
            bias = state->bidirectional->forward_bias;
            break;
        case NIMCP_OMNI_GPU_DIR_BACKWARD:
            weights = state->bidirectional->backward_weights;
            bias = state->bidirectional->backward_bias;
            break;
        default:
            LOG_ERROR("Unsupported direction for precision predict: %d", direction);
            return false;
    }

    if (!weights || !bias) {
        return false;
    }

    uint32_t batch_size = input->dims[0];
    uint32_t in_dim = state->bidirectional->input_dim;
    uint32_t out_dim = state->bidirectional->output_dim;

    uint32_t total = batch_size * out_dim;
    dim3 block(BLOCK_SIZE);
    dim3 grid(GRID_SIZE(total));

    cudaStream_t stream = nimcp_gpu_get_compute_stream(state->ctx);

    kernel_omni_precision_predict<<<grid, block, 0, stream>>>(
        (float*)output->data,
        (const float*)input->data,
        (const float*)weights->data,
        (const float*)bias->data,
        (const float*)precision->data,
        batch_size, in_dim, out_dim
    );
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    return true;
}

/* ============================================================================
 * Hopfield API
 * ============================================================================ */

bool nimcp_omni_gpu_hopfield_init(
    nimcp_omni_gpu_state_t* state,
    uint32_t pattern_dim,
    uint32_t capacity,
    float beta,
    nimcp_hopfield_gpu_mode_t mode)
{
    if (!nimcp_omni_gpu_is_valid(state)) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    state->hopfield->pattern_dim = pattern_dim;
    state->hopfield->capacity = capacity;
    state->hopfield->beta = beta;
    state->hopfield->mode = mode;
    state->hopfield->pattern_count = 0;

    /* Allocate GPU tensors */
    size_t pattern_dims[2] = {capacity, pattern_dim};
    state->hopfield->patterns = nimcp_gpu_tensor_create(
        state->ctx, pattern_dims, 2, NIMCP_GPU_PRECISION_FP32);

    size_t sim_dims[1] = {capacity};
    state->hopfield->similarities = nimcp_gpu_tensor_create(
        state->ctx, sim_dims, 1, NIMCP_GPU_PRECISION_FP32);

    state->hopfield->attention = nimcp_gpu_tensor_create(
        state->ctx, sim_dims, 1, NIMCP_GPU_PRECISION_FP32);

    size_t out_dims[1] = {pattern_dim};
    state->hopfield->output = nimcp_gpu_tensor_create(
        state->ctx, out_dims, 1, NIMCP_GPU_PRECISION_FP32);

    if (!state->hopfield->patterns || !state->hopfield->similarities ||
        !state->hopfield->attention || !state->hopfield->output) {
        LOG_ERROR("Failed to allocate Hopfield GPU tensors");
        return false;
    }

    LOG_INFO("Initialized Hopfield GPU: dim=%u, capacity=%u, beta=%.2f, mode=%s",
             pattern_dim, capacity, beta, HOPFIELD_MODE_STRINGS[mode]);
    return true;
}

int nimcp_omni_gpu_hopfield_store(
    nimcp_omni_gpu_state_t* state,
    const nimcp_gpu_tensor_t* pattern)
{
    if (!nimcp_omni_gpu_is_valid(state) || !pattern) return -1;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (state->hopfield->pattern_count >= state->hopfield->capacity) {
        LOG_WARN("Hopfield memory at capacity");
        return -1;
    }

    uint32_t idx = state->hopfield->pattern_count;
    uint32_t dim = state->hopfield->pattern_dim;
    size_t offset = idx * dim * sizeof(float);

    NIMCP_CUDA_RECOVER(cudaMemcpy(
        (char*)state->hopfield->patterns->data + offset,
        pattern->data,
        dim * sizeof(float),
        cudaMemcpyDeviceToDevice
    ), GPU_ERROR_CUDA_RUNTIME);

    state->hopfield->pattern_count++;
    return (int)idx;
}

int nimcp_omni_gpu_hopfield_store_batch(
    nimcp_omni_gpu_state_t* state,
    const nimcp_gpu_tensor_t* patterns,
    uint32_t num_patterns)
{
    if (!nimcp_omni_gpu_is_valid(state) || !patterns) return -1;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    uint32_t available = state->hopfield->capacity - state->hopfield->pattern_count;
    uint32_t to_store = (num_patterns < available) ? num_patterns : available;

    if (to_store == 0) return 0;

    uint32_t dim = state->hopfield->pattern_dim;
    size_t offset = state->hopfield->pattern_count * dim * sizeof(float);

    NIMCP_CUDA_RECOVER(cudaMemcpy(
        (char*)state->hopfield->patterns->data + offset,
        patterns->data,
        to_store * dim * sizeof(float),
        cudaMemcpyDeviceToDevice
    ), GPU_ERROR_CUDA_RUNTIME);

    state->hopfield->pattern_count += to_store;
    return (int)to_store;
}

bool nimcp_omni_gpu_hopfield_retrieve(
    nimcp_omni_gpu_state_t* state,
    const nimcp_gpu_tensor_t* query,
    nimcp_gpu_tensor_t* output,
    uint32_t max_iterations)
{
    if (!nimcp_omni_gpu_is_valid(state) || !query || !output) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }
    if (state->hopfield->pattern_count == 0) return false;

    cudaStream_t stream = nimcp_gpu_get_compute_stream(state->ctx);
    uint32_t pattern_count = state->hopfield->pattern_count;
    uint32_t pattern_dim = state->hopfield->pattern_dim;
    float beta = state->hopfield->beta;

    /* Copy query to working buffer */
    NIMCP_CUDA_RECOVER(cudaMemcpy(
        state->hopfield->output->data,
        query->data,
        pattern_dim * sizeof(float),
        cudaMemcpyDeviceToDevice
    ), GPU_ERROR_CUDA_RUNTIME);

    for (uint32_t iter = 0; iter < max_iterations; iter++) {
        /* Compute similarities */
        dim3 block1(BLOCK_SIZE);
        dim3 grid1(GRID_SIZE(pattern_count));

        kernel_hopfield_similarities<<<grid1, block1, 0, stream>>>(
            (float*)state->hopfield->similarities->data,
            (const float*)state->hopfield->output->data,
            (const float*)state->hopfield->patterns->data,
            pattern_count, pattern_dim, beta
        );
        NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

        /* Softmax attention */
        size_t shared_mem = BLOCK_SIZE * sizeof(float);
        kernel_hopfield_softmax<<<1, BLOCK_SIZE, shared_mem, stream>>>(
            (float*)state->hopfield->attention->data,
            (const float*)state->hopfield->similarities->data,
            pattern_count
        );
        NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

        /* Weighted retrieval */
        dim3 block3(BLOCK_SIZE);
        dim3 grid3(GRID_SIZE(pattern_dim));

        kernel_hopfield_retrieve<<<grid3, block3, 0, stream>>>(
            (float*)state->hopfield->output->data,
            (const float*)state->hopfield->patterns->data,
            (const float*)state->hopfield->attention->data,
            pattern_count, pattern_dim
        );
        NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    }

    /* Copy result to output */
    NIMCP_CUDA_RECOVER(cudaMemcpy(
        output->data,
        state->hopfield->output->data,
        pattern_dim * sizeof(float),
        cudaMemcpyDeviceToDevice
    ), GPU_ERROR_CUDA_RUNTIME);

    return true;
}

bool nimcp_omni_gpu_hopfield_energy(
    nimcp_omni_gpu_state_t* state,
    const nimcp_gpu_tensor_t* pattern,
    float* energy)
{
    if (!nimcp_omni_gpu_is_valid(state) || !pattern || !energy) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    float* d_energy;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_energy, sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMemset(d_energy, 0, sizeof(float)), GPU_ERROR_CUDA_RUNTIME);

    cudaStream_t stream = nimcp_gpu_get_compute_stream(state->ctx);
    size_t shared_mem = BLOCK_SIZE * sizeof(float);

    kernel_hopfield_energy<<<1, BLOCK_SIZE, shared_mem, stream>>>(
        d_energy,
        (const float*)pattern->data,
        (const float*)state->hopfield->patterns->data,
        state->hopfield->pattern_count,
        state->hopfield->pattern_dim,
        state->hopfield->beta
    );
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    NIMCP_CUDA_RECOVER(cudaMemcpy(energy, d_energy, sizeof(float), cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);
    cudaFree(d_energy);

    return true;
}

/* ============================================================================
 * Hierarchy API
 * ============================================================================ */

bool nimcp_omni_gpu_hierarchy_init(
    nimcp_omni_gpu_state_t* state,
    const uint32_t* level_dims,
    uint32_t num_levels,
    nimcp_precision_gpu_mode_t precision_mode)
{
    if (!nimcp_omni_gpu_is_valid(state) || !level_dims || num_levels == 0) {
        return false;
    }
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    state->hierarchy->num_levels = num_levels;
    state->hierarchy->precision_mode = precision_mode;

    /* Allocate level dimension array */
    state->hierarchy->level_dims = (uint32_t*)nimcp_malloc(num_levels * sizeof(uint32_t));
    memcpy(state->hierarchy->level_dims, level_dims, num_levels * sizeof(uint32_t));

    /* Allocate tensor arrays */
    state->hierarchy->predictions = (nimcp_gpu_tensor_t**)nimcp_calloc(num_levels, sizeof(void*));
    state->hierarchy->errors = (nimcp_gpu_tensor_t**)nimcp_calloc(num_levels, sizeof(void*));
    state->hierarchy->precisions = (nimcp_gpu_tensor_t**)nimcp_calloc(num_levels, sizeof(void*));
    state->hierarchy->states = (nimcp_gpu_tensor_t**)nimcp_calloc(num_levels, sizeof(void*));
    state->hierarchy->up_weights = (nimcp_gpu_tensor_t**)nimcp_calloc(num_levels, sizeof(void*));
    state->hierarchy->down_weights = (nimcp_gpu_tensor_t**)nimcp_calloc(num_levels, sizeof(void*));

    if (!state->hierarchy->predictions || !state->hierarchy->errors ||
        !state->hierarchy->precisions || !state->hierarchy->states) {
        return false;
    }

    /* Allocate tensors for each level */
    for (uint32_t i = 0; i < num_levels; i++) {
        size_t dims[1] = {level_dims[i]};

        state->hierarchy->predictions[i] = nimcp_gpu_tensor_create(
            state->ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
        state->hierarchy->errors[i] = nimcp_gpu_tensor_create(
            state->ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
        state->hierarchy->precisions[i] = nimcp_gpu_tensor_create(
            state->ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
        state->hierarchy->states[i] = nimcp_gpu_tensor_create(
            state->ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);

        /* Initialize precision to 1.0 */
        float* h_ones = (float*)nimcp_malloc(level_dims[i] * sizeof(float));
        for (uint32_t j = 0; j < level_dims[i]; j++) h_ones[j] = 1.0f;
        NIMCP_CUDA_RECOVER(cudaMemcpy(state->hierarchy->precisions[i]->data, h_ones,
                   level_dims[i] * sizeof(float), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);
        nimcp_free(h_ones);

        if (!state->hierarchy->predictions[i] || !state->hierarchy->errors[i] ||
            !state->hierarchy->precisions[i] || !state->hierarchy->states[i]) {
            return false;
        }
    }

    LOG_INFO("Initialized hierarchy GPU: %u levels", num_levels);
    return true;
}

bool nimcp_omni_gpu_hierarchy_compute_errors(nimcp_omni_gpu_state_t* state)
{
    if (!nimcp_omni_gpu_is_valid(state)) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(state->ctx);

    for (uint32_t level = 0; level < state->hierarchy->num_levels; level++) {
        uint32_t dim = state->hierarchy->level_dims[level];
        dim3 block(BLOCK_SIZE);
        dim3 grid(GRID_SIZE(dim));

        kernel_hierarchy_compute_error<<<grid, block, 0, stream>>>(
            (float*)state->hierarchy->errors[level]->data,
            (const float*)state->hierarchy->predictions[level]->data,
            (const float*)state->hierarchy->states[level]->data,
            (const float*)state->hierarchy->precisions[level]->data,
            dim
        );
        NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    }

    return true;
}

bool nimcp_omni_gpu_hierarchy_update_states(
    nimcp_omni_gpu_state_t* state,
    float learning_rate)
{
    if (!nimcp_omni_gpu_is_valid(state)) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(state->ctx);

    for (uint32_t level = 0; level < state->hierarchy->num_levels; level++) {
        uint32_t dim = state->hierarchy->level_dims[level];
        dim3 block(BLOCK_SIZE);
        dim3 grid(GRID_SIZE(dim));

        kernel_hierarchy_update_state<<<grid, block, 0, stream>>>(
            (float*)state->hierarchy->states[level]->data,
            (const float*)state->hierarchy->errors[level]->data,
            learning_rate,
            dim
        );
        NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    }

    return true;
}

bool nimcp_omni_gpu_hierarchy_update_precision(nimcp_omni_gpu_state_t* state)
{
    if (!nimcp_omni_gpu_is_valid(state)) return false;
    if (state->hierarchy->precision_mode == NIMCP_PRECISION_GPU_FIXED) return true;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(state->ctx);
    float decay = 0.99f;
    float min_prec = 0.01f;
    float max_prec = 100.0f;

    for (uint32_t level = 0; level < state->hierarchy->num_levels; level++) {
        uint32_t dim = state->hierarchy->level_dims[level];
        dim3 block(BLOCK_SIZE);
        dim3 grid(GRID_SIZE(dim));

        kernel_hierarchy_update_precision<<<grid, block, 0, stream>>>(
            (float*)state->hierarchy->precisions[level]->data,
            (const float*)state->hierarchy->errors[level]->data,
            decay, min_prec, max_prec,
            dim
        );
        NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    }

    return true;
}

bool nimcp_omni_gpu_hierarchy_free_energy(
    nimcp_omni_gpu_state_t* state,
    float* free_energy)
{
    if (!nimcp_omni_gpu_is_valid(state) || !free_energy) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    float* d_total_fe;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_total_fe, sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMemset(d_total_fe, 0, sizeof(float)), GPU_ERROR_CUDA_RUNTIME);

    cudaStream_t stream = nimcp_gpu_get_compute_stream(state->ctx);
    size_t shared_mem = BLOCK_SIZE * sizeof(float);

    for (uint32_t level = 0; level < state->hierarchy->num_levels; level++) {
        uint32_t dim = state->hierarchy->level_dims[level];
        dim3 block(BLOCK_SIZE);
        dim3 grid(GRID_SIZE(dim));

        kernel_hierarchy_level_free_energy<<<grid, block, shared_mem, stream>>>(
            d_total_fe,
            (const float*)state->hierarchy->errors[level]->data,
            (const float*)state->hierarchy->precisions[level]->data,
            dim
        );
        NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    }

    NIMCP_CUDA_RECOVER(cudaMemcpy(free_energy, d_total_fe, sizeof(float), cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);
    cudaFree(d_total_fe);

    return true;
}

/* ============================================================================
 * Replay API
 * ============================================================================ */

bool nimcp_omni_gpu_replay_init(
    nimcp_omni_gpu_state_t* state,
    uint32_t state_dim,
    uint32_t capacity,
    uint32_t sequence_len)
{
    if (!nimcp_omni_gpu_is_valid(state)) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    state->replay->state_dim = state_dim;
    state->replay->capacity = capacity;
    state->replay->sequence_len = sequence_len;
    state->replay->head = 0;
    state->replay->count = 0;

    /* Allocate sequence buffer */
    size_t seq_dims[3] = {capacity, sequence_len, state_dim};
    state->replay->sequences = nimcp_gpu_tensor_create(
        state->ctx, seq_dims, 3, NIMCP_GPU_PRECISION_FP32);

    size_t prio_dims[1] = {capacity};
    state->replay->priorities = nimcp_gpu_tensor_create(
        state->ctx, prio_dims, 1, NIMCP_GPU_PRECISION_FP32);

    if (!state->replay->sequences || !state->replay->priorities) {
        return false;
    }

    /* Initialize priorities to uniform */
    float uniform = 1.0f / capacity;
    float* h_prio = (float*)nimcp_malloc(capacity * sizeof(float));
    for (uint32_t i = 0; i < capacity; i++) h_prio[i] = uniform;
    NIMCP_CUDA_RECOVER(cudaMemcpy(state->replay->priorities->data, h_prio,
               capacity * sizeof(float), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);
    nimcp_free(h_prio);

    LOG_INFO("Initialized replay GPU: dim=%u, capacity=%u, seq_len=%u",
             state_dim, capacity, sequence_len);
    return true;
}

int nimcp_omni_gpu_replay_store(
    nimcp_omni_gpu_state_t* state,
    const nimcp_gpu_tensor_t* sequence,
    float priority)
{
    if (!nimcp_omni_gpu_is_valid(state) || !sequence) return -1;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    uint32_t idx = state->replay->head;
    uint32_t seq_len = state->replay->sequence_len;
    uint32_t dim = state->replay->state_dim;
    size_t offset = idx * seq_len * dim * sizeof(float);

    NIMCP_CUDA_RECOVER(cudaMemcpy(
        (char*)state->replay->sequences->data + offset,
        sequence->data,
        seq_len * dim * sizeof(float),
        cudaMemcpyDeviceToDevice
    ), GPU_ERROR_CUDA_RUNTIME);

    /* Update priority */
    NIMCP_CUDA_RECOVER(cudaMemcpy(
        (float*)state->replay->priorities->data + idx,
        &priority,
        sizeof(float),
        cudaMemcpyHostToDevice
    ), GPU_ERROR_CUDA_RUNTIME);

    state->replay->head = (state->replay->head + 1) % state->replay->capacity;
    if (state->replay->count < state->replay->capacity) {
        state->replay->count++;
    }

    return (int)idx;
}

bool nimcp_omni_gpu_replay_forward_sweep(
    nimcp_omni_gpu_state_t* state,
    uint32_t sequence_idx,
    uint32_t start,
    uint32_t length,
    nimcp_gpu_tensor_t* sweep_output)
{
    if (!nimcp_omni_gpu_is_valid(state) || !sweep_output) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(state->ctx);
    uint32_t total = length * state->replay->state_dim;
    dim3 block(BLOCK_SIZE);
    dim3 grid(GRID_SIZE(total));

    kernel_replay_forward_sweep<<<grid, block, 0, stream>>>(
        (float*)sweep_output->data,
        (const float*)state->replay->sequences->data,
        sequence_idx, start, length,
        state->replay->sequence_len,
        state->replay->state_dim
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_omni_gpu_replay_backward_sweep(
    nimcp_omni_gpu_state_t* state,
    uint32_t sequence_idx,
    uint32_t end,
    uint32_t length,
    nimcp_gpu_tensor_t* sweep_output)
{
    if (!nimcp_omni_gpu_is_valid(state) || !sweep_output) return false;
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(state->ctx);
    uint32_t total = length * state->replay->state_dim;
    dim3 block(BLOCK_SIZE);
    dim3 grid(GRID_SIZE(total));

    kernel_replay_backward_sweep<<<grid, block, 0, stream>>>(
        (float*)sweep_output->data,
        (const float*)state->replay->sequences->data,
        sequence_idx, end, length,
        state->replay->sequence_len,
        state->replay->state_dim
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/* ============================================================================
 * Free Energy API
 * ============================================================================ */

bool nimcp_omni_gpu_compute_free_energy(
    nimcp_omni_gpu_state_t* state,
    const nimcp_gpu_tensor_t* prediction_error,
    const nimcp_gpu_tensor_t* precision,
    float* total_fe)
{
    if (!nimcp_omni_gpu_is_valid(state) || !prediction_error || !precision || !total_fe) {
        return false;
    }
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    float* d_fe;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_fe, sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMemset(d_fe, 0, sizeof(float)), GPU_ERROR_CUDA_RUNTIME);

    uint32_t dim = prediction_error->dims[0];
    cudaStream_t stream = nimcp_gpu_get_compute_stream(state->ctx);
    size_t shared_mem = BLOCK_SIZE * sizeof(float);
    dim3 block(BLOCK_SIZE);
    dim3 grid(GRID_SIZE(dim));

    kernel_compute_free_energy<<<grid, block, shared_mem, stream>>>(
        d_fe,
        (const float*)prediction_error->data,
        (const float*)precision->data,
        dim
    );
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    NIMCP_CUDA_RECOVER(cudaMemcpy(total_fe, d_fe, sizeof(float), cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);
    cudaFree(d_fe);

    return true;
}

bool nimcp_omni_gpu_compute_fe_gradient(
    nimcp_omni_gpu_state_t* state,
    const nimcp_gpu_tensor_t* prediction_error,
    const nimcp_gpu_tensor_t* precision,
    nimcp_gpu_tensor_t* gradient)
{
    if (!nimcp_omni_gpu_is_valid(state) || !prediction_error || !precision || !gradient) {
        return false;
    }
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    uint32_t dim = prediction_error->dims[0];
    cudaStream_t stream = nimcp_gpu_get_compute_stream(state->ctx);
    dim3 block(BLOCK_SIZE);
    dim3 grid(GRID_SIZE(dim));

    kernel_compute_fe_gradient<<<grid, block, 0, stream>>>(
        (float*)gradient->data,
        (const float*)prediction_error->data,
        (const float*)precision->data,
        dim
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/* ============================================================================
 * Utility API
 * ============================================================================ */

bool nimcp_omni_gpu_synchronize(nimcp_omni_gpu_state_t* state)
{
    if (!nimcp_omni_gpu_is_valid(state)) return false;
    return nimcp_gpu_stream_synchronize(state->ctx) == 0;
}

void nimcp_omni_gpu_memory_usage(
    const nimcp_omni_gpu_state_t* state,
    size_t* allocated_bytes,
    size_t* peak_bytes)
{
    if (!state || !state->ctx) return;
    nimcp_gpu_memory_stats(state->ctx, allocated_bytes, peak_bytes, NULL);
}

const char* nimcp_omni_gpu_direction_to_string(nimcp_omni_gpu_direction_t dir)
{
    if (dir < 0 || dir >= NIMCP_OMNI_GPU_DIR_COUNT) {
        return "UNKNOWN";
    }
    return DIRECTION_STRINGS[dir];
}

const char* nimcp_hopfield_gpu_mode_to_string(nimcp_hopfield_gpu_mode_t mode)
{
    if (mode < 0 || mode > NIMCP_HOPFIELD_GPU_SPARSE_TOP_K) {
        return "UNKNOWN";
    }
    return HOPFIELD_MODE_STRINGS[mode];
}

const char* nimcp_replay_gpu_mode_to_string(nimcp_replay_gpu_mode_t mode)
{
    if (mode < 0 || mode > NIMCP_REPLAY_GPU_PRIORITY) {
        return "UNKNOWN";
    }
    return REPLAY_MODE_STRINGS[mode];
}

#else /* !NIMCP_ENABLE_CUDA */

/* Stub implementations when CUDA is not available */

nimcp_omni_gpu_state_t* nimcp_omni_gpu_create(
    nimcp_gpu_context_t* ctx,
    uint32_t latent_dim,
    uint32_t hidden_dim,
    uint32_t num_patterns,
    uint32_t num_levels)
{
    (void)ctx; (void)latent_dim; (void)hidden_dim;
    (void)num_patterns; (void)num_levels;
    return NULL;
}

void nimcp_omni_gpu_destroy(nimcp_omni_gpu_state_t* state) { (void)state; }

bool nimcp_omni_gpu_is_valid(const nimcp_omni_gpu_state_t* state)
{
    (void)state;
    return false;
}

bool nimcp_omni_gpu_predict(
    nimcp_omni_gpu_state_t* state,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    nimcp_omni_gpu_direction_t direction)
{
    (void)state; (void)input; (void)output; (void)direction;
    return false;
}

const char* nimcp_omni_gpu_direction_to_string(nimcp_omni_gpu_direction_t dir)
{
    static const char* strs[] = {"FORWARD", "BACKWARD", "LATERAL", "HIER_UP",
                                  "HIER_DOWN", "MASKED", "ASSOCIATIVE"};
    if (dir < 0 || dir >= 7) return "UNKNOWN";
    return strs[dir];
}

const char* nimcp_hopfield_gpu_mode_to_string(nimcp_hopfield_gpu_mode_t mode)
{
    static const char* strs[] = {"SOFTMAX", "EXPONENTIAL", "POLYNOMIAL", "SPARSE_TOP_K"};
    if (mode < 0 || mode > 3) return "UNKNOWN";
    return strs[mode];
}

const char* nimcp_replay_gpu_mode_to_string(nimcp_replay_gpu_mode_t mode)
{
    static const char* strs[] = {"FORWARD", "BACKWARD", "PRIORITY"};
    if (mode < 0 || mode > 2) return "UNKNOWN";
    return strs[mode];
}

#endif /* NIMCP_ENABLE_CUDA */
