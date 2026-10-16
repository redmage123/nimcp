/**
 * @file nimcp_lnn_ode_kernels.cu
 * @brief GPU LNN ODE Integration CUDA Kernels
 *
 * WHAT: Advanced CUDA kernels for batched ODE integration in LNN
 * WHY:  GPU parallelization across batch, neurons, and time for massive speedup
 * HOW:  Specialized kernels for Euler, RK4, RK45 (adaptive), reservoir computing
 *
 * RNG USAGE:
 * ==========
 * Inline curand_init is used for stochastic weight initialization.
 * For general GPU statistics RNG, see: gpu/statistics/nimcp_statistics_gpu.h
 * For shared device RNG functions, see: gpu/common/nimcp_device_utils.cuh
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

// Include CUDA headers FIRST (before any extern "C" blocks from our headers)
#include "utils/memory/nimcp_memory.h"
#include <cuda_runtime.h>
#include <curand_kernel.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>

// Now include our headers (which have extern "C" blocks)
#include "gpu/lnn/nimcp_lnn_ode_gpu.h"
#include "gpu/lnn/nimcp_lnn_gpu.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"

#define LOG_MODULE "LNN_ODE_GPU"

#define BLOCK_SIZE 256
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)
#define WARP_SIZE 32

//=============================================================================
// Activation Function Device Functions
//=============================================================================

__device__ inline float device_tanh(float x)
{
    return tanhf(x);
}

__device__ inline float device_sigmoid(float x)
{
    return 1.0f / (1.0f + expf(-x));
}

__device__ inline float device_relu(float x)
{
    return fmaxf(0.0f, x);
}

__device__ inline float device_gelu(float x)
{
    const float sqrt_2_over_pi = 0.7978845608f;
    float cdf = 0.5f * (1.0f + tanhf(sqrt_2_over_pi * (x + 0.044715f * x * x * x)));
    return x * cdf;
}

__device__ inline float device_silu(float x)
{
    return x * device_sigmoid(x);
}

__device__ inline float device_softplus(float x)
{
    return logf(1.0f + expf(x));
}

__device__ inline float device_activation(float x, int activation_type)
{
    switch (activation_type) {
        case 0: return device_tanh(x);      // LNN_ACTIVATION_TANH
        case 1: return device_sigmoid(x);   // LNN_ACTIVATION_SIGMOID
        case 2: return device_relu(x);      // LNN_ACTIVATION_RELU
        case 3: return device_gelu(x);      // LNN_ACTIVATION_GELU
        case 4: return device_silu(x);      // LNN_ACTIVATION_SILU
        case 5: return device_softplus(x);  // LNN_ACTIVATION_SOFTPLUS
        default: return device_tanh(x);
    }
}

//=============================================================================
// Default Configuration
//=============================================================================

nimcp_lnn_ode_batch_config_t nimcp_lnn_ode_batch_default_config(void)
{
    nimcp_lnn_ode_batch_config_t config;
    config.method = LNN_ODE_RK4;
    config.dt = 1.0f;
    config.dt_min = 0.01f;
    config.dt_max = 10.0f;
    config.error_tolerance = 1e-5f;
    config.relative_tolerance = 1e-4f;
    config.max_steps = 1000;
    config.num_substeps = 1;
    config.adaptive_stepping = false;
    config.use_checkpoint = false;
    config.enable_stability_check = true;
    config.stability_threshold = 1e6f;
    return config;
}

//=============================================================================
// Batched Euler Step Kernel
//=============================================================================

/**
 * @brief Batched Euler step: x_new = x + dt * dx_dt for all (batch, neuron) pairs
 *
 * Grid: (batch_size, neurons / BLOCK_SIZE)
 * Block: (BLOCK_SIZE)
 */
__global__ void kernel_euler_step_batched(
    const float* __restrict__ x,
    const float* __restrict__ dx_dt,
    float* __restrict__ x_new,
    float dt,
    uint32_t batch_size,
    uint32_t n_neurons)
{
    uint32_t batch = blockIdx.x;
    uint32_t neuron = blockIdx.y * blockDim.x + threadIdx.x;

    if (batch >= batch_size || neuron >= n_neurons) return;

    uint32_t idx = batch * n_neurons + neuron;
    x_new[idx] = x[idx] + dt * dx_dt[idx];
}

bool nimcp_gpu_lnn_euler_step_batched(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_ode_batch_state_t* batch_state,
    float dt)
{
    if (!ctx || !batch_state || !batch_state->x || !batch_state->dx_dt) {
        LOG_ERROR("Invalid parameters for batched Euler step");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    uint32_t batch_size = batch_state->batch_size;
    uint32_t n_neurons = batch_state->n_neurons;

    dim3 grid(batch_size, GRID_SIZE(n_neurons));
    dim3 block(BLOCK_SIZE);

    kernel_euler_step_batched<<<grid, block>>>(
        (const float*)batch_state->x->data,
        (const float*)batch_state->dx_dt->data,
        (float*)batch_state->x->data,  // In-place update
        dt, batch_size, n_neurons);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    batch_state->current_time += dt;

    return true;
}

//=============================================================================
// Batched LTC Derivative Computation
//=============================================================================

/**
 * @brief Compute LTC derivative for batched input: dx/dt = -x/tau + f(W*input + W_rec*x + b)
 *
 * Grid: (batch_size, neurons / BLOCK_SIZE)
 */
__global__ void kernel_compute_ltc_derivative_batched(
    const float* __restrict__ x,          // [batch, n_neurons]
    const float* __restrict__ input,      // [batch, n_inputs]
    const float* __restrict__ tau,        // [batch, n_neurons]
    const float* __restrict__ W_in,       // [n_neurons, n_inputs]
    const float* __restrict__ W_rec,      // [n_neurons, n_neurons]
    const float* __restrict__ b_in,       // [n_neurons]
    float* __restrict__ dx_dt,            // [batch, n_neurons]
    uint32_t batch_size,
    uint32_t n_neurons,
    uint32_t n_inputs,
    int activation_type)
{
    uint32_t batch = blockIdx.x;
    uint32_t neuron = blockIdx.y * blockDim.x + threadIdx.x;

    if (batch >= batch_size || neuron >= n_neurons) return;

    uint32_t batch_neuron_idx = batch * n_neurons + neuron;

    // Compute weighted sum: W_in * input + W_rec * x + b
    float sum = b_in[neuron];

    // Input contribution
    for (uint32_t j = 0; j < n_inputs; j++) {
        sum += W_in[neuron * n_inputs + j] * input[batch * n_inputs + j];
    }

    // Recurrent contribution
    for (uint32_t j = 0; j < n_neurons; j++) {
        sum += W_rec[neuron * n_neurons + j] * x[batch * n_neurons + j];
    }

    // Apply activation
    float activated = device_activation(sum, activation_type);

    // LTC dynamics: dx/dt = -x/tau + f(sum)
    dx_dt[batch_neuron_idx] = -x[batch_neuron_idx] / tau[batch_neuron_idx] + activated;
}

bool nimcp_gpu_lnn_compute_ltc_derivative_batched(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* input,
    nimcp_lnn_ode_batch_state_t* batch_state,
    lnn_activation_t activation)
{
    if (!ctx || !layer || !input || !batch_state) {
        LOG_ERROR("Invalid parameters for batched LTC derivative");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    uint32_t batch_size = batch_state->batch_size;
    uint32_t n_neurons = batch_state->n_neurons;
    uint32_t n_inputs = layer->n_inputs;

    dim3 grid(batch_size, GRID_SIZE(n_neurons));
    dim3 block(BLOCK_SIZE);

    kernel_compute_ltc_derivative_batched<<<grid, block>>>(
        (const float*)batch_state->x->data,
        (const float*)input->data,
        (const float*)batch_state->tau->data,
        (const float*)layer->W_in->data,
        (const float*)layer->W_rec->data,
        (const float*)layer->b_in->data,
        (float*)batch_state->dx_dt->data,
        batch_size, n_neurons, n_inputs,
        (int)activation);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Batched Tau Update with Stability Constraints
//=============================================================================

/**
 * @brief Update tau with stability constraints: tau = tau_base * sigmoid(W_tau * [x; input] + b_tau)
 */
__global__ void kernel_update_tau_batched(
    const float* __restrict__ x,          // [batch, n_neurons]
    const float* __restrict__ input,      // [batch, n_inputs]
    const float* __restrict__ W_tau,      // [n_neurons, n_inputs + n_neurons]
    const float* __restrict__ b_tau,      // [n_neurons]
    const float* __restrict__ tau_base,   // [n_neurons]
    float* __restrict__ tau,              // [batch, n_neurons]
    float tau_min,
    float tau_max,
    uint32_t batch_size,
    uint32_t n_neurons,
    uint32_t n_inputs)
{
    uint32_t batch = blockIdx.x;
    uint32_t neuron = blockIdx.y * blockDim.x + threadIdx.x;

    if (batch >= batch_size || neuron >= n_neurons) return;

    uint32_t batch_neuron_idx = batch * n_neurons + neuron;
    uint32_t tau_width = n_inputs + n_neurons;

    // Compute W_tau * [x; input] + b_tau
    float sum = b_tau[neuron];

    // Input contribution
    for (uint32_t j = 0; j < n_inputs; j++) {
        sum += W_tau[neuron * tau_width + j] * input[batch * n_inputs + j];
    }

    // State contribution
    for (uint32_t j = 0; j < n_neurons; j++) {
        sum += W_tau[neuron * tau_width + n_inputs + j] * x[batch * n_neurons + j];
    }

    // tau = tau_base * sigmoid(sum), clamped to [tau_min, tau_max]
    float tau_val = tau_base[neuron] * device_sigmoid(sum);
    tau_val = fmaxf(tau_min, fminf(tau_max, tau_val));
    tau[batch_neuron_idx] = tau_val;
}

bool nimcp_gpu_lnn_update_tau_batched(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* input,
    nimcp_lnn_ode_batch_state_t* batch_state,
    float tau_min,
    float tau_max)
{
    if (!ctx || !layer || !input || !batch_state) {
        LOG_ERROR("Invalid parameters for batched tau update");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    uint32_t batch_size = batch_state->batch_size;
    uint32_t n_neurons = batch_state->n_neurons;
    uint32_t n_inputs = layer->n_inputs;

    dim3 grid(batch_size, GRID_SIZE(n_neurons));
    dim3 block(BLOCK_SIZE);

    kernel_update_tau_batched<<<grid, block>>>(
        (const float*)batch_state->x->data,
        (const float*)input->data,
        (const float*)layer->W_tau->data,
        (const float*)layer->b_tau->data,
        (const float*)layer->tau_base->data,
        (float*)batch_state->tau->data,
        tau_min, tau_max,
        batch_size, n_neurons, n_inputs);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Batched RK4 Step with Fused Operations
//=============================================================================

/**
 * @brief Fused kernel: add scaled vector y = a + scale * b
 */
__global__ void kernel_add_scaled_batched(
    const float* __restrict__ a,
    const float* __restrict__ b,
    float scale,
    float* __restrict__ y,
    uint32_t n)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        y[idx] = a[idx] + scale * b[idx];
    }
}

/**
 * @brief RK4 final combination: x_new = x + (dt/6) * (k1 + 2*k2 + 2*k3 + k4)
 */
__global__ void kernel_rk4_combine_batched(
    const float* __restrict__ x,
    const float* __restrict__ k1,
    const float* __restrict__ k2,
    const float* __restrict__ k3,
    const float* __restrict__ k4,
    float dt,
    float* __restrict__ x_new,
    uint32_t n)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        x_new[idx] = x[idx] + (dt / 6.0f) * (k1[idx] + 2.0f * k2[idx] + 2.0f * k3[idx] + k4[idx]);
    }
}

bool nimcp_gpu_lnn_rk4_step_batched(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* input,
    nimcp_lnn_ode_batch_state_t* batch_state,
    nimcp_lnn_ode_cache_t* cache,
    const nimcp_lnn_ode_batch_config_t* config)
{
    if (!ctx || !layer || !input || !batch_state || !cache || !config) {
        LOG_ERROR("Invalid parameters for batched RK4 step");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (cache->n_stages < 4) {
        LOG_ERROR("RK4 requires at least 4 cache stages, got %u", cache->n_stages);
        return false;
    }

    float dt = config->dt;
    uint32_t n = batch_state->batch_size * batch_state->n_neurons;

    nimcp_gpu_tensor_t* k1 = cache->k_stages[0];
    nimcp_gpu_tensor_t* k2 = cache->k_stages[1];
    nimcp_gpu_tensor_t* k3 = cache->k_stages[2];
    nimcp_gpu_tensor_t* k4 = cache->k_stages[3];
    nimcp_gpu_tensor_t* x_temp = cache->x_temp;

    // Save original state
    if (cudaMemcpy(cache->x_checkpoint->data, batch_state->x->data,
                          n * sizeof(float), cudaMemcpyDeviceToDevice) != cudaSuccess) return false;

    // k1 = f(t, x)
    nimcp_gpu_lnn_update_tau_batched(ctx, layer, input, batch_state, LNN_TAU_MIN_DEFAULT, LNN_TAU_MAX_DEFAULT);
    nimcp_gpu_lnn_compute_ltc_derivative_batched(ctx, layer, input, batch_state, layer->activation);
    if (cudaMemcpy(k1->data, batch_state->dx_dt->data, n * sizeof(float), cudaMemcpyDeviceToDevice) != cudaSuccess) return false;

    // x_temp = x + 0.5 * dt * k1
    kernel_add_scaled_batched<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (const float*)cache->x_checkpoint->data, (const float*)k1->data,
        0.5f * dt, (float*)x_temp->data, n);
    if (cudaMemcpy(batch_state->x->data, x_temp->data, n * sizeof(float), cudaMemcpyDeviceToDevice) != cudaSuccess) return false;

    // k2 = f(t + dt/2, x + dt/2 * k1)
    nimcp_gpu_lnn_update_tau_batched(ctx, layer, input, batch_state, LNN_TAU_MIN_DEFAULT, LNN_TAU_MAX_DEFAULT);
    nimcp_gpu_lnn_compute_ltc_derivative_batched(ctx, layer, input, batch_state, layer->activation);
    if (cudaMemcpy(k2->data, batch_state->dx_dt->data, n * sizeof(float), cudaMemcpyDeviceToDevice) != cudaSuccess) return false;

    // x_temp = x + 0.5 * dt * k2
    kernel_add_scaled_batched<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (const float*)cache->x_checkpoint->data, (const float*)k2->data,
        0.5f * dt, (float*)x_temp->data, n);
    if (cudaMemcpy(batch_state->x->data, x_temp->data, n * sizeof(float), cudaMemcpyDeviceToDevice) != cudaSuccess) return false;

    // k3 = f(t + dt/2, x + dt/2 * k2)
    nimcp_gpu_lnn_update_tau_batched(ctx, layer, input, batch_state, LNN_TAU_MIN_DEFAULT, LNN_TAU_MAX_DEFAULT);
    nimcp_gpu_lnn_compute_ltc_derivative_batched(ctx, layer, input, batch_state, layer->activation);
    if (cudaMemcpy(k3->data, batch_state->dx_dt->data, n * sizeof(float), cudaMemcpyDeviceToDevice) != cudaSuccess) return false;

    // x_temp = x + dt * k3
    kernel_add_scaled_batched<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (const float*)cache->x_checkpoint->data, (const float*)k3->data,
        dt, (float*)x_temp->data, n);
    if (cudaMemcpy(batch_state->x->data, x_temp->data, n * sizeof(float), cudaMemcpyDeviceToDevice) != cudaSuccess) return false;

    // k4 = f(t + dt, x + dt * k3)
    nimcp_gpu_lnn_update_tau_batched(ctx, layer, input, batch_state, LNN_TAU_MIN_DEFAULT, LNN_TAU_MAX_DEFAULT);
    nimcp_gpu_lnn_compute_ltc_derivative_batched(ctx, layer, input, batch_state, layer->activation);
    if (cudaMemcpy(k4->data, batch_state->dx_dt->data, n * sizeof(float), cudaMemcpyDeviceToDevice) != cudaSuccess) return false;

    // x_new = x + (dt/6) * (k1 + 2*k2 + 2*k3 + k4)
    kernel_rk4_combine_batched<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (const float*)cache->x_checkpoint->data,
        (const float*)k1->data, (const float*)k2->data,
        (const float*)k3->data, (const float*)k4->data,
        dt, (float*)batch_state->x->data, n);

    if (cudaGetLastError() != cudaSuccess) return false;
    batch_state->current_time += dt;

    return true;
}

