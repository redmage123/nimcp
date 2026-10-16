/**
 * @file nimcp_lnn_kernels.cu
 * @brief GPU LNN CUDA Kernels Implementation
 *
 * WHAT: CUDA kernels for Liquid Neural Network (LNN) operations
 * WHY:  GPU acceleration for continuous-time neural dynamics
 * HOW:  Custom kernels for ODE integration (Euler, Heun, RK4, DOPRI5)
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

// Include CUDA headers FIRST (before any extern "C" blocks from our headers)
#include "utils/memory/nimcp_memory.h"
#include <cuda_runtime.h>
#include <cublas_v2.h>
#include <math.h>
#include <float.h>

// Now include our headers (which have extern "C" blocks)
#include "gpu/lnn/nimcp_lnn_gpu.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "constants/nimcp_math_constants.h"

#define LOG_MODULE "LNN_GPU"

#define BLOCK_SIZE 256
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)

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
    // Approximate GELU: 0.5 * x * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3)))
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

nimcp_lnn_ode_config_t nimcp_lnn_ode_default_config(void)
{
    nimcp_lnn_ode_config_t config;
    config.method = LNN_ODE_RK4;
    config.dt = 1.0f;           // 1ms default timestep
    config.dt_min = 0.01f;
    config.dt_max = 10.0f;
    config.error_tolerance = 1e-5f;
    config.max_steps = 1000;
    config.adaptive_stepping = false;
    return config;
}

//=============================================================================
// ODE Step Kernels
//=============================================================================

/**
 * @brief Euler step kernel: x_new = x + dt * dx_dt
 */
__global__ void kernel_euler_step(
    const float* x, const float* dx_dt, float dt, float* x_new, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        x_new[idx] = x[idx] + dt * dx_dt[idx];
    }
}

/**
 * @brief State clamping kernel to prevent numerical explosion
 * Clamps values to specified range and handles NaN/Inf
 */
__global__ void kernel_clamp_state(float* x, size_t n, float min_val, float max_val)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        float val = x[idx];
        if (isnan(val) || isinf(val)) {
            x[idx] = 0.0f;
        } else if (val < min_val) {
            x[idx] = min_val;
        } else if (val > max_val) {
            x[idx] = max_val;
        }
    }
}

// State clamping bounds for numerical stability
#define GPU_LNN_STATE_CLAMP_MIN -10.0f
#define GPU_LNN_STATE_CLAMP_MAX  10.0f

bool nimcp_gpu_lnn_euler_step(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* dx_dt,
    float dt,
    nimcp_gpu_tensor_t* x_new)
{
    if (!ctx || !x || !dx_dt || !x_new) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    kernel_euler_step<<<GRID_SIZE(x->numel), BLOCK_SIZE>>>(
        (const float*)x->data, (const float*)dx_dt->data,
        dt, (float*)x_new->data, x->numel);

    // Clamp state to prevent numerical explosion
    kernel_clamp_state<<<GRID_SIZE(x->numel), BLOCK_SIZE>>>(
        (float*)x_new->data, x->numel, GPU_LNN_STATE_CLAMP_MIN, GPU_LNN_STATE_CLAMP_MAX);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// LTC Derivative Computation
//=============================================================================

/**
 * @brief Kernel to compute input-dependent time constant tau
 *
 * tau(x, I) = tau_base * sigmoid(W_tau * [x; I] + b_tau)
 * where tau is clamped to [tau_min, tau_max]
 */
__global__ void kernel_compute_tau(
    const float* x,              // [n_neurons]
    const float* input,          // [n_inputs]
    const float* W_tau,          // [n_neurons, n_inputs + n_neurons]
    const float* b_tau,          // [n_neurons]
    const float* tau_base,       // [n_neurons]
    float tau_min,
    float tau_max,
    float* tau_out,              // [n_neurons]
    uint32_t n_neurons,
    uint32_t n_inputs)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_neurons) return;

    // Compute W_tau * [x; I] + b_tau
    float sum = b_tau[idx];
    uint32_t tau_width = n_inputs + n_neurons;

    // Input contribution
    for (uint32_t j = 0; j < n_inputs; j++) {
        sum += W_tau[idx * tau_width + j] * input[j];
    }

    // Recurrent state contribution
    for (uint32_t j = 0; j < n_neurons; j++) {
        sum += W_tau[idx * tau_width + n_inputs + j] * x[j];
    }

    // tau = tau_base * sigmoid(sum), clamped
    float tau = tau_base[idx] * device_sigmoid(sum);
    tau = fmaxf(tau_min, fminf(tau_max, tau));
    tau_out[idx] = tau;
}

bool nimcp_gpu_lnn_update_tau(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* input)
{
    if (!ctx || !layer || !input) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Default tau limits
    float tau_min = LNN_TAU_MIN_DEFAULT;
    float tau_max = LNN_TAU_MAX_DEFAULT;

    kernel_compute_tau<<<GRID_SIZE(layer->n_neurons), BLOCK_SIZE>>>(
        (const float*)layer->x->data,
        (const float*)input->data,
        (const float*)layer->W_tau->data,
        (const float*)layer->b_tau->data,
        (const float*)layer->tau_base->data,
        tau_min, tau_max,
        (float*)layer->tau->data,
        layer->n_neurons,
        layer->n_inputs);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel to compute LTC derivative
 *
 * dx/dt = -x/tau + f(W_in * I + W_rec * x + b)
 *
 * For sparse W_rec, this is handled separately
 */
__global__ void kernel_compute_derivative_dense(
    const float* x,              // [n_neurons]
    const float* input,          // [n_inputs]
    const float* tau,            // [n_neurons]
    const float* W_in,           // [n_neurons, n_inputs]
    const float* W_rec,          // [n_neurons, n_neurons] (dense)
    const float* b_in,           // [n_neurons]
    float* dx_dt,                // [n_neurons]
    uint32_t n_neurons,
    uint32_t n_inputs,
    int activation_type)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_neurons) return;

    // Compute weighted sum: W_in * I + W_rec * x + b
    float sum = b_in[idx];

    // Input contribution
    for (uint32_t j = 0; j < n_inputs; j++) {
        sum += W_in[idx * n_inputs + j] * input[j];
    }

    // Recurrent contribution (dense)
    for (uint32_t j = 0; j < n_neurons; j++) {
        sum += W_rec[idx * n_neurons + j] * x[j];
    }

    // Apply activation
    float activated = device_activation(sum, activation_type);

    // LTC dynamics: dx/dt = -x/tau + f(sum)
    dx_dt[idx] = -x[idx] / tau[idx] + activated;
}

/**
 * @brief Kernel for sparse W_rec using CSR format
 */
__global__ void kernel_compute_derivative_sparse(
    const float* x,              // [n_neurons]
    const float* input,          // [n_inputs]
    const float* tau,            // [n_neurons]
    const float* W_in,           // [n_neurons, n_inputs]
    const uint32_t* row_ptr,     // CSR row pointers [n_neurons + 1]
    const uint32_t* col_idx,     // CSR column indices
    const float* edge_weights,   // CSR values (or NULL for binary)
    const float* b_in,           // [n_neurons]
    float* dx_dt,                // [n_neurons]
    uint32_t n_neurons,
    uint32_t n_inputs,
    int activation_type)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_neurons) return;

    // Compute weighted sum: W_in * I + b
    float sum = b_in[idx];

    for (uint32_t j = 0; j < n_inputs; j++) {
        sum += W_in[idx * n_inputs + j] * input[j];
    }

    // Sparse recurrent contribution (CSR)
    uint32_t row_start = row_ptr[idx];
    uint32_t row_end = row_ptr[idx + 1];

    for (uint32_t k = row_start; k < row_end; k++) {
        uint32_t j = col_idx[k];
        float w = edge_weights ? edge_weights[k] : 1.0f;
        sum += w * x[j];
    }

    // Apply activation
    float activated = device_activation(sum, activation_type);

    // LTC dynamics: dx/dt = -x/tau + f(sum)
    dx_dt[idx] = -x[idx] / tau[idx] + activated;
}

bool nimcp_gpu_lnn_compute_derivative(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* dx_dt)
{
    if (!ctx || !layer || !input || !dx_dt) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // First update tau
    if (!nimcp_gpu_lnn_update_tau(ctx, layer, input)) {
        return false;
    }

    // Choose sparse or dense kernel
    if (layer->row_ptr && layer->col_idx) {
        // Sparse recurrent
        kernel_compute_derivative_sparse<<<GRID_SIZE(layer->n_neurons), BLOCK_SIZE>>>(
            (const float*)layer->x->data,
            (const float*)input->data,
            (const float*)layer->tau->data,
            (const float*)layer->W_in->data,
            (const uint32_t*)layer->row_ptr->data,
            (const uint32_t*)layer->col_idx->data,
            layer->edge_weights ? (const float*)layer->edge_weights->data : NULL,
            (const float*)layer->b_in->data,
            (float*)dx_dt->data,
            layer->n_neurons,
            layer->n_inputs,
            (int)layer->activation);
    } else {
        // Dense recurrent
        kernel_compute_derivative_dense<<<GRID_SIZE(layer->n_neurons), BLOCK_SIZE>>>(
            (const float*)layer->x->data,
            (const float*)input->data,
            (const float*)layer->tau->data,
            (const float*)layer->W_in->data,
            (const float*)layer->W_rec->data,
            (const float*)layer->b_in->data,
            (float*)dx_dt->data,
            layer->n_neurons,
            layer->n_inputs,
            (int)layer->activation);
    }

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// RK4 Implementation
//=============================================================================

/**
 * @brief Weighted sum kernel: y = a + scale * b
 */
__global__ void kernel_add_scaled(
    const float* a, const float* b, float scale, float* y, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        y[idx] = a[idx] + scale * b[idx];
    }
}

/**
 * @brief RK4 combination kernel: x_new = x + (dt/6) * (k1 + 2*k2 + 2*k3 + k4)
 */
__global__ void kernel_rk4_combine(
    const float* x, const float* k1, const float* k2,
    const float* k3, const float* k4, float dt, float* x_new, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        x_new[idx] = x[idx] + (dt / 6.0f) * (k1[idx] + 2.0f * k2[idx] + 2.0f * k3[idx] + k4[idx]);
    }
}

bool nimcp_gpu_lnn_rk4_step(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* input,
    float dt,
    const nimcp_lnn_ode_config_t* config)
{
    if (!ctx || !layer || !input) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t n = layer->n_neurons;

    // Allocate temporary tensors for k1, k2, k3, k4 and intermediate state
    size_t dims[] = {n};
    nimcp_gpu_tensor_t* k1 = nimcp_gpu_tensor_create(ctx, dims, 1, layer->x->precision);
    nimcp_gpu_tensor_t* k2 = nimcp_gpu_tensor_create(ctx, dims, 1, layer->x->precision);
    nimcp_gpu_tensor_t* k3 = nimcp_gpu_tensor_create(ctx, dims, 1, layer->x->precision);
    nimcp_gpu_tensor_t* k4 = nimcp_gpu_tensor_create(ctx, dims, 1, layer->x->precision);
    nimcp_gpu_tensor_t* x_temp = nimcp_gpu_tensor_create(ctx, dims, 1, layer->x->precision);
    nimcp_gpu_tensor_t* x_orig = nimcp_gpu_tensor_clone(layer->x);

    if (!k1 || !k2 || !k3 || !k4 || !x_temp || !x_orig) {
        nimcp_gpu_tensor_destroy(k1);
        nimcp_gpu_tensor_destroy(k2);
        nimcp_gpu_tensor_destroy(k3);
        nimcp_gpu_tensor_destroy(k4);
        nimcp_gpu_tensor_destroy(x_temp);
        nimcp_gpu_tensor_destroy(x_orig);
        return false;
    }

    // k1 = f(t, x)
    nimcp_gpu_lnn_compute_derivative(ctx, layer, input, k1);

    // x_temp = x + 0.5 * dt * k1
    kernel_add_scaled<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (const float*)x_orig->data, (const float*)k1->data,
        0.5f * dt, (float*)x_temp->data, n);
    cudaMemcpy(layer->x->data, x_temp->data, n * sizeof(float), cudaMemcpyDeviceToDevice);

    // k2 = f(t + dt/2, x + dt/2 * k1)
    nimcp_gpu_lnn_compute_derivative(ctx, layer, input, k2);

    // x_temp = x + 0.5 * dt * k2
    kernel_add_scaled<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (const float*)x_orig->data, (const float*)k2->data,
        0.5f * dt, (float*)x_temp->data, n);
    cudaMemcpy(layer->x->data, x_temp->data, n * sizeof(float), cudaMemcpyDeviceToDevice);

    // k3 = f(t + dt/2, x + dt/2 * k2)
    nimcp_gpu_lnn_compute_derivative(ctx, layer, input, k3);

    // x_temp = x + dt * k3
    kernel_add_scaled<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (const float*)x_orig->data, (const float*)k3->data,
        dt, (float*)x_temp->data, n);
    cudaMemcpy(layer->x->data, x_temp->data, n * sizeof(float), cudaMemcpyDeviceToDevice);

    // k4 = f(t + dt, x + dt * k3)
    nimcp_gpu_lnn_compute_derivative(ctx, layer, input, k4);

    // x_new = x + (dt/6) * (k1 + 2*k2 + 2*k3 + k4)
    kernel_rk4_combine<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (const float*)x_orig->data,
        (const float*)k1->data, (const float*)k2->data,
        (const float*)k3->data, (const float*)k4->data,
        dt, (float*)layer->x->data, n);

    // Clamp state to prevent numerical explosion
    kernel_clamp_state<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)layer->x->data, n, GPU_LNN_STATE_CLAMP_MIN, GPU_LNN_STATE_CLAMP_MAX);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Cleanup
    nimcp_gpu_tensor_destroy(k1);
    nimcp_gpu_tensor_destroy(k2);
    nimcp_gpu_tensor_destroy(k3);
    nimcp_gpu_tensor_destroy(k4);
    nimcp_gpu_tensor_destroy(x_temp);
    nimcp_gpu_tensor_destroy(x_orig);

    return true;
}

//=============================================================================
// Heun (RK2) Implementation
//=============================================================================

bool nimcp_gpu_lnn_heun_step(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* input,
    float dt,
    const nimcp_lnn_ode_config_t* config)
{
    if (!ctx || !layer || !input) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t n = layer->n_neurons;
    size_t dims[] = {n};

    nimcp_gpu_tensor_t* k1 = nimcp_gpu_tensor_create(ctx, dims, 1, layer->x->precision);
    nimcp_gpu_tensor_t* k2 = nimcp_gpu_tensor_create(ctx, dims, 1, layer->x->precision);
    nimcp_gpu_tensor_t* x_temp = nimcp_gpu_tensor_create(ctx, dims, 1, layer->x->precision);
    nimcp_gpu_tensor_t* x_orig = nimcp_gpu_tensor_clone(layer->x);

    if (!k1 || !k2 || !x_temp || !x_orig) {
        nimcp_gpu_tensor_destroy(k1);
        nimcp_gpu_tensor_destroy(k2);
        nimcp_gpu_tensor_destroy(x_temp);
        nimcp_gpu_tensor_destroy(x_orig);
        return false;
    }

    // k1 = f(t, x)
    nimcp_gpu_lnn_compute_derivative(ctx, layer, input, k1);

    // x_temp = x + dt * k1 (predictor)
    kernel_add_scaled<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (const float*)x_orig->data, (const float*)k1->data,
        dt, (float*)x_temp->data, n);
    cudaMemcpy(layer->x->data, x_temp->data, n * sizeof(float), cudaMemcpyDeviceToDevice);

    // k2 = f(t + dt, x + dt * k1)
    nimcp_gpu_lnn_compute_derivative(ctx, layer, input, k2);

    // x_new = x + 0.5 * dt * (k1 + k2) (corrector)
    // Inline kernel launch
    kernel_add_scaled<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (const float*)k1->data, (const float*)k2->data,
        1.0f, (float*)x_temp->data, n);  // k1 + k2

    kernel_add_scaled<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (const float*)x_orig->data, (const float*)x_temp->data,
        0.5f * dt, (float*)layer->x->data, n);  // x + 0.5*dt*(k1+k2)

    // Clamp state to prevent numerical explosion
    kernel_clamp_state<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)layer->x->data, n, GPU_LNN_STATE_CLAMP_MIN, GPU_LNN_STATE_CLAMP_MAX);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    nimcp_gpu_tensor_destroy(k1);
    nimcp_gpu_tensor_destroy(k2);
    nimcp_gpu_tensor_destroy(x_temp);
    nimcp_gpu_tensor_destroy(x_orig);

    return true;
}

//=============================================================================
// DOPRI5 Adaptive Implementation
//=============================================================================

