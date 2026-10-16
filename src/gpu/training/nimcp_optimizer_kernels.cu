/**
 * @file nimcp_optimizer_kernels.cu
 * @brief GPU Optimizer CUDA Kernels
 *
 * WHAT: CUDA kernels for neural network optimizers
 * WHY:  GPU acceleration for parameter updates during training
 * HOW:  Custom kernels for SGD, Adam, AdamW, RMSprop, AdaGrad
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

// Include CUDA headers FIRST (before any extern "C" blocks from our headers)
#include "utils/memory/nimcp_memory.h"
#include <cuda_runtime.h>
#include <stdlib.h>
#include <math.h>

// Now include our headers (which have extern "C" blocks)
#include "gpu/training/nimcp_training_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include <cublas_v2.h>

#define LOG_MODULE "OPTIMIZER_GPU"

#define BLOCK_SIZE 256
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)

//=============================================================================
// Optimizer State Management
//=============================================================================

nimcp_optim_state_t* nimcp_optim_state_create(
    nimcp_gpu_context_t* ctx,
    nimcp_optim_type_t type,
    const nimcp_gpu_tensor_t* param,
    float lr)
{
    if (!ctx || !param) {
        LOG_ERROR("Invalid parameters for optimizer state creation");
        return NULL;
    }

    nimcp_optim_state_t* state = (nimcp_optim_state_t*)nimcp_calloc(1, sizeof(nimcp_optim_state_t));
    if (!state) {
        LOG_ERROR("Failed to allocate optimizer state");
        return NULL;
    }

    state->type = type;
    state->lr = lr;
    state->t = 0;

    // Default hyperparameters
    state->beta1 = 0.9f;
    state->beta2 = 0.999f;
    state->eps = 1e-8f;
    state->weight_decay = 0.0f;
    state->momentum = 0.9f;
    state->nesterov = false;

    // Allocate momentum and variance tensors based on optimizer type
    switch (type) {
        case NIMCP_OPTIM_SGD_MOMENTUM:
        case NIMCP_OPTIM_ADAM:
        case NIMCP_OPTIM_ADAMW:
        case NIMCP_OPTIM_NADAM:
            state->m = nimcp_gpu_tensor_create(ctx, param->dims, param->ndim, param->precision);
            if (!state->m) goto cleanup;
            nimcp_gpu_zeros(ctx, state->m);
            // Fall through for Adam variants
            if (type != NIMCP_OPTIM_SGD_MOMENTUM) {
                state->v = nimcp_gpu_tensor_create(ctx, param->dims, param->ndim, param->precision);
                if (!state->v) goto cleanup;
                nimcp_gpu_zeros(ctx, state->v);
            }
            break;

        case NIMCP_OPTIM_RMSPROP:
        case NIMCP_OPTIM_ADAGRAD:
            state->v = nimcp_gpu_tensor_create(ctx, param->dims, param->ndim, param->precision);
            if (!state->v) goto cleanup;
            nimcp_gpu_zeros(ctx, state->v);
            break;

        case NIMCP_OPTIM_ADADELTA:
            state->v = nimcp_gpu_tensor_create(ctx, param->dims, param->ndim, param->precision);
            state->v_hat = nimcp_gpu_tensor_create(ctx, param->dims, param->ndim, param->precision);
            if (!state->v || !state->v_hat) goto cleanup;
            nimcp_gpu_zeros(ctx, state->v);
            nimcp_gpu_zeros(ctx, state->v_hat);
            break;

        case NIMCP_OPTIM_SGD:
        default:
            // No additional state needed
            break;
    }

    LOG_DEBUG("Created optimizer state: type=%d, lr=%f", type, lr);
    return state;

cleanup:
    if (state->m) nimcp_gpu_tensor_destroy(state->m);
    if (state->v) nimcp_gpu_tensor_destroy(state->v);
    if (state->v_hat) nimcp_gpu_tensor_destroy(state->v_hat);
    nimcp_free(state);
    return NULL;
}

void nimcp_optim_state_destroy(nimcp_optim_state_t* state)
{
    if (!state) return;

    if (state->m) nimcp_gpu_tensor_destroy(state->m);
    if (state->v) nimcp_gpu_tensor_destroy(state->v);
    if (state->v_hat) nimcp_gpu_tensor_destroy(state->v_hat);
    nimcp_free(state);
}

//=============================================================================
// SGD Optimizer Kernels
//=============================================================================

__global__ void kernel_sgd_step(
    float* param, const float* grad,
    float lr, float weight_decay, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        float g = grad[idx];
        if (weight_decay != 0.0f) {
            g += weight_decay * param[idx];
        }
        param[idx] -= lr * g;
    }
}

__global__ void kernel_sgd_momentum_step(
    float* param, const float* grad, float* velocity,
    float lr, float momentum, float weight_decay, bool nesterov, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        float g = grad[idx];
        if (weight_decay != 0.0f) {
            g += weight_decay * param[idx];
        }

        float v = momentum * velocity[idx] + g;
        velocity[idx] = v;

        if (nesterov) {
            param[idx] -= lr * (g + momentum * v);
        } else {
            param[idx] -= lr * v;
        }
    }
}

bool nimcp_gpu_optim_sgd(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* param,
    const nimcp_gpu_tensor_t* grad,
    nimcp_optim_state_t* state)
{
    if (!ctx || !param || !grad || !state) return false;

    // Initialize GPU recovery if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t n = param->numel;

    if (state->momentum != 0.0f && state->m) {
        kernel_sgd_momentum_step<<<GRID_SIZE(n), BLOCK_SIZE, 0, nimcp_gpu_get_pool_stream(ctx)>>>(
            (float*)param->data, (const float*)grad->data, (float*)state->m->data,
            state->lr, state->momentum, state->weight_decay, state->nesterov, n);
    } else {
        kernel_sgd_step<<<GRID_SIZE(n), BLOCK_SIZE, 0, nimcp_gpu_get_pool_stream(ctx)>>>(
            (float*)param->data, (const float*)grad->data,
            state->lr, state->weight_decay, n);
    }

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    state->t++;
    return true;
}

//=============================================================================
// Adam Optimizer Kernels
//=============================================================================

__global__ void kernel_adam_step(
    float* param, const float* grad, float* m, float* v,
    float lr, float beta1, float beta2, float eps,
    float weight_decay, uint64_t t, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        float g = grad[idx];

        // L2 regularization (for Adam, not AdamW)
        if (weight_decay != 0.0f) {
            g += weight_decay * param[idx];
        }

        // Update biased first moment estimate
        float m_new = beta1 * m[idx] + (1.0f - beta1) * g;
        m[idx] = m_new;

        // Update biased second raw moment estimate
        float v_new = beta2 * v[idx] + (1.0f - beta2) * g * g;
        v[idx] = v_new;

        // Bias correction
        float m_hat = m_new / (1.0f - powf(beta1, (float)t));
        float v_hat = v_new / (1.0f - powf(beta2, (float)t));

        // Update parameters
        param[idx] -= lr * m_hat / (sqrtf(v_hat) + eps);
    }
}

bool nimcp_gpu_optim_adam(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* param,
    const nimcp_gpu_tensor_t* grad,
    nimcp_optim_state_t* state)
{
    if (!ctx || !param || !grad || !state) return false;
    if (!state->m || !state->v) {
        LOG_ERROR("Adam requires momentum and variance tensors");
        return false;
    }

    // Initialize GPU recovery if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    state->t++;
    size_t n = param->numel;

    kernel_adam_step<<<GRID_SIZE(n), BLOCK_SIZE, 0, nimcp_gpu_get_pool_stream(ctx)>>>(
        (float*)param->data, (const float*)grad->data,
        (float*)state->m->data, (float*)state->v->data,
        state->lr, state->beta1, state->beta2, state->eps,
        state->weight_decay, state->t, n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// AdamW Optimizer Kernels
//=============================================================================

__global__ void kernel_adamw_step(
    float* param, const float* grad, float* m, float* v,
    float lr, float beta1, float beta2, float eps,
    float weight_decay, uint64_t t, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        float g = grad[idx];
        float p = param[idx];

        // Update biased first moment estimate
        float m_new = beta1 * m[idx] + (1.0f - beta1) * g;
        m[idx] = m_new;

        // Update biased second raw moment estimate
        float v_new = beta2 * v[idx] + (1.0f - beta2) * g * g;
        v[idx] = v_new;

        // Bias correction
        float m_hat = m_new / (1.0f - powf(beta1, (float)t));
        float v_hat = v_new / (1.0f - powf(beta2, (float)t));

        // AdamW: Decoupled weight decay (applied separately from gradient)
        float update = m_hat / (sqrtf(v_hat) + eps) + weight_decay * p;

        param[idx] = p - lr * update;
    }
}

bool nimcp_gpu_optim_adamw(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* param,
    const nimcp_gpu_tensor_t* grad,
    nimcp_optim_state_t* state)
{
    if (!ctx || !param || !grad || !state) return false;
    if (!state->m || !state->v) {
        LOG_ERROR("AdamW requires momentum and variance tensors");
        return false;
    }

    // Initialize GPU recovery if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    state->t++;
    size_t n = param->numel;

    kernel_adamw_step<<<GRID_SIZE(n), BLOCK_SIZE, 0, nimcp_gpu_get_pool_stream(ctx)>>>(
        (float*)param->data, (const float*)grad->data,
        (float*)state->m->data, (float*)state->v->data,
        state->lr, state->beta1, state->beta2, state->eps,
        state->weight_decay, state->t, n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// RMSprop Optimizer Kernels
//=============================================================================

__global__ void kernel_rmsprop_step(
    float* param, const float* grad, float* v,
    float lr, float alpha, float eps, float weight_decay, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        float g = grad[idx];
        if (weight_decay != 0.0f) {
            g += weight_decay * param[idx];
        }

        // Update squared gradient moving average
        float v_new = alpha * v[idx] + (1.0f - alpha) * g * g;
        v[idx] = v_new;

        // Update parameters
        param[idx] -= lr * g / (sqrtf(v_new) + eps);
    }
}

bool nimcp_gpu_optim_rmsprop(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* param,
    const nimcp_gpu_tensor_t* grad,
    nimcp_optim_state_t* state)
{
    if (!ctx || !param || !grad || !state) return false;
    if (!state->v) {
        LOG_ERROR("RMSprop requires variance tensor");
        return false;
    }

    // Initialize GPU recovery if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t n = param->numel;

    // RMSprop uses beta2 as alpha (decay factor)
    kernel_rmsprop_step<<<GRID_SIZE(n), BLOCK_SIZE, 0, nimcp_gpu_get_pool_stream(ctx)>>>(
        (float*)param->data, (const float*)grad->data, (float*)state->v->data,
        state->lr, state->beta2, state->eps, state->weight_decay, n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    state->t++;
    return true;
}

//=============================================================================
// AdaGrad Optimizer Kernels
//=============================================================================

__global__ void kernel_adagrad_step(
    float* param, const float* grad, float* v,
    float lr, float eps, float weight_decay, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        float g = grad[idx];
        if (weight_decay != 0.0f) {
            g += weight_decay * param[idx];
        }

        // Accumulate squared gradients
        float v_new = v[idx] + g * g;
        v[idx] = v_new;

        // Update parameters
        param[idx] -= lr * g / (sqrtf(v_new) + eps);
    }
}

bool nimcp_gpu_optim_adagrad(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* param,
    const nimcp_gpu_tensor_t* grad,
    nimcp_optim_state_t* state)
{
    if (!ctx || !param || !grad || !state) return false;
    if (!state->v) {
        LOG_ERROR("AdaGrad requires variance tensor");
        return false;
    }

    // Initialize GPU recovery if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t n = param->numel;

    kernel_adagrad_step<<<GRID_SIZE(n), BLOCK_SIZE, 0, nimcp_gpu_get_pool_stream(ctx)>>>(
        (float*)param->data, (const float*)grad->data, (float*)state->v->data,
        state->lr, state->eps, state->weight_decay, n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    state->t++;
    return true;
}

//=============================================================================
// Learning Rate Schedulers
//=============================================================================

float nimcp_lr_step(float initial_lr, uint64_t step, uint64_t step_size, float gamma)
{
    return initial_lr * powf(gamma, (float)(step / step_size));
}

float nimcp_lr_cosine(float max_lr, float min_lr, uint64_t step, uint64_t total_steps)
{
    if (step >= total_steps) return min_lr;
    float progress = (float)step / (float)total_steps;
    return min_lr + 0.5f * (max_lr - min_lr) * (1.0f + cosf(M_PI * progress));
}

float nimcp_lr_warmup_linear(float max_lr, uint64_t step, uint64_t warmup_steps, uint64_t total_steps)
{
    if (step < warmup_steps) {
        // Linear warmup
        return max_lr * (float)step / (float)warmup_steps;
    } else {
        // Linear decay
        float progress = (float)(step - warmup_steps) / (float)(total_steps - warmup_steps);
        return max_lr * (1.0f - progress);
    }
}

float nimcp_lr_exponential(float initial_lr, uint64_t step, float decay_rate)
{
    return initial_lr * expf(-decay_rate * (float)step);
}

//=============================================================================
// Cross-network bridge GPU GEMV
//=============================================================================

/**
 * @brief GPU-accelerated GEMV with persistent device buffers.
 *
 * On first call (or dimension change), allocates device memory and caches
 * pointers in gpu_d_W/gpu_d_x/gpu_d_y/gpu_buf_rows/gpu_buf_cols (passed
 * via the 6 trailing pointer args). Subsequent calls reuse the buffers,
 * avoiding ~50μs alloc/free overhead per step.
 */
