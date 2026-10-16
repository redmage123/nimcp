/**
 * @file nimcp_fuzzy_anfis_kernels.cu
 * @brief GPU ANFIS Training Kernels
 *
 * WHAT: GPU-accelerated ANFIS (Adaptive Neuro-Fuzzy Inference System) training
 * WHY:  Fast ANFIS training with backpropagation on GPU
 * HOW:  Forward pass, backward pass, parameter updates in parallel
 *
 * ANFIS Architecture (5 layers):
 *   Layer 1: Fuzzification - membership function evaluation
 *   Layer 2: Rule firing - T-norm of antecedent memberships
 *   Layer 3: Normalization - normalized firing strengths
 *   Layer 4: Consequent - linear functions weighted by firing strengths
 *   Layer 5: Summation - weighted sum of consequent outputs
 *
 * Training:
 *   - Hybrid learning: LSE for consequent params, gradient descent for premise
 *   - Or full backpropagation for both
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

#include "utils/memory/nimcp_memory.h"
#include <cuda_runtime.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <float.h>

#include "gpu/fuzzy/nimcp_fuzzy_gpu.h"
#include "gpu/fuzzy/nimcp_fuzzy_anfis_gpu.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Thread-Local Error Storage
//=============================================================================

static __thread char g_anfis_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_anfis_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_anfis_error, sizeof(g_anfis_error), fmt, args);
    va_end(args);
}

//=============================================================================
// ANFIS State Structure
//=============================================================================

typedef struct {
    // Architecture
    uint32_t num_inputs;
    uint32_t num_mfs_per_input;
    uint32_t num_rules;
    uint32_t num_outputs;

    // Device memory for parameters
    float* d_mf_params;          // Premise MF parameters [num_inputs * num_mfs * 3]
    float* d_consequent_params;  // Consequent parameters [num_rules * (num_inputs + 1)]

    // Device memory for activations (forward pass)
    float* d_layer1_out;         // MF outputs [batch * num_inputs * num_mfs]
    float* d_layer2_out;         // Firing strengths [batch * num_rules]
    float* d_layer3_out;         // Normalized firing [batch * num_rules]
    float* d_layer4_out;         // Consequent outputs [batch * num_rules]
    float* d_layer5_out;         // Final output [batch]

    // Device memory for gradients
    float* d_grad_mf_params;
    float* d_grad_consequent;

    // Training state
    float learning_rate_premise;
    float learning_rate_consequent;

    bool initialized;
} anfis_gpu_state_t;

//=============================================================================
// Device Helper Functions
//=============================================================================

/**
 * @brief Gaussian membership function
 */
__device__ __forceinline__ float mf_gaussian(float x, float c, float sigma) {
    float diff = x - c;
    return expf(-(diff * diff) / (2.0f * sigma * sigma + 1e-10f));
}

/**
 * @brief Gaussian MF derivative w.r.t. center c
 */
__device__ __forceinline__ float mf_gaussian_grad_c(float x, float c, float sigma, float mu) {
    return mu * (x - c) / (sigma * sigma + 1e-10f);
}

/**
 * @brief Gaussian MF derivative w.r.t. sigma
 */
__device__ __forceinline__ float mf_gaussian_grad_sigma(float x, float c, float sigma, float mu) {
    float diff = x - c;
    return mu * diff * diff / (sigma * sigma * sigma + 1e-10f);
}

/**
 * @brief Bell membership function
 */
__device__ __forceinline__ float mf_bell(float x, float a, float b, float c) {
    float term = fabsf((x - c) / (a + 1e-10f));
    return 1.0f / (1.0f + powf(term, 2.0f * b));
}

//=============================================================================
// Forward Pass Kernels
//=============================================================================

/**
 * @brief Layer 1: Fuzzification - compute membership values
 *
 * Output: mu[sample][input][mf] = MF(x[sample][input])
 */
__global__ void kernel_anfis_layer1_forward(
    const float* __restrict__ inputs,       // [batch_size x num_inputs]
    const float* __restrict__ mf_params,    // [num_inputs x num_mfs x 3] (c, sigma, unused)
    float* __restrict__ layer1_out,         // [batch_size x num_inputs x num_mfs]
    uint32_t batch_size,
    uint32_t num_inputs,
    uint32_t num_mfs)
{
    uint32_t sample = blockIdx.x;
    uint32_t input_idx = blockIdx.y;
    uint32_t mf_idx = threadIdx.x;

    if (sample >= batch_size || input_idx >= num_inputs || mf_idx >= num_mfs) return;

    float x = inputs[sample * num_inputs + input_idx];

    // Get MF parameters
    uint32_t param_idx = (input_idx * num_mfs + mf_idx) * 3;
    float c = mf_params[param_idx];
    float sigma = mf_params[param_idx + 1];

    // Compute Gaussian membership
    float mu = mf_gaussian(x, c, sigma);

    // Store output
    uint32_t out_idx = sample * num_inputs * num_mfs + input_idx * num_mfs + mf_idx;
    layer1_out[out_idx] = mu;
}

/**
 * @brief Layer 2: Rule firing strengths (T-norm of antecedents)
 *
 * For simplicity, assume rules are structured as Cartesian product
 * Rule r uses MF[i, r % num_mfs] for each input i
 */
__global__ void kernel_anfis_layer2_forward(
    const float* __restrict__ layer1_out,   // [batch_size x num_inputs x num_mfs]
    float* __restrict__ layer2_out,         // [batch_size x num_rules]
    uint32_t batch_size,
    uint32_t num_inputs,
    uint32_t num_mfs,
    uint32_t num_rules)
{
    uint32_t sample = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t rule = blockIdx.y;

    if (sample >= batch_size || rule >= num_rules) return;

    // Compute rule index in terms of MF indices
    // Rule r corresponds to MF combination (mf_0, mf_1, ..., mf_{n-1})
    // where mf_i = (r / num_mfs^i) % num_mfs

    float firing = 1.0f;
    uint32_t rule_tmp = rule;

    for (uint32_t i = 0; i < num_inputs; i++) {
        uint32_t mf_idx = rule_tmp % num_mfs;
        rule_tmp /= num_mfs;

        uint32_t l1_idx = sample * num_inputs * num_mfs + i * num_mfs + mf_idx;
        firing *= layer1_out[l1_idx];  // Product T-norm
    }

    layer2_out[sample * num_rules + rule] = firing;
}