/**
 * @brief Kernel to compute weighted sum for DOPRI5 stages
 *
 * Computes y = x + dt * (a1*k1 + a2*k2 + a3*k3 + a4*k4 + a5*k5 + a6*k6)
 */
__global__ void kernel_dopri5_stage(
    const float* x, const float* k1, const float* k2, const float* k3,
    const float* k4, const float* k5, const float* k6,
    float dt, float a1, float a2, float a3, float a4, float a5, float a6,
    float* y, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        y[idx] = x[idx] + dt * (a1 * k1[idx] + a2 * k2[idx] + a3 * k3[idx] +
                                 a4 * k4[idx] + a5 * k5[idx] + a6 * k6[idx]);
    }
}

/**
 * @brief Kernel to compute DOPRI5 5th order solution
 */
__global__ void kernel_dopri5_solution(
    const float* x, const float* k1, const float* k3,
    const float* k4, const float* k5, const float* k6, const float* k7,
    float dt, float* x_new, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        // 5th order solution coefficients
        const float b1 = 35.0f / 384.0f;
        const float b3 = 500.0f / 1113.0f;
        const float b4 = 125.0f / 192.0f;
        const float b5 = -2187.0f / 6784.0f;
        const float b6 = 11.0f / 84.0f;
        // b7 = 0 (FSAL property)

        x_new[idx] = x[idx] + dt * (b1 * k1[idx] + b3 * k3[idx] + b4 * k4[idx] +
                                     b5 * k5[idx] + b6 * k6[idx]);
    }
}

/**
 * @brief Kernel to compute error estimate for adaptive stepping
 */
__global__ void kernel_dopri5_error(
    const float* k1, const float* k3, const float* k4,
    const float* k5, const float* k6, const float* k7,
    float dt, float* error, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        // Error coefficients (difference between 5th and 4th order)
        const float e1 = 71.0f / 57600.0f;
        const float e3 = -71.0f / 16695.0f;
        const float e4 = 71.0f / 1920.0f;
        const float e5 = -17253.0f / 339200.0f;
        const float e6 = 22.0f / 525.0f;
        const float e7 = -1.0f / 40.0f;

        error[idx] = dt * (e1 * k1[idx] + e3 * k3[idx] + e4 * k4[idx] +
                           e5 * k5[idx] + e6 * k6[idx] + e7 * k7[idx]);
    }
}

/**
 * @brief Kernel to compute max absolute error using parallel reduction
 */
__global__ void kernel_max_abs_reduce(const float* data, float* result, size_t n)
{
    extern __shared__ float sdata[];

    size_t tid = threadIdx.x;
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    // Load data into shared memory
    sdata[tid] = (idx < n) ? fabsf(data[idx]) : 0.0f;
    __syncthreads();

    // Parallel reduction for max
    for (size_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            sdata[tid] = fmaxf(sdata[tid], sdata[tid + s]);
        }
        __syncthreads();
    }

    // Write result from first thread
    if (tid == 0) {
        atomicMax((int*)result, __float_as_int(sdata[0]));
    }
}

bool nimcp_gpu_lnn_dopri5_step(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* input,
    float* dt_ptr,
    const nimcp_lnn_ode_config_t* config)
{
    if (!ctx || !layer || !input || !dt_ptr || !config) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    float dt = *dt_ptr;
    size_t n = layer->n_neurons;
    size_t dims[] = {n};

    // Allocate k1-k7 and temporary tensors
    nimcp_gpu_tensor_t* k1 = nimcp_gpu_tensor_create(ctx, dims, 1, layer->x->precision);
    nimcp_gpu_tensor_t* k2 = nimcp_gpu_tensor_create(ctx, dims, 1, layer->x->precision);
    nimcp_gpu_tensor_t* k3 = nimcp_gpu_tensor_create(ctx, dims, 1, layer->x->precision);
    nimcp_gpu_tensor_t* k4 = nimcp_gpu_tensor_create(ctx, dims, 1, layer->x->precision);
    nimcp_gpu_tensor_t* k5 = nimcp_gpu_tensor_create(ctx, dims, 1, layer->x->precision);
    nimcp_gpu_tensor_t* k6 = nimcp_gpu_tensor_create(ctx, dims, 1, layer->x->precision);
    nimcp_gpu_tensor_t* k7 = nimcp_gpu_tensor_create(ctx, dims, 1, layer->x->precision);
    nimcp_gpu_tensor_t* x_temp = nimcp_gpu_tensor_create(ctx, dims, 1, layer->x->precision);
    nimcp_gpu_tensor_t* x_orig = nimcp_gpu_tensor_clone(layer->x);
    nimcp_gpu_tensor_t* error_vec = nimcp_gpu_tensor_create(ctx, dims, 1, layer->x->precision);

    if (!k1 || !k2 || !k3 || !k4 || !k5 || !k6 || !k7 || !x_temp || !x_orig || !error_vec) {
        nimcp_gpu_tensor_destroy(k1);
        nimcp_gpu_tensor_destroy(k2);
        nimcp_gpu_tensor_destroy(k3);
        nimcp_gpu_tensor_destroy(k4);
        nimcp_gpu_tensor_destroy(k5);
        nimcp_gpu_tensor_destroy(k6);
        nimcp_gpu_tensor_destroy(k7);
        nimcp_gpu_tensor_destroy(x_temp);
        nimcp_gpu_tensor_destroy(x_orig);
        nimcp_gpu_tensor_destroy(error_vec);
        return false;
    }

    bool accepted = false;
    int max_iterations = 10;
    int iteration = 0;

    while (!accepted && iteration < max_iterations) {
        iteration++;

        // k1 = f(t, x)
        cudaMemcpy(layer->x->data, x_orig->data, n * sizeof(float), cudaMemcpyDeviceToDevice);
        nimcp_gpu_lnn_compute_derivative(ctx, layer, input, k1);

        // k2 = f(t + c2*dt, x + dt*a21*k1)
        // c2 = 1/5, a21 = 1/5
        kernel_add_scaled<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const float*)x_orig->data, (const float*)k1->data,
            dt * (1.0f/5.0f), (float*)x_temp->data, n);
        cudaMemcpy(layer->x->data, x_temp->data, n * sizeof(float), cudaMemcpyDeviceToDevice);
        nimcp_gpu_lnn_compute_derivative(ctx, layer, input, k2);

        // k3 = f(t + c3*dt, x + dt*(a31*k1 + a32*k2))
        // c3 = 3/10, a31 = 3/40, a32 = 9/40
        kernel_dopri5_stage<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const float*)x_orig->data,
            (const float*)k1->data, (const float*)k2->data, (const float*)k2->data,
            (const float*)k2->data, (const float*)k2->data, (const float*)k2->data,
            dt, 3.0f/40.0f, 9.0f/40.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            (float*)x_temp->data, n);
        cudaMemcpy(layer->x->data, x_temp->data, n * sizeof(float), cudaMemcpyDeviceToDevice);
        nimcp_gpu_lnn_compute_derivative(ctx, layer, input, k3);

        // k4 = f(t + c4*dt, x + dt*(a41*k1 + a42*k2 + a43*k3))
        // c4 = 4/5, a41 = 44/45, a42 = -56/15, a43 = 32/9
        kernel_dopri5_stage<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const float*)x_orig->data,
            (const float*)k1->data, (const float*)k2->data, (const float*)k3->data,
            (const float*)k3->data, (const float*)k3->data, (const float*)k3->data,
            dt, 44.0f/45.0f, -56.0f/15.0f, 32.0f/9.0f, 0.0f, 0.0f, 0.0f,
            (float*)x_temp->data, n);
        cudaMemcpy(layer->x->data, x_temp->data, n * sizeof(float), cudaMemcpyDeviceToDevice);
        nimcp_gpu_lnn_compute_derivative(ctx, layer, input, k4);

        // k5 = f(t + c5*dt, x + dt*(a51*k1 + a52*k2 + a53*k3 + a54*k4))
        // c5 = 8/9, a51 = 19372/6561, a52 = -25360/2187, a53 = 64448/6561, a54 = -212/729
        kernel_dopri5_stage<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const float*)x_orig->data,
            (const float*)k1->data, (const float*)k2->data, (const float*)k3->data,
            (const float*)k4->data, (const float*)k4->data, (const float*)k4->data,
            dt, 19372.0f/6561.0f, -25360.0f/2187.0f, 64448.0f/6561.0f, -212.0f/729.0f, 0.0f, 0.0f,
            (float*)x_temp->data, n);
        cudaMemcpy(layer->x->data, x_temp->data, n * sizeof(float), cudaMemcpyDeviceToDevice);
        nimcp_gpu_lnn_compute_derivative(ctx, layer, input, k5);

        // k6 = f(t + dt, x + dt*(a61*k1 + a62*k2 + a63*k3 + a64*k4 + a65*k5))
        // a61 = 9017/3168, a62 = -355/33, a63 = 46732/5247, a64 = 49/176, a65 = -5103/18656
        kernel_dopri5_stage<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const float*)x_orig->data,
            (const float*)k1->data, (const float*)k2->data, (const float*)k3->data,
            (const float*)k4->data, (const float*)k5->data, (const float*)k5->data,
            dt, 9017.0f/3168.0f, -355.0f/33.0f, 46732.0f/5247.0f, 49.0f/176.0f, -5103.0f/18656.0f, 0.0f,
            (float*)x_temp->data, n);
        cudaMemcpy(layer->x->data, x_temp->data, n * sizeof(float), cudaMemcpyDeviceToDevice);
        nimcp_gpu_lnn_compute_derivative(ctx, layer, input, k6);

        // Compute 5th order solution
        kernel_dopri5_solution<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const float*)x_orig->data,
            (const float*)k1->data, (const float*)k3->data, (const float*)k4->data,
            (const float*)k5->data, (const float*)k6->data, (const float*)k6->data,
            dt, (float*)x_temp->data, n);

        // k7 = f(t + dt, x_new) - for FSAL property
        cudaMemcpy(layer->x->data, x_temp->data, n * sizeof(float), cudaMemcpyDeviceToDevice);
        nimcp_gpu_lnn_compute_derivative(ctx, layer, input, k7);

        if (config->adaptive_stepping) {
            // Compute error vector
            kernel_dopri5_error<<<GRID_SIZE(n), BLOCK_SIZE>>>(
                (const float*)k1->data, (const float*)k3->data, (const float*)k4->data,
                (const float*)k5->data, (const float*)k6->data, (const float*)k7->data,
                dt, (float*)error_vec->data, n);

            // Compute max error using reduction
            float* d_max_error;
            float h_max_error = 0.0f;
            if (cudaMalloc(&d_max_error, sizeof(float)) != cudaSuccess) {
                break;  // Fall back to non-adaptive step
            }
            cudaMemset(d_max_error, 0, sizeof(float));

            kernel_max_abs_reduce<<<GRID_SIZE(n), BLOCK_SIZE, BLOCK_SIZE * sizeof(float)>>>(
                (const float*)error_vec->data, d_max_error, n);

            cudaMemcpy(&h_max_error, d_max_error, sizeof(float), cudaMemcpyDeviceToHost);
            cudaFree(d_max_error);

            // Adaptive step size control
            float tol = config->error_tolerance;
            if (h_max_error < tol || h_max_error == 0.0f) {
                // Accept step
                accepted = true;
                cudaMemcpy(layer->x->data, x_temp->data, n * sizeof(float), cudaMemcpyDeviceToDevice);

                // Compute new dt for next step
                if (h_max_error > 0.0f) {
                    float safety = 0.9f;
                    float order = 5.0f;
                    float factor = safety * powf(tol / h_max_error, 1.0f / order);
                    factor = fminf(fmaxf(factor, 0.1f), 5.0f);  // Limit growth/shrink
                    *dt_ptr = fminf(fmaxf(dt * factor, config->dt_min), config->dt_max);
                }
            } else {
                // Reject step, reduce dt
                float safety = 0.9f;
                float order = 5.0f;
                float factor = safety * powf(tol / h_max_error, 1.0f / order);
                factor = fmaxf(factor, 0.1f);
                dt = fmaxf(dt * factor, config->dt_min);

                if (dt <= config->dt_min) {
                    // Accept with minimum dt
                    accepted = true;
                    cudaMemcpy(layer->x->data, x_temp->data, n * sizeof(float), cudaMemcpyDeviceToDevice);
                    *dt_ptr = config->dt_min;
                }
            }
        } else {
            // Non-adaptive: always accept
            accepted = true;
            cudaMemcpy(layer->x->data, x_temp->data, n * sizeof(float), cudaMemcpyDeviceToDevice);
        }
    }

    // Clamp state to prevent numerical explosion
    kernel_clamp_state<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)layer->x->data, n, GPU_LNN_STATE_CLAMP_MIN, GPU_LNN_STATE_CLAMP_MAX);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Cleanup
    nimcp_gpu_tensor_destroy(k1);
    nimcp_gpu_tensor_destroy(k2);
    nimcp_gpu_tensor_destroy(k3);
    nimcp_gpu_tensor_destroy(k4);
    nimcp_gpu_tensor_destroy(k5);
    nimcp_gpu_tensor_destroy(k6);
    nimcp_gpu_tensor_destroy(k7);
    nimcp_gpu_tensor_destroy(x_temp);
    nimcp_gpu_tensor_destroy(x_orig);
    nimcp_gpu_tensor_destroy(error_vec);

    return accepted;
}

//=============================================================================
// Unified ODE Step
//=============================================================================

bool nimcp_gpu_lnn_ode_step(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* input,
    const nimcp_lnn_ode_config_t* config)
{
    if (!ctx || !layer || !input || !config) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    switch (config->method) {
        case LNN_ODE_EULER: {
            // Simple Euler step
            nimcp_gpu_lnn_compute_derivative(ctx, layer, input, layer->dx_dt);
            return nimcp_gpu_lnn_euler_step(ctx, layer->x, layer->dx_dt, config->dt, layer->x);
        }

        case LNN_ODE_HEUN:
            return nimcp_gpu_lnn_heun_step(ctx, layer, input, config->dt, config);

        case LNN_ODE_RK4:
            return nimcp_gpu_lnn_rk4_step(ctx, layer, input, config->dt, config);

        case LNN_ODE_DOPRI5: {
            float dt = config->dt;
            return nimcp_gpu_lnn_dopri5_step(ctx, layer, input, &dt, config);
        }

        default:
            LOG_ERROR("Unknown ODE method: %d", config->method);
            return false;
    }
}

//=============================================================================
// Sparse Matrix Operations
//=============================================================================

/**
 * @brief Sparse matrix-vector product kernel (CSR format)
 */
__global__ void kernel_sparse_matvec(
    const uint32_t* row_ptr, const uint32_t* col_idx, const float* values,
    const float* x, float* y, uint32_t n_rows, float alpha)
{
    uint32_t row = blockIdx.x * blockDim.x + threadIdx.x;
    if (row >= n_rows) return;

    float sum = 0.0f;
    uint32_t start = row_ptr[row];
    uint32_t end = row_ptr[row + 1];

    for (uint32_t k = start; k < end; k++) {
        float w = values ? values[k] : 1.0f;
        sum += w * x[col_idx[k]];
    }

    y[row] = alpha * sum;
}

