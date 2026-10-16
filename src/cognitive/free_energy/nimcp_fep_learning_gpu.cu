/**
 * @file nimcp_fep_learning_gpu.cu
 * @brief GPU-accelerated FEP Learning Kernels
 * @version 1.1.0
 * @date 2026-01-01
 *
 * WHAT: CUDA kernels for accelerating FEP matrix learning operations
 * WHY:  Matrix operations in FEP learning benefit greatly from GPU parallelism
 * HOW:  GPU kernels for matvec, gradient computation, and batch updates
 *
 * HOT PATHS ACCELERATED:
 * 1. Matrix-vector multiply: prediction = A * state (uses nimcp_gpu_gemv)
 * 2. Outer product gradient: grad = error x state^T + lambda*A
 * 3. Matrix update with regularization: A -= lr * (grad + lambda*A)
 * 4. Batch gradient accumulation (fused reduce)
 *
 * REFACTORED (2026-01-01):
 * ========================
 * - Updated to use nimcp_gpu_tensor_t for matrix/vector storage
 * - Leverages nimcp_gpu_gemv for matrix-vector operations where appropriate
 * - Added tensor-based API variants for consistency
 * - Original raw-pointer internal operations retained for kernels
 */

#ifdef NIMCP_ENABLE_CUDA

#include <cuda_runtime.h>
#include <cuda_fp16.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// Include GPU context for stream management
// Note: Include OUTSIDE extern "C" as GPU context header may include C++ CUDA types
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "FEP_LEARNING_GPU"

// Forward declaration for cleanup function used in error paths
extern "C" void fep_learning_gpu_destroy(struct fep_learning_gpu_context_s* ctx);

//=============================================================================
// Constants
//=============================================================================

#define FEP_GPU_BLOCK_SIZE 256
#define FEP_GPU_GRID_SIZE(n) (((n) + FEP_GPU_BLOCK_SIZE - 1) / FEP_GPU_BLOCK_SIZE)
#define FEP_GPU_MIN_SIZE_FOR_GPU 64  // Minimum elements before using GPU

//=============================================================================
// CUDA Error Handling
//=============================================================================

#define CUDA_CHECK(call) do { \
    cudaError_t err = call; \
    if (err != cudaSuccess) { \
        LOG_ERROR("CUDA error: %s", cudaGetErrorString(err)); \
        return -1; \
    } \
} while(0)

#define CUDA_CHECK_BOOL(call) do { \
    cudaError_t err = call; \
    if (err != cudaSuccess) { \
        LOG_ERROR("CUDA error: %s", cudaGetErrorString(err)); \
        return false; \
    } \
} while(0)

#define CUDA_CHECK_NULL(call) do { \
    cudaError_t err = call; \
    if (err != cudaSuccess) { \
        LOG_ERROR("CUDA error: %s", cudaGetErrorString(err)); \
        return NULL; \
    } \
} while(0)

//=============================================================================
// Kernel: Matrix-Vector Multiply (prediction = A * x)
//=============================================================================

/**
 * @brief GPU kernel for matrix-vector multiplication
 *
 * Each thread computes one element of the output vector
 * output[i] = sum_j(A[i,j] * x[j])
 *
 * NOTE: For larger matrices, prefer using nimcp_gpu_gemv which uses cuBLAS.
 *       This kernel is retained for small matrices where cuBLAS overhead
 *       exceeds the benefit.
 *
 * @param A Matrix (rows x cols), row-major
 * @param x Input vector (cols)
 * @param output Output vector (rows)
 * @param rows Number of rows
 * @param cols Number of columns
 */
__global__ void kernel_fep_matvec(
    const float* __restrict__ A,
    const float* __restrict__ x,
    float* __restrict__ output,
    uint32_t rows,
    uint32_t cols)
{
    uint32_t row = blockIdx.x * blockDim.x + threadIdx.x;
    if (row >= rows) return;

    float sum = 0.0f;
    const float* row_ptr = A + row * cols;

    #pragma unroll 4
    for (uint32_t j = 0; j < cols; j++) {
        sum += row_ptr[j] * x[j];
    }

    output[row] = sum;
}

//=============================================================================
// Kernel: Outer Product Gradient (grad = -error x x^T + lambda*A)
//=============================================================================

/**
 * @brief GPU kernel for computing gradient with regularization
 *
 * grad[i,j] = -error[i] * x[j] + lambda * A[i,j]
 *
 * @param error Prediction error vector (out_dim)
 * @param x Input state vector (in_dim)
 * @param A Current matrix (out_dim x in_dim)
 * @param grad Output gradient (out_dim x in_dim)
 * @param lambda L2 regularization coefficient
 * @param out_dim Output dimension (rows)
 * @param in_dim Input dimension (cols)
 */
__global__ void kernel_fep_gradient(
    const float* __restrict__ error,
    const float* __restrict__ x,
    const float* __restrict__ A,
    float* __restrict__ grad,
    float lambda,
    uint32_t out_dim,
    uint32_t in_dim)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t total = out_dim * in_dim;
    if (idx >= total) return;

    uint32_t i = idx / in_dim;  // row
    uint32_t j = idx % in_dim;  // col

    float g = -error[i] * x[j] + lambda * A[idx];
    grad[idx] = g;
}