/**
 * @brief Layer 3: Normalization of firing strengths
 *
 * w_bar[i] = w[i] / sum(w)
 */
__global__ void kernel_anfis_layer3_forward(
    const float* __restrict__ layer2_out,   // [batch_size x num_rules]
    float* __restrict__ layer3_out,         // [batch_size x num_rules]
    uint32_t batch_size,
    uint32_t num_rules)
{
    extern __shared__ float s_sum[];

    uint32_t sample = blockIdx.x;
    uint32_t tid = threadIdx.x;

    if (sample >= batch_size) return;

    // Compute sum of firing strengths
    float sum = 0.0f;
    for (uint32_t r = tid; r < num_rules; r += blockDim.x) {
        sum += layer2_out[sample * num_rules + r];
    }

    s_sum[tid] = sum;
    __syncthreads();

    // Parallel reduction
    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            s_sum[tid] += s_sum[tid + s];
        }
        __syncthreads();
    }

    float total = s_sum[0] + 1e-10f;  // Avoid division by zero
    __syncthreads();

    // Normalize
    for (uint32_t r = tid; r < num_rules; r += blockDim.x) {
        layer3_out[sample * num_rules + r] =
            layer2_out[sample * num_rules + r] / total;
    }
}

/**
 * @brief Layer 4: Consequent computation (Takagi-Sugeno)
 *
 * f[r] = p[r,0] + p[r,1]*x[0] + ... + p[r,n]*x[n-1]
 * out[r] = w_bar[r] * f[r]
 */
__global__ void kernel_anfis_layer4_forward(
    const float* __restrict__ inputs,           // [batch_size x num_inputs]
    const float* __restrict__ layer3_out,       // [batch_size x num_rules]
    const float* __restrict__ consequent_params, // [num_rules x (num_inputs + 1)]
    float* __restrict__ layer4_out,             // [batch_size x num_rules]
    uint32_t batch_size,
    uint32_t num_inputs,
    uint32_t num_rules)
{
    uint32_t sample = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t rule = blockIdx.y;

    if (sample >= batch_size || rule >= num_rules) return;

    // Compute consequent linear function
    const float* p = &consequent_params[rule * (num_inputs + 1)];
    float f = p[0];  // Bias term

    for (uint32_t i = 0; i < num_inputs; i++) {
        f += p[i + 1] * inputs[sample * num_inputs + i];
    }

    // Multiply by normalized firing strength
    float w_bar = layer3_out[sample * num_rules + rule];
    layer4_out[sample * num_rules + rule] = w_bar * f;
}

/**
 * @brief Layer 5: Final output (sum of layer 4)
 */
__global__ void kernel_anfis_layer5_forward(
    const float* __restrict__ layer4_out,   // [batch_size x num_rules]
    float* __restrict__ output,             // [batch_size]
    uint32_t batch_size,
    uint32_t num_rules)
{
    extern __shared__ float s_sum[];

    uint32_t sample = blockIdx.x;
    uint32_t tid = threadIdx.x;

    if (sample >= batch_size) return;

    float sum = 0.0f;
    for (uint32_t r = tid; r < num_rules; r += blockDim.x) {
        sum += layer4_out[sample * num_rules + r];
    }

    s_sum[tid] = sum;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            s_sum[tid] += s_sum[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        output[sample] = s_sum[0];
    }
}

//=============================================================================
// Backward Pass Kernels
//=============================================================================

/**
 * @brief Compute output error
 */
__global__ void kernel_anfis_compute_error(
    const float* __restrict__ predictions,  // [batch_size]
    const float* __restrict__ targets,      // [batch_size]
    float* __restrict__ errors,             // [batch_size]
    float* __restrict__ loss,               // scalar (sum of squared errors)
    uint32_t batch_size)
{
    extern __shared__ float s_loss[];

    uint32_t tid = threadIdx.x;
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;

    float local_loss = 0.0f;
    if (i < batch_size) {
        float err = predictions[i] - targets[i];
        errors[i] = err;
        local_loss = 0.5f * err * err;
    }

    s_loss[tid] = local_loss;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            s_loss[tid] += s_loss[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        atomicAdd(loss, s_loss[0]);
    }
}

/**
 * @brief Backward through Layer 4: gradient w.r.t. consequent parameters
 *
 * dL/dp[r,i] = sum_sample(dL/dy * w_bar[r] * x[i])
 */
__global__ void kernel_anfis_backward_consequent(
    const float* __restrict__ inputs,           // [batch_size x num_inputs]
    const float* __restrict__ layer3_out,       // [batch_size x num_rules]
    const float* __restrict__ errors,           // [batch_size]
    float* __restrict__ grad_consequent,        // [num_rules x (num_inputs + 1)]
    uint32_t batch_size,
    uint32_t num_inputs,
    uint32_t num_rules)
{
    uint32_t rule = blockIdx.x;
    uint32_t param_idx = threadIdx.x;  // 0 = bias, 1..num_inputs = coefficients

    if (rule >= num_rules || param_idx > num_inputs) return;

    float grad = 0.0f;

    for (uint32_t s = 0; s < batch_size; s++) {
        float err = errors[s];
        float w_bar = layer3_out[s * num_rules + rule];

        if (param_idx == 0) {
            // Gradient for bias
            grad += err * w_bar;
        } else {
            // Gradient for coefficient
            float x = inputs[s * num_inputs + (param_idx - 1)];
            grad += err * w_bar * x;
        }
    }

    grad_consequent[rule * (num_inputs + 1) + param_idx] = grad / (float)batch_size;
}

/**
 * @brief Backward through Layers 1-3: gradient w.r.t. MF parameters
 *
 * This is complex due to the chain rule through normalization
 * Simplified version using numerical approximation or full derivation
 */