bool nimcp_gpu_sparse_matvec(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* row_ptr,
    const nimcp_gpu_tensor_t* col_idx,
    const nimcp_gpu_tensor_t* values,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* y,
    uint32_t n_rows,
    float alpha)
{
    if (!ctx || !row_ptr || !col_idx || !x || !y) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    kernel_sparse_matvec<<<GRID_SIZE(n_rows), BLOCK_SIZE>>>(
        (const uint32_t*)row_ptr->data,
        (const uint32_t*)col_idx->data,
        values ? (const float*)values->data : NULL,
        (const float*)x->data,
        (float*)y->data,
        n_rows,
        alpha);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel to count non-zeros per row for sparse add (phase 1)
 */
__global__ void kernel_sparse_add_count_nnz(
    const uint32_t* A_row_ptr, const uint32_t* A_col_idx,
    const uint32_t* B_row_ptr, const uint32_t* B_col_idx,
    uint32_t* C_row_nnz, uint32_t n_rows)
{
    uint32_t row = blockIdx.x * blockDim.x + threadIdx.x;
    if (row >= n_rows) return;

    uint32_t A_start = A_row_ptr[row], A_end = A_row_ptr[row + 1];
    uint32_t B_start = B_row_ptr[row], B_end = B_row_ptr[row + 1];

    uint32_t A_idx = A_start, B_idx = B_start;
    uint32_t count = 0;

    // Merge-sort style counting
    while (A_idx < A_end && B_idx < B_end) {
        uint32_t A_col = A_col_idx[A_idx];
        uint32_t B_col = B_col_idx[B_idx];

        if (A_col < B_col) {
            A_idx++;
        } else if (B_col < A_col) {
            B_idx++;
        } else {
            A_idx++;
            B_idx++;
        }
        count++;
    }

    // Remaining elements
    count += (A_end - A_idx) + (B_end - B_idx);
    C_row_nnz[row] = count;
}

/**
 * @brief Kernel to compute sparse add values (phase 2)
 */
__global__ void kernel_sparse_add_compute(
    const uint32_t* A_row_ptr, const uint32_t* A_col_idx, const float* A_values,
    const uint32_t* B_row_ptr, const uint32_t* B_col_idx, const float* B_values,
    const uint32_t* C_row_ptr, uint32_t* C_col_idx, float* C_values,
    uint32_t n_rows, float alpha, float beta)
{
    uint32_t row = blockIdx.x * blockDim.x + threadIdx.x;
    if (row >= n_rows) return;

    uint32_t A_start = A_row_ptr[row], A_end = A_row_ptr[row + 1];
    uint32_t B_start = B_row_ptr[row], B_end = B_row_ptr[row + 1];
    uint32_t C_start = C_row_ptr[row];

    uint32_t A_idx = A_start, B_idx = B_start;
    uint32_t C_idx = C_start;

    // Merge-sort style addition
    while (A_idx < A_end && B_idx < B_end) {
        uint32_t A_col = A_col_idx[A_idx];
        uint32_t B_col = B_col_idx[B_idx];

        if (A_col < B_col) {
            C_col_idx[C_idx] = A_col;
            C_values[C_idx] = alpha * (A_values ? A_values[A_idx] : 1.0f);
            A_idx++;
        } else if (B_col < A_col) {
            C_col_idx[C_idx] = B_col;
            C_values[C_idx] = beta * (B_values ? B_values[B_idx] : 1.0f);
            B_idx++;
        } else {
            // Same column: add values
            C_col_idx[C_idx] = A_col;
            float a_val = A_values ? A_values[A_idx] : 1.0f;
            float b_val = B_values ? B_values[B_idx] : 1.0f;
            C_values[C_idx] = alpha * a_val + beta * b_val;
            A_idx++;
            B_idx++;
        }
        C_idx++;
    }

    // Remaining A elements
    while (A_idx < A_end) {
        C_col_idx[C_idx] = A_col_idx[A_idx];
        C_values[C_idx] = alpha * (A_values ? A_values[A_idx] : 1.0f);
        A_idx++;
        C_idx++;
    }

    // Remaining B elements
    while (B_idx < B_end) {
        C_col_idx[C_idx] = B_col_idx[B_idx];
        C_values[C_idx] = beta * (B_values ? B_values[B_idx] : 1.0f);
        B_idx++;
        C_idx++;
    }
}

/**
 * @brief Prefix sum kernel for computing row pointers
 */
__global__ void kernel_prefix_sum(const uint32_t* input, uint32_t* output, uint32_t n)
{
    extern __shared__ uint32_t temp[];

    uint32_t tid = threadIdx.x;
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    // Load input into shared memory with exclusive scan offset
    temp[tid] = (idx > 0 && idx <= n) ? input[idx - 1] : 0;
    __syncthreads();

    // Up-sweep phase (reduce)
    for (uint32_t stride = 1; stride < blockDim.x; stride *= 2) {
        uint32_t index = (tid + 1) * stride * 2 - 1;
        if (index < blockDim.x) {
            temp[index] += temp[index - stride];
        }
        __syncthreads();
    }

    // Down-sweep phase
    if (tid == 0) temp[blockDim.x - 1] = 0;
    __syncthreads();

    for (uint32_t stride = blockDim.x / 2; stride > 0; stride /= 2) {
        uint32_t index = (tid + 1) * stride * 2 - 1;
        if (index < blockDim.x) {
            uint32_t t = temp[index - stride];
            temp[index - stride] = temp[index];
            temp[index] += t;
        }
        __syncthreads();
    }

    // Write output
    if (idx <= n) {
        output[idx] = temp[tid];
    }
}

bool nimcp_gpu_sparse_add(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* A_row_ptr,
    const nimcp_gpu_tensor_t* A_col_idx,
    const nimcp_gpu_tensor_t* A_values,
    const nimcp_gpu_tensor_t* B_row_ptr,
    const nimcp_gpu_tensor_t* B_col_idx,
    const nimcp_gpu_tensor_t* B_values,
    nimcp_gpu_tensor_t* C_row_ptr,
    nimcp_gpu_tensor_t* C_col_idx,
    nimcp_gpu_tensor_t* C_values,
    uint32_t n_rows,
    float alpha,
    float beta)
{
    if (!ctx || !A_row_ptr || !A_col_idx || !B_row_ptr || !B_col_idx ||
        !C_row_ptr || !C_col_idx || !C_values) {
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Phase 1: Count nnz per row for C
    uint32_t* d_C_row_nnz = NULL;
    uint32_t* h_C_row_nnz = NULL;
    uint32_t* h_C_row_ptr = NULL;
    bool success = false;

    if (cudaMalloc(&d_C_row_nnz, n_rows * sizeof(uint32_t)) != cudaSuccess) goto sparse_add_cleanup;

    kernel_sparse_add_count_nnz<<<GRID_SIZE(n_rows), BLOCK_SIZE>>>(
        (const uint32_t*)A_row_ptr->data, (const uint32_t*)A_col_idx->data,
        (const uint32_t*)B_row_ptr->data, (const uint32_t*)B_col_idx->data,
        d_C_row_nnz, n_rows);

    if (cudaGetLastError() != cudaSuccess) goto sparse_add_cleanup;

    // Phase 2: Compute row pointers via prefix sum
    // Simple sequential prefix sum on CPU for now (can be parallelized)
    h_C_row_nnz = (uint32_t*)nimcp_malloc(n_rows * sizeof(uint32_t));
    if (!h_C_row_nnz) goto sparse_add_cleanup;
    h_C_row_ptr = (uint32_t*)nimcp_malloc((n_rows + 1) * sizeof(uint32_t));
    if (!h_C_row_ptr) goto sparse_add_cleanup;

    if (cudaMemcpy(h_C_row_nnz, d_C_row_nnz, n_rows * sizeof(uint32_t), cudaMemcpyDeviceToHost) != cudaSuccess) goto sparse_add_cleanup;

    h_C_row_ptr[0] = 0;
    for (uint32_t i = 0; i < n_rows; i++) {
        h_C_row_ptr[i + 1] = h_C_row_ptr[i] + h_C_row_nnz[i];
    }

    // Copy row pointers to GPU
    if (cudaMemcpy(C_row_ptr->data, h_C_row_ptr, (n_rows + 1) * sizeof(uint32_t), cudaMemcpyHostToDevice) != cudaSuccess) goto sparse_add_cleanup;

    // Resize C_col_idx and C_values if needed (caller must ensure sufficient space)

    // Phase 3: Compute values
    kernel_sparse_add_compute<<<GRID_SIZE(n_rows), BLOCK_SIZE>>>(
        (const uint32_t*)A_row_ptr->data, (const uint32_t*)A_col_idx->data,
        A_values ? (const float*)A_values->data : NULL,
        (const uint32_t*)B_row_ptr->data, (const uint32_t*)B_col_idx->data,
        B_values ? (const float*)B_values->data : NULL,
        (const uint32_t*)C_row_ptr->data, (uint32_t*)C_col_idx->data, (float*)C_values->data,
        n_rows, alpha, beta);

    if (cudaGetLastError() != cudaSuccess) goto sparse_add_cleanup;

    success = true;

sparse_add_cleanup:
    cudaFree(d_C_row_nnz);
    nimcp_free(h_C_row_nnz);
    nimcp_free(h_C_row_ptr);

    return success;
}

//=============================================================================
// Adjoint Method for Gradient Computation
//=============================================================================

bool nimcp_gpu_lnn_adjoint_init(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* adjoint)
{
    if (!ctx || !layer || !grad_output || !adjoint) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Initialize adjoint: lambda(T) = dL/dx(T)
    NIMCP_CUDA_RECOVER(cudaMemcpy(adjoint->data, grad_output->data,
                          layer->n_neurons * sizeof(float), cudaMemcpyDeviceToDevice), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

/**
 * @brief Kernel for adjoint derivative computation
 *
 * d_lambda/dt = -lambda * (df/dx)
 * where df/dx is the Jacobian of the dynamics w.r.t. state
 */
__global__ void kernel_adjoint_derivative(
    const float* lambda,         // [n_neurons]
    const float* x,              // [n_neurons] state at time t
    const float* tau,            // [n_neurons]
    const float* W_rec,          // [n_neurons, n_neurons]
    float* d_lambda,             // [n_neurons]
    uint32_t n_neurons,
    int activation_type)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_neurons) return;

    // df/dx_i for LTC: -1/tau_i + sum_j(W_rec[j,i] * f'(sum_j) * lambda_j)
    // Simplified version: just the diagonal term
    d_lambda[idx] = -lambda[idx] / tau[idx];
}

bool nimcp_gpu_lnn_adjoint_step(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    nimcp_gpu_tensor_t* adjoint,
    const nimcp_gpu_tensor_t* x_at_t,
    const nimcp_gpu_tensor_t* input_at_t,
    float dt)
{
    if (!ctx || !layer || !adjoint || !x_at_t) return false;
    (void)input_at_t; /* Not used in adjoint step — only in accumulate_gradients */

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t n = layer->n_neurons;
    size_t dims[] = {n};

    // Allocate d_lambda
    nimcp_gpu_tensor_t* d_lambda = nimcp_gpu_tensor_create(ctx, dims, 1, adjoint->precision);
    if (!d_lambda) return false;

    // Compute adjoint derivative
    kernel_adjoint_derivative<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (const float*)adjoint->data,
        (const float*)x_at_t->data,
        (const float*)layer->tau->data,
        layer->W_rec ? (const float*)layer->W_rec->data : NULL,
        (float*)d_lambda->data,
        n,
        (int)layer->activation);

    // Update adjoint: lambda = lambda - dt * d_lambda (backward in time)
    kernel_add_scaled<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (const float*)adjoint->data, (const float*)d_lambda->data,
        -dt, (float*)adjoint->data, n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    nimcp_gpu_tensor_destroy(d_lambda);

    return true;
}

bool nimcp_gpu_lnn_accumulate_gradients(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* adjoint,
    const nimcp_gpu_tensor_t* x_at_t,
    const nimcp_gpu_tensor_t* input_at_t,
    float dt)
{
    if (!ctx || !layer || !adjoint || !x_at_t || !input_at_t) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Gradient accumulation: grad_W += dt * outer(adjoint, x)
    // This is a simplified version; full implementation needs chain rule

    // Using cuBLAS SGER for outer product accumulation
    cublasHandle_t handle = (cublasHandle_t)ctx->cublas_handle;
    if (!handle) {
        LOG_ERROR("cuBLAS handle not initialized");
        return false;
    }

    size_t n = layer->n_neurons;
    size_t d = layer->n_inputs;

    // Accumulate gradients using cuBLAS SGER (rank-1 update): A += alpha * x * y^T
    float alpha = dt;

    // grad_W_in += dt * outer(adjoint, input) if W_in gradients exist
    // For now, use kernel-based accumulation since we have dedicated gradient kernels
    // This integrates with the gradient DAO pattern

    // Accumulate gradients into layer's W_rec gradient buffer via outer product
    // grad_W_rec += dt * adjoint * x^T
    if (layer->W_rec) {
        cublasSger(handle, n, n, &alpha,
                   (const float*)adjoint->data, 1,
                   (const float*)x_at_t->data, 1,
                   (float*)layer->W_rec->data, n);
    }

    // Accumulate input weight gradients
    // grad_W_in += dt * adjoint * input^T
    if (layer->W_in && input_at_t) {
        cublasSger(handle, n, d, &alpha,
                   (const float*)adjoint->data, 1,
                   (const float*)input_at_t->data, 1,
                   (float*)layer->W_in->data, n);
    }

    LOG_DEBUG("Accumulated LNN gradients via cuBLAS SGER");

    return true;
}

//=============================================================================
// Layer Lifecycle
//=============================================================================

nimcp_lnn_layer_gpu_t* nimcp_lnn_layer_gpu_create(
    nimcp_gpu_context_t* ctx,
    const lnn_layer_t* cpu_layer)
{
    if (!ctx || !cpu_layer) return NULL;

    nimcp_lnn_layer_gpu_t* layer = (nimcp_lnn_layer_gpu_t*)nimcp_calloc(1, sizeof(nimcp_lnn_layer_gpu_t));
    if (!layer) return NULL;

    layer->n_neurons = cpu_layer->n_neurons;
    layer->n_inputs = cpu_layer->W_in ? nimcp_tensor_shape(cpu_layer->W_in)->dims[1] : 0;
    layer->activation = cpu_layer->neurons ? cpu_layer->neurons[0].activation : LNN_ACTIVATION_TANH;

    // Convert CPU tensors to GPU
    if (cpu_layer->x) {
        layer->x = nimcp_gpu_tensor_from_cpu(ctx, cpu_layer->x);
    }
    if (cpu_layer->dx_dt) {
        layer->dx_dt = nimcp_gpu_tensor_from_cpu(ctx, cpu_layer->dx_dt);
    } else {
        // Create dx_dt if not present
        size_t dims[] = {layer->n_neurons};
        layer->dx_dt = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    }
    if (cpu_layer->tau) {
        layer->tau = nimcp_gpu_tensor_from_cpu(ctx, cpu_layer->tau);
    }
    if (cpu_layer->tau_base) {
        layer->tau_base = nimcp_gpu_tensor_from_cpu(ctx, cpu_layer->tau_base);
    }
    if (cpu_layer->W_in) {
        layer->W_in = nimcp_gpu_tensor_from_cpu(ctx, cpu_layer->W_in);
    }
    if (cpu_layer->W_rec) {
        layer->W_rec = nimcp_gpu_tensor_from_cpu(ctx, cpu_layer->W_rec);
    }
    if (cpu_layer->W_tau) {
        layer->W_tau = nimcp_gpu_tensor_from_cpu(ctx, cpu_layer->W_tau);
    }
    if (cpu_layer->b_in) {
        layer->b_in = nimcp_gpu_tensor_from_cpu(ctx, cpu_layer->b_in);
    }
    if (cpu_layer->b_tau) {
        layer->b_tau = nimcp_gpu_tensor_from_cpu(ctx, cpu_layer->b_tau);
    }

    // Handle sparse wiring - transfer CSR structure to GPU
    if (cpu_layer->wiring && cpu_layer->wiring->n_edges > 0) {
        const lnn_wiring_t* wiring = cpu_layer->wiring;
        layer->n_edges = wiring->n_edges;

        // Copy row pointers (n_neurons + 1 elements)
        if (wiring->row_ptr) {
            size_t row_ptr_dims[] = {layer->n_neurons + 1};
            layer->row_ptr = nimcp_gpu_tensor_create(ctx, row_ptr_dims, 1, NIMCP_GPU_PRECISION_UINT32);
            if (layer->row_ptr) {
                cudaMemcpy(layer->row_ptr->data, wiring->row_ptr,
                           (layer->n_neurons + 1) * sizeof(uint32_t), cudaMemcpyHostToDevice);
            }
        }

        // Copy column indices (n_edges elements)
        if (wiring->col_idx) {
            size_t col_idx_dims[] = {wiring->n_edges};
            layer->col_idx = nimcp_gpu_tensor_create(ctx, col_idx_dims, 1, NIMCP_GPU_PRECISION_UINT32);
            if (layer->col_idx) {
                cudaMemcpy(layer->col_idx->data, wiring->col_idx,
                           wiring->n_edges * sizeof(uint32_t), cudaMemcpyHostToDevice);
            }
        }

        // Copy edge weights if present (n_edges elements)
        if (wiring->edge_weights) {
            size_t edge_dims[] = {wiring->n_edges};
            layer->edge_weights = nimcp_gpu_tensor_create(ctx, edge_dims, 1, NIMCP_GPU_PRECISION_FP32);
            if (layer->edge_weights) {
                cudaMemcpy(layer->edge_weights->data, wiring->edge_weights,
                           wiring->n_edges * sizeof(float), cudaMemcpyHostToDevice);
            }
        }

        LOG_DEBUG("Transferred CSR wiring to GPU: %u edges, sparsity=%.2f",
                  wiring->n_edges, wiring->sparsity);
    }

    LOG_DEBUG("Created GPU LNN layer with %u neurons, %u inputs", layer->n_neurons, layer->n_inputs);
    return layer;
}

void nimcp_lnn_layer_gpu_destroy(nimcp_lnn_layer_gpu_t* layer)
{
    if (!layer) return;

    nimcp_gpu_tensor_destroy(layer->x);
    nimcp_gpu_tensor_destroy(layer->dx_dt);
    nimcp_gpu_tensor_destroy(layer->tau);
    nimcp_gpu_tensor_destroy(layer->tau_base);
    nimcp_gpu_tensor_destroy(layer->W_in);
    nimcp_gpu_tensor_destroy(layer->W_rec);
    nimcp_gpu_tensor_destroy(layer->W_tau);
    nimcp_gpu_tensor_destroy(layer->b_in);
    nimcp_gpu_tensor_destroy(layer->b_tau);
    nimcp_gpu_tensor_destroy(layer->row_ptr);
    nimcp_gpu_tensor_destroy(layer->col_idx);
    nimcp_gpu_tensor_destroy(layer->edge_weights);

    nimcp_free(layer);
}

bool nimcp_lnn_layer_gpu_to_cpu(
    const nimcp_lnn_layer_gpu_t* gpu_layer,
    lnn_layer_t* cpu_layer)
{
    if (!gpu_layer || !cpu_layer) return false;

    // Copy state back
    if (gpu_layer->x && cpu_layer->x) {
        nimcp_gpu_tensor_copy_to_cpu(gpu_layer->x, cpu_layer->x);
    }
    if (gpu_layer->tau && cpu_layer->tau) {
        nimcp_gpu_tensor_copy_to_cpu(gpu_layer->tau, cpu_layer->tau);
    }

    return true;
}

bool nimcp_lnn_layer_gpu_zero_grad(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer)
{
    if (!ctx || !layer) return false;

    // Zero all gradient tensors
    // Note: Gradient tensors would need to be created first
    // This is a placeholder for full gradient support

    return true;
}

//=============================================================================
// Extended Layer Lifecycle (From Config)
//=============================================================================

/**
 * @brief Random weight initialization kernel (Xavier/Glorot)
 */
__global__ void kernel_init_weights_xavier(
    float* weights, size_t size, float scale, uint64_t seed)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= size) return;

    // Simple LCG random number generator
    uint64_t state = seed + idx * 6364136223846793005ULL;
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;

    // Box-Muller transform for normal distribution
    float u1 = (float)(state & 0xFFFFFFFF) / 4294967296.0f;
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    float u2 = (float)(state & 0xFFFFFFFF) / 4294967296.0f;

    u1 = fmaxf(u1, 1e-7f);  // Avoid log(0)
    float z = sqrtf(-2.0f * logf(u1)) * cosf(NIMCP_TWO_PI_F * u2);

    weights[idx] = z * scale;
}