//=============================================================================
// Adaptive RK45 (Dormand-Prince) with Per-Sample Step Sizing
//=============================================================================

/**
 * @brief Compute DOPRI5 5th order solution
 */
__global__ void kernel_dopri5_solution_batched(
    const float* __restrict__ x,
    const float* __restrict__ k1,
    const float* __restrict__ k3,
    const float* __restrict__ k4,
    const float* __restrict__ k5,
    const float* __restrict__ k6,
    float dt,
    float* __restrict__ x_new,
    uint32_t n)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        // 5th order solution coefficients
        const float b1 = 35.0f / 384.0f;
        const float b3 = 500.0f / 1113.0f;
        const float b4 = 125.0f / 192.0f;
        const float b5 = -2187.0f / 6784.0f;
        const float b6 = 11.0f / 84.0f;

        x_new[idx] = x[idx] + dt * (b1 * k1[idx] + b3 * k3[idx] + b4 * k4[idx] +
                                     b5 * k5[idx] + b6 * k6[idx]);
    }
}

/**
 * @brief Compute error estimate per element for RK45
 */
__global__ void kernel_dopri5_error_batched(
    const float* __restrict__ k1,
    const float* __restrict__ k3,
    const float* __restrict__ k4,
    const float* __restrict__ k5,
    const float* __restrict__ k6,
    const float* __restrict__ k7,
    float dt,
    float* __restrict__ error,
    uint32_t n)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        // Error coefficients (difference between 5th and 4th order)
        const float e1 = 71.0f / 57600.0f;
        const float e3 = -71.0f / 16695.0f;
        const float e4 = 71.0f / 1920.0f;
        const float e5 = -17253.0f / 339200.0f;
        const float e6 = 22.0f / 525.0f;
        const float e7 = -1.0f / 40.0f;

        error[idx] = fabsf(dt * (e1 * k1[idx] + e3 * k3[idx] + e4 * k4[idx] +
                                  e5 * k5[idx] + e6 * k6[idx] + e7 * k7[idx]));
    }
}

/**
 * @brief Compute max error per batch sample using block reduction
 */
__global__ void kernel_max_error_per_sample(
    const float* __restrict__ error,
    float* __restrict__ max_error,
    uint32_t batch_size,
    uint32_t n_neurons)
{
    extern __shared__ float sdata[];

    uint32_t batch = blockIdx.x;
    uint32_t tid = threadIdx.x;

    if (batch >= batch_size) return;

    // Each thread finds max across its portion of neurons
    float local_max = 0.0f;
    for (uint32_t i = tid; i < n_neurons; i += blockDim.x) {
        float val = error[batch * n_neurons + i];
        local_max = fmaxf(local_max, val);
    }

    sdata[tid] = local_max;
    __syncthreads();

    // Parallel reduction for max
    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            sdata[tid] = fmaxf(sdata[tid], sdata[tid + s]);
        }
        __syncthreads();
    }

    if (tid == 0) {
        max_error[batch] = sdata[0];
    }
}

/**
 * @brief Update per-sample dt based on error
 */
__global__ void kernel_update_dt_per_sample(
    float* __restrict__ dt_per_sample,
    const float* __restrict__ max_error,
    float tolerance,
    float dt_min,
    float dt_max,
    uint32_t batch_size)
{
    uint32_t batch = blockIdx.x * blockDim.x + threadIdx.x;
    if (batch >= batch_size) return;

    float err = max_error[batch];
    float dt = dt_per_sample[batch];

    if (err > 0.0f && err < 1e10f) {
        float safety = 0.9f;
        float order = 5.0f;
        float factor = safety * powf(tolerance / err, 1.0f / order);
        factor = fminf(fmaxf(factor, 0.1f), 5.0f);
        dt = fminf(fmaxf(dt * factor, dt_min), dt_max);
    }

    dt_per_sample[batch] = dt;
}