__global__ void kernel_anfis_backward_premise(
    const float* __restrict__ inputs,           // [batch_size x num_inputs]
    const float* __restrict__ mf_params,        // [num_inputs x num_mfs x 3]
    const float* __restrict__ layer1_out,       // [batch_size x num_inputs x num_mfs]
    const float* __restrict__ layer2_out,       // [batch_size x num_rules]
    const float* __restrict__ layer3_out,       // [batch_size x num_rules]
    const float* __restrict__ consequent_params, // [num_rules x (num_inputs+1)]
    const float* __restrict__ errors,           // [batch_size]
    float* __restrict__ grad_mf_params,         // [num_inputs x num_mfs x 3]
    uint32_t batch_size,
    uint32_t num_inputs,
    uint32_t num_mfs,
    uint32_t num_rules)
{
    uint32_t input_idx = blockIdx.x;
    uint32_t mf_idx = blockIdx.y;
    uint32_t param_type = threadIdx.x;  // 0 = c, 1 = sigma

    if (input_idx >= num_inputs || mf_idx >= num_mfs || param_type >= 2) return;

    uint32_t param_base = (input_idx * num_mfs + mf_idx) * 3;
    float c = mf_params[param_base];
    float sigma = mf_params[param_base + 1];

    float grad = 0.0f;

    for (uint32_t s = 0; s < batch_size; s++) {
        float x = inputs[s * num_inputs + input_idx];
        float mu = layer1_out[s * num_inputs * num_mfs + input_idx * num_mfs + mf_idx];
        float err = errors[s];

        // Compute dmu/dparam
        float dmu_dparam;
        if (param_type == 0) {
            dmu_dparam = mf_gaussian_grad_c(x, c, sigma, mu);
        } else {
            dmu_dparam = mf_gaussian_grad_sigma(x, c, sigma, mu);
        }

        // For each rule that uses this MF
        float dL_dmu = 0.0f;
        uint32_t rule_tmp = 0;

        for (uint32_t r = 0; r < num_rules; r++) {
            // Check if rule r uses mf_idx for input input_idx
            uint32_t rule_check = r;
            uint32_t mf_for_input = 0;
            for (uint32_t i = 0; i <= input_idx; i++) {
                mf_for_input = rule_check % num_mfs;
                rule_check /= num_mfs;
            }

            if (mf_for_input == mf_idx) {
                // This rule uses this MF
                float w = layer2_out[s * num_rules + r];
                float w_bar = layer3_out[s * num_rules + r];

                // Compute consequent value for this rule
                const float* p = &consequent_params[r * (num_inputs + 1)];
                float f = p[0];
                for (uint32_t i = 0; i < num_inputs; i++) {
                    f += p[i + 1] * inputs[s * num_inputs + i];
                }

                // Simplified gradient contribution
                if (mu > 1e-10f) {
                    dL_dmu += err * (w_bar * f / mu) * (w > 1e-10f ? 1.0f : 0.0f);
                }
            }
        }

        grad += dL_dmu * dmu_dparam;
    }

    grad_mf_params[param_base + param_type] = grad / (float)batch_size;
}

/**
 * @brief Update parameters using gradient descent
 */
__global__ void kernel_anfis_update_params(
    float* __restrict__ params,
    const float* __restrict__ gradients,
    float learning_rate,
    uint32_t n)
{
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) {
        params[i] -= learning_rate * gradients[i];
    }
}

//=============================================================================
// Host API Implementation
//=============================================================================