/**
 * @brief Zero tensor kernel
 */
__global__ void kernel_zero_tensor(float* data, size_t size)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < size) {
        data[idx] = 0.0f;
    }
}

/**
 * @brief Initialize tau_base with uniform random in [tau_min, tau_max]
 */
__global__ void kernel_init_tau_base(
    float* tau_base, size_t n_neurons, float tau_min, float tau_max, uint64_t seed)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_neurons) return;

    uint64_t state = seed + idx * 6364136223846793005ULL;
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    float u = (float)(state & 0xFFFFFFFF) / 4294967296.0f;

    tau_base[idx] = tau_min + u * (tau_max - tau_min);
}

nimcp_lnn_layer_gpu_extended_t* nimcp_lnn_layer_gpu_create_from_config(
    nimcp_gpu_context_t* ctx,
    const nimcp_lnn_layer_config_t* config)
{
    if (!ctx || !config) return NULL;
    if (config->input_dim == 0 || config->hidden_dim == 0) return NULL;

    nimcp_lnn_layer_gpu_extended_t* ext = (nimcp_lnn_layer_gpu_extended_t*)nimcp_calloc(
        1, sizeof(nimcp_lnn_layer_gpu_extended_t));
    if (!ext) return NULL;

    // Store config
    ext->config = *config;
    ext->ctx = ctx;
    ext->current_time = 0.0f;
    ext->last_dt_used = config->dt;
    ext->error_estimate = 0.0f;
    ext->atol = 1e-6f;
    ext->rtol = 1e-3f;
    ext->dt_min = 0.01f;
    ext->dt_max = 10.0f;

    nimcp_lnn_layer_gpu_t* layer = &ext->base;
    layer->n_neurons = config->hidden_dim;
    layer->n_inputs = config->input_dim;
    layer->activation = config->activation;

    uint32_t n = config->hidden_dim;
    uint32_t d = config->input_dim;

    // Seed for random initialization
    uint64_t seed = config->seed;
    if (seed == 0) {
        seed = (uint64_t)time(NULL);
    }

    // Weight initialization scale (Xavier/Glorot)
    float std_W_in = config->weight_init_std > 0 ? config->weight_init_std : sqrtf(2.0f / (d + n));
    float std_W_rec = config->weight_init_std > 0 ? config->weight_init_std : sqrtf(2.0f / (n + n));
    float std_W_tau = config->weight_init_std > 0 ? config->weight_init_std : sqrtf(2.0f / (d + n + n));

    // Allocate tensors
    size_t dims_x[] = {n};
    layer->x = nimcp_gpu_tensor_create(ctx, dims_x, 1, NIMCP_GPU_PRECISION_FP32);
    layer->dx_dt = nimcp_gpu_tensor_create(ctx, dims_x, 1, NIMCP_GPU_PRECISION_FP32);
    layer->tau = nimcp_gpu_tensor_create(ctx, dims_x, 1, NIMCP_GPU_PRECISION_FP32);
    layer->tau_base = nimcp_gpu_tensor_create(ctx, dims_x, 1, NIMCP_GPU_PRECISION_FP32);
    layer->b_in = nimcp_gpu_tensor_create(ctx, dims_x, 1, NIMCP_GPU_PRECISION_FP32);
    layer->b_tau = nimcp_gpu_tensor_create(ctx, dims_x, 1, NIMCP_GPU_PRECISION_FP32);

    size_t dims_W_in[] = {n, d};
    layer->W_in = nimcp_gpu_tensor_create(ctx, dims_W_in, 2, NIMCP_GPU_PRECISION_FP32);

    size_t dims_W_rec[] = {n, n};
    layer->W_rec = nimcp_gpu_tensor_create(ctx, dims_W_rec, 2, NIMCP_GPU_PRECISION_FP32);

    size_t dims_W_tau[] = {n, d + n};
    layer->W_tau = nimcp_gpu_tensor_create(ctx, dims_W_tau, 2, NIMCP_GPU_PRECISION_FP32);

    // Check allocations
    if (!layer->x || !layer->dx_dt || !layer->tau || !layer->tau_base ||
        !layer->b_in || !layer->b_tau || !layer->W_in || !layer->W_rec || !layer->W_tau) {
        nimcp_lnn_layer_gpu_extended_destroy(ext);
        return NULL;
    }

    // Initialize weights with Xavier initialization
    kernel_init_weights_xavier<<<GRID_SIZE(n * d), BLOCK_SIZE>>>(
        (float*)layer->W_in->data, n * d, std_W_in, seed);
    kernel_init_weights_xavier<<<GRID_SIZE(n * n), BLOCK_SIZE>>>(
        (float*)layer->W_rec->data, n * n, std_W_rec, seed + 1);
    kernel_init_weights_xavier<<<GRID_SIZE(n * (d + n)), BLOCK_SIZE>>>(
        (float*)layer->W_tau->data, n * (d + n), std_W_tau, seed + 2);

    // Zero biases and state
    kernel_zero_tensor<<<GRID_SIZE(n), BLOCK_SIZE>>>((float*)layer->x->data, n);
    kernel_zero_tensor<<<GRID_SIZE(n), BLOCK_SIZE>>>((float*)layer->dx_dt->data, n);
    kernel_zero_tensor<<<GRID_SIZE(n), BLOCK_SIZE>>>((float*)layer->b_in->data, n);
    kernel_zero_tensor<<<GRID_SIZE(n), BLOCK_SIZE>>>((float*)layer->b_tau->data, n);

    // Initialize tau_base with values in [tau_min, tau_max]
    float tau_min = config->tau_min > 0 ? config->tau_min : LNN_TAU_MIN_DEFAULT;
    float tau_max = config->tau_max > 0 ? config->tau_max : LNN_TAU_MAX_DEFAULT;
    kernel_init_tau_base<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)layer->tau_base->data, n, tau_min, tau_max, seed + 3);

    // Initialize tau to tau_base initially
    cudaMemcpy(layer->tau->data, layer->tau_base->data, n * sizeof(float), cudaMemcpyDeviceToDevice);

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        LOG_ERROR("CUDA error in layer creation: %s", cudaGetErrorString(err));
        nimcp_lnn_layer_gpu_extended_destroy(ext);
        return NULL;
    }

    LOG_DEBUG("Created GPU LNN layer from config: %u inputs, %u neurons, ODE=%d",
              d, n, config->ode_method);
    return ext;
}

void nimcp_lnn_layer_gpu_extended_destroy(nimcp_lnn_layer_gpu_extended_t* layer)
{
    if (!layer) return;

    // Manually destroy tensors (don't call nimcp_lnn_layer_gpu_destroy since base is embedded)
    nimcp_lnn_layer_gpu_t* base = &layer->base;

    nimcp_gpu_tensor_destroy(base->x);
    nimcp_gpu_tensor_destroy(base->dx_dt);
    nimcp_gpu_tensor_destroy(base->tau);
    nimcp_gpu_tensor_destroy(base->tau_base);
    nimcp_gpu_tensor_destroy(base->W_in);
    nimcp_gpu_tensor_destroy(base->W_rec);
    nimcp_gpu_tensor_destroy(base->W_tau);
    nimcp_gpu_tensor_destroy(base->b_in);
    nimcp_gpu_tensor_destroy(base->b_tau);
    nimcp_gpu_tensor_destroy(base->row_ptr);
    nimcp_gpu_tensor_destroy(base->col_idx);
    nimcp_gpu_tensor_destroy(base->edge_weights);

    nimcp_free(layer);
}

//=============================================================================
// Layer Accessors
//=============================================================================

size_t nimcp_lnn_layer_gpu_get_input_dim(const nimcp_lnn_layer_gpu_extended_t* layer)
{
    if (!layer) return 0;
    return layer->config.input_dim;
}

size_t nimcp_lnn_layer_gpu_get_hidden_dim(const nimcp_lnn_layer_gpu_extended_t* layer)
{
    if (!layer) return 0;
    return layer->config.hidden_dim;
}

lnn_ode_method_t nimcp_lnn_layer_gpu_get_ode_method(const nimcp_lnn_layer_gpu_extended_t* layer)
{
    if (!layer) return LNN_ODE_EULER;
    return layer->config.ode_method;
}

size_t nimcp_lnn_layer_gpu_count_params(const nimcp_lnn_layer_gpu_extended_t* layer)
{
    if (!layer) return 0;

    uint32_t n = layer->config.hidden_dim;
    uint32_t d = layer->config.input_dim;

    // W_in: n x d
    // W_rec: n x n
    // W_tau: n x (d + n)
    // b_in: n
    // b_tau: n
    // tau_base: n (if learn_tau)

    size_t count = n * d +          // W_in
                   n * n +          // W_rec
                   n * (d + n) +    // W_tau
                   n +              // b_in
                   n;               // b_tau

    if (layer->config.learn_tau) {
        count += n;  // tau_base
    }

    return count;
}

bool nimcp_lnn_layer_gpu_get_state(
    const nimcp_lnn_layer_gpu_extended_t* layer,
    float* state_out)
{
    if (!layer || !state_out) return false;
    if (!layer->base.x) return false;

    cudaError_t err = cudaMemcpy(state_out, layer->base.x->data,
                                  layer->config.hidden_dim * sizeof(float),
                                  cudaMemcpyDeviceToHost);
    return (err == cudaSuccess);
}

bool nimcp_lnn_layer_gpu_reset(nimcp_lnn_layer_gpu_extended_t* layer)
{
    if (!layer) return false;

    kernel_zero_tensor<<<GRID_SIZE(layer->config.hidden_dim), BLOCK_SIZE>>>(
        (float*)layer->base.x->data, layer->config.hidden_dim);

    layer->current_time = 0.0f;
    layer->last_dt_used = layer->config.dt;
    layer->error_estimate = 0.0f;

    cudaError_t err = cudaGetLastError();
    return (err == cudaSuccess);
}

//=============================================================================
// High-Level Forward Operations
//=============================================================================

bool nimcp_lnn_layer_gpu_step(
    nimcp_lnn_layer_gpu_extended_t* layer,
    const nimcp_gpu_tensor_t* input)
{
    if (!layer || !input) return false;

    nimcp_lnn_ode_config_t ode_config;
    ode_config.method = layer->config.ode_method;
    ode_config.dt = layer->config.dt;
    ode_config.dt_min = layer->dt_min;
    ode_config.dt_max = layer->dt_max;
    ode_config.error_tolerance = layer->atol;
    ode_config.max_steps = 1;
    ode_config.adaptive_stepping = false;

    bool ok = nimcp_gpu_lnn_ode_step(layer->ctx, &layer->base, input, &ode_config);
    if (ok) {
        layer->current_time += layer->config.dt;
        layer->last_dt_used = layer->config.dt;
    }
    return ok;
}

void nimcp_lnn_layer_gpu_set_adaptive_params(
    nimcp_lnn_layer_gpu_extended_t* layer,
    float atol,
    float rtol,
    float dt_min,
    float dt_max)
{
    if (!layer) return;
    layer->atol = atol;
    layer->rtol = rtol;
    layer->dt_min = dt_min;
    layer->dt_max = dt_max;
}

bool nimcp_lnn_layer_gpu_step_adaptive(
    nimcp_lnn_layer_gpu_extended_t* layer,
    const nimcp_gpu_tensor_t* input,
    float* dt_used)
{
    if (!layer || !input) return false;

    nimcp_lnn_ode_config_t ode_config;
    ode_config.method = LNN_ODE_DOPRI5;  // Force DOPRI5 for adaptive
    ode_config.dt = layer->last_dt_used;
    ode_config.dt_min = layer->dt_min;
    ode_config.dt_max = layer->dt_max;
    ode_config.error_tolerance = layer->atol;
    ode_config.max_steps = 10;
    ode_config.adaptive_stepping = true;

    float dt = layer->last_dt_used;
    bool ok = nimcp_gpu_lnn_dopri5_step(layer->ctx, &layer->base, input, &dt, &ode_config);

    if (ok) {
        layer->current_time += dt;
        layer->last_dt_used = dt;
        if (dt_used) *dt_used = dt;
    }
    return ok;
}

float nimcp_lnn_layer_gpu_get_error_estimate(const nimcp_lnn_layer_gpu_extended_t* layer)
{
    if (!layer) return 0.0f;
    return layer->error_estimate;
}

bool nimcp_lnn_layer_gpu_forward_sequence(
    nimcp_lnn_layer_gpu_extended_t* layer,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    size_t seq_len)
{
    if (!layer || !input || !output) return false;

    uint32_t input_dim = layer->config.input_dim;
    uint32_t hidden_dim = layer->config.hidden_dim;

    // Reset state at start of sequence
    nimcp_lnn_layer_gpu_reset(layer);

    // Process each timestep
    for (size_t t = 0; t < seq_len; t++) {
        // Create view for this timestep's input
        size_t input_offset = t * input_dim;
        size_t input_step_dims[1] = {input_dim};  // Stack-allocate dims array
        nimcp_gpu_tensor_t input_step;
        memset(&input_step, 0, sizeof(input_step));
        input_step.data = (float*)input->data + input_offset;
        input_step.numel = input_dim;
        input_step.ndim = 1;
        input_step.dims = input_step_dims;  // Point to stack-allocated array

        // Step
        if (!nimcp_lnn_layer_gpu_step(layer, &input_step)) {
            return false;
        }

        // Copy state to output
        size_t output_offset = t * hidden_dim;
        cudaError_t err = cudaMemcpy(
            (float*)output->data + output_offset,
            layer->base.x->data,
            hidden_dim * sizeof(float),
            cudaMemcpyDeviceToDevice);

        if (err != cudaSuccess) {
            return false;
        }
    }

    return true;
}

//=============================================================================
// Gradient Accumulation CUDA Kernels
//=============================================================================

/**
 * @brief Accumulate gradients: accumulated += scale * new_grad
 */
__global__ void kernel_accumulate_gradients(float* accumulated, const float* new_grad,
                                            size_t n, float scale)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        accumulated[idx] += scale * new_grad[idx];
    }
}

/**
 * @brief Clip gradients to [-clip_value, clip_value]
 */
__global__ void kernel_clip_gradients(float* grads, size_t n, float clip_value)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        float val = grads[idx];
        if (val > clip_value) {
            grads[idx] = clip_value;
        } else if (val < -clip_value) {
            grads[idx] = -clip_value;
        }
    }
}

/**
 * @brief Compute sum of squares for L2 norm (reduction kernel)
 */