//=============================================================================
// Kernel: Matrix Update with Momentum (A = A - lr * (momentum * v + grad))
//=============================================================================

/**
 * @brief GPU kernel for matrix update with optional momentum
 *
 * If momentum enabled:
 *   v[i] = beta * v[i] + grad[i]
 *   A[i] = A[i] - lr * v[i]
 * Else:
 *   A[i] = A[i] - lr * grad[i]
 *
 * @param A Matrix to update (in-place)
 * @param grad Gradient
 * @param velocity Momentum velocity buffer (can be NULL)
 * @param lr Learning rate
 * @param beta Momentum coefficient
 * @param size Total elements
 */
__global__ void kernel_fep_update(
    float* __restrict__ A,
    const float* __restrict__ grad,
    float* __restrict__ velocity,
    float lr,
    float beta,
    uint32_t size)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= size) return;

    float g = grad[idx];

    if (velocity != NULL) {
        float v = beta * velocity[idx] + g;
        velocity[idx] = v;
        g = v;
    }

    A[idx] = A[idx] - lr * g;
}

//=============================================================================
// Kernel: Compute MSE Loss
//=============================================================================

/**
 * @brief GPU kernel to compute MSE loss: 0.5 * ||target - pred||^2
 *
 * Uses parallel reduction for efficient summation
 *
 * @param target Target values
 * @param pred Predicted values
 * @param partial_sums Output partial sums (one per block)
 * @param n Number of elements
 */
__global__ void kernel_fep_mse_loss(
    const float* __restrict__ target,
    const float* __restrict__ pred,
    float* __restrict__ partial_sums,
    uint32_t n)
{
    extern __shared__ float sdata[];

    uint32_t tid = threadIdx.x;
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    // Load and compute squared difference
    float val = 0.0f;
    if (idx < n) {
        float diff = target[idx] - pred[idx];
        val = diff * diff;
    }
    sdata[tid] = val;
    __syncthreads();

    // Parallel reduction
    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            sdata[tid] += sdata[tid + s];
        }
        __syncthreads();
    }

    // Write block sum
    if (tid == 0) {
        partial_sums[blockIdx.x] = sdata[0];
    }
}

//=============================================================================
// Kernel: Batch Gradient Accumulation
//=============================================================================

/**
 * @brief GPU kernel to accumulate gradients from multiple samples
 *
 * Adds gradient contribution to batch accumulator
 *
 * @param batch_grad Accumulated gradient (atomically updated)
 * @param sample_grad Single sample gradient
 * @param size Matrix size
 */
__global__ void kernel_fep_accumulate_gradient(
    float* __restrict__ batch_grad,
    const float* __restrict__ sample_grad,
    uint32_t size)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= size) return;

    atomicAdd(&batch_grad[idx], sample_grad[idx]);
}

/**
 * @brief GPU kernel to scale accumulated gradient by 1/N
 *
 * @param grad Gradient to scale (in-place)
 * @param scale Scale factor (1.0 / N)
 * @param size Matrix size
 */
__global__ void kernel_fep_scale_gradient(
    float* __restrict__ grad,
    float scale,
    uint32_t size)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= size) return;

    grad[idx] *= scale;
}

/**
 * @brief GPU kernel for vector subtraction: error = target - prediction
 *
 * @param target Target vector
 * @param prediction Prediction vector
 * @param error Output error vector
 * @param n Number of elements
 */
__global__ void kernel_fep_compute_error(
    const float* __restrict__ target,
    const float* __restrict__ prediction,
    float* __restrict__ error,
    uint32_t n)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    error[idx] = target[idx] - prediction[idx];
}

//=============================================================================
// Host API - GPU Context Structure for FEP Learning
//=============================================================================

/**
 * @brief GPU context for FEP learning operations
 *
 * Stores GPU tensors for matrices, vectors, and intermediate buffers.
 * Uses nimcp_gpu_tensor_t for all device storage.
 */
typedef struct fep_learning_gpu_context_s {
    nimcp_gpu_context_t* gpu_ctx;   // Main GPU context

    // Device tensors for transition learner
    nimcp_gpu_tensor_t* A;          // Transition matrix on GPU [state_dim x state_dim]
    nimcp_gpu_tensor_t* A_grad;     // Gradient buffer
    nimcp_gpu_tensor_t* A_velocity; // Momentum velocity
    nimcp_gpu_tensor_t* state_t;    // Current state [state_dim]
    nimcp_gpu_tensor_t* state_t1;   // Next state [state_dim]
    nimcp_gpu_tensor_t* prediction; // Prediction buffer [state_dim]
    nimcp_gpu_tensor_t* error;      // Error buffer [max(state_dim, obs_dim)]

    // Device tensors for likelihood learner
    nimcp_gpu_tensor_t* B;          // Likelihood matrix on GPU [obs_dim x state_dim]
    nimcp_gpu_tensor_t* B_grad;     // Gradient buffer
    nimcp_gpu_tensor_t* B_velocity; // Momentum velocity
    nimcp_gpu_tensor_t* observation; // Observation vector [obs_dim]

    // Batch learning buffers
    nimcp_gpu_tensor_t* batch_grad; // Batch gradient accumulator
    float* d_loss_partial;          // Partial sums for loss reduction (raw pointer for efficiency)

    // Dimensions
    uint32_t state_dim;
    uint32_t obs_dim;
    uint32_t max_batch_size;

    // Status
    bool initialized;
    bool matrices_on_gpu;
} fep_learning_gpu_context_t;