extern "C" {

// Legacy API for backward compatibility - uses raw float pointers
bool nimcp_gpu_anfis_train_raw(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_fuzzy_inference_state_t* state,
    const float* train_inputs,
    const float* train_targets,
    uint32_t num_samples,
    float* out_final_error,
    const nimcp_gpu_anfis_params_t* params)
{
    // Initialize recovery system if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Parameter validation with recovery attempt
    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        nimcp_gpu_recovery_result_t result;
        if (!nimcp_gpu_try_recover(NULL, GPU_ERROR_CONTEXT_INVALID, cudaSuccess, &result)) {
            NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context for ANFIS training");
            return false;
        }
    }
    if (!state || !train_inputs || !train_targets || num_samples == 0) {
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid training data for ANFIS");
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);

    // Get dimensions from state
    uint32_t batch_size = num_samples;
    uint32_t num_inputs = state->num_inputs;
    uint32_t num_mfs = 3;  // Default MFs per input
    uint32_t num_rules = 1;
    for (uint32_t i = 0; i < num_inputs; i++) {
        num_rules *= num_mfs;
    }

    // Use single learning rate, apply different scales for premise vs consequent
    float base_lr = params ? params->learning_rate : 0.01f;
    float lr_premise = base_lr;         // Premise (MF params) learning rate
    float lr_consequent = base_lr * 10.0f;  // Consequent params typically need higher LR

    // Pre-declare all variables before any goto to satisfy C++ rules
    float* d_inputs = NULL;
    float* d_targets = NULL;
    float* d_mf_params = NULL;
    float* d_consequent = NULL;
    float* d_layer1 = NULL;
    float* d_layer2 = NULL;
    float* d_layer3 = NULL;
    float* d_layer4 = NULL;
    float* d_output = NULL;
    float* d_errors = NULL;
    float* d_loss = NULL;
    float* d_grad_mf = NULL;
    float* d_grad_consequent = NULL;
    float* h_mf_params = NULL;
    float* h_consequent = NULL;

    cudaError_t err;

    // Pre-compute sizes before any goto statements (C++ requirement)
    size_t inputs_size = batch_size * num_inputs * sizeof(float);
    size_t mf_params_size = num_inputs * num_mfs * 3 * sizeof(float);
    size_t consequent_size = num_rules * (num_inputs + 1) * sizeof(float);
    size_t layer1_size = batch_size * num_inputs * num_mfs * sizeof(float);
    size_t layer2_size = batch_size * num_rules * sizeof(float);

    float final_loss = 0.0f;
    uint32_t num_epochs_local = params ? params->max_epochs : 1000;

    // Allocate device memory with recovery support
    err = cudaMalloc(&d_inputs, inputs_size);
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &result)) {
            err = cudaMalloc(&d_inputs, inputs_size);
        }
        if (err != cudaSuccess) goto cleanup_anfis;
    }
    err = cudaMalloc(&d_targets, batch_size * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &result)) {
            err = cudaMalloc(&d_targets, batch_size * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_anfis;
    }
    err = cudaMalloc(&d_mf_params, mf_params_size);
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &result)) {
            err = cudaMalloc(&d_mf_params, mf_params_size);
        }
        if (err != cudaSuccess) goto cleanup_anfis;
    }
    err = cudaMalloc(&d_consequent, consequent_size);
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &result)) {
            err = cudaMalloc(&d_consequent, consequent_size);
        }
        if (err != cudaSuccess) goto cleanup_anfis;
    }
    err = cudaMalloc(&d_layer1, layer1_size);
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &result)) {
            err = cudaMalloc(&d_layer1, layer1_size);
        }
        if (err != cudaSuccess) goto cleanup_anfis;
    }
    err = cudaMalloc(&d_layer2, layer2_size);
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &result)) {
            err = cudaMalloc(&d_layer2, layer2_size);
        }
        if (err != cudaSuccess) goto cleanup_anfis;
    }
    err = cudaMalloc(&d_layer3, layer2_size);
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &result)) {
            err = cudaMalloc(&d_layer3, layer2_size);
        }
        if (err != cudaSuccess) goto cleanup_anfis;
    }
    err = cudaMalloc(&d_layer4, layer2_size);
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &result)) {
            err = cudaMalloc(&d_layer4, layer2_size);
        }
        if (err != cudaSuccess) goto cleanup_anfis;
    }
    err = cudaMalloc(&d_output, batch_size * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &result)) {
            err = cudaMalloc(&d_output, batch_size * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_anfis;
    }
    err = cudaMalloc(&d_errors, batch_size * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &result)) {
            err = cudaMalloc(&d_errors, batch_size * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_anfis;
    }
    err = cudaMalloc(&d_loss, sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &result)) {
            err = cudaMalloc(&d_loss, sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_anfis;
    }
    err = cudaMalloc(&d_grad_mf, mf_params_size);
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &result)) {
            err = cudaMalloc(&d_grad_mf, mf_params_size);
        }
        if (err != cudaSuccess) goto cleanup_anfis;
    }
    err = cudaMalloc(&d_grad_consequent, consequent_size);
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &result)) {
            err = cudaMalloc(&d_grad_consequent, consequent_size);
        }
        if (err != cudaSuccess) goto cleanup_anfis;
    }

    // Copy inputs and targets (direct float* pointers from host)
    NIMCP_CUDA_RECOVER(cudaMemcpy(d_inputs, train_inputs, inputs_size, cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpy(d_targets, train_targets, batch_size * sizeof(float),
               cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);

    // Initialize parameters
    {
        h_mf_params = (float*)nimcp_malloc(mf_params_size);
        h_consequent = (float*)nimcp_malloc(consequent_size);

        // Initialize MF params: spread centers evenly, reasonable sigmas
        for (uint32_t i = 0; i < num_inputs; i++) {
            for (uint32_t m = 0; m < num_mfs; m++) {
                uint32_t idx = (i * num_mfs + m) * 3;
                h_mf_params[idx] = (float)m / (float)(num_mfs - 1);  // center
                h_mf_params[idx + 1] = 0.5f / (float)num_mfs;        // sigma
                h_mf_params[idx + 2] = 0.0f;                         // unused
            }
        }

        // Initialize consequent params to small random values
        for (uint32_t r = 0; r < num_rules; r++) {
            for (uint32_t p = 0; p <= num_inputs; p++) {
                h_consequent[r * (num_inputs + 1) + p] = 0.01f * ((float)rand() / RAND_MAX - 0.5f);
            }
        }

        NIMCP_CUDA_RECOVER(cudaMemcpy(d_mf_params, h_mf_params, mf_params_size, cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);
        NIMCP_CUDA_RECOVER(cudaMemcpy(d_consequent, h_consequent, consequent_size, cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);

        nimcp_free(h_mf_params);
        nimcp_free(h_consequent);
    }

    // Training loop
    for (uint32_t epoch = 0; epoch < num_epochs_local; epoch++) {
        // Reset loss
        cudaMemset(d_loss, 0, sizeof(float));

        // Forward pass
        // Layer 1
        dim3 l1_grid(batch_size, num_inputs);
        kernel_anfis_layer1_forward<<<l1_grid, num_mfs, 0, stream>>>(
            d_inputs, d_mf_params, d_layer1, batch_size, num_inputs, num_mfs);

        // Layer 2
        uint32_t l2_block = 256;
        dim3 l2_grid((batch_size + l2_block - 1) / l2_block, num_rules);
        kernel_anfis_layer2_forward<<<l2_grid, l2_block, 0, stream>>>(
            d_layer1, d_layer2, batch_size, num_inputs, num_mfs, num_rules);

        // Layer 3
        kernel_anfis_layer3_forward<<<batch_size, 256, 256 * sizeof(float), stream>>>(
            d_layer2, d_layer3, batch_size, num_rules);

        // Layer 4
        kernel_anfis_layer4_forward<<<l2_grid, l2_block, 0, stream>>>(
            d_inputs, d_layer3, d_consequent, d_layer4, batch_size, num_inputs, num_rules);

        // Layer 5
        kernel_anfis_layer5_forward<<<batch_size, 256, 256 * sizeof(float), stream>>>(
            d_layer4, d_output, batch_size, num_rules);

        // Compute error
        uint32_t err_grid = (batch_size + 255) / 256;
        kernel_anfis_compute_error<<<err_grid, 256, 256 * sizeof(float), stream>>>(
            d_output, d_targets, d_errors, d_loss, batch_size);

        // Backward pass
        // Gradient for consequent params
        kernel_anfis_backward_consequent<<<num_rules, num_inputs + 1, 0, stream>>>(
            d_inputs, d_layer3, d_errors, d_grad_consequent,
            batch_size, num_inputs, num_rules);

        // Gradient for premise params
        dim3 premise_grid(num_inputs, num_mfs);
        kernel_anfis_backward_premise<<<premise_grid, 2, 0, stream>>>(
            d_inputs, d_mf_params, d_layer1, d_layer2, d_layer3,
            d_consequent, d_errors, d_grad_mf,
            batch_size, num_inputs, num_mfs, num_rules);

        // Update parameters
        uint32_t mf_n = num_inputs * num_mfs * 3;
        kernel_anfis_update_params<<<(mf_n + 255) / 256, 256, 0, stream>>>(
            d_mf_params, d_grad_mf, lr_premise, mf_n);

        uint32_t conseq_n = num_rules * (num_inputs + 1);
        kernel_anfis_update_params<<<(conseq_n + 255) / 256, 256, 0, stream>>>(
            d_consequent, d_grad_consequent, lr_consequent, conseq_n);

        // Get loss for monitoring
        if (epoch % 100 == 0 || epoch == num_epochs_local - 1) {
            cudaMemcpy(&final_loss, d_loss, sizeof(float), cudaMemcpyDeviceToHost);
            final_loss /= (float)batch_size;
        }
    }

    if (out_final_error) {
        *out_final_error = final_loss;
    }

    // Cleanup
    cudaFree(d_inputs);
    cudaFree(d_targets);
    cudaFree(d_mf_params);
    cudaFree(d_consequent);
    cudaFree(d_layer1);
    cudaFree(d_layer2);
    cudaFree(d_layer3);
    cudaFree(d_layer4);
    cudaFree(d_output);
    cudaFree(d_errors);
    cudaFree(d_loss);
    cudaFree(d_grad_mf);
    cudaFree(d_grad_consequent);

    return true;

cleanup_anfis:
    cudaFree(d_inputs);
    cudaFree(d_targets);
    cudaFree(d_mf_params);
    cudaFree(d_consequent);
    cudaFree(d_layer1);
    cudaFree(d_layer2);
    cudaFree(d_layer3);
    cudaFree(d_layer4);
    cudaFree(d_output);
    cudaFree(d_errors);
    cudaFree(d_loss);
    cudaFree(d_grad_mf);
    cudaFree(d_grad_consequent);
    NIMCP_THROW_GPU(NIMCP_ERROR_NO_MEMORY, 0, (int)err, "ANFIS GPU memory allocation failed");
    return false;
}

//=============================================================================
// ANFIS Lifecycle and Inference (New API)
//=============================================================================

nimcp_gpu_anfis_state_t* nimcp_gpu_anfis_create(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_anfis_create_params_t* params)
{
    // Initialize recovery system if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Parameter validation - non-recoverable errors (recovery can't fix bad params)
    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context for ANFIS creation");
        return NULL;
    }
    if (!params) {
        NIMCP_THROW_GPU(NIMCP_ERROR_NULL_POINTER, 0, 0, "NULL parameters for ANFIS creation");
        return NULL;
    }
    if (params->num_inputs == 0 || params->num_mfs_per_input == 0) {
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0,
            "Invalid ANFIS config: num_inputs=%u, num_mfs=%u",
            params->num_inputs, params->num_mfs_per_input);
        return NULL;
    }

    anfis_gpu_state_t* state = (anfis_gpu_state_t*)nimcp_calloc(1, sizeof(anfis_gpu_state_t));
    if (!state) {
        NIMCP_THROW_GPU(NIMCP_ERROR_NO_MEMORY, 0, 0, "Failed to allocate ANFIS state structure");
        return NULL;
    }

    state->num_inputs = params->num_inputs;
    state->num_outputs = params->num_outputs > 0 ? params->num_outputs : 1;
    state->num_mfs_per_input = params->num_mfs_per_input;
    state->num_rules = 1;
    for (uint32_t i = 0; i < params->num_inputs; i++) {
        state->num_rules *= params->num_mfs_per_input;
    }

    state->learning_rate_premise = params->learning_rate;
    state->learning_rate_consequent = params->learning_rate;

    // Allocate device memory for MF parameters (each MF has 3 params: c, sigma, height)
    uint32_t mf_param_count = params->num_inputs * params->num_mfs_per_input * 3;
    cudaError_t err = cudaMalloc(&state->d_mf_params, mf_param_count * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &result)) {
            err = cudaMalloc(&state->d_mf_params, mf_param_count * sizeof(float));
        }
        if (err != cudaSuccess) {
            NIMCP_THROW_GPU(NIMCP_ERROR_NO_MEMORY, 0, (int)err,
                "Failed to allocate ANFIS MF params: %s", cudaGetErrorString(err));
            nimcp_free(state);
            return NULL;
        }
    }

    // Initialize MF params with reasonable defaults
    float* h_mf_params = (float*)nimcp_malloc(mf_param_count * sizeof(float));
    if (!h_mf_params) {
        NIMCP_THROW_GPU(NIMCP_ERROR_NO_MEMORY, 0, 0, "Failed to allocate host MF params buffer");
        cudaFree(state->d_mf_params);
        nimcp_free(state);
        return NULL;
    }

    for (uint32_t inp = 0; inp < params->num_inputs; inp++) {
        for (uint32_t mf = 0; mf < params->num_mfs_per_input; mf++) {
            uint32_t idx = (inp * params->num_mfs_per_input + mf) * 3;
            // Spread centers evenly across [0,1]
            h_mf_params[idx + 0] = (float)mf / (params->num_mfs_per_input - 1 + 1e-6f);  // center
            h_mf_params[idx + 1] = 0.5f / params->num_mfs_per_input;  // sigma
            h_mf_params[idx + 2] = 1.0f;  // height
        }
    }
    NIMCP_CUDA_RECOVER_NULL(cudaMemcpy(state->d_mf_params, h_mf_params, mf_param_count * sizeof(float),
               cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);
    nimcp_free(h_mf_params);

    // Allocate consequent params (Sugeno: num_rules * (num_inputs + 1))
    uint32_t cons_param_count = state->num_rules * (params->num_inputs + 1);
    err = cudaMalloc(&state->d_consequent_params, cons_param_count * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &result)) {
            err = cudaMalloc(&state->d_consequent_params, cons_param_count * sizeof(float));
        }
        if (err != cudaSuccess) {
            NIMCP_THROW_GPU(NIMCP_ERROR_NO_MEMORY, 0, (int)err,
                "Failed to allocate ANFIS consequent params: %s", cudaGetErrorString(err));
            cudaFree(state->d_mf_params);
            nimcp_free(state);
            return NULL;
        }
    }

    // Initialize consequent params to small random values
    float* h_cons = (float*)nimcp_malloc(cons_param_count * sizeof(float));
    if (!h_cons) {
        NIMCP_THROW_GPU(NIMCP_ERROR_NO_MEMORY, 0, 0, "Failed to allocate host consequent buffer");
        cudaFree(state->d_mf_params);
        cudaFree(state->d_consequent_params);
        nimcp_free(state);
        return NULL;
    }

    for (uint32_t i = 0; i < cons_param_count; i++) {
        h_cons[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
    }
    NIMCP_CUDA_RECOVER_NULL(cudaMemcpy(state->d_consequent_params, h_cons, cons_param_count * sizeof(float),
               cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);
    nimcp_free(h_cons);

    state->initialized = true;
    return (nimcp_gpu_anfis_state_t*)state;
}

void nimcp_gpu_anfis_destroy(nimcp_gpu_anfis_state_t* anfis_pub)
{
    if (!anfis_pub) return;
    anfis_gpu_state_t* anfis = (anfis_gpu_state_t*)anfis_pub;

    cudaFree(anfis->d_mf_params);
    cudaFree(anfis->d_consequent_params);
    cudaFree(anfis->d_layer1_out);
    cudaFree(anfis->d_layer2_out);
    cudaFree(anfis->d_layer3_out);
    cudaFree(anfis->d_layer4_out);
    cudaFree(anfis->d_layer5_out);
    cudaFree(anfis->d_grad_mf_params);
    cudaFree(anfis->d_grad_consequent);

    nimcp_free(anfis);
}

bool nimcp_gpu_anfis_train(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_anfis_state_t* anfis_pub,
    const nimcp_gpu_tensor_t* train_inputs,
    const nimcp_gpu_tensor_t* train_targets,
    const nimcp_gpu_anfis_train_params_t* params,
    float* out_initial_error,
    float* out_final_error)
{
    // Initialize recovery system if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Parameter validation with recovery attempt
    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        nimcp_gpu_recovery_result_t result;
        if (!nimcp_gpu_try_recover(NULL, GPU_ERROR_CONTEXT_INVALID, cudaSuccess, &result)) {
            NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context for ANFIS training");
            return false;
        }
    }
    anfis_gpu_state_t* anfis = (anfis_gpu_state_t*)anfis_pub;
    if (!anfis || !anfis->initialized) {
        nimcp_gpu_recovery_result_t result;
        if (!nimcp_gpu_try_recover(NULL, GPU_ERROR_INVALID_PARAMS, cudaSuccess, &result)) {
            NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_STATE, 0, 0, "Invalid or uninitialized ANFIS state");
            return false;
        }
    }
    if (!train_inputs || !train_targets) {
        NIMCP_THROW_GPU(NIMCP_ERROR_NULL_POINTER, 0, 0, "NULL training tensors for ANFIS");
        return false;
    }
    if (!params) {
        NIMCP_THROW_GPU(NIMCP_ERROR_NULL_POINTER, 0, 0, "NULL training parameters for ANFIS");
        return false;
    }
    if (train_inputs->ndim < 2) {
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0,
            "ANFIS train_inputs must be 2D, got %u dimensions", train_inputs->ndim);
        return false;
    }

    // Get tensor dimensions from struct fields
    uint32_t num_samples = (uint32_t)train_inputs->dims[0];
    uint32_t num_inputs_t = (uint32_t)train_inputs->dims[1];

    if (num_inputs_t != anfis->num_inputs) {
        NIMCP_THROW_GPU(NIMCP_ERROR_DIMENSION_MISMATCH, 0, 0,
            "ANFIS input dimension mismatch: expected %u, got %u",
            anfis->num_inputs, num_inputs_t);
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    uint32_t batch_size = num_samples;
    uint32_t num_mfs = anfis->num_mfs_per_input;
    uint32_t num_rules = anfis->num_rules;
    uint32_t num_inputs = anfis->num_inputs;

    // Allocate workspace for training
    float* d_layer1 = NULL;
    float* d_layer2 = NULL;
    float* d_layer3 = NULL;
    float* d_layer4 = NULL;
    float* d_output = NULL;
    float* d_errors = NULL;
    float* d_loss = NULL;
    float* d_grad_mf = NULL;
    float* d_grad_consequent = NULL;

    cudaError_t cerr;
    size_t layer1_size = (size_t)batch_size * num_inputs * num_mfs * sizeof(float);
    size_t layer2_size = (size_t)batch_size * num_rules * sizeof(float);
    uint32_t mf_param_count = num_inputs * num_mfs * 3;
    uint32_t cons_param_count = num_rules * (num_inputs + 1);

    // Pre-declare all variables before goto (C++ requirement)
    float initial_error = 0.0f;
    float final_error = 0.0f;
    float lr_premise = anfis->learning_rate_premise;
    float lr_consequent = anfis->learning_rate_consequent * 10.0f;
    uint32_t l2_block = 256;
    uint32_t err_grid = (batch_size + 255) / 256;

    cerr = cudaMalloc(&d_layer1, layer1_size);
    if (cerr != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, cerr, &result)) {
            cerr = cudaMalloc(&d_layer1, layer1_size);
        }
        if (cerr != cudaSuccess) goto train_cleanup;
    }
    cerr = cudaMalloc(&d_layer2, layer2_size);
    if (cerr != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, cerr, &result)) {
            cerr = cudaMalloc(&d_layer2, layer2_size);
        }
        if (cerr != cudaSuccess) goto train_cleanup;
    }
    cerr = cudaMalloc(&d_layer3, layer2_size);
    if (cerr != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, cerr, &result)) {
            cerr = cudaMalloc(&d_layer3, layer2_size);
        }
        if (cerr != cudaSuccess) goto train_cleanup;
    }
    cerr = cudaMalloc(&d_layer4, layer2_size);
    if (cerr != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, cerr, &result)) {
            cerr = cudaMalloc(&d_layer4, layer2_size);
        }
        if (cerr != cudaSuccess) goto train_cleanup;
    }
    cerr = cudaMalloc(&d_output, batch_size * sizeof(float));
    if (cerr != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, cerr, &result)) {
            cerr = cudaMalloc(&d_output, batch_size * sizeof(float));
        }
        if (cerr != cudaSuccess) goto train_cleanup;
    }
    cerr = cudaMalloc(&d_errors, batch_size * sizeof(float));
    if (cerr != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, cerr, &result)) {
            cerr = cudaMalloc(&d_errors, batch_size * sizeof(float));
        }
        if (cerr != cudaSuccess) goto train_cleanup;
    }
    cerr = cudaMalloc(&d_loss, sizeof(float));
    if (cerr != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, cerr, &result)) {
            cerr = cudaMalloc(&d_loss, sizeof(float));
        }
        if (cerr != cudaSuccess) goto train_cleanup;
    }
    cerr = cudaMalloc(&d_grad_mf, mf_param_count * sizeof(float));
    if (cerr != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, cerr, &result)) {
            cerr = cudaMalloc(&d_grad_mf, mf_param_count * sizeof(float));
        }
        if (cerr != cudaSuccess) goto train_cleanup;
    }
    cerr = cudaMalloc(&d_grad_consequent, cons_param_count * sizeof(float));
    if (cerr != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, cerr, &result)) {
            cerr = cudaMalloc(&d_grad_consequent, cons_param_count * sizeof(float));
        }
        if (cerr != cudaSuccess) goto train_cleanup;
    }

    // Training loop using actual CUDA kernels
    for (uint32_t epoch = 0; epoch < params->num_epochs; epoch++) {
        cudaMemset(d_loss, 0, sizeof(float));

        // Forward pass
        {
        dim3 l1_grid(batch_size, num_inputs);
        kernel_anfis_layer1_forward<<<l1_grid, num_mfs, 0, stream>>>(
            (const float*)train_inputs->data, anfis->d_mf_params, d_layer1,
            batch_size, num_inputs, num_mfs);
        }

        {
        dim3 l2_grid((batch_size + l2_block - 1) / l2_block, num_rules);
        kernel_anfis_layer2_forward<<<l2_grid, l2_block, 0, stream>>>(
            d_layer1, d_layer2, batch_size, num_inputs, num_mfs, num_rules);

        kernel_anfis_layer3_forward<<<batch_size, 256, 256 * sizeof(float), stream>>>(
            d_layer2, d_layer3, batch_size, num_rules);

        kernel_anfis_layer4_forward<<<l2_grid, l2_block, 0, stream>>>(
            (const float*)train_inputs->data, d_layer3, anfis->d_consequent_params,
            d_layer4, batch_size, num_inputs, num_rules);

        kernel_anfis_layer5_forward<<<batch_size, 256, 256 * sizeof(float), stream>>>(
            d_layer4, d_output, batch_size, num_rules);

        // Compute error
        uint32_t err_grid = (batch_size + 255) / 256;
        kernel_anfis_compute_error<<<err_grid, 256, 256 * sizeof(float), stream>>>(
            d_output, (const float*)train_targets->data, d_errors, d_loss, batch_size);

        // Backward pass
        kernel_anfis_backward_consequent<<<num_rules, num_inputs + 1, 0, stream>>>(
            (const float*)train_inputs->data, d_layer3, d_errors, d_grad_consequent,
            batch_size, num_inputs, num_rules);

        dim3 premise_grid(num_inputs, num_mfs);
        kernel_anfis_backward_premise<<<premise_grid, 2, 0, stream>>>(
            (const float*)train_inputs->data, anfis->d_mf_params, d_layer1, d_layer2, d_layer3,
            anfis->d_consequent_params, d_errors, d_grad_mf,
            batch_size, num_inputs, num_mfs, num_rules);

        // Update parameters
        kernel_anfis_update_params<<<(mf_param_count + 255) / 256, 256, 0, stream>>>(
            anfis->d_mf_params, d_grad_mf, lr_premise, mf_param_count);

        kernel_anfis_update_params<<<(cons_param_count + 255) / 256, 256, 0, stream>>>(
            anfis->d_consequent_params, d_grad_consequent, lr_consequent, cons_param_count);

        // Record loss
        if (epoch == 0 || epoch == params->num_epochs - 1 || epoch % 100 == 0) {
            cudaMemcpy(&final_error, d_loss, sizeof(float), cudaMemcpyDeviceToHost);
            final_error /= (float)batch_size;
            if (epoch == 0) initial_error = final_error;
        }

        if (final_error < params->early_stop_threshold) {
            break;
        }
        }  // Close block for l2_grid scope
    }

    if (out_initial_error) *out_initial_error = initial_error;
    if (out_final_error) *out_final_error = final_error;

    // Cleanup workspace
    cudaFree(d_layer1);
    cudaFree(d_layer2);
    cudaFree(d_layer3);
    cudaFree(d_layer4);
    cudaFree(d_output);
    cudaFree(d_errors);
    cudaFree(d_loss);
    cudaFree(d_grad_mf);
    cudaFree(d_grad_consequent);
    return true;