extern "C" bool nimcp_gpu_bridge_gemv(void* gpu_ctx, const float* W, const float* x,
                            const float* bias, float* y,
                            uint32_t rows, uint32_t cols,
                            void** p_d_W, void** p_d_x, void** p_d_y,
                            uint32_t* p_buf_rows, uint32_t* p_buf_cols) {
    if (!gpu_ctx || !W || !x || !y || rows == 0 || cols == 0) return false;

    nimcp_gpu_context_t* ctx = (nimcp_gpu_context_t*)gpu_ctx;
    if (!ctx->cublas_initialized) return false;

    cublasHandle_t handle = (cublasHandle_t)ctx->cublas_handle;

    size_t W_bytes = (size_t)rows * cols * sizeof(float);
    size_t x_bytes = (size_t)cols * sizeof(float);
    size_t y_bytes = (size_t)rows * sizeof(float);

    /* Reuse persistent buffers if dimensions match, else (re)allocate */
    float* d_W = p_d_W ? (float*)*p_d_W : NULL;
    float* d_x = p_d_x ? (float*)*p_d_x : NULL;
    float* d_y = p_d_y ? (float*)*p_d_y : NULL;

    if (!d_W || !d_x || !d_y ||
        (p_buf_rows && *p_buf_rows != rows) ||
        (p_buf_cols && *p_buf_cols != cols)) {
        /* Free old buffers if dimensions changed */
        if (d_W) nimcp_gpu_free(ctx, d_W);
        if (d_x) nimcp_gpu_free(ctx, d_x);
        if (d_y) nimcp_gpu_free(ctx, d_y);

        d_W = (float*)nimcp_gpu_malloc(ctx, W_bytes);
        d_x = (float*)nimcp_gpu_malloc(ctx, x_bytes);
        d_y = (float*)nimcp_gpu_malloc(ctx, y_bytes);
        if (!d_W || !d_x || !d_y) {
            if (d_W) nimcp_gpu_free(ctx, d_W);
            if (d_x) nimcp_gpu_free(ctx, d_x);
            if (d_y) nimcp_gpu_free(ctx, d_y);
            if (p_d_W) *p_d_W = NULL;
            if (p_d_x) *p_d_x = NULL;
            if (p_d_y) *p_d_y = NULL;
            return false;
        }

        /* Cache for reuse */
        if (p_d_W) *p_d_W = d_W;
        if (p_d_x) *p_d_x = d_x;
        if (p_d_y) *p_d_y = d_y;
        if (p_buf_rows) *p_buf_rows = rows;
        if (p_buf_cols) *p_buf_cols = cols;
    }

    cudaStream_t stream = nimcp_gpu_get_pool_stream(ctx);
    cudaMemcpyAsync(d_W, W, W_bytes, cudaMemcpyHostToDevice, stream);
    cudaMemcpyAsync(d_x, x, x_bytes, cudaMemcpyHostToDevice, stream);

    if (bias) {
        cudaMemcpyAsync(d_y, bias, y_bytes, cudaMemcpyHostToDevice, stream);
    } else {
        cudaMemsetAsync(d_y, 0, y_bytes, stream);
    }

    cublasSetStream(handle, stream);
    float alpha = 1.0f;
    float beta_val = bias ? 1.0f : 0.0f;

    cublasStatus_t status = cublasSgemv(handle,
        CUBLAS_OP_T, (int)cols, (int)rows, &alpha,
        d_W, (int)cols, d_x, 1, &beta_val, d_y, 1);

    if (status != CUBLAS_STATUS_SUCCESS) {
        return false;  /* Buffers stay allocated for retry */
    }

    cudaMemcpyAsync(y, d_y, y_bytes, cudaMemcpyDeviceToHost, stream);
    cudaStreamSynchronize(stream);
    return true;
}