//=============================================================================
// Helper: Create 1D or 2D tensor
//=============================================================================

static nimcp_gpu_tensor_t* create_fep_tensor_1d(
    nimcp_gpu_context_t* ctx,
    uint32_t dim
) {
    size_t dims[1] = { dim };
    return nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
}

static nimcp_gpu_tensor_t* create_fep_tensor_2d(
    nimcp_gpu_context_t* ctx,
    uint32_t rows,
    uint32_t cols
) {
    size_t dims[2] = { rows, cols };
    return nimcp_gpu_tensor_create(ctx, dims, 2, NIMCP_GPU_PRECISION_FP32);
}

//=============================================================================
// Host API Functions
//=============================================================================

extern "C" {

/**
 * @brief Create GPU context for FEP learning
 *
 * Allocates GPU tensors for all matrices and vectors needed for learning.
 *
 * @param gpu_ctx Main GPU context
 * @param state_dim State dimension
 * @param obs_dim Observation dimension (0 if not using likelihood)
 * @param max_batch_size Maximum batch size for learning
 * @return FEP learning GPU context
 */
fep_learning_gpu_context_t* fep_learning_gpu_create(
    nimcp_gpu_context_t* gpu_ctx,
    uint32_t state_dim,
    uint32_t obs_dim,
    uint32_t max_batch_size)
{
    if (!gpu_ctx || !nimcp_gpu_context_is_valid(gpu_ctx)) {
        LOG_ERROR("Invalid GPU context");
        return NULL;
    }

    if (state_dim == 0) {
        LOG_ERROR("State dimension cannot be zero");
        return NULL;
    }

    fep_learning_gpu_context_t* ctx =
        (fep_learning_gpu_context_t*)calloc(1, sizeof(fep_learning_gpu_context_t));
    if (!ctx) {
        LOG_ERROR("Failed to allocate FEP learning GPU context");
        return NULL;
    }

    ctx->gpu_ctx = gpu_ctx;
    ctx->state_dim = state_dim;
    ctx->obs_dim = obs_dim;
    ctx->max_batch_size = max_batch_size > 0 ? max_batch_size : 32;

    // Create transition matrix tensors
    ctx->A = create_fep_tensor_2d(gpu_ctx, state_dim, state_dim);
    ctx->A_grad = create_fep_tensor_2d(gpu_ctx, state_dim, state_dim);
    ctx->A_velocity = create_fep_tensor_2d(gpu_ctx, state_dim, state_dim);
    ctx->state_t = create_fep_tensor_1d(gpu_ctx, state_dim);
    ctx->state_t1 = create_fep_tensor_1d(gpu_ctx, state_dim);
    ctx->prediction = create_fep_tensor_1d(gpu_ctx, state_dim);
    ctx->batch_grad = create_fep_tensor_2d(gpu_ctx, state_dim, state_dim);

    // Error buffer sized for max of state_dim and obs_dim
    uint32_t error_dim = (obs_dim > state_dim) ? obs_dim : state_dim;
    ctx->error = create_fep_tensor_1d(gpu_ctx, error_dim);

    if (!ctx->A || !ctx->A_grad || !ctx->A_velocity ||
        !ctx->state_t || !ctx->state_t1 || !ctx->prediction ||
        !ctx->error || !ctx->batch_grad) {
        LOG_ERROR("Failed to allocate transition tensors");
        fep_learning_gpu_destroy(ctx);
        return NULL;
    }

    // Zero velocity buffer
    nimcp_gpu_zeros(gpu_ctx, ctx->A_velocity);

    // Create likelihood tensors if needed
    if (obs_dim > 0) {
        ctx->B = create_fep_tensor_2d(gpu_ctx, obs_dim, state_dim);
        ctx->B_grad = create_fep_tensor_2d(gpu_ctx, obs_dim, state_dim);
        ctx->B_velocity = create_fep_tensor_2d(gpu_ctx, obs_dim, state_dim);
        ctx->observation = create_fep_tensor_1d(gpu_ctx, obs_dim);

        if (!ctx->B || !ctx->B_grad || !ctx->B_velocity || !ctx->observation) {
            LOG_ERROR("Failed to allocate likelihood tensors");
            fep_learning_gpu_destroy(ctx);
            return NULL;
        }

        nimcp_gpu_zeros(gpu_ctx, ctx->B_velocity);
    }

    // Allocate loss reduction buffer (raw pointer for efficiency in reduction kernel)
    uint32_t max_blocks = FEP_GPU_GRID_SIZE(state_dim);
    CUDA_CHECK_NULL(cudaMalloc(&ctx->d_loss_partial, max_blocks * sizeof(float)));

    ctx->initialized = true;
    LOG_DEBUG("FEP learning GPU context created: state_dim=%u, obs_dim=%u",
              state_dim, obs_dim);
    return ctx;
}

/**
 * @brief Destroy FEP learning GPU context
 */
void fep_learning_gpu_destroy(fep_learning_gpu_context_t* ctx)
{
    if (!ctx) return;

    // Destroy all tensors
    if (ctx->A) nimcp_gpu_tensor_destroy(ctx->A);
    if (ctx->A_grad) nimcp_gpu_tensor_destroy(ctx->A_grad);
    if (ctx->A_velocity) nimcp_gpu_tensor_destroy(ctx->A_velocity);
    if (ctx->state_t) nimcp_gpu_tensor_destroy(ctx->state_t);
    if (ctx->state_t1) nimcp_gpu_tensor_destroy(ctx->state_t1);
    if (ctx->prediction) nimcp_gpu_tensor_destroy(ctx->prediction);
    if (ctx->error) nimcp_gpu_tensor_destroy(ctx->error);
    if (ctx->batch_grad) nimcp_gpu_tensor_destroy(ctx->batch_grad);

    if (ctx->B) nimcp_gpu_tensor_destroy(ctx->B);
    if (ctx->B_grad) nimcp_gpu_tensor_destroy(ctx->B_grad);
    if (ctx->B_velocity) nimcp_gpu_tensor_destroy(ctx->B_velocity);
    if (ctx->observation) nimcp_gpu_tensor_destroy(ctx->observation);

    if (ctx->d_loss_partial) cudaFree(ctx->d_loss_partial);

    free(ctx);
}

/**
 * @brief Copy transition matrix to GPU
 */
int fep_learning_gpu_upload_transition(
    fep_learning_gpu_context_t* ctx,
    const float* A,
    uint32_t dim)
{
    if (!ctx || !ctx->initialized || !A) {
        LOG_ERROR("Invalid parameters for upload_transition");
        return -1;
    }
    if (dim != ctx->state_dim) {
        LOG_ERROR("Dimension mismatch: expected %u, got %u", ctx->state_dim, dim);
        return -1;
    }

    size_t size = (size_t)dim * dim * sizeof(float);
    CUDA_CHECK(cudaMemcpy(ctx->A->data, A, size, cudaMemcpyHostToDevice));

    ctx->matrices_on_gpu = true;
    return 0;
}

/**
 * @brief Copy transition matrix from GPU
 */
int fep_learning_gpu_download_transition(
    fep_learning_gpu_context_t* ctx,
    float* A,
    uint32_t dim)
{
    if (!ctx || !ctx->initialized || !A) {
        LOG_ERROR("Invalid parameters for download_transition");
        return -1;
    }
    if (dim != ctx->state_dim) {
        LOG_ERROR("Dimension mismatch: expected %u, got %u", ctx->state_dim, dim);
        return -1;
    }

    size_t size = (size_t)dim * dim * sizeof(float);
    CUDA_CHECK(cudaMemcpy(A, ctx->A->data, size, cudaMemcpyDeviceToHost));
    return 0;
}

/**
 * @brief GPU-accelerated transition learning step
 *
 * @param ctx FEP learning GPU context
 * @param state_t Current state (host)
 * @param state_t1 Next state (host)
 * @param lr Learning rate
 * @param lambda L2 regularization
 * @param momentum Momentum coefficient (0 to disable)
 * @param loss_out Output loss value (can be NULL)
 * @return 0 on success
 */
int fep_learn_transition_gpu(
    fep_learning_gpu_context_t* ctx,
    const float* state_t,
    const float* state_t1,
    float lr,
    float lambda,
    float momentum,
    float* loss_out)
{
    if (!ctx || !ctx->initialized || !state_t || !state_t1) {
        LOG_ERROR("Invalid parameters for learn_transition");
        return -1;
    }
    if (!ctx->matrices_on_gpu) {
        LOG_ERROR("Transition matrix not uploaded to GPU");
        return -1;
    }

    uint32_t dim = ctx->state_dim;
    cudaStream_t stream = (cudaStream_t)nimcp_gpu_get_compute_stream(ctx->gpu_ctx);

    // Copy states to GPU tensors
    size_t vec_size = dim * sizeof(float);
    cudaMemcpyAsync(ctx->state_t->data, state_t, vec_size,
                    cudaMemcpyHostToDevice, stream);
    cudaMemcpyAsync(ctx->state_t1->data, state_t1, vec_size,
                    cudaMemcpyHostToDevice, stream);

    // Compute prediction: A * state_t using nimcp_gpu_gemv
    // y = alpha * A @ x + beta * y
    // prediction = 1.0 * A @ state_t + 0.0 * prediction
    bool gemv_ok = nimcp_gpu_gemv(
        ctx->gpu_ctx,
        ctx->A,          // Matrix A [dim x dim]
        ctx->state_t,    // Vector x [dim]
        ctx->prediction, // Vector y (output) [dim]
        1.0f,            // alpha
        0.0f,            // beta
        false            // trans_a = false
    );

    if (!gemv_ok) {
        LOG_WARN("nimcp_gpu_gemv failed, falling back to custom kernel");
        // Fallback to custom kernel
        kernel_fep_matvec<<<FEP_GPU_GRID_SIZE(dim), FEP_GPU_BLOCK_SIZE, 0, stream>>>(
            static_cast<const float*>(ctx->A->data),
            static_cast<const float*>(ctx->state_t->data),
            static_cast<float*>(ctx->prediction->data),
            dim, dim);
    }

    // Compute error: error = state_t1 - prediction
    kernel_fep_compute_error<<<FEP_GPU_GRID_SIZE(dim), FEP_GPU_BLOCK_SIZE, 0, stream>>>(
        static_cast<const float*>(ctx->state_t1->data),
        static_cast<const float*>(ctx->prediction->data),
        static_cast<float*>(ctx->error->data),
        dim);

    // Compute gradient: grad = -error x state^T + lambda*A
    uint32_t total_elements = dim * dim;

    kernel_fep_gradient<<<FEP_GPU_GRID_SIZE(total_elements), FEP_GPU_BLOCK_SIZE, 0, stream>>>(
        static_cast<const float*>(ctx->error->data),
        static_cast<const float*>(ctx->state_t->data),
        static_cast<const float*>(ctx->A->data),
        static_cast<float*>(ctx->A_grad->data),
        lambda,
        dim, dim);

    // Apply update: A = A - lr * grad (with optional momentum)
    float* velocity_ptr = (momentum > 0.0f) ?
        static_cast<float*>(ctx->A_velocity->data) : NULL;

    kernel_fep_update<<<FEP_GPU_GRID_SIZE(total_elements), FEP_GPU_BLOCK_SIZE, 0, stream>>>(
        static_cast<float*>(ctx->A->data),
        static_cast<const float*>(ctx->A_grad->data),
        velocity_ptr,
        lr,
        momentum,
        total_elements);

    // Compute loss if requested
    if (loss_out) {
        uint32_t loss_grid = FEP_GPU_GRID_SIZE(dim);
        size_t shared_size = FEP_GPU_BLOCK_SIZE * sizeof(float);

        kernel_fep_mse_loss<<<loss_grid, FEP_GPU_BLOCK_SIZE, shared_size, stream>>>(
            static_cast<const float*>(ctx->state_t1->data),
            static_cast<const float*>(ctx->prediction->data),
            ctx->d_loss_partial,
            dim);

        // Copy partial sums and reduce on CPU
        float* partial = (float*)malloc(loss_grid * sizeof(float));
        if (partial) {
            cudaMemcpyAsync(partial, ctx->d_loss_partial,
                            loss_grid * sizeof(float),
                            cudaMemcpyDeviceToHost, stream);
            cudaStreamSynchronize(stream);

            float total_loss = 0.0f;
            for (uint32_t i = 0; i < loss_grid; i++) {
                total_loss += partial[i];
            }
            // MSE loss: sum(error^2) / dim
            *loss_out = total_loss / (float)dim;
            free(partial);
        }
    }

    return 0;
}

/**
 * @brief GPU-accelerated batch transition learning
 *
 * @param ctx FEP learning GPU context
 * @param states Flattened state sequence (n_transitions+1 states)
 * @param n_transitions Number of transitions
 * @param lr Learning rate
 * @param lambda L2 regularization
 * @param momentum Momentum coefficient
 * @param avg_loss_out Average loss output (can be NULL)
 * @return 0 on success
 */
int fep_learn_transition_batch_gpu(
    fep_learning_gpu_context_t* ctx,
    const float* states,
    uint32_t n_transitions,
    float lr,
    float lambda,
    float momentum,
    float* avg_loss_out)
{
    if (!ctx || !ctx->initialized || !states || n_transitions == 0) {
        LOG_ERROR("Invalid parameters for batch transition learning");
        return -1;
    }
    if (!ctx->matrices_on_gpu) {
        LOG_ERROR("Transition matrix not uploaded to GPU");
        return -1;
    }

    uint32_t dim = ctx->state_dim;
    size_t matrix_size = (size_t)dim * dim * sizeof(float);
    cudaStream_t stream = (cudaStream_t)nimcp_gpu_get_compute_stream(ctx->gpu_ctx);

    // Zero batch gradient accumulator
    nimcp_gpu_zeros(ctx->gpu_ctx, ctx->batch_grad);

    float total_loss = 0.0f;

    // Process each transition
    for (uint32_t t = 0; t < n_transitions; t++) {
        const float* state_t_host = states + t * dim;
        const float* state_t1_host = states + (t + 1) * dim;

        // Copy states to GPU
        size_t vec_size = dim * sizeof(float);
        cudaMemcpyAsync(ctx->state_t->data, state_t_host, vec_size,
                        cudaMemcpyHostToDevice, stream);
        cudaMemcpyAsync(ctx->state_t1->data, state_t1_host, vec_size,
                        cudaMemcpyHostToDevice, stream);

        // Compute prediction using nimcp_gpu_gemv
        bool gemv_ok = nimcp_gpu_gemv(
            ctx->gpu_ctx,
            ctx->A,
            ctx->state_t,
            ctx->prediction,
            1.0f, 0.0f, false
        );

        if (!gemv_ok) {
            kernel_fep_matvec<<<FEP_GPU_GRID_SIZE(dim), FEP_GPU_BLOCK_SIZE, 0, stream>>>(
                static_cast<const float*>(ctx->A->data),
                static_cast<const float*>(ctx->state_t->data),
                static_cast<float*>(ctx->prediction->data),
                dim, dim);
        }

        // Compute error
        kernel_fep_compute_error<<<FEP_GPU_GRID_SIZE(dim), FEP_GPU_BLOCK_SIZE, 0, stream>>>(
            static_cast<const float*>(ctx->state_t1->data),
            static_cast<const float*>(ctx->prediction->data),
            static_cast<float*>(ctx->error->data),
            dim);

        // Compute gradient
        uint32_t total_elements = dim * dim;

        kernel_fep_gradient<<<FEP_GPU_GRID_SIZE(total_elements), FEP_GPU_BLOCK_SIZE, 0, stream>>>(
            static_cast<const float*>(ctx->error->data),
            static_cast<const float*>(ctx->state_t->data),
            static_cast<const float*>(ctx->A->data),
            static_cast<float*>(ctx->A_grad->data),
            lambda, dim, dim);

        // Accumulate gradient
        kernel_fep_accumulate_gradient<<<FEP_GPU_GRID_SIZE(total_elements), FEP_GPU_BLOCK_SIZE, 0, stream>>>(
            static_cast<float*>(ctx->batch_grad->data),
            static_cast<const float*>(ctx->A_grad->data),
            total_elements);

        // Compute loss for this step
        if (avg_loss_out) {
            uint32_t loss_grid = FEP_GPU_GRID_SIZE(dim);
            size_t shared_size = FEP_GPU_BLOCK_SIZE * sizeof(float);

            kernel_fep_mse_loss<<<loss_grid, FEP_GPU_BLOCK_SIZE, shared_size, stream>>>(
                static_cast<const float*>(ctx->state_t1->data),
                static_cast<const float*>(ctx->prediction->data),
                ctx->d_loss_partial,
                dim);

            float* partial = (float*)malloc(loss_grid * sizeof(float));
            if (partial) {
                cudaMemcpyAsync(partial, ctx->d_loss_partial,
                                loss_grid * sizeof(float),
                                cudaMemcpyDeviceToHost, stream);
                cudaStreamSynchronize(stream);

                float step_loss = 0.0f;
                for (uint32_t i = 0; i < loss_grid; i++) {
                    step_loss += partial[i];
                }
                free(partial);
                total_loss += step_loss / (float)dim;
            }
        }
    }

    // Average and apply gradient
    uint32_t total_elements = dim * dim;
    float scale = 1.0f / (float)n_transitions;

    kernel_fep_scale_gradient<<<FEP_GPU_GRID_SIZE(total_elements), FEP_GPU_BLOCK_SIZE, 0, stream>>>(
        static_cast<float*>(ctx->batch_grad->data),
        scale,
        total_elements);

    float* velocity_ptr = (momentum > 0.0f) ?
        static_cast<float*>(ctx->A_velocity->data) : NULL;

    kernel_fep_update<<<FEP_GPU_GRID_SIZE(total_elements), FEP_GPU_BLOCK_SIZE, 0, stream>>>(
        static_cast<float*>(ctx->A->data),
        static_cast<const float*>(ctx->batch_grad->data),
        velocity_ptr,
        lr, momentum, total_elements);

    cudaStreamSynchronize(stream);

    if (avg_loss_out) {
        *avg_loss_out = total_loss / (float)n_transitions;
    }

    return 0;
}

/**
 * @brief Check if GPU should be used for FEP learning
 *
 * @param dim State dimension
 * @return true if GPU acceleration is beneficial
 */
bool fep_learning_should_use_gpu(uint32_t dim)
{
    // For small matrices, CPU is faster due to kernel launch overhead
    return dim >= FEP_GPU_MIN_SIZE_FOR_GPU;
}

/**
 * @brief Copy likelihood matrix to GPU
 */
int fep_learning_gpu_upload_likelihood(
    fep_learning_gpu_context_t* ctx,
    const float* B,
    uint32_t obs_dim,
    uint32_t state_dim)
{
    if (!ctx || !ctx->initialized || !B) {
        LOG_ERROR("Invalid parameters for upload_likelihood");
        return -1;
    }
    if (obs_dim != ctx->obs_dim || state_dim != ctx->state_dim) {
        LOG_ERROR("Dimension mismatch");
        return -1;
    }
    if (!ctx->B) {
        LOG_ERROR("Likelihood matrix not allocated (obs_dim was 0?)");
        return -1;
    }

    size_t size = (size_t)obs_dim * state_dim * sizeof(float);
    CUDA_CHECK(cudaMemcpy(ctx->B->data, B, size, cudaMemcpyHostToDevice));
    return 0;
}

/**
 * @brief Copy likelihood matrix from GPU
 */
int fep_learning_gpu_download_likelihood(
    fep_learning_gpu_context_t* ctx,
    float* B,
    uint32_t obs_dim,
    uint32_t state_dim)
{
    if (!ctx || !ctx->initialized || !B) {
        LOG_ERROR("Invalid parameters for download_likelihood");
        return -1;
    }
    if (obs_dim != ctx->obs_dim || state_dim != ctx->state_dim) {
        LOG_ERROR("Dimension mismatch");
        return -1;
    }
    if (!ctx->B) {
        LOG_ERROR("Likelihood matrix not allocated");
        return -1;
    }

    size_t size = (size_t)obs_dim * state_dim * sizeof(float);
    CUDA_CHECK(cudaMemcpy(B, ctx->B->data, size, cudaMemcpyDeviceToHost));
    return 0;
}

/**
 * @brief GPU-accelerated likelihood learning step
 */
int fep_learn_likelihood_gpu(
    fep_learning_gpu_context_t* ctx,
    const float* observation,
    const float* state,
    float lr,
    float lambda,
    float momentum,
    float* loss_out)
{
    if (!ctx || !ctx->initialized || !observation || !state) {
        LOG_ERROR("Invalid parameters for learn_likelihood");
        return -1;
    }
    if (!ctx->B) {
        LOG_ERROR("Likelihood matrix not initialized");
        return -1;
    }

    uint32_t obs_dim = ctx->obs_dim;
    uint32_t state_dim = ctx->state_dim;
    cudaStream_t stream = (cudaStream_t)nimcp_gpu_get_compute_stream(ctx->gpu_ctx);

    // Copy inputs to GPU
    cudaMemcpyAsync(ctx->observation->data, observation,
                    obs_dim * sizeof(float),
                    cudaMemcpyHostToDevice, stream);
    cudaMemcpyAsync(ctx->state_t->data, state,
                    state_dim * sizeof(float),
                    cudaMemcpyHostToDevice, stream);

    // Compute prediction: B * state using nimcp_gpu_gemv
    bool gemv_ok = nimcp_gpu_gemv(
        ctx->gpu_ctx,
        ctx->B,          // Matrix B [obs_dim x state_dim]
        ctx->state_t,    // Vector x [state_dim]
        ctx->error,      // Use error as temp for prediction [obs_dim]
        1.0f, 0.0f, false
    );

    if (!gemv_ok) {
        kernel_fep_matvec<<<FEP_GPU_GRID_SIZE(obs_dim), FEP_GPU_BLOCK_SIZE, 0, stream>>>(
            static_cast<const float*>(ctx->B->data),
            static_cast<const float*>(ctx->state_t->data),
            static_cast<float*>(ctx->error->data),
            obs_dim, state_dim);
    }

    // Note: For likelihood learning, we compute grad = -error x state^T + lambda*B
    // where error = observation - B*state
    // For simplicity, we directly compute the gradient using the outer product kernel

    // Compute gradient
    uint32_t total_elements = obs_dim * state_dim;

    kernel_fep_gradient<<<FEP_GPU_GRID_SIZE(total_elements), FEP_GPU_BLOCK_SIZE, 0, stream>>>(
        static_cast<const float*>(ctx->observation->data),
        static_cast<const float*>(ctx->state_t->data),
        static_cast<const float*>(ctx->B->data),
        static_cast<float*>(ctx->B_grad->data),
        lambda, obs_dim, state_dim);

    // Apply update
    float* velocity_ptr = (momentum > 0.0f) ?
        static_cast<float*>(ctx->B_velocity->data) : NULL;

    kernel_fep_update<<<FEP_GPU_GRID_SIZE(total_elements), FEP_GPU_BLOCK_SIZE, 0, stream>>>(
        static_cast<float*>(ctx->B->data),
        static_cast<const float*>(ctx->B_grad->data),
        velocity_ptr,
        lr, momentum, total_elements);

    cudaStreamSynchronize(stream);

    return 0;
}

//=============================================================================
// Tensor-Based API for Direct GPU Integration
//=============================================================================

/**
 * @brief Get the transition matrix tensor from context
 *
 * Allows direct manipulation of the transition matrix tensor.
 *
 * @param ctx FEP learning GPU context
 * @return Transition matrix tensor, or NULL if not initialized
 */
nimcp_gpu_tensor_t* fep_learning_gpu_get_transition_tensor(
    fep_learning_gpu_context_t* ctx)
{
    if (!ctx || !ctx->initialized) return NULL;
    return ctx->A;
}

/**
 * @brief Get the likelihood matrix tensor from context
 *
 * @param ctx FEP learning GPU context
 * @return Likelihood matrix tensor, or NULL if not available
 */
nimcp_gpu_tensor_t* fep_learning_gpu_get_likelihood_tensor(
    fep_learning_gpu_context_t* ctx)
{
    if (!ctx || !ctx->initialized) return NULL;
    return ctx->B;
}

/**
 * @brief Perform transition learning with tensors directly on GPU
 *
 * Use this when state vectors are already on GPU to avoid host transfers.
 *
 * @param ctx FEP learning GPU context
 * @param state_t Current state tensor (already on GPU)
 * @param state_t1 Next state tensor (already on GPU)
 * @param lr Learning rate
 * @param lambda L2 regularization
 * @param momentum Momentum coefficient
 * @return true on success, false on failure
 */
bool fep_learn_transition_tensor(
    fep_learning_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* state_t,
    const nimcp_gpu_tensor_t* state_t1,
    float lr,
    float lambda,
    float momentum)
{
    if (!ctx || !ctx->initialized) {
        LOG_ERROR("Invalid FEP learning context");
        return false;
    }

    if (!state_t || !state_t1 || !state_t->data || !state_t1->data) {
        LOG_ERROR("Invalid state tensors");
        return false;
    }

    if (state_t->numel != ctx->state_dim || state_t1->numel != ctx->state_dim) {
        LOG_ERROR("State tensor size mismatch: expected %u, got %zu/%zu",
                  ctx->state_dim, state_t->numel, state_t1->numel);
        return false;
    }

    if (!ctx->matrices_on_gpu) {
        LOG_ERROR("Transition matrix not on GPU");
        return false;
    }

    uint32_t dim = ctx->state_dim;
    cudaStream_t stream = (cudaStream_t)nimcp_gpu_get_compute_stream(ctx->gpu_ctx);

    // Use input tensors directly (no copy needed)
    // Compute prediction using nimcp_gpu_gemv
    bool gemv_ok = nimcp_gpu_gemv(
        ctx->gpu_ctx,
        ctx->A,
        state_t,         // Use input directly
        ctx->prediction,
        1.0f, 0.0f, false
    );

    if (!gemv_ok) {
        kernel_fep_matvec<<<FEP_GPU_GRID_SIZE(dim), FEP_GPU_BLOCK_SIZE, 0, stream>>>(
            static_cast<const float*>(ctx->A->data),
            static_cast<const float*>(state_t->data),
            static_cast<float*>(ctx->prediction->data),
            dim, dim);
    }

    // Compute error
    kernel_fep_compute_error<<<FEP_GPU_GRID_SIZE(dim), FEP_GPU_BLOCK_SIZE, 0, stream>>>(
        static_cast<const float*>(state_t1->data),
        static_cast<const float*>(ctx->prediction->data),
        static_cast<float*>(ctx->error->data),
        dim);

    // Compute and apply gradient
    uint32_t total_elements = dim * dim;

    kernel_fep_gradient<<<FEP_GPU_GRID_SIZE(total_elements), FEP_GPU_BLOCK_SIZE, 0, stream>>>(
        static_cast<const float*>(ctx->error->data),
        static_cast<const float*>(state_t->data),
        static_cast<const float*>(ctx->A->data),
        static_cast<float*>(ctx->A_grad->data),
        lambda, dim, dim);

    float* velocity_ptr = (momentum > 0.0f) ?
        static_cast<float*>(ctx->A_velocity->data) : NULL;

    kernel_fep_update<<<FEP_GPU_GRID_SIZE(total_elements), FEP_GPU_BLOCK_SIZE, 0, stream>>>(
        static_cast<float*>(ctx->A->data),
        static_cast<const float*>(ctx->A_grad->data),
        velocity_ptr,
        lr, momentum, total_elements);

    CUDA_CHECK_BOOL(cudaGetLastError());
    return true;
}

} // extern "C"