train_cleanup:
    cudaFree(d_layer1);
    cudaFree(d_layer2);
    cudaFree(d_layer3);
    cudaFree(d_layer4);
    cudaFree(d_output);
    cudaFree(d_errors);
    cudaFree(d_loss);
    cudaFree(d_grad_mf);
    cudaFree(d_grad_consequent);
    NIMCP_THROW_GPU(NIMCP_ERROR_NO_MEMORY, 0, (int)cerr,
        "Failed to allocate ANFIS training workspace: %s", cudaGetErrorString(cerr));
    return false;
}

bool nimcp_gpu_anfis_forward(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_anfis_state_t* anfis_pub,
    const nimcp_gpu_tensor_t* inputs,
    nimcp_gpu_tensor_t* outputs)
{
    // Initialize recovery system if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Parameter validation with recovery attempt
    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        nimcp_gpu_recovery_result_t result;
        if (!nimcp_gpu_try_recover(NULL, GPU_ERROR_CONTEXT_INVALID, cudaSuccess, &result)) {
            NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context for ANFIS forward");
            return false;
        }
    }
    anfis_gpu_state_t* anfis = (anfis_gpu_state_t*)anfis_pub;
    if (!anfis || !anfis->initialized) {
        nimcp_gpu_recovery_result_t result;
        if (!nimcp_gpu_try_recover(NULL, GPU_ERROR_INVALID_PARAMS, cudaSuccess, &result)) {
            NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_STATE, 0, 0, "Invalid or uninitialized ANFIS state");
            return false;
        }
    }
    if (!inputs || !outputs) {
        NIMCP_THROW_GPU(NIMCP_ERROR_NULL_POINTER, 0, 0, "NULL tensor for ANFIS forward");
        return false;
    }
    if (inputs->ndim < 2) {
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0,
            "ANFIS inputs must be 2D, got %u dimensions", inputs->ndim);
        return false;
    }

    // Get dimensions from struct fields
    uint32_t num_samples = (uint32_t)inputs->dims[0];
    uint32_t num_inputs_t = (uint32_t)inputs->dims[1];

    if (num_inputs_t != anfis->num_inputs) {
        NIMCP_THROW_GPU(NIMCP_ERROR_DIMENSION_MISMATCH, 0, 0,
            "ANFIS input dimension mismatch: expected %u, got %u",
            anfis->num_inputs, num_inputs_t);
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    uint32_t batch_size = num_samples;
    uint32_t num_mfs = anfis->num_mfs_per_input;
    uint32_t num_rules = anfis->num_rules;
    uint32_t num_inputs = anfis->num_inputs;

    // Allocate workspace for forward pass
    float* d_layer1 = NULL;
    float* d_layer2 = NULL;
    float* d_layer3 = NULL;
    float* d_layer4 = NULL;

    cudaError_t cerr;
    size_t layer1_size = (size_t)batch_size * num_inputs * num_mfs * sizeof(float);
    size_t layer2_size = (size_t)batch_size * num_rules * sizeof(float);

    // Pre-declare all variables before goto (CUDA/C++ requirement)
    dim3 l1_grid;
    uint32_t l2_block = 256;
    dim3 l2_grid;

    cerr = cudaMalloc(&d_layer1, layer1_size);
    if (cerr != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, cerr, &result)) {
            cerr = cudaMalloc(&d_layer1, layer1_size);
        }
        if (cerr != cudaSuccess) goto forward_cleanup;
    }
    cerr = cudaMalloc(&d_layer2, layer2_size);
    if (cerr != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, cerr, &result)) {
            cerr = cudaMalloc(&d_layer2, layer2_size);
        }
        if (cerr != cudaSuccess) goto forward_cleanup;
    }
    cerr = cudaMalloc(&d_layer3, layer2_size);
    if (cerr != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, cerr, &result)) {
            cerr = cudaMalloc(&d_layer3, layer2_size);
        }
        if (cerr != cudaSuccess) goto forward_cleanup;
    }
    cerr = cudaMalloc(&d_layer4, layer2_size);
    if (cerr != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, cerr, &result)) {
            cerr = cudaMalloc(&d_layer4, layer2_size);
        }
        if (cerr != cudaSuccess) goto forward_cleanup;
    }

    // Forward pass using CUDA kernels
    l1_grid = dim3(batch_size, num_inputs);
    kernel_anfis_layer1_forward<<<l1_grid, num_mfs, 0, stream>>>(
        (const float*)inputs->data, anfis->d_mf_params, d_layer1,
        batch_size, num_inputs, num_mfs);

    l2_grid = dim3((batch_size + l2_block - 1) / l2_block, num_rules);
    kernel_anfis_layer2_forward<<<l2_grid, l2_block, 0, stream>>>(
        d_layer1, d_layer2, batch_size, num_inputs, num_mfs, num_rules);

    kernel_anfis_layer3_forward<<<batch_size, 256, 256 * sizeof(float), stream>>>(
        d_layer2, d_layer3, batch_size, num_rules);

    kernel_anfis_layer4_forward<<<l2_grid, l2_block, 0, stream>>>(
        (const float*)inputs->data, d_layer3, anfis->d_consequent_params,
        d_layer4, batch_size, num_inputs, num_rules);

    // Layer 5: final output directly to outputs tensor
    kernel_anfis_layer5_forward<<<batch_size, 256, 256 * sizeof(float), stream>>>(
        d_layer4, (float*)outputs->data, batch_size, num_rules);

    // Synchronize to ensure completion
    cerr = cudaStreamSynchronize(stream);
    if (cerr != cudaSuccess) {
        NIMCP_THROW_GPU(NIMCP_ERROR_GPU_SYNC, 0, (int)cerr,
            "ANFIS forward CUDA synchronization failed: %s", cudaGetErrorString(cerr));
        goto forward_cleanup;
    }

    // Cleanup workspace
    cudaFree(d_layer1);
    cudaFree(d_layer2);
    cudaFree(d_layer3);
    cudaFree(d_layer4);
    return true;