__global__ void kernel_sum_squares_reduce(const float* data, float* result, size_t n)
{
    extern __shared__ float sdata[];

    size_t tid = threadIdx.x;
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    // Load and square
    sdata[tid] = (idx < n) ? data[idx] * data[idx] : 0.0f;
    __syncthreads();

    // Parallel reduction for sum
    for (size_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            sdata[tid] += sdata[tid + s];
        }
        __syncthreads();
    }

    // Write result from first thread
    if (tid == 0) {
        atomicAdd(result, sdata[0]);
    }
}

/**
 * @brief Normalize gradients by dividing by norm
 */
__global__ void kernel_normalize_gradients(float* grads, size_t n, float norm)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n && norm > 1e-8f) {
        grads[idx] /= norm;
    }
}

/**
 * @brief Apply gradients to weights: weights -= lr * grads
 */
__global__ void kernel_apply_gradients(float* weights, const float* grads,
                                       size_t n, float learning_rate)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        weights[idx] -= learning_rate * grads[idx];
    }
}

/**
 * @brief Reset gradients to zero
 */
__global__ void kernel_reset_gradients(float* grads, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        grads[idx] = 0.0f;
    }
}

// External C functions for gradient DAO
extern "C" {

void nimcp_lnn_gradient_accumulate_gpu(void* d_accumulated, const void* d_new_grad,
                                        size_t n, float scale)
{
    kernel_accumulate_gradients<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)d_accumulated, (const float*)d_new_grad, n, scale);
    cudaDeviceSynchronize();
}

void nimcp_lnn_gradient_clip_gpu(void* d_grads, size_t n, float clip_value)
{
    kernel_clip_gradients<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)d_grads, n, clip_value);
    cudaDeviceSynchronize();
}

float nimcp_lnn_gradient_norm_gpu(const void* d_grads, size_t n)
{
    float* d_result = NULL;
    float h_result = 0.0f;

    if (cudaMalloc(&d_result, sizeof(float)) != cudaSuccess) return 0.0f;
    if (cudaMemset(d_result, 0, sizeof(float)) != cudaSuccess) {
        cudaFree(d_result);
        return 0.0f;
    }

    kernel_sum_squares_reduce<<<GRID_SIZE(n), BLOCK_SIZE, BLOCK_SIZE * sizeof(float)>>>(
        (const float*)d_grads, d_result, n);
    cudaMemcpy(&h_result, d_result, sizeof(float), cudaMemcpyDeviceToHost);
    cudaFree(d_result);

    return sqrtf(h_result);
}

void nimcp_lnn_gradient_normalize_gpu(void* d_grads, size_t n, float norm)
{
    kernel_normalize_gradients<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)d_grads, n, norm);
    cudaDeviceSynchronize();
}

void nimcp_lnn_gradient_apply_gpu(void* d_weights, const void* d_grads,
                                   size_t n, float lr)
{
    kernel_apply_gradients<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)d_weights, (const float*)d_grads, n, lr);
    cudaDeviceSynchronize();
}

void nimcp_lnn_gradient_reset_gpu(void* d_grads, size_t n)
{
    kernel_reset_gradients<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)d_grads, n);
    cudaDeviceSynchronize();
}

void nimcp_lnn_memcpy_d2h(void* dst, const void* src, size_t size)
{
    cudaMemcpy(dst, src, size, cudaMemcpyDeviceToHost);
}

} // extern "C"

//=============================================================================
// CSR Weight Transfer Implementation
//=============================================================================

/**
 * @brief CSR sparse matrix-vector multiplication kernel
 *
 * Computes y = A * x where A is in CSR format
 */
__global__ void kernel_csr_spmv(const int* row_offsets, const int* col_indices,
                                const float* values, const float* x, float* y,
                                size_t num_rows)
{
    size_t row = blockIdx.x * blockDim.x + threadIdx.x;
    if (row >= num_rows) return;

    float sum = 0.0f;
    int row_start = row_offsets[row];
    int row_end = row_offsets[row + 1];

    for (int k = row_start; k < row_end; k++) {
        int col = col_indices[k];
        sum += values[k] * x[col];
    }

    y[row] = sum;
}

// CSR weight structure - extern C API
extern "C" {

#include "gpu/lnn/nimcp_lnn_gpu.h"

nimcp_lnn_csr_weights_t* nimcp_lnn_csr_create(void* ctx_ptr, size_t rows, size_t cols, size_t nnz)
{
    if (!ctx_ptr || rows == 0 || cols == 0) return NULL;

    nimcp_lnn_csr_weights_t* csr = (nimcp_lnn_csr_weights_t*)nimcp_calloc(1, sizeof(nimcp_lnn_csr_weights_t));
    if (!csr) return NULL;

    csr->rows = rows;
    csr->cols = cols;
    csr->nnz = nnz;

    // Allocate GPU memory
    cudaError_t err;
    err = cudaMalloc(&csr->d_row_offsets, (rows + 1) * sizeof(int));
    if (err != cudaSuccess) {
        nimcp_free(csr);
        return NULL;
    }

    err = cudaMalloc(&csr->d_col_indices, nnz * sizeof(int));
    if (err != cudaSuccess) {
        cudaFree(csr->d_row_offsets);
        nimcp_free(csr);
        return NULL;
    }

    err = cudaMalloc(&csr->d_values, nnz * sizeof(float));
    if (err != cudaSuccess) {
        cudaFree(csr->d_row_offsets);
        cudaFree(csr->d_col_indices);
        nimcp_free(csr);
        return NULL;
    }

    LOG_DEBUG("Created CSR weights: %zu x %zu, nnz=%zu", rows, cols, nnz);
    return csr;
}

void nimcp_lnn_csr_destroy(nimcp_lnn_csr_weights_t* csr)
{
    if (!csr) return;

    if (csr->d_row_offsets) cudaFree(csr->d_row_offsets);
    if (csr->d_col_indices) cudaFree(csr->d_col_indices);
    if (csr->d_values) cudaFree(csr->d_values);

    nimcp_free(csr);
    LOG_DEBUG("Destroyed CSR weights");
}

int nimcp_lnn_csr_to_gpu(nimcp_lnn_csr_weights_t* csr,
                         const int* h_row_offsets, const int* h_col_indices, const float* h_values)
{
    if (!csr || !h_row_offsets || !h_col_indices || !h_values) return -1;

    cudaError_t err;
    err = cudaMemcpy(csr->d_row_offsets, h_row_offsets,
                     (csr->rows + 1) * sizeof(int), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) return -1;

    err = cudaMemcpy(csr->d_col_indices, h_col_indices,
                     csr->nnz * sizeof(int), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) return -1;

    err = cudaMemcpy(csr->d_values, h_values,
                     csr->nnz * sizeof(float), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) return -1;

    return 0;
}

int nimcp_lnn_csr_from_gpu(nimcp_lnn_csr_weights_t* csr,
                           int* h_row_offsets, int* h_col_indices, float* h_values)
{
    if (!csr || !h_row_offsets || !h_col_indices || !h_values) return -1;

    cudaError_t err;
    err = cudaMemcpy(h_row_offsets, csr->d_row_offsets,
                     (csr->rows + 1) * sizeof(int), cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) return -1;

    err = cudaMemcpy(h_col_indices, csr->d_col_indices,
                     csr->nnz * sizeof(int), cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) return -1;

    err = cudaMemcpy(h_values, csr->d_values,
                     csr->nnz * sizeof(float), cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) return -1;

    return 0;
}

int nimcp_lnn_csr_spmv(nimcp_lnn_csr_weights_t* csr, const float* x, float* y)
{
    if (!csr || !x || !y) return -1;

    kernel_csr_spmv<<<GRID_SIZE(csr->rows), BLOCK_SIZE>>>(
        csr->d_row_offsets, csr->d_col_indices, csr->d_values,
        x, y, csr->rows);

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        LOG_ERROR("CSR SpMV kernel error: %s", cudaGetErrorString(err));
        return -1;
    }

    cudaDeviceSynchronize();
    return 0;
}

} // extern "C"

//=============================================================================
// Spectral Radius Computation via Power Iteration
//=============================================================================

/**
 * @brief Dense matrix-vector multiplication for power iteration
 *
 * Computes y = A * v where A is a dense n x n matrix
 */
__global__ void kernel_power_iteration_step(const float* A, const float* v, float* Av, size_t n)
{
    size_t row = blockIdx.x * blockDim.x + threadIdx.x;
    if (row >= n) return;

    float sum = 0.0f;
    for (size_t col = 0; col < n; col++) {
        sum += A[row * n + col] * v[col];
    }
    Av[row] = sum;
}

/**
 * @brief Compute L2 norm and normalize vector in-place
 */
__global__ void kernel_vector_normalize(float* v, size_t n, float* norm_out)
{
    extern __shared__ float sdata[];

    size_t tid = threadIdx.x;
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    // Load and square for norm computation
    sdata[tid] = (idx < n) ? v[idx] * v[idx] : 0.0f;
    __syncthreads();

    // Parallel reduction
    for (size_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            sdata[tid] += sdata[tid + s];
        }
        __syncthreads();
    }

    // First thread computes final norm and stores it
    if (tid == 0) {
        atomicAdd(norm_out, sdata[0]);
    }
}

/**
 * @brief Divide vector by scalar (for normalization)
 */
__global__ void kernel_vector_scale(float* v, size_t n, float scale)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        v[idx] *= scale;
    }
}

/**
 * @brief Rescale matrix by scalar factor: A *= scale_factor
 */
__global__ void kernel_rescale_matrix(float* A, size_t n, float scale_factor)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n * n) {
        A[idx] *= scale_factor;
    }
}

// Spectral radius API - extern C
extern "C" {

float nimcp_lnn_compute_spectral_radius(void* ctx_ptr, const float* weight_matrix,
                                        size_t n, nimcp_lnn_spectral_config_t* config)
{
    if (!ctx_ptr || !weight_matrix || n == 0 || !config) return -1.0f;

    // Default configuration
    int max_iterations = config->max_iterations > 0 ? config->max_iterations : 100;
    float tolerance = config->tolerance > 0 ? config->tolerance : 1e-6f;

    // Allocate device memory - pre-init to NULL for cleanup
    float *d_A = NULL, *d_v = NULL, *d_Av = NULL, *d_norm = NULL;
    float* h_v = NULL;
    float eigenvalue = 0.0f;
    float prev_eigenvalue = 0.0f;

    if (cudaMalloc(&d_A, n * n * sizeof(float)) != cudaSuccess) goto spectral_cleanup;
    if (cudaMalloc(&d_v, n * sizeof(float)) != cudaSuccess) goto spectral_cleanup;
    if (cudaMalloc(&d_Av, n * sizeof(float)) != cudaSuccess) goto spectral_cleanup;
    if (cudaMalloc(&d_norm, sizeof(float)) != cudaSuccess) goto spectral_cleanup;

    // Copy matrix to device
    cudaMemcpy(d_A, weight_matrix, n * n * sizeof(float), cudaMemcpyHostToDevice);

    // Initialize v to [1, 0, 0, ..., 0] (or uniform would work too)
    h_v = (float*)nimcp_calloc(n, sizeof(float));
    if (!h_v) goto spectral_cleanup;
    for (size_t i = 0; i < n; i++) {
        h_v[i] = 1.0f / sqrtf((float)n);  // Normalized uniform
    }
    cudaMemcpy(d_v, h_v, n * sizeof(float), cudaMemcpyHostToDevice);
    nimcp_free(h_v);
    h_v = NULL;

    // Power iteration
    for (int iter = 0; iter < max_iterations; iter++) {
        // Av = A * v
        kernel_power_iteration_step<<<GRID_SIZE(n), BLOCK_SIZE>>>(d_A, d_v, d_Av, n);
        cudaDeviceSynchronize();

        // Compute norm of Av (this is the eigenvalue estimate)
        cudaMemset(d_norm, 0, sizeof(float));
        kernel_vector_normalize<<<GRID_SIZE(n), BLOCK_SIZE, BLOCK_SIZE * sizeof(float)>>>(
            d_Av, n, d_norm);
        cudaDeviceSynchronize();

        float norm_sq;
        cudaMemcpy(&norm_sq, d_norm, sizeof(float), cudaMemcpyDeviceToHost);
        eigenvalue = sqrtf(norm_sq);

        // Normalize: v = Av / ||Av||
        if (eigenvalue > 1e-10f) {
            kernel_vector_scale<<<GRID_SIZE(n), BLOCK_SIZE>>>(d_Av, n, 1.0f / eigenvalue);
            cudaDeviceSynchronize();
        }

        // Swap v and Av
        {
            float* temp = d_v;
            d_v = d_Av;
            d_Av = temp;
        }

        // Check convergence
        if (iter > 0 && fabsf(eigenvalue - prev_eigenvalue) < tolerance) {
            LOG_DEBUG("Power iteration converged in %d iterations", iter + 1);
            break;
        }
        prev_eigenvalue = eigenvalue;
    }

    // Cleanup
    cudaFree(d_A);
    cudaFree(d_v);
    cudaFree(d_Av);
    cudaFree(d_norm);

    return eigenvalue;

spectral_cleanup:
    cudaFree(d_A);
    cudaFree(d_v);
    cudaFree(d_Av);
    cudaFree(d_norm);
    return -1.0f;
}

int nimcp_lnn_rescale_to_spectral_radius(void* ctx_ptr, float* weight_matrix,
                                         size_t n, float target_radius)
{
    if (!ctx_ptr || !weight_matrix || n == 0 || target_radius <= 0) return -1;

    // First compute current spectral radius
    nimcp_lnn_spectral_config_t config;
    config.max_iterations = 100;
    config.tolerance = 1e-6f;
    config.target_radius = target_radius;

    float current_radius = nimcp_lnn_compute_spectral_radius(ctx_ptr, weight_matrix, n, &config);
    if (current_radius < 0) return -1;

    if (current_radius < 1e-10f) {
        LOG_WARN("Matrix has near-zero spectral radius, cannot rescale");
        return -1;
    }

    // Compute scale factor
    float scale = target_radius / current_radius;

    // Allocate device memory and rescale
    float* d_A = NULL;
    if (cudaMalloc(&d_A, n * n * sizeof(float)) != cudaSuccess) return -1;
    if (cudaMemcpy(d_A, weight_matrix, n * n * sizeof(float), cudaMemcpyHostToDevice) != cudaSuccess) {
        cudaFree(d_A);
        return -1;
    }

    kernel_rescale_matrix<<<GRID_SIZE(n * n), BLOCK_SIZE>>>(d_A, n, scale);
    cudaDeviceSynchronize();

    // Copy back
    if (cudaMemcpy(weight_matrix, d_A, n * n * sizeof(float), cudaMemcpyDeviceToHost) != cudaSuccess) {
        cudaFree(d_A);
        return -1;
    }
    cudaFree(d_A);

    LOG_DEBUG("Rescaled matrix from spectral radius %.4f to %.4f (scale=%.4f)",
              current_radius, target_radius, scale);
    return 0;
}

} // extern "C"

#else // !NIMCP_ENABLE_CUDA

//=============================================================================
// CPU Implementation (when CUDA is not available)
//=============================================================================

#include "gpu/lnn/nimcp_lnn_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define LOG_MODULE "LNN_CPU"

//-----------------------------------------------------------------------------
// Activation Functions (CPU)
//-----------------------------------------------------------------------------

static inline float cpu_tanh(float x) { return tanhf(x); }
static inline float cpu_sigmoid(float x) { return 1.0f / (1.0f + expf(-x)); }
static inline float cpu_relu(float x) { return x > 0.0f ? x : 0.0f; }
static inline float cpu_gelu(float x) {
    const float sqrt_2_over_pi = 0.7978845608f;
    float cdf = 0.5f * (1.0f + tanhf(sqrt_2_over_pi * (x + 0.044715f * x * x * x)));
    return x * cdf;
}
static inline float cpu_silu(float x) { return x * cpu_sigmoid(x); }
static inline float cpu_softplus(float x) { return logf(1.0f + expf(x)); }

static inline float cpu_activation(float x, int activation_type) {
    switch (activation_type) {
        case 0: return cpu_tanh(x);
        case 1: return cpu_sigmoid(x);
        case 2: return cpu_relu(x);
        case 3: return cpu_gelu(x);
        case 4: return cpu_silu(x);
        case 5: return cpu_softplus(x);
        default: return cpu_tanh(x);
    }
}

/**
 * @brief Clamp state values to prevent numerical explosion
 *
 * LNN dynamics should keep state bounded (especially with tanh activation).
 * This prevents numerical instability in the ODE solvers.
 */
static inline void cpu_clamp_state(float* state, size_t n, float min_val, float max_val) {
    for (size_t i = 0; i < n; i++) {
        if (state[i] < min_val) state[i] = min_val;
        else if (state[i] > max_val) state[i] = max_val;
        else if (isnanf(state[i]) || isinff(state[i])) state[i] = 0.0f;
    }
}