#else // !NIMCP_ENABLE_CUDA

#include "gpu/training/nimcp_training_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include <stdlib.h>
#include <math.h>

#define LOG_MODULE "OPTIMIZER_GPU"

nimcp_optim_state_t* nimcp_optim_state_create(nimcp_gpu_context_t* ctx,
    nimcp_optim_type_t type, const nimcp_gpu_tensor_t* param, float lr)
{
    LOG_WARN("CUDA not available - optimizer requires GPU");
    return NULL;
}

void nimcp_optim_state_destroy(nimcp_optim_state_t* state)
{
    if (state) nimcp_free(state);
}

bool nimcp_gpu_optim_sgd(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* param,
    const nimcp_gpu_tensor_t* grad, nimcp_optim_state_t* state)
{
    return false;
}

bool nimcp_gpu_optim_adam(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* param,
    const nimcp_gpu_tensor_t* grad, nimcp_optim_state_t* state)
{
    return false;
}

bool nimcp_gpu_optim_adamw(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* param,
    const nimcp_gpu_tensor_t* grad, nimcp_optim_state_t* state)
{
    return false;
}

bool nimcp_gpu_optim_rmsprop(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* param,
    const nimcp_gpu_tensor_t* grad, nimcp_optim_state_t* state)
{
    return false;
}