bool nimcp_gpu_lnn_rk45_adaptive_batched(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* input,
    nimcp_lnn_ode_batch_state_t* batch_state,
    nimcp_lnn_ode_cache_t* cache,
    const nimcp_lnn_ode_batch_config_t* config)
{
    if (!ctx || !layer || !input || !batch_state || !cache || !config) {
        LOG_ERROR("Invalid parameters for batched RK45 adaptive step");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (cache->n_stages < 7) {
        LOG_ERROR("RK45 requires 7 cache stages, got %u", cache->n_stages);
        return false;
    }

    uint32_t batch_size = batch_state->batch_size;
    uint32_t n_neurons = batch_state->n_neurons;
    uint32_t n = batch_size * n_neurons;
    float dt = config->dt;

    // Use cached stages
    nimcp_gpu_tensor_t* k1 = cache->k_stages[0];
    nimcp_gpu_tensor_t* k2 = cache->k_stages[1];
    nimcp_gpu_tensor_t* k3 = cache->k_stages[2];
    nimcp_gpu_tensor_t* k4 = cache->k_stages[3];
    nimcp_gpu_tensor_t* k5 = cache->k_stages[4];
    nimcp_gpu_tensor_t* k6 = cache->k_stages[5];
    nimcp_gpu_tensor_t* k7 = cache->k_stages[6];
    nimcp_gpu_tensor_t* x_temp = cache->x_temp;

    float* d_max_error = NULL;
    bool success = false;

    // Save original state
    if (cudaMemcpy(cache->x_checkpoint->data, batch_state->x->data,
                          n * sizeof(float), cudaMemcpyDeviceToDevice) != cudaSuccess) goto rk45_cleanup;

    // DOPRI5 coefficients (Butcher tableau)
    {
        const float a21 = 1.0f/5.0f;
        const float a31 = 3.0f/40.0f, a32 = 9.0f/40.0f;
        const float a41 = 44.0f/45.0f, a42 = -56.0f/15.0f, a43 = 32.0f/9.0f;
        const float a51 = 19372.0f/6561.0f, a52 = -25360.0f/2187.0f, a53 = 64448.0f/6561.0f, a54 = -212.0f/729.0f;
        const float a61 = 9017.0f/3168.0f, a62 = -355.0f/33.0f, a63 = 46732.0f/5247.0f, a64 = 49.0f/176.0f, a65 = -5103.0f/18656.0f;

        // k1 = f(t, x)
        nimcp_gpu_lnn_update_tau_batched(ctx, layer, input, batch_state, LNN_TAU_MIN_DEFAULT, LNN_TAU_MAX_DEFAULT);
        nimcp_gpu_lnn_compute_ltc_derivative_batched(ctx, layer, input, batch_state, layer->activation);
        if (cudaMemcpy(k1->data, batch_state->dx_dt->data, n * sizeof(float), cudaMemcpyDeviceToDevice) != cudaSuccess) goto rk45_cleanup;

        // k2 = f(t + c2*dt, x + dt*a21*k1)
        kernel_add_scaled_batched<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const float*)cache->x_checkpoint->data, (const float*)k1->data,
            dt * a21, (float*)x_temp->data, n);
        if (cudaMemcpy(batch_state->x->data, x_temp->data, n * sizeof(float), cudaMemcpyDeviceToDevice) != cudaSuccess) goto rk45_cleanup;
        nimcp_gpu_lnn_update_tau_batched(ctx, layer, input, batch_state, LNN_TAU_MIN_DEFAULT, LNN_TAU_MAX_DEFAULT);
        nimcp_gpu_lnn_compute_ltc_derivative_batched(ctx, layer, input, batch_state, layer->activation);
        if (cudaMemcpy(k2->data, batch_state->dx_dt->data, n * sizeof(float), cudaMemcpyDeviceToDevice) != cudaSuccess) goto rk45_cleanup;

        // k3 = f(t + c3*dt, x + dt*(a31*k1 + a32*k2))
        kernel_add_scaled_batched<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const float*)cache->x_checkpoint->data, (const float*)k1->data,
            dt * a31, (float*)x_temp->data, n);
        kernel_add_scaled_batched<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const float*)x_temp->data, (const float*)k2->data,
            dt * a32, (float*)x_temp->data, n);
        if (cudaMemcpy(batch_state->x->data, x_temp->data, n * sizeof(float), cudaMemcpyDeviceToDevice) != cudaSuccess) goto rk45_cleanup;
        nimcp_gpu_lnn_update_tau_batched(ctx, layer, input, batch_state, LNN_TAU_MIN_DEFAULT, LNN_TAU_MAX_DEFAULT);
        nimcp_gpu_lnn_compute_ltc_derivative_batched(ctx, layer, input, batch_state, layer->activation);
        if (cudaMemcpy(k3->data, batch_state->dx_dt->data, n * sizeof(float), cudaMemcpyDeviceToDevice) != cudaSuccess) goto rk45_cleanup;

        // k4 = f(t + c4*dt, x + dt*(a41*k1 + a42*k2 + a43*k3))
        kernel_add_scaled_batched<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const float*)cache->x_checkpoint->data, (const float*)k1->data,
            dt * a41, (float*)x_temp->data, n);
        kernel_add_scaled_batched<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const float*)x_temp->data, (const float*)k2->data,
            dt * a42, (float*)x_temp->data, n);
        kernel_add_scaled_batched<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const float*)x_temp->data, (const float*)k3->data,
            dt * a43, (float*)x_temp->data, n);
        if (cudaMemcpy(batch_state->x->data, x_temp->data, n * sizeof(float), cudaMemcpyDeviceToDevice) != cudaSuccess) goto rk45_cleanup;
        nimcp_gpu_lnn_update_tau_batched(ctx, layer, input, batch_state, LNN_TAU_MIN_DEFAULT, LNN_TAU_MAX_DEFAULT);
        nimcp_gpu_lnn_compute_ltc_derivative_batched(ctx, layer, input, batch_state, layer->activation);
        if (cudaMemcpy(k4->data, batch_state->dx_dt->data, n * sizeof(float), cudaMemcpyDeviceToDevice) != cudaSuccess) goto rk45_cleanup;

        // k5 = f(t + c5*dt, x + dt*(a51*k1 + a52*k2 + a53*k3 + a54*k4))
        kernel_add_scaled_batched<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const float*)cache->x_checkpoint->data, (const float*)k1->data,
            dt * a51, (float*)x_temp->data, n);
        kernel_add_scaled_batched<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const float*)x_temp->data, (const float*)k2->data,
            dt * a52, (float*)x_temp->data, n);
        kernel_add_scaled_batched<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const float*)x_temp->data, (const float*)k3->data,
            dt * a53, (float*)x_temp->data, n);
        kernel_add_scaled_batched<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const float*)x_temp->data, (const float*)k4->data,
            dt * a54, (float*)x_temp->data, n);
        if (cudaMemcpy(batch_state->x->data, x_temp->data, n * sizeof(float), cudaMemcpyDeviceToDevice) != cudaSuccess) goto rk45_cleanup;
        nimcp_gpu_lnn_update_tau_batched(ctx, layer, input, batch_state, LNN_TAU_MIN_DEFAULT, LNN_TAU_MAX_DEFAULT);
        nimcp_gpu_lnn_compute_ltc_derivative_batched(ctx, layer, input, batch_state, layer->activation);
        if (cudaMemcpy(k5->data, batch_state->dx_dt->data, n * sizeof(float), cudaMemcpyDeviceToDevice) != cudaSuccess) goto rk45_cleanup;

        // k6 = f(t + dt, x + dt*(a61*k1 + a62*k2 + a63*k3 + a64*k4 + a65*k5))
        kernel_add_scaled_batched<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const float*)cache->x_checkpoint->data, (const float*)k1->data,
            dt * a61, (float*)x_temp->data, n);
        kernel_add_scaled_batched<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const float*)x_temp->data, (const float*)k2->data,
            dt * a62, (float*)x_temp->data, n);
        kernel_add_scaled_batched<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const float*)x_temp->data, (const float*)k3->data,
            dt * a63, (float*)x_temp->data, n);
        kernel_add_scaled_batched<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const float*)x_temp->data, (const float*)k4->data,
            dt * a64, (float*)x_temp->data, n);
        kernel_add_scaled_batched<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const float*)x_temp->data, (const float*)k5->data,
            dt * a65, (float*)x_temp->data, n);
        if (cudaMemcpy(batch_state->x->data, x_temp->data, n * sizeof(float), cudaMemcpyDeviceToDevice) != cudaSuccess) goto rk45_cleanup;
        nimcp_gpu_lnn_update_tau_batched(ctx, layer, input, batch_state, LNN_TAU_MIN_DEFAULT, LNN_TAU_MAX_DEFAULT);
        nimcp_gpu_lnn_compute_ltc_derivative_batched(ctx, layer, input, batch_state, layer->activation);
        if (cudaMemcpy(k6->data, batch_state->dx_dt->data, n * sizeof(float), cudaMemcpyDeviceToDevice) != cudaSuccess) goto rk45_cleanup;
    }

    // Compute 5th order solution
    kernel_dopri5_solution_batched<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (const float*)cache->x_checkpoint->data,
        (const float*)k1->data, (const float*)k3->data, (const float*)k4->data,
        (const float*)k5->data, (const float*)k6->data,
        dt, (float*)x_temp->data, n);

    // k7 = f(t + dt, x_5th) for FSAL
    if (cudaMemcpy(batch_state->x->data, x_temp->data, n * sizeof(float), cudaMemcpyDeviceToDevice) != cudaSuccess) goto rk45_cleanup;
    nimcp_gpu_lnn_update_tau_batched(ctx, layer, input, batch_state, LNN_TAU_MIN_DEFAULT, LNN_TAU_MAX_DEFAULT);
    nimcp_gpu_lnn_compute_ltc_derivative_batched(ctx, layer, input, batch_state, layer->activation);
    if (cudaMemcpy(k7->data, batch_state->dx_dt->data, n * sizeof(float), cudaMemcpyDeviceToDevice) != cudaSuccess) goto rk45_cleanup;

    // Compute error estimate
    kernel_dopri5_error_batched<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (const float*)k1->data, (const float*)k3->data, (const float*)k4->data,
        (const float*)k5->data, (const float*)k6->data, (const float*)k7->data,
        dt, (float*)batch_state->error->data, n);

    // Compute max error per sample
    if (cudaMalloc(&d_max_error, batch_size * sizeof(float)) != cudaSuccess) goto rk45_cleanup;

    kernel_max_error_per_sample<<<batch_size, BLOCK_SIZE, BLOCK_SIZE * sizeof(float)>>>(
        (const float*)batch_state->error->data, d_max_error, batch_size, n_neurons);

    // Update per-sample dt based on error
    if (config->adaptive_stepping && batch_state->dt_per_sample) {
        kernel_update_dt_per_sample<<<GRID_SIZE(batch_size), BLOCK_SIZE>>>(
            (float*)batch_state->dt_per_sample->data,
            d_max_error,
            config->error_tolerance,
            config->dt_min,
            config->dt_max,
            batch_size);
    }

    if (cudaGetLastError() != cudaSuccess) goto rk45_cleanup;
    batch_state->current_time += dt;

    success = true;

rk45_cleanup:
    cudaFree(d_max_error);
    return success;
}

//=============================================================================
// Multi-Step Integration
//=============================================================================