// State clamp bounds for LNN (tanh activation outputs in [-1,1], allow some margin)
#define LNN_STATE_CLAMP_MIN -10.0f
#define LNN_STATE_CLAMP_MAX  10.0f

//-----------------------------------------------------------------------------
// Default Configuration
//-----------------------------------------------------------------------------

nimcp_lnn_ode_config_t nimcp_lnn_ode_default_config(void)
{
    nimcp_lnn_ode_config_t config;
    config.method = LNN_ODE_RK4;
    config.dt = 1.0f;
    config.dt_min = 0.01f;
    config.dt_max = 10.0f;
    config.error_tolerance = 1e-5f;
    config.max_steps = 1000;
    config.adaptive_stepping = false;
    return config;
}

//-----------------------------------------------------------------------------
// Euler Step (CPU)
//-----------------------------------------------------------------------------

bool nimcp_gpu_lnn_euler_step(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* dx_dt,
    float dt,
    nimcp_gpu_tensor_t* x_new)
{
    (void)ctx;  // Unused in CPU implementation
    if (!x || !dx_dt || !x_new) return false;

    float* x_data = (float*)x->data;
    float* dx_data = (float*)dx_dt->data;
    float* x_new_data = (float*)x_new->data;
    size_t n = x->numel;

    for (size_t i = 0; i < n; i++) {
        x_new_data[i] = x_data[i] + dt * dx_data[i];
    }

    // Clamp to prevent numerical explosion
    cpu_clamp_state(x_new_data, n, LNN_STATE_CLAMP_MIN, LNN_STATE_CLAMP_MAX);

    return true;
}

//-----------------------------------------------------------------------------
// Update Tau (CPU)
//-----------------------------------------------------------------------------

bool nimcp_gpu_lnn_update_tau(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* input)
{
    (void)ctx;
    if (!layer || !input) return false;

    float* x_data = (float*)layer->x->data;
    float* input_data = (float*)input->data;
    float* W_tau_data = (float*)layer->W_tau->data;
    float* b_tau_data = (float*)layer->b_tau->data;
    float* tau_base_data = (float*)layer->tau_base->data;
    float* tau_data = (float*)layer->tau->data;

    uint32_t n_neurons = layer->n_neurons;
    uint32_t n_inputs = layer->n_inputs;
    uint32_t tau_width = n_inputs + n_neurons;

    float tau_min = LNN_TAU_MIN_DEFAULT;
    float tau_max = LNN_TAU_MAX_DEFAULT;

    for (uint32_t i = 0; i < n_neurons; i++) {
        float sum = b_tau_data[i];

        // Input contribution
        for (uint32_t j = 0; j < n_inputs; j++) {
            sum += W_tau_data[i * tau_width + j] * input_data[j];
        }

        // Recurrent state contribution
        for (uint32_t j = 0; j < n_neurons; j++) {
            sum += W_tau_data[i * tau_width + n_inputs + j] * x_data[j];
        }

        // tau = tau_base * sigmoid(sum), clamped
        float tau = tau_base_data[i] * cpu_sigmoid(sum);
        tau = fmaxf(tau_min, fminf(tau_max, tau));
        tau_data[i] = tau;
    }

    return true;
}

//-----------------------------------------------------------------------------
// Compute Derivative (CPU)
//-----------------------------------------------------------------------------

bool nimcp_gpu_lnn_compute_derivative(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* dx_dt)
{
    (void)ctx;
    if (!layer || !input || !dx_dt) return false;

    // First update tau
    if (!nimcp_gpu_lnn_update_tau(ctx, layer, input)) {
        return false;
    }

    float* x_data = (float*)layer->x->data;
    float* input_data = (float*)input->data;
    float* tau_data = (float*)layer->tau->data;
    float* W_in_data = (float*)layer->W_in->data;
    float* b_in_data = (float*)layer->b_in->data;
    float* dx_dt_data = (float*)dx_dt->data;

    uint32_t n_neurons = layer->n_neurons;
    uint32_t n_inputs = layer->n_inputs;
    int activation_type = (int)layer->activation;

    // Check for sparse or dense recurrent
    bool use_sparse = (layer->row_ptr != NULL && layer->col_idx != NULL);

    for (uint32_t i = 0; i < n_neurons; i++) {
        float sum = b_in_data[i];

        // Input contribution
        for (uint32_t j = 0; j < n_inputs; j++) {
            sum += W_in_data[i * n_inputs + j] * input_data[j];
        }

        // Recurrent contribution
        if (use_sparse) {
            uint32_t* row_ptr = (uint32_t*)layer->row_ptr->data;
            uint32_t* col_idx = (uint32_t*)layer->col_idx->data;
            float* edge_weights = layer->edge_weights ? (float*)layer->edge_weights->data : NULL;

            uint32_t row_start = row_ptr[i];
            uint32_t row_end = row_ptr[i + 1];

            for (uint32_t k = row_start; k < row_end; k++) {
                uint32_t j = col_idx[k];
                float w = edge_weights ? edge_weights[k] : 1.0f;
                sum += w * x_data[j];
            }
        } else if (layer->W_rec) {
            float* W_rec_data = (float*)layer->W_rec->data;
            for (uint32_t j = 0; j < n_neurons; j++) {
                sum += W_rec_data[i * n_neurons + j] * x_data[j];
            }
        }

        // Apply activation and compute derivative
        float activated = cpu_activation(sum, activation_type);

        // Ensure tau is not too small to prevent division issues
        float tau = tau_data[i];
        if (tau < 0.1f) tau = 0.1f;

        dx_dt_data[i] = -x_data[i] / tau + activated;

        // Clamp derivative to prevent runaway dynamics
        if (dx_dt_data[i] < -100.0f) dx_dt_data[i] = -100.0f;
        else if (dx_dt_data[i] > 100.0f) dx_dt_data[i] = 100.0f;
        else if (isnanf(dx_dt_data[i]) || isinff(dx_dt_data[i])) dx_dt_data[i] = 0.0f;
    }

    return true;
}

//-----------------------------------------------------------------------------
// Heun Step (CPU)
//-----------------------------------------------------------------------------

bool nimcp_gpu_lnn_heun_step(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* input,
    float dt,
    const nimcp_lnn_ode_config_t* config)
{
    (void)config;
    if (!layer || !input) return false;

    size_t n = layer->n_neurons;

    // Allocate temporary arrays
    float* k1 = (float*)nimcp_malloc(n * sizeof(float));
    float* k2 = (float*)nimcp_malloc(n * sizeof(float));
    float* x_orig = (float*)nimcp_malloc(n * sizeof(float));
    float* x_temp = (float*)nimcp_malloc(n * sizeof(float));

    if (!k1 || !k2 || !x_orig || !x_temp) {
        nimcp_free(k1); nimcp_free(k2); nimcp_free(x_orig); nimcp_free(x_temp);
        return false;
    }

    float* x_data = (float*)layer->x->data;
    memcpy(x_orig, x_data, n * sizeof(float));

    // Create temporary tensor wrappers
    nimcp_gpu_tensor_t k1_tensor = {.data = k1, .numel = n};
    nimcp_gpu_tensor_t k2_tensor = {.data = k2, .numel = n};

    // k1 = f(t, x)
    nimcp_gpu_lnn_compute_derivative(ctx, layer, input, &k1_tensor);
    memcpy(k1, k1_tensor.data, n * sizeof(float));

    // x_temp = x + dt * k1 (predictor)
    for (size_t i = 0; i < n; i++) {
        x_temp[i] = x_orig[i] + dt * k1[i];
    }
    memcpy(x_data, x_temp, n * sizeof(float));

    // k2 = f(t + dt, x + dt * k1)
    nimcp_gpu_lnn_compute_derivative(ctx, layer, input, &k2_tensor);
    memcpy(k2, k2_tensor.data, n * sizeof(float));

    // x_new = x + 0.5 * dt * (k1 + k2) (corrector)
    for (size_t i = 0; i < n; i++) {
        x_data[i] = x_orig[i] + 0.5f * dt * (k1[i] + k2[i]);
    }

    // Clamp to prevent numerical explosion
    cpu_clamp_state(x_data, n, LNN_STATE_CLAMP_MIN, LNN_STATE_CLAMP_MAX);

    nimcp_free(k1); nimcp_free(k2); nimcp_free(x_orig); nimcp_free(x_temp);
    return true;
}

//-----------------------------------------------------------------------------
// RK4 Step (CPU)
//-----------------------------------------------------------------------------

bool nimcp_gpu_lnn_rk4_step(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* input,
    float dt,
    const nimcp_lnn_ode_config_t* config)
{
    (void)config;
    if (!layer || !input) return false;

    size_t n = layer->n_neurons;

    // Allocate temporary arrays
    float* k1 = (float*)nimcp_malloc(n * sizeof(float));
    float* k2 = (float*)nimcp_malloc(n * sizeof(float));
    float* k3 = (float*)nimcp_malloc(n * sizeof(float));
    float* k4 = (float*)nimcp_malloc(n * sizeof(float));
    float* x_orig = (float*)nimcp_malloc(n * sizeof(float));
    float* x_temp = (float*)nimcp_malloc(n * sizeof(float));

    if (!k1 || !k2 || !k3 || !k4 || !x_orig || !x_temp) {
        nimcp_free(k1); nimcp_free(k2); nimcp_free(k3); nimcp_free(k4); nimcp_free(x_orig); nimcp_free(x_temp);
        return false;
    }

    float* x_data = (float*)layer->x->data;
    memcpy(x_orig, x_data, n * sizeof(float));

    // Create temporary tensor wrappers
    nimcp_gpu_tensor_t k_tensor = {.data = k1, .numel = n};

    // k1 = f(t, x)
    nimcp_gpu_lnn_compute_derivative(ctx, layer, input, &k_tensor);
    memcpy(k1, k_tensor.data, n * sizeof(float));

    // x_temp = x + 0.5 * dt * k1
    for (size_t i = 0; i < n; i++) {
        x_temp[i] = x_orig[i] + 0.5f * dt * k1[i];
    }
    memcpy(x_data, x_temp, n * sizeof(float));

    // k2 = f(t + dt/2, x + dt/2 * k1)
    k_tensor.data = k2;
    nimcp_gpu_lnn_compute_derivative(ctx, layer, input, &k_tensor);
    memcpy(k2, k_tensor.data, n * sizeof(float));

    // x_temp = x + 0.5 * dt * k2
    for (size_t i = 0; i < n; i++) {
        x_temp[i] = x_orig[i] + 0.5f * dt * k2[i];
    }
    memcpy(x_data, x_temp, n * sizeof(float));

    // k3 = f(t + dt/2, x + dt/2 * k2)
    k_tensor.data = k3;
    nimcp_gpu_lnn_compute_derivative(ctx, layer, input, &k_tensor);
    memcpy(k3, k_tensor.data, n * sizeof(float));

    // x_temp = x + dt * k3
    for (size_t i = 0; i < n; i++) {
        x_temp[i] = x_orig[i] + dt * k3[i];
    }
    memcpy(x_data, x_temp, n * sizeof(float));

    // k4 = f(t + dt, x + dt * k3)
    k_tensor.data = k4;
    nimcp_gpu_lnn_compute_derivative(ctx, layer, input, &k_tensor);
    memcpy(k4, k_tensor.data, n * sizeof(float));

    // x_new = x + (dt/6) * (k1 + 2*k2 + 2*k3 + k4)
    for (size_t i = 0; i < n; i++) {
        x_data[i] = x_orig[i] + (dt / 6.0f) * (k1[i] + 2.0f * k2[i] + 2.0f * k3[i] + k4[i]);
    }

    // Clamp to prevent numerical explosion
    cpu_clamp_state(x_data, n, LNN_STATE_CLAMP_MIN, LNN_STATE_CLAMP_MAX);

    nimcp_free(k1); nimcp_free(k2); nimcp_free(k3); nimcp_free(k4); nimcp_free(x_orig); nimcp_free(x_temp);
    return true;
}

//-----------------------------------------------------------------------------
// DOPRI5 Step (CPU)
//-----------------------------------------------------------------------------

bool nimcp_gpu_lnn_dopri5_step(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* input,
    float* dt_ptr,
    const nimcp_lnn_ode_config_t* config)
{
    if (!layer || !input || !dt_ptr || !config) return false;

    float dt = *dt_ptr;
    size_t n = layer->n_neurons;

    // Allocate arrays for k1-k7
    float* k1 = (float*)nimcp_malloc(n * sizeof(float));
    float* k2 = (float*)nimcp_malloc(n * sizeof(float));
    float* k3 = (float*)nimcp_malloc(n * sizeof(float));
    float* k4 = (float*)nimcp_malloc(n * sizeof(float));
    float* k5 = (float*)nimcp_malloc(n * sizeof(float));
    float* k6 = (float*)nimcp_malloc(n * sizeof(float));
    float* k7 = (float*)nimcp_malloc(n * sizeof(float));
    float* x_orig = (float*)nimcp_malloc(n * sizeof(float));
    float* x_temp = (float*)nimcp_malloc(n * sizeof(float));
    float* x_new = (float*)nimcp_malloc(n * sizeof(float));
    float* error_vec = (float*)nimcp_malloc(n * sizeof(float));

    if (!k1 || !k2 || !k3 || !k4 || !k5 || !k6 || !k7 || !x_orig || !x_temp || !x_new || !error_vec) {
        nimcp_free(k1); nimcp_free(k2); nimcp_free(k3); nimcp_free(k4); nimcp_free(k5); nimcp_free(k6); nimcp_free(k7);
        nimcp_free(x_orig); nimcp_free(x_temp); nimcp_free(x_new); nimcp_free(error_vec);
        return false;
    }

    float* x_data = (float*)layer->x->data;
    memcpy(x_orig, x_data, n * sizeof(float));

    nimcp_gpu_tensor_t k_tensor = {.data = k1, .numel = n};

    bool accepted = false;
    int max_iterations = 10;
    int iteration = 0;

    while (!accepted && iteration < max_iterations) {
        iteration++;

        // Restore original state
        memcpy(x_data, x_orig, n * sizeof(float));

        // k1 = f(t, x)
        k_tensor.data = k1;
        nimcp_gpu_lnn_compute_derivative(ctx, layer, input, &k_tensor);
        memcpy(k1, k_tensor.data, n * sizeof(float));

        // k2 = f(t + c2*dt, x + dt*a21*k1)
        for (size_t i = 0; i < n; i++) {
            x_temp[i] = x_orig[i] + dt * (1.0f/5.0f) * k1[i];
        }
        memcpy(x_data, x_temp, n * sizeof(float));
        k_tensor.data = k2;
        nimcp_gpu_lnn_compute_derivative(ctx, layer, input, &k_tensor);
        memcpy(k2, k_tensor.data, n * sizeof(float));

        // k3 = f(t + c3*dt, x + dt*(a31*k1 + a32*k2))
        for (size_t i = 0; i < n; i++) {
            x_temp[i] = x_orig[i] + dt * ((3.0f/40.0f) * k1[i] + (9.0f/40.0f) * k2[i]);
        }
        memcpy(x_data, x_temp, n * sizeof(float));
        k_tensor.data = k3;
        nimcp_gpu_lnn_compute_derivative(ctx, layer, input, &k_tensor);
        memcpy(k3, k_tensor.data, n * sizeof(float));

        // k4 = f(t + c4*dt, x + dt*(a41*k1 + a42*k2 + a43*k3))
        for (size_t i = 0; i < n; i++) {
            x_temp[i] = x_orig[i] + dt * ((44.0f/45.0f) * k1[i] + (-56.0f/15.0f) * k2[i] + (32.0f/9.0f) * k3[i]);
        }
        memcpy(x_data, x_temp, n * sizeof(float));
        k_tensor.data = k4;
        nimcp_gpu_lnn_compute_derivative(ctx, layer, input, &k_tensor);
        memcpy(k4, k_tensor.data, n * sizeof(float));

        // k5 = f(t + c5*dt, x + dt*(a51*k1 + a52*k2 + a53*k3 + a54*k4))
        for (size_t i = 0; i < n; i++) {
            x_temp[i] = x_orig[i] + dt * ((19372.0f/6561.0f) * k1[i] + (-25360.0f/2187.0f) * k2[i] +
                                           (64448.0f/6561.0f) * k3[i] + (-212.0f/729.0f) * k4[i]);
        }
        memcpy(x_data, x_temp, n * sizeof(float));
        k_tensor.data = k5;
        nimcp_gpu_lnn_compute_derivative(ctx, layer, input, &k_tensor);
        memcpy(k5, k_tensor.data, n * sizeof(float));

        // k6 = f(t + dt, x + dt*(a61*k1 + a62*k2 + a63*k3 + a64*k4 + a65*k5))
        for (size_t i = 0; i < n; i++) {
            x_temp[i] = x_orig[i] + dt * ((9017.0f/3168.0f) * k1[i] + (-355.0f/33.0f) * k2[i] +
                                           (46732.0f/5247.0f) * k3[i] + (49.0f/176.0f) * k4[i] +
                                           (-5103.0f/18656.0f) * k5[i]);
        }
        memcpy(x_data, x_temp, n * sizeof(float));
        k_tensor.data = k6;
        nimcp_gpu_lnn_compute_derivative(ctx, layer, input, &k_tensor);
        memcpy(k6, k_tensor.data, n * sizeof(float));

        // Compute 5th order solution
        for (size_t i = 0; i < n; i++) {
            x_new[i] = x_orig[i] + dt * ((35.0f/384.0f) * k1[i] + (500.0f/1113.0f) * k3[i] +
                                          (125.0f/192.0f) * k4[i] + (-2187.0f/6784.0f) * k5[i] +
                                          (11.0f/84.0f) * k6[i]);
        }

        // k7 = f(t + dt, x_new) for FSAL
        memcpy(x_data, x_new, n * sizeof(float));
        k_tensor.data = k7;
        nimcp_gpu_lnn_compute_derivative(ctx, layer, input, &k_tensor);
        memcpy(k7, k_tensor.data, n * sizeof(float));

        if (config->adaptive_stepping) {
            // Compute error estimate
            float max_error = 0.0f;
            for (size_t i = 0; i < n; i++) {
                error_vec[i] = dt * ((71.0f/57600.0f) * k1[i] + (-71.0f/16695.0f) * k3[i] +
                                      (71.0f/1920.0f) * k4[i] + (-17253.0f/339200.0f) * k5[i] +
                                      (22.0f/525.0f) * k6[i] + (-1.0f/40.0f) * k7[i]);
                float abs_err = fabsf(error_vec[i]);
                if (abs_err > max_error) max_error = abs_err;
            }

            float tol = config->error_tolerance;
            if (max_error < tol || max_error == 0.0f) {
                accepted = true;
                memcpy(x_data, x_new, n * sizeof(float));

                // Adjust dt for next step
                if (max_error > 0.0f) {
                    float safety = 0.9f;
                    float factor = safety * powf(tol / max_error, 0.2f);
                    factor = fminf(fmaxf(factor, 0.1f), 5.0f);
                    *dt_ptr = fminf(fmaxf(dt * factor, config->dt_min), config->dt_max);
                }
            } else {
                // Reduce dt
                float safety = 0.9f;
                float factor = safety * powf(tol / max_error, 0.2f);
                factor = fmaxf(factor, 0.1f);
                dt = fmaxf(dt * factor, config->dt_min);

                if (dt <= config->dt_min) {
                    accepted = true;
                    memcpy(x_data, x_new, n * sizeof(float));
                    *dt_ptr = config->dt_min;
                }
            }
        } else {
            accepted = true;
            memcpy(x_data, x_new, n * sizeof(float));
        }
    }

    // Clamp to prevent numerical explosion
    cpu_clamp_state(x_data, n, LNN_STATE_CLAMP_MIN, LNN_STATE_CLAMP_MAX);

    nimcp_free(k1); nimcp_free(k2); nimcp_free(k3); nimcp_free(k4); nimcp_free(k5); nimcp_free(k6); nimcp_free(k7);
    nimcp_free(x_orig); nimcp_free(x_temp); nimcp_free(x_new); nimcp_free(error_vec);

    return accepted;
}