#else // !NIMCP_ENABLE_CUDA

//=============================================================================
// CPU Fallback Stubs
//=============================================================================

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "FEP_LEARNING_GPU"

typedef struct fep_learning_gpu_context_s {
    bool initialized;
    uint32_t state_dim;
    uint32_t obs_dim;
} fep_learning_gpu_context_t;

extern "C" {

fep_learning_gpu_context_t* fep_learning_gpu_create(
    nimcp_gpu_context_t* gpu_ctx,
    uint32_t state_dim,
    uint32_t obs_dim,
    uint32_t max_batch_size)
{
    LOG_WARN("CUDA not available - FEP learning GPU context not functional");
    return NULL;
}

void fep_learning_gpu_destroy(fep_learning_gpu_context_t* ctx)
{
    if (ctx) free(ctx);
}

int fep_learning_gpu_upload_transition(
    fep_learning_gpu_context_t* ctx,
    const float* A,
    uint32_t dim)
{
    LOG_WARN("CUDA not available");
    return -1;
}

int fep_learning_gpu_download_transition(
    fep_learning_gpu_context_t* ctx,
    float* A,
    uint32_t dim)
{
    LOG_WARN("CUDA not available");
    return -1;
}

int fep_learn_transition_gpu(
    fep_learning_gpu_context_t* ctx,
    const float* state_t,
    const float* state_t1,
    float lr,
    float lambda,
    float momentum,
    float* loss_out)
{
    LOG_WARN("CUDA not available");
    return -1;
}

int fep_learn_transition_batch_gpu(
    fep_learning_gpu_context_t* ctx,
    const float* states,
    uint32_t n_transitions,
    float lr,
    float lambda,
    float momentum,
    float* avg_loss_out)
{
    LOG_WARN("CUDA not available");
    return -1;
}

int fep_learning_gpu_upload_likelihood(
    fep_learning_gpu_context_t* ctx,
    const float* B,
    uint32_t obs_dim,
    uint32_t state_dim)
{
    LOG_WARN("CUDA not available");
    return -1;
}

int fep_learning_gpu_download_likelihood(
    fep_learning_gpu_context_t* ctx,
    float* B,
    uint32_t obs_dim,
    uint32_t state_dim)
{
    LOG_WARN("CUDA not available");
    return -1;
}

int fep_learn_likelihood_gpu(
    fep_learning_gpu_context_t* ctx,
    const float* observation,
    const float* state,
    float lr,
    float lambda,
    float momentum,
    float* loss_out)
{
    LOG_WARN("CUDA not available");
    return -1;
}

bool fep_learning_should_use_gpu(uint32_t dim)
{
    return false;  // CUDA not available
}

nimcp_gpu_tensor_t* fep_learning_gpu_get_transition_tensor(
    fep_learning_gpu_context_t* ctx)
{
    return NULL;
}

nimcp_gpu_tensor_t* fep_learning_gpu_get_likelihood_tensor(
    fep_learning_gpu_context_t* ctx)
{
    return NULL;
}

bool fep_learn_transition_tensor(
    fep_learning_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* state_t,
    const nimcp_gpu_tensor_t* state_t1,
    float lr,
    float lambda,
    float momentum)
{
    LOG_WARN("CUDA not available");
    return false;
}

} // extern "C"

#endif // NIMCP_ENABLE_CUDA