bool nimcp_gpu_optim_adagrad(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* param,
    const nimcp_gpu_tensor_t* grad, nimcp_optim_state_t* state)
{
    return false;
}

float nimcp_lr_step(float initial_lr, uint64_t step, uint64_t step_size, float gamma)
{
    return initial_lr * powf(gamma, (float)(step / step_size));
}

float nimcp_lr_cosine(float max_lr, float min_lr, uint64_t step, uint64_t total_steps)
{
    if (step >= total_steps) return min_lr;
    float progress = (float)step / (float)total_steps;
    return min_lr + 0.5f * (max_lr - min_lr) * (1.0f + cosf(M_PI * progress));
}

float nimcp_lr_warmup_linear(float max_lr, uint64_t step, uint64_t warmup_steps, uint64_t total_steps)
{
    if (step < warmup_steps) {
        return max_lr * (float)step / (float)warmup_steps;
    } else {
        float progress = (float)(step - warmup_steps) / (float)(total_steps - warmup_steps);
        return max_lr * (1.0f - progress);
    }
}

float nimcp_lr_exponential(float initial_lr, uint64_t step, float decay_rate)
{
    return initial_lr * expf(-decay_rate * (float)step);
}

//=============================================================================
// Cross-Network Bridge GPU GEMV (Optimization #15)
#endif // NIMCP_ENABLE_CUDA