bool nimcp_gpu_lnn_integrate_multistep(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* input_sequence,
    nimcp_lnn_ode_batch_state_t* batch_state,
    nimcp_lnn_ode_cache_t* cache,
    const nimcp_lnn_ode_batch_config_t* config,
    uint32_t n_steps)
{
    if (!ctx || !layer || !input_sequence || !batch_state || !config) {
        LOG_ERROR("Invalid parameters for multi-step integration");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    uint32_t batch_size = batch_state->batch_size;
    uint32_t n_inputs = layer->n_inputs;

    // Create temporary tensor for single timestep input
    size_t input_dims[] = {batch_size, n_inputs};
    nimcp_gpu_tensor_t* step_input = nimcp_gpu_tensor_create(ctx, input_dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (!step_input) {
        LOG_ERROR("Failed to create step input tensor");
        return false;
    }

    bool success = true;
    for (uint32_t step = 0; step < n_steps && success; step++) {
        // Extract input for this timestep: input_sequence[step, :, :]
        size_t offset = step * batch_size * n_inputs * sizeof(float);
        cudaMemcpy(step_input->data,
                   (const char*)input_sequence->data + offset,
                   batch_size * n_inputs * sizeof(float),
                   cudaMemcpyDeviceToDevice);

        // Perform ODE step based on method
        switch (config->method) {
            case LNN_ODE_EULER:
                nimcp_gpu_lnn_update_tau_batched(ctx, layer, step_input, batch_state,
                                                 LNN_TAU_MIN_DEFAULT, LNN_TAU_MAX_DEFAULT);
                nimcp_gpu_lnn_compute_ltc_derivative_batched(ctx, layer, step_input, batch_state,
                                                            layer->activation);
                success = nimcp_gpu_lnn_euler_step_batched(ctx, batch_state, config->dt);
                break;

            case LNN_ODE_RK4:
                success = nimcp_gpu_lnn_rk4_step_batched(ctx, layer, step_input, batch_state, cache, config);
                break;

            case LNN_ODE_DOPRI5:
                success = nimcp_gpu_lnn_rk45_adaptive_batched(ctx, layer, step_input, batch_state, cache, config);
                break;

            default:
                LOG_WARN("Unsupported ODE method %d, falling back to Euler", config->method);
                nimcp_gpu_lnn_update_tau_batched(ctx, layer, step_input, batch_state,
                                                 LNN_TAU_MIN_DEFAULT, LNN_TAU_MAX_DEFAULT);
                nimcp_gpu_lnn_compute_ltc_derivative_batched(ctx, layer, step_input, batch_state,
                                                            layer->activation);
                success = nimcp_gpu_lnn_euler_step_batched(ctx, batch_state, config->dt);
                break;
        }
    }

    nimcp_gpu_tensor_destroy(step_input);
    return success;
}

//=============================================================================
// Reservoir Computing Operations
//=============================================================================

/**
 * @brief Initialize reservoir weights with random values
 */
__global__ void kernel_init_random_matrix(
    float* __restrict__ matrix,
    uint32_t rows,
    uint32_t cols,
    uint64_t seed,
    float scale)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t n = rows * cols;
    if (idx >= n) return;

    // cuRAND state initialization
    curandState state;
    curand_init(seed, idx, 0, &state);

    // Generate random value in [-1, 1] scaled
    matrix[idx] = (curand_uniform(&state) * 2.0f - 1.0f) * scale;
}

/**
 * @brief Apply sparsity mask to matrix
 */
__global__ void kernel_apply_sparsity(
    float* __restrict__ matrix,
    uint32_t n,
    float sparsity,
    uint64_t seed)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    curandState state;
    curand_init(seed + 12345, idx, 0, &state);

    if (curand_uniform(&state) < sparsity) {
        matrix[idx] = 0.0f;
    }
}