forward_cleanup:
    cudaFree(d_layer1);
    cudaFree(d_layer2);
    cudaFree(d_layer3);
    cudaFree(d_layer4);
    NIMCP_THROW_GPU(NIMCP_ERROR_NO_MEMORY, 0, (int)cerr,
        "Failed to allocate ANFIS forward workspace: %s", cudaGetErrorString(cerr));
    return false;
}

const char* nimcp_gpu_anfis_get_last_error(void) {
    return g_anfis_error;
}

} // extern "C"

#else // !NIMCP_ENABLE_CUDA

//=============================================================================
// CPU Fallback Stubs
//=============================================================================

#include "gpu/fuzzy/nimcp_fuzzy_anfis_gpu.h"

extern "C" {

nimcp_gpu_anfis_state_t* nimcp_gpu_anfis_create(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_anfis_create_params_t* params)
{
    (void)ctx; (void)params;
    return NULL;
}

void nimcp_gpu_anfis_destroy(nimcp_gpu_anfis_state_t* anfis)
{
    (void)anfis;
}

bool nimcp_gpu_anfis_train(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_anfis_state_t* anfis,
    const nimcp_gpu_tensor_t* train_inputs,
    const nimcp_gpu_tensor_t* train_targets,
    const nimcp_gpu_anfis_train_params_t* params,
    float* out_initial_error,
    float* out_final_error)
{
    (void)ctx; (void)anfis; (void)train_inputs; (void)train_targets;
    (void)params; (void)out_initial_error; (void)out_final_error;
    return false;
}

bool nimcp_gpu_anfis_forward(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_anfis_state_t* anfis,
    const nimcp_gpu_tensor_t* inputs,
    nimcp_gpu_tensor_t* outputs)
{
    (void)ctx; (void)anfis; (void)inputs; (void)outputs;
    return false;
}

const char* nimcp_gpu_anfis_get_last_error(void) {
    return "GPU support not compiled";
}

} // extern "C"

#endif // NIMCP_ENABLE_CUDA