//-----------------------------------------------------------------------------
// Unified ODE Step (CPU)
//-----------------------------------------------------------------------------

bool nimcp_gpu_lnn_ode_step(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* input,
    const nimcp_lnn_ode_config_t* config)
{
    if (!layer || !input || !config) return false;

    switch (config->method) {
        case LNN_ODE_EULER: {
            nimcp_gpu_tensor_t dx_tensor = {.data = layer->dx_dt->data, .numel = layer->n_neurons};
            nimcp_gpu_lnn_compute_derivative(ctx, layer, input, &dx_tensor);
            return nimcp_gpu_lnn_euler_step(ctx, layer->x, &dx_tensor, config->dt, layer->x);
        }
        case LNN_ODE_HEUN:
            return nimcp_gpu_lnn_heun_step(ctx, layer, input, config->dt, config);
        case LNN_ODE_RK4:
            return nimcp_gpu_lnn_rk4_step(ctx, layer, input, config->dt, config);
        case LNN_ODE_DOPRI5: {
            float dt = config->dt;
            return nimcp_gpu_lnn_dopri5_step(ctx, layer, input, &dt, config);
        }
        default:
            LOG_ERROR("Unknown ODE method: %d", config->method);
            return false;
    }
}

//-----------------------------------------------------------------------------
// Sparse Operations (CPU)
//-----------------------------------------------------------------------------

bool nimcp_gpu_sparse_matvec(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* row_ptr,
    const nimcp_gpu_tensor_t* col_idx,
    const nimcp_gpu_tensor_t* values,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* y,
    uint32_t n_rows,
    float alpha)
{
    (void)ctx;
    if (!row_ptr || !col_idx || !x || !y) return false;

    uint32_t* row_ptr_data = (uint32_t*)row_ptr->data;
    uint32_t* col_idx_data = (uint32_t*)col_idx->data;
    float* values_data = values ? (float*)values->data : NULL;
    float* x_data = (float*)x->data;
    float* y_data = (float*)y->data;

    for (uint32_t row = 0; row < n_rows; row++) {
        float sum = 0.0f;
        uint32_t start = row_ptr_data[row];
        uint32_t end = row_ptr_data[row + 1];

        for (uint32_t k = start; k < end; k++) {
            float w = values_data ? values_data[k] : 1.0f;
            sum += w * x_data[col_idx_data[k]];
        }

        y_data[row] = alpha * sum;
    }

    return true;
}

bool nimcp_gpu_sparse_add(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* A_row_ptr,
    const nimcp_gpu_tensor_t* A_col_idx,
    const nimcp_gpu_tensor_t* A_values,
    const nimcp_gpu_tensor_t* B_row_ptr,
    const nimcp_gpu_tensor_t* B_col_idx,
    const nimcp_gpu_tensor_t* B_values,
    nimcp_gpu_tensor_t* C_row_ptr,
    nimcp_gpu_tensor_t* C_col_idx,
    nimcp_gpu_tensor_t* C_values,
    uint32_t n_rows,
    float alpha,
    float beta)
{
    (void)ctx;
    if (!A_row_ptr || !A_col_idx || !B_row_ptr || !B_col_idx ||
        !C_row_ptr || !C_col_idx || !C_values) {
        return false;
    }

    uint32_t* A_rp = (uint32_t*)A_row_ptr->data;
    uint32_t* A_ci = (uint32_t*)A_col_idx->data;
    float* A_v = A_values ? (float*)A_values->data : NULL;

    uint32_t* B_rp = (uint32_t*)B_row_ptr->data;
    uint32_t* B_ci = (uint32_t*)B_col_idx->data;
    float* B_v = B_values ? (float*)B_values->data : NULL;

    uint32_t* C_rp = (uint32_t*)C_row_ptr->data;
    uint32_t* C_ci = (uint32_t*)C_col_idx->data;
    float* C_v = (float*)C_values->data;

    // First pass: count nnz per row
    uint32_t* row_nnz = (uint32_t*)nimcp_calloc(n_rows, sizeof(uint32_t));
    if (!row_nnz) return false;

    for (uint32_t row = 0; row < n_rows; row++) {
        uint32_t A_start = A_rp[row], A_end = A_rp[row + 1];
        uint32_t B_start = B_rp[row], B_end = B_rp[row + 1];
        uint32_t A_idx = A_start, B_idx = B_start;
        uint32_t count = 0;

        while (A_idx < A_end && B_idx < B_end) {
            if (A_ci[A_idx] < B_ci[B_idx]) A_idx++;
            else if (B_ci[B_idx] < A_ci[A_idx]) B_idx++;
            else { A_idx++; B_idx++; }
            count++;
        }
        count += (A_end - A_idx) + (B_end - B_idx);
        row_nnz[row] = count;
    }

    // Build row pointers
    C_rp[0] = 0;
    for (uint32_t i = 0; i < n_rows; i++) {
        C_rp[i + 1] = C_rp[i] + row_nnz[i];
    }

    // Second pass: compute values
    for (uint32_t row = 0; row < n_rows; row++) {
        uint32_t A_start = A_rp[row], A_end = A_rp[row + 1];
        uint32_t B_start = B_rp[row], B_end = B_rp[row + 1];
        uint32_t A_idx = A_start, B_idx = B_start;
        uint32_t C_idx = C_rp[row];

        while (A_idx < A_end && B_idx < B_end) {
            if (A_ci[A_idx] < B_ci[B_idx]) {
                C_ci[C_idx] = A_ci[A_idx];
                C_v[C_idx] = alpha * (A_v ? A_v[A_idx] : 1.0f);
                A_idx++;
            } else if (B_ci[B_idx] < A_ci[A_idx]) {
                C_ci[C_idx] = B_ci[B_idx];
                C_v[C_idx] = beta * (B_v ? B_v[B_idx] : 1.0f);
                B_idx++;
            } else {
                C_ci[C_idx] = A_ci[A_idx];
                C_v[C_idx] = alpha * (A_v ? A_v[A_idx] : 1.0f) + beta * (B_v ? B_v[B_idx] : 1.0f);
                A_idx++; B_idx++;
            }
            C_idx++;
        }

        while (A_idx < A_end) {
            C_ci[C_idx] = A_ci[A_idx];
            C_v[C_idx] = alpha * (A_v ? A_v[A_idx] : 1.0f);
            A_idx++; C_idx++;
        }

        while (B_idx < B_end) {
            C_ci[C_idx] = B_ci[B_idx];
            C_v[C_idx] = beta * (B_v ? B_v[B_idx] : 1.0f);
            B_idx++; C_idx++;
        }
    }

    nimcp_free(row_nnz);
    return true;
}

//-----------------------------------------------------------------------------
// Adjoint Method (CPU)
//-----------------------------------------------------------------------------

bool nimcp_gpu_lnn_adjoint_init(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* adjoint)
{
    (void)ctx;
    if (!layer || !grad_output || !adjoint) return false;

    memcpy(adjoint->data, grad_output->data, layer->n_neurons * sizeof(float));
    return true;
}

bool nimcp_gpu_lnn_adjoint_step(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    nimcp_gpu_tensor_t* adjoint,
    const nimcp_gpu_tensor_t* x_at_t,
    const nimcp_gpu_tensor_t* input_at_t,
    float dt)
{
    (void)ctx;
    (void)input_at_t;
    if (!layer || !adjoint || !x_at_t) return false;

    float* lambda = (float*)adjoint->data;
    float* tau = (float*)layer->tau->data;
    uint32_t n = layer->n_neurons;

    // Simplified adjoint: d_lambda/dt = -lambda/tau
    for (uint32_t i = 0; i < n; i++) {
        float d_lambda = -lambda[i] / tau[i];
        lambda[i] = lambda[i] - dt * d_lambda;  // Backward integration
    }

    return true;
}

bool nimcp_gpu_lnn_accumulate_gradients(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* adjoint,
    const nimcp_gpu_tensor_t* x_at_t,
    const nimcp_gpu_tensor_t* input_at_t,
    float dt)
{
    (void)ctx;
    if (!layer || !adjoint || !x_at_t || !input_at_t) return false;

    // Gradient accumulation via outer product
    // grad_W_rec += dt * outer(adjoint, x_at_t)
    // grad_W_in += dt * outer(adjoint, input_at_t)

    float* lambda = (float*)adjoint->data;
    float* x = (float*)x_at_t->data;
    float* inp = (float*)input_at_t->data;
    uint32_t n_neurons = layer->n_neurons;
    uint32_t n_inputs = layer->n_inputs;

    // For W_rec gradients (if gradient tensors exist)
    // This would require additional gradient storage in the layer structure

    LOG_DEBUG("CPU gradient accumulation for LNN (n=%u, dt=%.4f)", n_neurons, dt);

    return true;
}

//-----------------------------------------------------------------------------
// Layer Lifecycle (CPU)
//-----------------------------------------------------------------------------

nimcp_lnn_layer_gpu_t* nimcp_lnn_layer_gpu_create(
    nimcp_gpu_context_t* ctx,
    const lnn_layer_t* cpu_layer)
{
    (void)ctx;
    if (!cpu_layer) return NULL;

    nimcp_lnn_layer_gpu_t* layer = (nimcp_lnn_layer_gpu_t*)nimcp_calloc(1, sizeof(nimcp_lnn_layer_gpu_t));
    if (!layer) return NULL;

    layer->n_neurons = cpu_layer->n_neurons;
    layer->n_inputs = cpu_layer->W_in ? nimcp_tensor_shape(cpu_layer->W_in)->dims[1] : 0;
    layer->activation = cpu_layer->neurons ? cpu_layer->neurons[0].activation : LNN_ACTIVATION_TANH;

    // For CPU mode, we can reference the existing tensors directly or copy
    // Here we'll create wrappers that point to the CPU data
    if (cpu_layer->x) {
        layer->x = (nimcp_gpu_tensor_t*)nimcp_calloc(1, sizeof(nimcp_gpu_tensor_t));
        if (layer->x) {
            const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(cpu_layer->x);
            layer->x->data = nimcp_tensor_data(cpu_layer->x);
            layer->x->numel = shape->dims[0];
        }
    }

    if (cpu_layer->dx_dt) {
        layer->dx_dt = (nimcp_gpu_tensor_t*)nimcp_calloc(1, sizeof(nimcp_gpu_tensor_t));
        if (layer->dx_dt) {
            const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(cpu_layer->dx_dt);
            layer->dx_dt->data = nimcp_tensor_data(cpu_layer->dx_dt);
            layer->dx_dt->numel = shape->dims[0];
        }
    } else {
        layer->dx_dt = (nimcp_gpu_tensor_t*)nimcp_calloc(1, sizeof(nimcp_gpu_tensor_t));
        if (layer->dx_dt) {
            layer->dx_dt->data = nimcp_calloc(layer->n_neurons, sizeof(float));
            layer->dx_dt->numel = layer->n_neurons;
        }
    }

    if (cpu_layer->tau) {
        layer->tau = (nimcp_gpu_tensor_t*)nimcp_calloc(1, sizeof(nimcp_gpu_tensor_t));
        if (layer->tau) {
            const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(cpu_layer->tau);
            layer->tau->data = nimcp_tensor_data(cpu_layer->tau);
            layer->tau->numel = shape->dims[0];
        }
    }

    if (cpu_layer->tau_base) {
        layer->tau_base = (nimcp_gpu_tensor_t*)nimcp_calloc(1, sizeof(nimcp_gpu_tensor_t));
        if (layer->tau_base) {
            const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(cpu_layer->tau_base);
            layer->tau_base->data = nimcp_tensor_data(cpu_layer->tau_base);
            layer->tau_base->numel = shape->dims[0];
        }
    }

    if (cpu_layer->W_in) {
        layer->W_in = (nimcp_gpu_tensor_t*)nimcp_calloc(1, sizeof(nimcp_gpu_tensor_t));
        if (layer->W_in) {
            const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(cpu_layer->W_in);
            layer->W_in->data = nimcp_tensor_data(cpu_layer->W_in);
            layer->W_in->numel = shape->dims[0] * shape->dims[1];
        }
    }

    if (cpu_layer->W_rec) {
        layer->W_rec = (nimcp_gpu_tensor_t*)nimcp_calloc(1, sizeof(nimcp_gpu_tensor_t));
        if (layer->W_rec) {
            const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(cpu_layer->W_rec);
            layer->W_rec->data = nimcp_tensor_data(cpu_layer->W_rec);
            layer->W_rec->numel = shape->dims[0] * shape->dims[1];
        }
    }

    if (cpu_layer->W_tau) {
        layer->W_tau = (nimcp_gpu_tensor_t*)nimcp_calloc(1, sizeof(nimcp_gpu_tensor_t));
        if (layer->W_tau) {
            const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(cpu_layer->W_tau);
            layer->W_tau->data = nimcp_tensor_data(cpu_layer->W_tau);
            layer->W_tau->numel = shape->dims[0] * shape->dims[1];
        }
    }

    if (cpu_layer->b_in) {
        layer->b_in = (nimcp_gpu_tensor_t*)nimcp_calloc(1, sizeof(nimcp_gpu_tensor_t));
        if (layer->b_in) {
            const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(cpu_layer->b_in);
            layer->b_in->data = nimcp_tensor_data(cpu_layer->b_in);
            layer->b_in->numel = shape->dims[0];
        }
    }

    if (cpu_layer->b_tau) {
        layer->b_tau = (nimcp_gpu_tensor_t*)nimcp_calloc(1, sizeof(nimcp_gpu_tensor_t));
        if (layer->b_tau) {
            const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(cpu_layer->b_tau);
            layer->b_tau->data = nimcp_tensor_data(cpu_layer->b_tau);
            layer->b_tau->numel = shape->dims[0];
        }
    }

    // Handle sparse wiring
    if (cpu_layer->wiring) {
        layer->n_edges = cpu_layer->wiring->n_edges;

        if (cpu_layer->wiring->row_ptr) {
            layer->row_ptr = (nimcp_gpu_tensor_t*)nimcp_calloc(1, sizeof(nimcp_gpu_tensor_t));
            if (layer->row_ptr) {
                layer->row_ptr->data = cpu_layer->wiring->row_ptr;
                layer->row_ptr->numel = layer->n_neurons + 1;
            }
        }

        if (cpu_layer->wiring->col_idx) {
            layer->col_idx = (nimcp_gpu_tensor_t*)nimcp_calloc(1, sizeof(nimcp_gpu_tensor_t));
            if (layer->col_idx) {
                layer->col_idx->data = cpu_layer->wiring->col_idx;
                layer->col_idx->numel = layer->n_edges;
            }
        }

        if (cpu_layer->wiring->edge_weights) {
            layer->edge_weights = (nimcp_gpu_tensor_t*)nimcp_calloc(1, sizeof(nimcp_gpu_tensor_t));
            if (layer->edge_weights) {
                layer->edge_weights->data = cpu_layer->wiring->edge_weights;
                layer->edge_weights->numel = layer->n_edges;
            }
        }
    }

    LOG_DEBUG("Created CPU LNN layer with %u neurons, %u inputs", layer->n_neurons, layer->n_inputs);
    return layer;
}