bool nimcp_gpu_lnn_reservoir_init(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_reservoir_state_t* reservoir,
    uint32_t reservoir_size,
    uint32_t n_inputs,
    uint32_t n_outputs,
    float spectral_radius,
    float sparsity,
    uint64_t seed)
{
    if (!ctx || !reservoir) {
        LOG_ERROR("Invalid parameters for reservoir init");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    reservoir->reservoir_size = reservoir_size;
    reservoir->n_inputs = n_inputs;
    reservoir->n_outputs = n_outputs;
    reservoir->spectral_radius = spectral_radius;
    reservoir->sparsity = sparsity;

    // Allocate tensors
    size_t res_dims[] = {reservoir_size, reservoir_size};
    size_t in_dims[] = {reservoir_size, n_inputs};
    size_t out_dims[] = {n_outputs, reservoir_size};
    size_t leak_dims[] = {reservoir_size};
    size_t state_dims[] = {1, reservoir_size};  // Batch size 1 initially

    reservoir->W_reservoir = nimcp_gpu_tensor_create(ctx, res_dims, 2, NIMCP_GPU_PRECISION_FP32);
    reservoir->W_input = nimcp_gpu_tensor_create(ctx, in_dims, 2, NIMCP_GPU_PRECISION_FP32);
    reservoir->W_output = nimcp_gpu_tensor_create(ctx, out_dims, 2, NIMCP_GPU_PRECISION_FP32);
    reservoir->leaking_rate = nimcp_gpu_tensor_create(ctx, leak_dims, 1, NIMCP_GPU_PRECISION_FP32);
    reservoir->reservoir_state = nimcp_gpu_tensor_create(ctx, state_dims, 2, NIMCP_GPU_PRECISION_FP32);

    if (!reservoir->W_reservoir || !reservoir->W_input || !reservoir->W_output ||
        !reservoir->leaking_rate || !reservoir->reservoir_state) {
        nimcp_gpu_lnn_reservoir_destroy(reservoir);
        return false;
    }

    // Initialize with random values
    uint32_t n_res = reservoir_size * reservoir_size;
    uint32_t n_in = reservoir_size * n_inputs;
    uint32_t n_out = n_outputs * reservoir_size;

    float init_scale = 1.0f / sqrtf((float)reservoir_size);

    kernel_init_random_matrix<<<GRID_SIZE(n_res), BLOCK_SIZE>>>(
        (float*)reservoir->W_reservoir->data, reservoir_size, reservoir_size, seed, 1.0f);
    kernel_init_random_matrix<<<GRID_SIZE(n_in), BLOCK_SIZE>>>(
        (float*)reservoir->W_input->data, reservoir_size, n_inputs, seed + 1, init_scale);
    kernel_init_random_matrix<<<GRID_SIZE(n_out), BLOCK_SIZE>>>(
        (float*)reservoir->W_output->data, n_outputs, reservoir_size, seed + 2, init_scale);

    // Apply sparsity to reservoir weights
    kernel_apply_sparsity<<<GRID_SIZE(n_res), BLOCK_SIZE>>>(
        (float*)reservoir->W_reservoir->data, n_res, sparsity, seed + 3);

    // Initialize leaking rate to default 0.3
    nimcp_gpu_fill(ctx, reservoir->leaking_rate, 0.3f);

    // Zero initial state
    nimcp_gpu_zeros(ctx, reservoir->reservoir_state);

    // TODO: Rescale to target spectral radius (requires power iteration)

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    LOG_DEBUG("Initialized reservoir with %u neurons, sparsity %.2f", reservoir_size, sparsity);

    return true;
}

/**
 * @brief Reservoir step: x(t+1) = (1-a)*x(t) + a*tanh(W_in*u(t) + W*x(t))
 */
__global__ void kernel_reservoir_step(
    float* __restrict__ state,           // [batch, reservoir_size]
    const float* __restrict__ input,     // [batch, n_inputs]
    const float* __restrict__ W_input,   // [reservoir_size, n_inputs]
    const float* __restrict__ W_reservoir, // [reservoir_size, reservoir_size]
    const float* __restrict__ leaking_rate, // [reservoir_size]
    uint32_t batch_size,
    uint32_t reservoir_size,
    uint32_t n_inputs)
{
    uint32_t batch = blockIdx.x;
    uint32_t neuron = blockIdx.y * blockDim.x + threadIdx.x;

    if (batch >= batch_size || neuron >= reservoir_size) return;

    uint32_t idx = batch * reservoir_size + neuron;
    float alpha = leaking_rate[neuron];

    // Compute W_in * u + W * x
    float sum = 0.0f;

    // Input contribution
    for (uint32_t j = 0; j < n_inputs; j++) {
        sum += W_input[neuron * n_inputs + j] * input[batch * n_inputs + j];
    }

    // Reservoir contribution
    for (uint32_t j = 0; j < reservoir_size; j++) {
        sum += W_reservoir[neuron * reservoir_size + j] * state[batch * reservoir_size + j];
    }

    // Leaky integration: x = (1-a)*x + a*tanh(sum)
    float old_state = state[idx];
    state[idx] = (1.0f - alpha) * old_state + alpha * tanhf(sum);
}

bool nimcp_gpu_lnn_reservoir_step(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_reservoir_state_t* reservoir,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output)
{
    if (!ctx || !reservoir || !input) {
        LOG_ERROR("Invalid parameters for reservoir step");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Infer batch size from input
    uint32_t batch_size = (input->ndim == 2) ? input->dims[0] : 1;
    uint32_t reservoir_size = reservoir->reservoir_size;
    uint32_t n_inputs = reservoir->n_inputs;

    dim3 grid(batch_size, GRID_SIZE(reservoir_size));
    dim3 block(BLOCK_SIZE);

    kernel_reservoir_step<<<grid, block>>>(
        (float*)reservoir->reservoir_state->data,
        (const float*)input->data,
        (const float*)reservoir->W_input->data,
        (const float*)reservoir->W_reservoir->data,
        (const float*)reservoir->leaking_rate->data,
        batch_size, reservoir_size, n_inputs);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Compute output if requested: y = W_out * x
    if (output && reservoir->W_output) {
        // Use GEMM: output = W_output @ state^T
        nimcp_gpu_gemm(ctx, reservoir->W_output, reservoir->reservoir_state,
                       output, 1.0f, 0.0f, false, true);
    }

    return true;
}

bool nimcp_gpu_lnn_reservoir_propagate_sequence(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_reservoir_state_t* reservoir,
    const nimcp_gpu_tensor_t* input_sequence,
    nimcp_gpu_tensor_t* output_sequence,
    nimcp_gpu_tensor_t* state_history,
    uint32_t seq_len)
{
    if (!ctx || !reservoir || !input_sequence) {
        LOG_ERROR("Invalid parameters for reservoir sequence propagation");
        return false;
    }

    if (!input_sequence->dims || input_sequence->ndim < 2) {
        LOG_ERROR("Invalid input_sequence dimensions for reservoir propagation");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // input_sequence: [seq_len, batch, n_inputs]
    uint32_t batch_size = input_sequence->dims[1];
    uint32_t n_inputs = reservoir->n_inputs;
    uint32_t reservoir_size = reservoir->reservoir_size;

    // Create temporary input tensor for each step
    size_t step_dims[] = {batch_size, n_inputs};
    nimcp_gpu_tensor_t* step_input = nimcp_gpu_tensor_create(ctx, step_dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (!step_input) return false;

    size_t out_dims[] = {batch_size, reservoir->n_outputs};
    nimcp_gpu_tensor_t* step_output = NULL;
    if (output_sequence) {
        step_output = nimcp_gpu_tensor_create(ctx, out_dims, 2, NIMCP_GPU_PRECISION_FP32);
        if (!step_output) {
            nimcp_gpu_tensor_destroy(step_input);
            return false;
        }
    }

    for (uint32_t t = 0; t < seq_len; t++) {
        // Extract input for this timestep
        size_t input_offset = t * batch_size * n_inputs * sizeof(float);
        cudaMemcpy(step_input->data,
                   (const char*)input_sequence->data + input_offset,
                   batch_size * n_inputs * sizeof(float),
                   cudaMemcpyDeviceToDevice);

        // Propagate reservoir
        bool success = nimcp_gpu_lnn_reservoir_step(ctx, reservoir, step_input, step_output);
        if (!success) {
            nimcp_gpu_tensor_destroy(step_input);
            if (step_output) nimcp_gpu_tensor_destroy(step_output);
            return false;
        }

        // Save output if requested
        if (output_sequence && step_output) {
            size_t output_offset = t * batch_size * reservoir->n_outputs * sizeof(float);
            cudaMemcpy((char*)output_sequence->data + output_offset,
                       step_output->data,
                       batch_size * reservoir->n_outputs * sizeof(float),
                       cudaMemcpyDeviceToDevice);
        }

        // Save state history if requested
        if (state_history) {
            size_t state_offset = t * batch_size * reservoir_size * sizeof(float);
            cudaMemcpy((char*)state_history->data + state_offset,
                       reservoir->reservoir_state->data,
                       batch_size * reservoir_size * sizeof(float),
                       cudaMemcpyDeviceToDevice);
        }
    }

    nimcp_gpu_tensor_destroy(step_input);
    if (step_output) nimcp_gpu_tensor_destroy(step_output);

    return true;
}

void nimcp_gpu_lnn_reservoir_destroy(nimcp_lnn_reservoir_state_t* reservoir)
{
    if (!reservoir) return;

    if (reservoir->reservoir_state) nimcp_gpu_tensor_destroy(reservoir->reservoir_state);
    if (reservoir->W_reservoir) nimcp_gpu_tensor_destroy(reservoir->W_reservoir);
    if (reservoir->W_input) nimcp_gpu_tensor_destroy(reservoir->W_input);
    if (reservoir->W_output) nimcp_gpu_tensor_destroy(reservoir->W_output);
    if (reservoir->leaking_rate) nimcp_gpu_tensor_destroy(reservoir->leaking_rate);

    // Zero out pointers
    reservoir->reservoir_state = NULL;
    reservoir->W_reservoir = NULL;
    reservoir->W_input = NULL;
    reservoir->W_output = NULL;
    reservoir->leaking_rate = NULL;
}

//=============================================================================
// Wiring/Connectivity Operations
//=============================================================================

/**
 * @brief Apply sparse wiring (CSR SpMV) for batched state propagation
 */
__global__ void kernel_apply_wiring_sparse_batched(
    const uint32_t* __restrict__ row_ptr,
    const uint32_t* __restrict__ col_idx,
    const float* __restrict__ values,
    const float* __restrict__ x,
    float* __restrict__ output,
    uint32_t batch_size,
    uint32_t n_neurons)
{
    uint32_t batch = blockIdx.x;
    uint32_t row = blockIdx.y * blockDim.x + threadIdx.x;

    if (batch >= batch_size || row >= n_neurons) return;

    float sum = 0.0f;
    uint32_t start = row_ptr[row];
    uint32_t end = row_ptr[row + 1];

    for (uint32_t k = start; k < end; k++) {
        uint32_t col = col_idx[k];
        float w = values ? values[k] : 1.0f;
        sum += w * x[batch * n_neurons + col];
    }

    output[batch * n_neurons + row] = sum;
}

bool nimcp_gpu_lnn_apply_wiring_batched(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* x_batch,
    nimcp_gpu_tensor_t* output)
{
    if (!ctx || !layer || !x_batch || !output) {
        LOG_ERROR("Invalid parameters for batched wiring application");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!layer->row_ptr || !layer->col_idx) {
        LOG_ERROR("Layer has no sparse wiring defined");
        return false;
    }

    uint32_t batch_size = x_batch->dims[0];
    uint32_t n_neurons = layer->n_neurons;

    dim3 grid(batch_size, GRID_SIZE(n_neurons));
    dim3 block(BLOCK_SIZE);

    kernel_apply_wiring_sparse_batched<<<grid, block>>>(
        (const uint32_t*)layer->row_ptr->data,
        (const uint32_t*)layer->col_idx->data,
        layer->edge_weights ? (const float*)layer->edge_weights->data : NULL,
        (const float*)x_batch->data,
        (float*)output->data,
        batch_size, n_neurons);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Generate random wiring pattern on GPU
 */
__global__ void kernel_generate_random_edges(
    uint32_t* __restrict__ row_nnz,
    uint32_t n_neurons,
    float target_density,
    uint64_t seed)
{
    uint32_t row = blockIdx.x * blockDim.x + threadIdx.x;
    if (row >= n_neurons) return;

    curandState state;
    curand_init(seed, row, 0, &state);

    uint32_t expected_edges = (uint32_t)(target_density * n_neurons + 0.5f);
    row_nnz[row] = expected_edges;
}

uint32_t nimcp_gpu_lnn_generate_wiring(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* row_ptr,
    nimcp_gpu_tensor_t* col_idx,
    nimcp_gpu_tensor_t* values,
    uint32_t n_neurons,
    float target_density,
    uint64_t seed)
{
    if (!ctx || !row_ptr || !col_idx) {
        LOG_ERROR("Invalid parameters for wiring generation");
        return 0;
    }

    // Estimate number of edges
    uint32_t expected_edges = (uint32_t)(target_density * n_neurons * n_neurons);

    // Allocate temporary for nnz per row
    uint32_t* d_row_nnz = NULL;
    uint32_t* h_row_nnz = NULL;
    uint32_t* h_row_ptr = NULL;
    uint32_t* h_col_idx = NULL;
    uint32_t total_edges = 0;

    if (cudaMalloc(&d_row_nnz, n_neurons * sizeof(uint32_t)) != cudaSuccess) goto wiring_cleanup;

    kernel_generate_random_edges<<<GRID_SIZE(n_neurons), BLOCK_SIZE>>>(
        d_row_nnz, n_neurons, target_density, seed);

    // Compute prefix sum for row_ptr (simplified CPU version)
    h_row_nnz = (uint32_t*)nimcp_malloc(n_neurons * sizeof(uint32_t));
    if (!h_row_nnz) goto wiring_cleanup;
    h_row_ptr = (uint32_t*)nimcp_malloc((n_neurons + 1) * sizeof(uint32_t));
    if (!h_row_ptr) goto wiring_cleanup;

    if (cudaMemcpy(h_row_nnz, d_row_nnz, n_neurons * sizeof(uint32_t), cudaMemcpyDeviceToHost) != cudaSuccess) goto wiring_cleanup;

    h_row_ptr[0] = 0;
    for (uint32_t i = 0; i < n_neurons; i++) {
        h_row_ptr[i + 1] = h_row_ptr[i] + h_row_nnz[i];
    }
    total_edges = h_row_ptr[n_neurons];

    if (cudaMemcpy(row_ptr->data, h_row_ptr, (n_neurons + 1) * sizeof(uint32_t), cudaMemcpyHostToDevice) != cudaSuccess) goto wiring_cleanup;

    // Generate column indices (simplified: random selection)
    h_col_idx = (uint32_t*)nimcp_malloc(total_edges * sizeof(uint32_t));
    if (!h_col_idx) goto wiring_cleanup;
    srand((unsigned int)seed);
    for (uint32_t i = 0; i < total_edges; i++) {
        h_col_idx[i] = rand() % n_neurons;
    }
    if (cudaMemcpy(col_idx->data, h_col_idx, total_edges * sizeof(uint32_t), cudaMemcpyHostToDevice) != cudaSuccess) goto wiring_cleanup;

    // Initialize values if provided
    if (values) {
        float init_scale = 1.0f / sqrtf((float)n_neurons);
        kernel_init_random_matrix<<<GRID_SIZE(total_edges), BLOCK_SIZE>>>(
            (float*)values->data, total_edges, 1, seed + 100, init_scale);
    }

    // Cleanup
    cudaFree(d_row_nnz);
    nimcp_free(h_row_nnz);
    nimcp_free(h_row_ptr);
    nimcp_free(h_col_idx);

    LOG_DEBUG("Generated wiring with %u edges (density %.3f)", total_edges, target_density);
    return total_edges;

wiring_cleanup:
    cudaFree(d_row_nnz);
    nimcp_free(h_row_nnz);
    nimcp_free(h_row_ptr);
    nimcp_free(h_col_idx);
    return 0;
}

/**
 * @brief Power iteration for spectral radius estimation
 */
__global__ void kernel_power_iteration_step(
    const float* __restrict__ W,
    const float* __restrict__ v,
    float* __restrict__ v_new,
    uint32_t n)
{
    uint32_t row = blockIdx.x * blockDim.x + threadIdx.x;
    if (row >= n) return;

    float sum = 0.0f;
    for (uint32_t col = 0; col < n; col++) {
        sum += W[row * n + col] * v[col];
    }
    v_new[row] = sum;
}

__global__ void kernel_compute_norm(const float* v, float* norm, uint32_t n)
{
    extern __shared__ float sdata[];
    uint32_t tid = threadIdx.x;
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    sdata[tid] = (idx < n) ? v[idx] * v[idx] : 0.0f;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            sdata[tid] += sdata[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        atomicAdd(norm, sdata[0]);
    }
}

__global__ void kernel_normalize_vector(float* v, float norm, uint32_t n)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n && norm > 0.0f) {
        v[idx] /= norm;
    }
}

bool nimcp_gpu_lnn_compute_spectral_radius(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    float* spectral_radius,
    uint32_t max_iterations,
    float tolerance)
{
    if (!ctx || !layer || !spectral_radius || !layer->W_rec) {
        LOG_ERROR("Invalid parameters for spectral radius computation");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    uint32_t n = layer->n_neurons;
    size_t dims[] = {n};

    // Allocate vectors for power iteration
    nimcp_gpu_tensor_t* v = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* v_new = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);

    if (!v || !v_new) {
        nimcp_gpu_tensor_destroy(v);
        nimcp_gpu_tensor_destroy(v_new);
        return false;
    }

    // Initialize with random vector
    nimcp_gpu_fill(ctx, v, 1.0f / sqrtf((float)n));

    float* d_norm = NULL;
    if (cudaMalloc(&d_norm, sizeof(float)) != cudaSuccess) {
        nimcp_gpu_tensor_destroy(v);
        nimcp_gpu_tensor_destroy(v_new);
        return false;
    }

    float prev_eigenvalue = 0.0f;
    float eigenvalue = 0.0f;

    for (uint32_t iter = 0; iter < max_iterations; iter++) {
        // v_new = W @ v
        kernel_power_iteration_step<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const float*)layer->W_rec->data,
            (const float*)v->data,
            (float*)v_new->data, n);

        // Compute norm of v_new
        cudaMemset(d_norm, 0, sizeof(float));
        kernel_compute_norm<<<GRID_SIZE(n), BLOCK_SIZE, BLOCK_SIZE * sizeof(float)>>>(
            (const float*)v_new->data, d_norm, n);

        float h_norm;
        cudaMemcpy(&h_norm, d_norm, sizeof(float), cudaMemcpyDeviceToHost);
        h_norm = sqrtf(h_norm);

        eigenvalue = h_norm;

        // Normalize v_new
        kernel_normalize_vector<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (float*)v_new->data, h_norm, n);

        // Swap v and v_new
        void* temp = v->data;
        v->data = v_new->data;
        v_new->data = temp;

        // Check convergence
        if (fabsf(eigenvalue - prev_eigenvalue) < tolerance) {
            break;
        }
        prev_eigenvalue = eigenvalue;
    }

    *spectral_radius = eigenvalue;

    cudaFree(d_norm);
    nimcp_gpu_tensor_destroy(v);
    nimcp_gpu_tensor_destroy(v_new);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_lnn_rescale_spectral_radius(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    float target_spectral_radius)
{
    if (!ctx || !layer || !layer->W_rec) {
        LOG_ERROR("Invalid parameters for spectral radius rescaling");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Compute current spectral radius
    float current_radius;
    if (!nimcp_gpu_lnn_compute_spectral_radius(ctx, layer, &current_radius, 100, 1e-6f)) {
        return false;
    }

    if (current_radius < 1e-10f) {
        LOG_WARN("Current spectral radius is near zero, cannot rescale");
        return false;
    }

    // Rescale weights
    float scale = target_spectral_radius / current_radius;
    nimcp_gpu_mul_scalar(ctx, layer->W_rec, scale, layer->W_rec);

    LOG_DEBUG("Rescaled spectral radius from %.4f to %.4f", current_radius, target_spectral_radius);
    return true;
}

//=============================================================================
// State and Cache Management
//=============================================================================

nimcp_lnn_ode_batch_state_t* nimcp_lnn_ode_batch_state_create(
    nimcp_gpu_context_t* ctx,
    uint32_t batch_size,
    uint32_t n_neurons)
{
    if (!ctx || batch_size == 0 || n_neurons == 0) {
        LOG_ERROR("Invalid parameters for batch state creation");
        return NULL;
    }

    nimcp_lnn_ode_batch_state_t* state = (nimcp_lnn_ode_batch_state_t*)nimcp_calloc(1, sizeof(nimcp_lnn_ode_batch_state_t));
    if (!state) return NULL;

    state->batch_size = batch_size;
    state->n_neurons = n_neurons;
    state->current_time = 0.0f;

    size_t dims[] = {batch_size, n_neurons};
    size_t batch_dims[] = {batch_size};

    state->x = nimcp_gpu_tensor_create(ctx, dims, 2, NIMCP_GPU_PRECISION_FP32);
    state->dx_dt = nimcp_gpu_tensor_create(ctx, dims, 2, NIMCP_GPU_PRECISION_FP32);
    state->tau = nimcp_gpu_tensor_create(ctx, dims, 2, NIMCP_GPU_PRECISION_FP32);
    state->error = nimcp_gpu_tensor_create(ctx, dims, 2, NIMCP_GPU_PRECISION_FP32);
    state->dt_per_sample = nimcp_gpu_tensor_create(ctx, batch_dims, 1, NIMCP_GPU_PRECISION_FP32);

    if (!state->x || !state->dx_dt || !state->tau || !state->error || !state->dt_per_sample) {
        nimcp_lnn_ode_batch_state_destroy(state);
        return NULL;
    }

    // Initialize with default values
    nimcp_gpu_zeros(ctx, state->x);
    nimcp_gpu_zeros(ctx, state->dx_dt);
    nimcp_gpu_fill(ctx, state->tau, 10.0f);  // Default tau = 10ms
    nimcp_gpu_zeros(ctx, state->error);
    nimcp_gpu_fill(ctx, state->dt_per_sample, 1.0f);  // Default dt = 1ms

    LOG_DEBUG("Created batch ODE state: batch=%u, neurons=%u", batch_size, n_neurons);
    return state;
}

void nimcp_lnn_ode_batch_state_destroy(nimcp_lnn_ode_batch_state_t* state)
{
    if (!state) return;

    if (state->x) nimcp_gpu_tensor_destroy(state->x);
    if (state->dx_dt) nimcp_gpu_tensor_destroy(state->dx_dt);
    if (state->tau) nimcp_gpu_tensor_destroy(state->tau);
    if (state->error) nimcp_gpu_tensor_destroy(state->error);
    if (state->dt_per_sample) nimcp_gpu_tensor_destroy(state->dt_per_sample);

    nimcp_free(state);
}

bool nimcp_lnn_ode_batch_state_reset(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_ode_batch_state_t* state,
    float initial_x)
{
    if (!ctx || !state) {
        LOG_ERROR("Invalid parameters for batch state reset");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    nimcp_gpu_fill(ctx, state->x, initial_x);
    nimcp_gpu_zeros(ctx, state->dx_dt);
    nimcp_gpu_zeros(ctx, state->error);
    state->current_time = 0.0f;

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

nimcp_lnn_ode_cache_t* nimcp_lnn_ode_cache_create(
    nimcp_gpu_context_t* ctx,
    uint32_t batch_size,
    uint32_t n_neurons,
    uint32_t n_stages)
{
    if (!ctx || batch_size == 0 || n_neurons == 0 || n_stages == 0) {
        LOG_ERROR("Invalid parameters for ODE cache creation");
        return NULL;
    }

    nimcp_lnn_ode_cache_t* cache = (nimcp_lnn_ode_cache_t*)nimcp_calloc(1, sizeof(nimcp_lnn_ode_cache_t));
    if (!cache) return NULL;

    cache->n_stages = n_stages;
    cache->checkpoint_interval = 10;

    size_t dims[] = {batch_size, n_neurons};

    // Allocate k stages
    cache->k_stages = (nimcp_gpu_tensor_t**)nimcp_calloc(n_stages, sizeof(nimcp_gpu_tensor_t*));
    if (!cache->k_stages) {
        nimcp_free(cache);
        return NULL;
    }

    for (uint32_t i = 0; i < n_stages; i++) {
        cache->k_stages[i] = nimcp_gpu_tensor_create(ctx, dims, 2, NIMCP_GPU_PRECISION_FP32);
        if (!cache->k_stages[i]) {
            nimcp_lnn_ode_cache_destroy(cache);
            return NULL;
        }
    }

    cache->x_temp = nimcp_gpu_tensor_create(ctx, dims, 2, NIMCP_GPU_PRECISION_FP32);
    cache->x_checkpoint = nimcp_gpu_tensor_create(ctx, dims, 2, NIMCP_GPU_PRECISION_FP32);

    if (!cache->x_temp || !cache->x_checkpoint) {
        nimcp_lnn_ode_cache_destroy(cache);
        return NULL;
    }

    LOG_DEBUG("Created ODE cache: batch=%u, neurons=%u, stages=%u", batch_size, n_neurons, n_stages);
    return cache;
}

void nimcp_lnn_ode_cache_destroy(nimcp_lnn_ode_cache_t* cache)
{
    if (!cache) return;

    if (cache->k_stages) {
        for (uint32_t i = 0; i < cache->n_stages; i++) {
            if (cache->k_stages[i]) {
                nimcp_gpu_tensor_destroy(cache->k_stages[i]);
            }
        }
        nimcp_free(cache->k_stages);
    }

    if (cache->x_temp) nimcp_gpu_tensor_destroy(cache->x_temp);
    if (cache->x_checkpoint) nimcp_gpu_tensor_destroy(cache->x_checkpoint);

    nimcp_free(cache);
}

//=============================================================================
// Stability Checking and State Statistics
//=============================================================================

/**
 * @brief Check for NaN and Inf in array
 */
__global__ void kernel_check_numerical_issues(
    const float* __restrict__ data,
    uint32_t* __restrict__ has_nan,
    uint32_t* __restrict__ has_inf,
    float* __restrict__ max_abs,
    uint32_t n)
{
    extern __shared__ float sdata[];

    uint32_t tid = threadIdx.x;
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    float val = (idx < n) ? data[idx] : 0.0f;
    bool is_nan = isnan(val);
    bool is_inf = isinf(val);
    float abs_val = fabsf(val);

    // Check for NaN
    if (is_nan) {
        atomicAdd(has_nan, 1);
    }

    // Check for Inf
    if (is_inf) {
        atomicAdd(has_inf, 1);
    }

    // Find max absolute value
    sdata[tid] = (is_nan || is_inf) ? 0.0f : abs_val;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            sdata[tid] = fmaxf(sdata[tid], sdata[tid + s]);
        }
        __syncthreads();
    }

    if (tid == 0) {
        atomicMax((int*)max_abs, __float_as_int(sdata[0]));
    }
}

bool nimcp_gpu_lnn_check_stability(
    nimcp_gpu_context_t* ctx,
    const nimcp_lnn_ode_batch_state_t* batch_state,
    bool* has_nan,
    bool* has_inf,
    float* max_norm)
{
    if (!ctx || !batch_state || !batch_state->x || !has_nan || !has_inf || !max_norm) {
        LOG_ERROR("Invalid parameters for stability check");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    uint32_t n = batch_state->batch_size * batch_state->n_neurons;

    uint32_t* d_has_nan = NULL;
    uint32_t* d_has_inf = NULL;
    float* d_max_abs = NULL;
    bool success = false;

    if (cudaMalloc(&d_has_nan, sizeof(uint32_t)) != cudaSuccess) goto stability_cleanup;
    if (cudaMalloc(&d_has_inf, sizeof(uint32_t)) != cudaSuccess) goto stability_cleanup;
    if (cudaMalloc(&d_max_abs, sizeof(float)) != cudaSuccess) goto stability_cleanup;

    if (cudaMemset(d_has_nan, 0, sizeof(uint32_t)) != cudaSuccess) goto stability_cleanup;
    if (cudaMemset(d_has_inf, 0, sizeof(uint32_t)) != cudaSuccess) goto stability_cleanup;
    if (cudaMemset(d_max_abs, 0, sizeof(float)) != cudaSuccess) goto stability_cleanup;

    kernel_check_numerical_issues<<<GRID_SIZE(n), BLOCK_SIZE, BLOCK_SIZE * sizeof(float)>>>(
        (const float*)batch_state->x->data, d_has_nan, d_has_inf, d_max_abs, n);

    {
        uint32_t h_has_nan, h_has_inf;
        float h_max_abs;

        if (cudaMemcpy(&h_has_nan, d_has_nan, sizeof(uint32_t), cudaMemcpyDeviceToHost) != cudaSuccess) goto stability_cleanup;
        if (cudaMemcpy(&h_has_inf, d_has_inf, sizeof(uint32_t), cudaMemcpyDeviceToHost) != cudaSuccess) goto stability_cleanup;
        if (cudaMemcpy(&h_max_abs, d_max_abs, sizeof(float), cudaMemcpyDeviceToHost) != cudaSuccess) goto stability_cleanup;

        *has_nan = (h_has_nan > 0);
        *has_inf = (h_has_inf > 0);
        *max_norm = h_max_abs;
    }

    if (cudaGetLastError() != cudaSuccess) goto stability_cleanup;

    success = true;

stability_cleanup:
    cudaFree(d_has_nan);
    cudaFree(d_has_inf);
    cudaFree(d_max_abs);

    return success;
}

/**
 * @brief Compute mean, variance, min, max using parallel reduction
 */
__global__ void kernel_compute_stats_phase1(
    const float* __restrict__ data,
    float* __restrict__ sum,
    float* __restrict__ sum_sq,
    float* __restrict__ min_val,
    float* __restrict__ max_val,
    uint32_t n)
{
    extern __shared__ float sdata[];
    float* s_sum = sdata;
    float* s_sum_sq = sdata + blockDim.x;
    float* s_min = sdata + 2 * blockDim.x;
    float* s_max = sdata + 3 * blockDim.x;

    uint32_t tid = threadIdx.x;
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    float val = (idx < n) ? data[idx] : 0.0f;

    s_sum[tid] = (idx < n) ? val : 0.0f;
    s_sum_sq[tid] = (idx < n) ? val * val : 0.0f;
    s_min[tid] = (idx < n) ? val : FLT_MAX;
    s_max[tid] = (idx < n) ? val : -FLT_MAX;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            s_sum[tid] += s_sum[tid + s];
            s_sum_sq[tid] += s_sum_sq[tid + s];
            s_min[tid] = fminf(s_min[tid], s_min[tid + s]);
            s_max[tid] = fmaxf(s_max[tid], s_max[tid + s]);
        }
        __syncthreads();
    }

    if (tid == 0) {
        atomicAdd(sum, s_sum[0]);
        atomicAdd(sum_sq, s_sum_sq[0]);
        atomicMin((int*)min_val, __float_as_int(s_min[0]));
        atomicMax((int*)max_val, __float_as_int(s_max[0]));
    }
}

bool nimcp_gpu_lnn_compute_state_stats(
    nimcp_gpu_context_t* ctx,
    const nimcp_lnn_ode_batch_state_t* batch_state,
    float* mean,
    float* std,
    float* min_val,
    float* max_val)
{
    if (!ctx || !batch_state || !batch_state->x || !mean || !std || !min_val || !max_val) {
        LOG_ERROR("Invalid parameters for state stats computation");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    uint32_t n = batch_state->batch_size * batch_state->n_neurons;

    float* d_sum = NULL;
    float* d_sum_sq = NULL;
    float* d_min = NULL;
    float* d_max = NULL;
    bool success = false;

    if (cudaMalloc(&d_sum, sizeof(float)) != cudaSuccess) goto state_stats_cleanup;
    if (cudaMalloc(&d_sum_sq, sizeof(float)) != cudaSuccess) goto state_stats_cleanup;
    if (cudaMalloc(&d_min, sizeof(float)) != cudaSuccess) goto state_stats_cleanup;
    if (cudaMalloc(&d_max, sizeof(float)) != cudaSuccess) goto state_stats_cleanup;

    if (cudaMemset(d_sum, 0, sizeof(float)) != cudaSuccess) goto state_stats_cleanup;
    if (cudaMemset(d_sum_sq, 0, sizeof(float)) != cudaSuccess) goto state_stats_cleanup;
    {
        float init_min = FLT_MAX, init_max = -FLT_MAX;
        if (cudaMemcpy(d_min, &init_min, sizeof(float), cudaMemcpyHostToDevice) != cudaSuccess) goto state_stats_cleanup;
        if (cudaMemcpy(d_max, &init_max, sizeof(float), cudaMemcpyHostToDevice) != cudaSuccess) goto state_stats_cleanup;
    }

    {
        size_t shared_mem = 4 * BLOCK_SIZE * sizeof(float);
        kernel_compute_stats_phase1<<<GRID_SIZE(n), BLOCK_SIZE, shared_mem>>>(
            (const float*)batch_state->x->data, d_sum, d_sum_sq, d_min, d_max, n);
    }

    {
        float h_sum, h_sum_sq, h_min, h_max;
        if (cudaMemcpy(&h_sum, d_sum, sizeof(float), cudaMemcpyDeviceToHost) != cudaSuccess) goto state_stats_cleanup;
        if (cudaMemcpy(&h_sum_sq, d_sum_sq, sizeof(float), cudaMemcpyDeviceToHost) != cudaSuccess) goto state_stats_cleanup;
        if (cudaMemcpy(&h_min, d_min, sizeof(float), cudaMemcpyDeviceToHost) != cudaSuccess) goto state_stats_cleanup;
        if (cudaMemcpy(&h_max, d_max, sizeof(float), cudaMemcpyDeviceToHost) != cudaSuccess) goto state_stats_cleanup;

        *mean = h_sum / (float)n;
        float variance = (h_sum_sq / (float)n) - (*mean * *mean);
        *std = sqrtf(fmaxf(0.0f, variance));
        *min_val = h_min;
        *max_val = h_max;
    }

    if (cudaGetLastError() != cudaSuccess) goto state_stats_cleanup;

    success = true;

state_stats_cleanup:
    cudaFree(d_sum);
    cudaFree(d_sum_sq);
    cudaFree(d_min);
    cudaFree(d_max);

    return success;
}

//=============================================================================
// CPU Fallbacks
//=============================================================================

bool nimcp_cpu_lnn_euler_step_batched(
    const float* x,
    const float* dx_dt,
    float* x_new,
    uint32_t batch_size,
    uint32_t n_neurons,
    float dt)
{
    if (!x || !dx_dt || !x_new) return false;

    for (uint32_t b = 0; b < batch_size; b++) {
        for (uint32_t n = 0; n < n_neurons; n++) {
            uint32_t idx = b * n_neurons + n;
            x_new[idx] = x[idx] + dt * dx_dt[idx];
        }
    }

    return true;
}

// Helper function for CPU activation
static inline float cpu_activation(float x, lnn_activation_t activation)
{
    switch (activation) {
        case LNN_ACTIVATION_TANH:
            return tanhf(x);
        case LNN_ACTIVATION_SIGMOID:
            return 1.0f / (1.0f + expf(-x));
        case LNN_ACTIVATION_RELU:
            return fmaxf(0.0f, x);
        case LNN_ACTIVATION_GELU: {
            float sqrt_2_over_pi = 0.7978845608f;
            float cdf = 0.5f * (1.0f + tanhf(sqrt_2_over_pi * (x + 0.044715f * x * x * x)));
            return x * cdf;
        }
        case LNN_ACTIVATION_SILU:
            return x / (1.0f + expf(-x));
        case LNN_ACTIVATION_SOFTPLUS:
            return logf(1.0f + expf(x));
        default:
            return tanhf(x);
    }
}

bool nimcp_cpu_lnn_rk4_step_batched(
    const float* x,
    const float* tau,
    const float* input,
    const float* W_in,
    const float* W_rec,
    const float* b_in,
    float* x_new,
    uint32_t batch_size,
    uint32_t n_neurons,
    uint32_t n_inputs,
    float dt,
    lnn_activation_t activation)
{
    if (!x || !tau || !input || !W_in || !W_rec || !b_in || !x_new) return false;

    // Allocate temporary storage
    float* k1 = (float*)nimcp_malloc(batch_size * n_neurons * sizeof(float));
    float* k2 = (float*)nimcp_malloc(batch_size * n_neurons * sizeof(float));
    float* k3 = (float*)nimcp_malloc(batch_size * n_neurons * sizeof(float));
    float* k4 = (float*)nimcp_malloc(batch_size * n_neurons * sizeof(float));
    float* x_temp = (float*)nimcp_malloc(batch_size * n_neurons * sizeof(float));

    if (!k1 || !k2 || !k3 || !k4 || !x_temp) {
        nimcp_free(k1); nimcp_free(k2); nimcp_free(k3); nimcp_free(k4); nimcp_free(x_temp);
        return false;
    }

    // Lambda for computing derivative
    auto compute_derivative = [&](const float* x_state, float* dx_dt) {
        for (uint32_t b = 0; b < batch_size; b++) {
            for (uint32_t i = 0; i < n_neurons; i++) {
                uint32_t idx = b * n_neurons + i;

                // Compute weighted sum
                float sum = b_in[i];

                // Input contribution
                for (uint32_t j = 0; j < n_inputs; j++) {
                    sum += W_in[i * n_inputs + j] * input[b * n_inputs + j];
                }

                // Recurrent contribution
                for (uint32_t j = 0; j < n_neurons; j++) {
                    sum += W_rec[i * n_neurons + j] * x_state[b * n_neurons + j];
                }

                // LTC dynamics
                dx_dt[idx] = -x_state[idx] / tau[idx] + cpu_activation(sum, activation);
            }
        }
    };

    // k1 = f(t, x)
    compute_derivative(x, k1);

    // x_temp = x + 0.5 * dt * k1
    for (uint32_t i = 0; i < batch_size * n_neurons; i++) {
        x_temp[i] = x[i] + 0.5f * dt * k1[i];
    }

    // k2 = f(t + dt/2, x + dt/2 * k1)
    compute_derivative(x_temp, k2);

    // x_temp = x + 0.5 * dt * k2
    for (uint32_t i = 0; i < batch_size * n_neurons; i++) {
        x_temp[i] = x[i] + 0.5f * dt * k2[i];
    }

    // k3 = f(t + dt/2, x + dt/2 * k2)
    compute_derivative(x_temp, k3);

    // x_temp = x + dt * k3
    for (uint32_t i = 0; i < batch_size * n_neurons; i++) {
        x_temp[i] = x[i] + dt * k3[i];
    }

    // k4 = f(t + dt, x + dt * k3)
    compute_derivative(x_temp, k4);

    // x_new = x + (dt/6) * (k1 + 2*k2 + 2*k3 + k4)
    for (uint32_t i = 0; i < batch_size * n_neurons; i++) {
        x_new[i] = x[i] + (dt / 6.0f) * (k1[i] + 2.0f * k2[i] + 2.0f * k3[i] + k4[i]);
    }

    nimcp_free(k1);
    nimcp_free(k2);
    nimcp_free(k3);
    nimcp_free(k4);
    nimcp_free(x_temp);

    return true;
}

#else // !NIMCP_ENABLE_CUDA

//=============================================================================
// Non-CUDA Stubs
//=============================================================================

#include "gpu/lnn/nimcp_lnn_ode_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define LOG_MODULE "LNN_ODE_GPU"

nimcp_lnn_ode_batch_config_t nimcp_lnn_ode_batch_default_config(void)
{
    nimcp_lnn_ode_batch_config_t config;
    memset(&config, 0, sizeof(config));
    config.method = LNN_ODE_RK4;
    config.dt = 1.0f;
    config.dt_min = 0.01f;
    config.dt_max = 10.0f;
    config.error_tolerance = 1e-5f;
    config.relative_tolerance = 1e-4f;
    config.max_steps = 1000;
    config.num_substeps = 1;
    return config;
}

bool nimcp_gpu_lnn_euler_step_batched(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_ode_batch_state_t* batch_state,
    float dt)
{
    LOG_WARN("CUDA not available - LNN ODE GPU operations require CUDA");
    return false;
}

bool nimcp_gpu_lnn_rk4_step_batched(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* input,
    nimcp_lnn_ode_batch_state_t* batch_state,
    nimcp_lnn_ode_cache_t* cache,
    const nimcp_lnn_ode_batch_config_t* config)
{
    return false;
}

bool nimcp_gpu_lnn_rk45_adaptive_batched(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* input,
    nimcp_lnn_ode_batch_state_t* batch_state,
    nimcp_lnn_ode_cache_t* cache,
    const nimcp_lnn_ode_batch_config_t* config)
{
    return false;
}

bool nimcp_gpu_lnn_integrate_multistep(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* input_sequence,
    nimcp_lnn_ode_batch_state_t* batch_state,
    nimcp_lnn_ode_cache_t* cache,
    const nimcp_lnn_ode_batch_config_t* config,
    uint32_t n_steps)
{
    return false;
}

bool nimcp_gpu_lnn_compute_ltc_derivative_batched(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* input,
    nimcp_lnn_ode_batch_state_t* batch_state,
    lnn_activation_t activation)
{
    return false;
}

bool nimcp_gpu_lnn_update_tau_batched(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* input,
    nimcp_lnn_ode_batch_state_t* batch_state,
    float tau_min,
    float tau_max)
{
    return false;
}

bool nimcp_gpu_lnn_reservoir_init(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_reservoir_state_t* reservoir,
    uint32_t reservoir_size,
    uint32_t n_inputs,
    uint32_t n_outputs,
    float spectral_radius,
    float sparsity,
    uint64_t seed)
{
    return false;
}

bool nimcp_gpu_lnn_reservoir_step(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_reservoir_state_t* reservoir,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output)
{
    return false;
}

bool nimcp_gpu_lnn_reservoir_propagate_sequence(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_reservoir_state_t* reservoir,
    const nimcp_gpu_tensor_t* input_sequence,
    nimcp_gpu_tensor_t* output_sequence,
    nimcp_gpu_tensor_t* state_history,
    uint32_t seq_len)
{
    return false;
}

void nimcp_gpu_lnn_reservoir_destroy(nimcp_lnn_reservoir_state_t* reservoir)
{
    // No-op when CUDA not available
}

bool nimcp_gpu_lnn_apply_wiring_batched(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* x_batch,
    nimcp_gpu_tensor_t* output)
{
    return false;
}

uint32_t nimcp_gpu_lnn_generate_wiring(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* row_ptr,
    nimcp_gpu_tensor_t* col_idx,
    nimcp_gpu_tensor_t* values,
    uint32_t n_neurons,
    float target_density,
    uint64_t seed)
{
    return 0;
}

bool nimcp_gpu_lnn_compute_spectral_radius(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    float* spectral_radius,
    uint32_t max_iterations,
    float tolerance)
{
    return false;
}

bool nimcp_gpu_lnn_rescale_spectral_radius(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    float target_spectral_radius)
{
    return false;
}

nimcp_lnn_ode_batch_state_t* nimcp_lnn_ode_batch_state_create(
    nimcp_gpu_context_t* ctx,
    uint32_t batch_size,
    uint32_t n_neurons)
{
    return NULL;
}

void nimcp_lnn_ode_batch_state_destroy(nimcp_lnn_ode_batch_state_t* state)
{
    if (state) nimcp_free(state);
}

bool nimcp_lnn_ode_batch_state_reset(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_ode_batch_state_t* state,
    float initial_x)
{
    return false;
}

nimcp_lnn_ode_cache_t* nimcp_lnn_ode_cache_create(
    nimcp_gpu_context_t* ctx,
    uint32_t batch_size,
    uint32_t n_neurons,
    uint32_t n_stages)
{
    return NULL;
}

void nimcp_lnn_ode_cache_destroy(nimcp_lnn_ode_cache_t* cache)
{
    if (cache) nimcp_free(cache);
}

bool nimcp_gpu_lnn_check_stability(
    nimcp_gpu_context_t* ctx,
    const nimcp_lnn_ode_batch_state_t* batch_state,
    bool* has_nan,
    bool* has_inf,
    float* max_norm)
{
    return false;
}

bool nimcp_gpu_lnn_compute_state_stats(
    nimcp_gpu_context_t* ctx,
    const nimcp_lnn_ode_batch_state_t* batch_state,
    float* mean,
    float* std,
    float* min_val,
    float* max_val)
{
    return false;
}

// CPU fallbacks remain available even without CUDA

static inline float cpu_activation_fallback(float x, lnn_activation_t activation)
{
    switch (activation) {
        case LNN_ACTIVATION_TANH:
            return tanhf(x);
        case LNN_ACTIVATION_SIGMOID:
            return 1.0f / (1.0f + expf(-x));
        case LNN_ACTIVATION_RELU:
            return fmaxf(0.0f, x);
        case LNN_ACTIVATION_GELU: {
            float sqrt_2_over_pi = 0.7978845608f;
            float cdf = 0.5f * (1.0f + tanhf(sqrt_2_over_pi * (x + 0.044715f * x * x * x)));
            return x * cdf;
        }
        case LNN_ACTIVATION_SILU:
            return x / (1.0f + expf(-x));
        case LNN_ACTIVATION_SOFTPLUS:
            return logf(1.0f + expf(x));
        default:
            return tanhf(x);
    }
}

bool nimcp_cpu_lnn_euler_step_batched(
    const float* x,
    const float* dx_dt,
    float* x_new,
    uint32_t batch_size,
    uint32_t n_neurons,
    float dt)
{
    if (!x || !dx_dt || !x_new) return false;

    for (uint32_t b = 0; b < batch_size; b++) {
        for (uint32_t n = 0; n < n_neurons; n++) {
            uint32_t idx = b * n_neurons + n;
            x_new[idx] = x[idx] + dt * dx_dt[idx];
        }
    }

    return true;
}

bool nimcp_cpu_lnn_rk4_step_batched(
    const float* x,
    const float* tau,
    const float* input,
    const float* W_in,
    const float* W_rec,
    const float* b_in,
    float* x_new,
    uint32_t batch_size,
    uint32_t n_neurons,
    uint32_t n_inputs,
    float dt,
    lnn_activation_t activation)
{
    if (!x || !tau || !input || !W_in || !W_rec || !b_in || !x_new) return false;

    // Allocate temporary storage
    float* k1 = (float*)nimcp_malloc(batch_size * n_neurons * sizeof(float));
    float* k2 = (float*)nimcp_malloc(batch_size * n_neurons * sizeof(float));
    float* k3 = (float*)nimcp_malloc(batch_size * n_neurons * sizeof(float));
    float* k4 = (float*)nimcp_malloc(batch_size * n_neurons * sizeof(float));
    float* x_temp = (float*)nimcp_malloc(batch_size * n_neurons * sizeof(float));

    if (!k1 || !k2 || !k3 || !k4 || !x_temp) {
        nimcp_free(k1); nimcp_free(k2); nimcp_free(k3); nimcp_free(k4); nimcp_free(x_temp);
        return false;
    }

    // Compute derivative helper
    #define COMPUTE_DERIVATIVE(x_state, dx_out) \
        for (uint32_t b = 0; b < batch_size; b++) { \
            for (uint32_t i = 0; i < n_neurons; i++) { \
                uint32_t idx = b * n_neurons + i; \
                float sum = b_in[i]; \
                for (uint32_t j = 0; j < n_inputs; j++) { \
                    sum += W_in[i * n_inputs + j] * input[b * n_inputs + j]; \
                } \
                for (uint32_t j = 0; j < n_neurons; j++) { \
                    sum += W_rec[i * n_neurons + j] * x_state[b * n_neurons + j]; \
                } \
                dx_out[idx] = -x_state[idx] / tau[idx] + cpu_activation_fallback(sum, activation); \
            } \
        }

    COMPUTE_DERIVATIVE(x, k1);

    for (uint32_t i = 0; i < batch_size * n_neurons; i++) {
        x_temp[i] = x[i] + 0.5f * dt * k1[i];
    }
    COMPUTE_DERIVATIVE(x_temp, k2);

    for (uint32_t i = 0; i < batch_size * n_neurons; i++) {
        x_temp[i] = x[i] + 0.5f * dt * k2[i];
    }
    COMPUTE_DERIVATIVE(x_temp, k3);

    for (uint32_t i = 0; i < batch_size * n_neurons; i++) {
        x_temp[i] = x[i] + dt * k3[i];
    }
    COMPUTE_DERIVATIVE(x_temp, k4);

    for (uint32_t i = 0; i < batch_size * n_neurons; i++) {
        x_new[i] = x[i] + (dt / 6.0f) * (k1[i] + 2.0f * k2[i] + 2.0f * k3[i] + k4[i]);
    }

    #undef COMPUTE_DERIVATIVE

    nimcp_free(k1);
    nimcp_free(k2);
    nimcp_free(k3);
    nimcp_free(k4);
    nimcp_free(x_temp);

    return true;
}

#endif // NIMCP_ENABLE_CUDA