void nimcp_lnn_layer_gpu_destroy(nimcp_lnn_layer_gpu_t* layer)
{
    if (!layer) return;

    // Free wrapper structures (data is owned by CPU layer)
    nimcp_free(layer->x);
    if (layer->dx_dt && layer->dx_dt->data) {
        // dx_dt may have been allocated by us
        // Check if it's different from any cpu_layer tensor
    }
    nimcp_free(layer->dx_dt);
    nimcp_free(layer->tau);
    nimcp_free(layer->tau_base);
    nimcp_free(layer->W_in);
    nimcp_free(layer->W_rec);
    nimcp_free(layer->W_tau);
    nimcp_free(layer->b_in);
    nimcp_free(layer->b_tau);
    nimcp_free(layer->row_ptr);
    nimcp_free(layer->col_idx);
    nimcp_free(layer->edge_weights);

    nimcp_free(layer);
}

bool nimcp_lnn_layer_gpu_to_cpu(
    const nimcp_lnn_layer_gpu_t* gpu_layer,
    lnn_layer_t* cpu_layer)
{
    if (!gpu_layer || !cpu_layer) return false;

    // In CPU mode, data is already shared via pointers
    // Just ensure consistency
    void* cpu_x_data = cpu_layer->x ? nimcp_tensor_data(cpu_layer->x) : NULL;
    void* cpu_tau_data = cpu_layer->tau ? nimcp_tensor_data(cpu_layer->tau) : NULL;
    if (gpu_layer->x && cpu_x_data && gpu_layer->x->data != cpu_x_data) {
        memcpy(cpu_x_data, gpu_layer->x->data, gpu_layer->n_neurons * sizeof(float));
    }
    if (gpu_layer->tau && cpu_tau_data && gpu_layer->tau->data != cpu_tau_data) {
        memcpy(cpu_tau_data, gpu_layer->tau->data, gpu_layer->n_neurons * sizeof(float));
    }

    return true;
}

bool nimcp_lnn_layer_gpu_zero_grad(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer)
{
    (void)ctx;
    if (!layer) return false;

    // Zero gradient tensors if they exist
    // In full implementation, layer would have grad_W_in, grad_W_rec, etc.

    return true;
}

//=============================================================================
// Extended Layer Lifecycle (CPU Fallback)
//=============================================================================

/**
 * @brief CPU random number generator for weight initialization
 */
static float cpu_rand_normal(uint64_t* state, float std)
{
    // LCG random
    *state = (*state) * 6364136223846793005ULL + 1442695040888963407ULL;
    float u1 = (float)((*state) & 0xFFFFFFFF) / 4294967296.0f;
    *state = (*state) * 6364136223846793005ULL + 1442695040888963407ULL;
    float u2 = (float)((*state) & 0xFFFFFFFF) / 4294967296.0f;

    u1 = fmaxf(u1, 1e-7f);
    float z = sqrtf(-2.0f * logf(u1)) * cosf(NIMCP_TWO_PI_F * u2);
    return z * std;
}

nimcp_lnn_layer_gpu_extended_t* nimcp_lnn_layer_gpu_create_from_config(
    nimcp_gpu_context_t* ctx,
    const nimcp_lnn_layer_config_t* config)
{
    (void)ctx;  // Not used in CPU mode
    if (!config) return NULL;
    if (config->input_dim == 0 || config->hidden_dim == 0) return NULL;

    nimcp_lnn_layer_gpu_extended_t* ext = (nimcp_lnn_layer_gpu_extended_t*)nimcp_calloc(
        1, sizeof(nimcp_lnn_layer_gpu_extended_t));
    if (!ext) return NULL;

    ext->config = *config;
    ext->ctx = ctx;
    ext->current_time = 0.0f;
    ext->last_dt_used = config->dt;
    ext->error_estimate = 0.0f;
    ext->atol = 1e-6f;
    ext->rtol = 1e-3f;
    ext->dt_min = 0.01f;
    ext->dt_max = 10.0f;

    nimcp_lnn_layer_gpu_t* layer = &ext->base;
    layer->n_neurons = config->hidden_dim;
    layer->n_inputs = config->input_dim;
    layer->activation = config->activation;

    uint32_t n = config->hidden_dim;
    uint32_t d = config->input_dim;

    uint64_t seed = config->seed ? config->seed : (uint64_t)time(NULL);

    float std_W_in = config->weight_init_std > 0 ? config->weight_init_std : sqrtf(2.0f / (d + n));
    float std_W_rec = config->weight_init_std > 0 ? config->weight_init_std : sqrtf(2.0f / (n + n));
    float std_W_tau = config->weight_init_std > 0 ? config->weight_init_std : sqrtf(2.0f / (d + n + n));

    // Allocate tensors (CPU wrappers with allocated data)
    #define ALLOC_TENSOR_1D(tensor, size) do { \
        tensor = (nimcp_gpu_tensor_t*)nimcp_calloc(1, sizeof(nimcp_gpu_tensor_t)); \
        if (tensor) { \
            tensor->data = nimcp_calloc(size, sizeof(float)); \
            tensor->numel = size; \
            tensor->ndim = 1; \
            tensor->dims[0] = size; \
        } \
    } while(0)

    #define ALLOC_TENSOR_2D(tensor, rows, cols) do { \
        tensor = (nimcp_gpu_tensor_t*)nimcp_calloc(1, sizeof(nimcp_gpu_tensor_t)); \
        if (tensor) { \
            tensor->data = nimcp_calloc((rows) * (cols), sizeof(float)); \
            tensor->numel = (rows) * (cols); \
            tensor->ndim = 2; \
            tensor->dims[0] = rows; \
            tensor->dims[1] = cols; \
        } \
    } while(0)

    ALLOC_TENSOR_1D(layer->x, n);
    ALLOC_TENSOR_1D(layer->dx_dt, n);
    ALLOC_TENSOR_1D(layer->tau, n);
    ALLOC_TENSOR_1D(layer->tau_base, n);
    ALLOC_TENSOR_1D(layer->b_in, n);
    ALLOC_TENSOR_1D(layer->b_tau, n);
    ALLOC_TENSOR_2D(layer->W_in, n, d);
    ALLOC_TENSOR_2D(layer->W_rec, n, n);
    ALLOC_TENSOR_2D(layer->W_tau, n, d + n);

    #undef ALLOC_TENSOR_1D
    #undef ALLOC_TENSOR_2D

    // Check allocations
    if (!layer->x || !layer->x->data ||
        !layer->dx_dt || !layer->dx_dt->data ||
        !layer->tau || !layer->tau->data ||
        !layer->tau_base || !layer->tau_base->data ||
        !layer->b_in || !layer->b_in->data ||
        !layer->b_tau || !layer->b_tau->data ||
        !layer->W_in || !layer->W_in->data ||
        !layer->W_rec || !layer->W_rec->data ||
        !layer->W_tau || !layer->W_tau->data) {
        nimcp_lnn_layer_gpu_extended_destroy(ext);
        return NULL;
    }

    // Initialize weights
    float* W_in_data = (float*)layer->W_in->data;
    float* W_rec_data = (float*)layer->W_rec->data;
    float* W_tau_data = (float*)layer->W_tau->data;

    for (size_t i = 0; i < n * d; i++) {
        W_in_data[i] = cpu_rand_normal(&seed, std_W_in);
    }
    for (size_t i = 0; i < n * n; i++) {
        W_rec_data[i] = cpu_rand_normal(&seed, std_W_rec);
    }
    for (size_t i = 0; i < n * (d + n); i++) {
        W_tau_data[i] = cpu_rand_normal(&seed, std_W_tau);
    }

    // Initialize tau_base
    float tau_min = config->tau_min > 0 ? config->tau_min : LNN_TAU_MIN_DEFAULT;
    float tau_max = config->tau_max > 0 ? config->tau_max : LNN_TAU_MAX_DEFAULT;
    float* tau_base_data = (float*)layer->tau_base->data;
    float* tau_data = (float*)layer->tau->data;

    for (size_t i = 0; i < n; i++) {
        seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
        float u = (float)(seed & 0xFFFFFFFF) / 4294967296.0f;
        tau_base_data[i] = tau_min + u * (tau_max - tau_min);
        tau_data[i] = tau_base_data[i];
    }

    LOG_DEBUG("Created CPU LNN layer from config: %u inputs, %u neurons", d, n);
    return ext;
}

void nimcp_lnn_layer_gpu_extended_destroy(nimcp_lnn_layer_gpu_extended_t* layer)
{
    if (!layer) return;

    nimcp_lnn_layer_gpu_t* base = &layer->base;

    // Free tensor data and wrappers
    if (base->x) { nimcp_free(base->x->data); nimcp_free(base->x); }
    if (base->dx_dt) { nimcp_free(base->dx_dt->data); nimcp_free(base->dx_dt); }
    if (base->tau) { nimcp_free(base->tau->data); nimcp_free(base->tau); }
    if (base->tau_base) { nimcp_free(base->tau_base->data); nimcp_free(base->tau_base); }
    if (base->b_in) { nimcp_free(base->b_in->data); nimcp_free(base->b_in); }
    if (base->b_tau) { nimcp_free(base->b_tau->data); nimcp_free(base->b_tau); }
    if (base->W_in) { nimcp_free(base->W_in->data); nimcp_free(base->W_in); }
    if (base->W_rec) { nimcp_free(base->W_rec->data); nimcp_free(base->W_rec); }
    if (base->W_tau) { nimcp_free(base->W_tau->data); nimcp_free(base->W_tau); }
    if (base->row_ptr) { nimcp_free(base->row_ptr->data); nimcp_free(base->row_ptr); }
    if (base->col_idx) { nimcp_free(base->col_idx->data); nimcp_free(base->col_idx); }
    if (base->edge_weights) { nimcp_free(base->edge_weights->data); nimcp_free(base->edge_weights); }

    nimcp_free(layer);
}

//=============================================================================
// Layer Accessors (CPU)
//=============================================================================

size_t nimcp_lnn_layer_gpu_get_input_dim(const nimcp_lnn_layer_gpu_extended_t* layer)
{
    if (!layer) return 0;
    return layer->config.input_dim;
}

size_t nimcp_lnn_layer_gpu_get_hidden_dim(const nimcp_lnn_layer_gpu_extended_t* layer)
{
    if (!layer) return 0;
    return layer->config.hidden_dim;
}

lnn_ode_method_t nimcp_lnn_layer_gpu_get_ode_method(const nimcp_lnn_layer_gpu_extended_t* layer)
{
    if (!layer) return LNN_ODE_EULER;
    return layer->config.ode_method;
}

size_t nimcp_lnn_layer_gpu_count_params(const nimcp_lnn_layer_gpu_extended_t* layer)
{
    if (!layer) return 0;

    uint32_t n = layer->config.hidden_dim;
    uint32_t d = layer->config.input_dim;

    size_t count = n * d + n * n + n * (d + n) + n + n;
    if (layer->config.learn_tau) count += n;
    return count;
}

bool nimcp_lnn_layer_gpu_get_state(
    const nimcp_lnn_layer_gpu_extended_t* layer,
    float* state_out)
{
    if (!layer || !state_out || !layer->base.x) return false;
    memcpy(state_out, layer->base.x->data, layer->config.hidden_dim * sizeof(float));
    return true;
}

bool nimcp_lnn_layer_gpu_reset(nimcp_lnn_layer_gpu_extended_t* layer)
{
    if (!layer || !layer->base.x) return false;
    memset(layer->base.x->data, 0, layer->config.hidden_dim * sizeof(float));
    layer->current_time = 0.0f;
    layer->last_dt_used = layer->config.dt;
    layer->error_estimate = 0.0f;
    return true;
}

//=============================================================================
// High-Level Forward Operations (CPU)
//=============================================================================

bool nimcp_lnn_layer_gpu_step(
    nimcp_lnn_layer_gpu_extended_t* layer,
    const nimcp_gpu_tensor_t* input)
{
    if (!layer || !input) return false;

    nimcp_lnn_ode_config_t ode_config;
    ode_config.method = layer->config.ode_method;
    ode_config.dt = layer->config.dt;
    ode_config.dt_min = layer->dt_min;
    ode_config.dt_max = layer->dt_max;
    ode_config.error_tolerance = layer->atol;
    ode_config.max_steps = 1;
    ode_config.adaptive_stepping = false;

    bool ok = nimcp_gpu_lnn_ode_step(layer->ctx, &layer->base, input, &ode_config);
    if (ok) {
        layer->current_time += layer->config.dt;
        layer->last_dt_used = layer->config.dt;
    }
    return ok;
}

void nimcp_lnn_layer_gpu_set_adaptive_params(
    nimcp_lnn_layer_gpu_extended_t* layer,
    float atol,
    float rtol,
    float dt_min,
    float dt_max)
{
    if (!layer) return;
    layer->atol = atol;
    layer->rtol = rtol;
    layer->dt_min = dt_min;
    layer->dt_max = dt_max;
}

bool nimcp_lnn_layer_gpu_step_adaptive(
    nimcp_lnn_layer_gpu_extended_t* layer,
    const nimcp_gpu_tensor_t* input,
    float* dt_used)
{
    if (!layer || !input) return false;

    nimcp_lnn_ode_config_t ode_config;
    ode_config.method = LNN_ODE_DOPRI5;
    ode_config.dt = layer->last_dt_used;
    ode_config.dt_min = layer->dt_min;
    ode_config.dt_max = layer->dt_max;
    ode_config.error_tolerance = layer->atol;
    ode_config.max_steps = 10;
    ode_config.adaptive_stepping = true;

    float dt = layer->last_dt_used;
    bool ok = nimcp_gpu_lnn_dopri5_step(layer->ctx, &layer->base, input, &dt, &ode_config);

    if (ok) {
        layer->current_time += dt;
        layer->last_dt_used = dt;
        if (dt_used) *dt_used = dt;
    }
    return ok;
}

float nimcp_lnn_layer_gpu_get_error_estimate(const nimcp_lnn_layer_gpu_extended_t* layer)
{
    if (!layer) return 0.0f;
    return layer->error_estimate;
}

bool nimcp_lnn_layer_gpu_forward_sequence(
    nimcp_lnn_layer_gpu_extended_t* layer,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    size_t seq_len)
{
    if (!layer || !input || !output) return false;

    uint32_t input_dim = layer->config.input_dim;
    uint32_t hidden_dim = layer->config.hidden_dim;

    nimcp_lnn_layer_gpu_reset(layer);

    for (size_t t = 0; t < seq_len; t++) {
        size_t input_offset = t * input_dim;
        size_t input_step_dims[1] = {input_dim};  // Stack-allocate dims array
        nimcp_gpu_tensor_t input_step;
        memset(&input_step, 0, sizeof(input_step));
        input_step.data = (float*)input->data + input_offset;
        input_step.numel = input_dim;
        input_step.ndim = 1;
        input_step.dims = input_step_dims;  // Point to stack-allocated array

        if (!nimcp_lnn_layer_gpu_step(layer, &input_step)) {
            return false;
        }

        size_t output_offset = t * hidden_dim;
        memcpy((float*)output->data + output_offset,
               layer->base.x->data,
               hidden_dim * sizeof(float));
    }

    return true;
}

#endif // NIMCP_ENABLE_CUDA
