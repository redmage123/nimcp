//=============================================================================
// nimcp_tensor_gpu.h - GPU Tensor Operations (CUDA Kernels)
//=============================================================================
/**
 * @file nimcp_tensor_gpu.h
 * @brief GPU-accelerated tensor operations using CUDA
 *
 * WHAT: GPU kernels for tensor operations (GEMM, element-wise, reductions)
 * WHY:  Enables massive parallel acceleration for neural network computations
 * HOW:  Uses cuBLAS for GEMM, custom kernels for element-wise and reductions
 *
 * ARCHITECTURE:
 * - cuBLAS integration for optimized matrix operations
 * - Custom CUDA kernels for element-wise ops with fused activations
 * - Warp-shuffle reductions for efficient aggregations
 * - Mixed precision support (FP32, FP16, BF16)
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_TENSOR_GPU_H
#define NIMCP_TENSOR_GPU_H

// Include GPU context BEFORE extern "C" block - it brings in CUDA headers
// which contain C++ code that cannot have C linkage
#include "gpu/context/nimcp_gpu_context.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include "utils/tensor/nimcp_tensor.h"  // CPU tensor library integration
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

//=============================================================================
// Data Types
//=============================================================================

/**
 * @brief GPU tensor precision modes
 */
typedef enum {
    NIMCP_GPU_PRECISION_FP32 = 0,   /**< Full precision (32-bit float) */
    NIMCP_GPU_PRECISION_FP16 = 1,   /**< Half precision (16-bit float) */
    NIMCP_GPU_PRECISION_BF16 = 2,   /**< Brain float (16-bit) */
    NIMCP_GPU_PRECISION_INT8 = 3,   /**< Quantized 8-bit integer */
    NIMCP_GPU_PRECISION_TF32 = 4,   /**< Tensor Float 32 (Ampere+) */
    NIMCP_GPU_PRECISION_UINT32 = 5, /**< Unsigned 32-bit integer (for indices) */
    NIMCP_GPU_PRECISION_INT32 = 6   /**< Signed 32-bit integer */
} nimcp_gpu_precision_t;

/**
 * @brief GPU tensor memory layout
 */
typedef enum {
    NIMCP_TENSOR_LAYOUT_ROW_MAJOR = 0,   /**< Row-major (C-style) */
    NIMCP_TENSOR_LAYOUT_COL_MAJOR = 1    /**< Column-major (Fortran-style) */
} nimcp_tensor_layout_t;

/**
 * @brief GPU tensor descriptor
 */
typedef struct nimcp_gpu_tensor_s {
    void* data;                     /**< Device memory pointer */
    size_t* dims;                   /**< Dimension sizes (device memory) */
    size_t* strides;                /**< Strides for each dimension (device memory) */
    uint32_t ndim;                  /**< Number of dimensions */
    size_t numel;                   /**< Total number of elements */
    size_t elem_size;               /**< Size of each element in bytes */
    nimcp_gpu_precision_t precision; /**< Data precision */
    nimcp_tensor_layout_t layout;   /**< Memory layout */
    nimcp_gpu_context_t* ctx;       /**< GPU context (for stream/device) */
    bool owns_data;                 /**< Whether tensor owns its data buffer */
} nimcp_gpu_tensor_t;

//=============================================================================
// Tensor Lifecycle
//=============================================================================

/**
 * @brief Create a GPU tensor
 *
 * @param ctx GPU context
 * @param dims Array of dimension sizes
 * @param ndim Number of dimensions
 * @param precision Data precision
 * @return GPU tensor or NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_gpu_tensor_create(
    nimcp_gpu_context_t* ctx,
    const size_t* dims,
    uint32_t ndim,
    nimcp_gpu_precision_t precision
);

/**
 * @brief Create GPU tensor from host data
 *
 * @param ctx GPU context
 * @param host_data Host memory pointer
 * @param dims Array of dimension sizes
 * @param ndim Number of dimensions
 * @param precision Data precision
 * @return GPU tensor or NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_gpu_tensor_from_host(
    nimcp_gpu_context_t* ctx,
    const void* host_data,
    const size_t* dims,
    uint32_t ndim,
    nimcp_gpu_precision_t precision
);

/**
 * @brief Copy GPU tensor to host memory
 *
 * @param tensor GPU tensor
 * @param host_data Host memory (must be pre-allocated)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_tensor_to_host(
    const nimcp_gpu_tensor_t* tensor,
    void* host_data
);

/**
 * @brief Upload host data to existing GPU tensor (no allocation)
 *
 * @param tensor Existing GPU tensor (must be pre-allocated)
 * @param host_data Host memory to copy from
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_tensor_upload(
    nimcp_gpu_tensor_t* tensor,
    const void* host_data
);

/**
 * @brief Destroy GPU tensor and free memory
 *
 * @param tensor GPU tensor to destroy
 */
NIMCP_EXPORT void nimcp_gpu_tensor_destroy(nimcp_gpu_tensor_t* tensor);

/**
 * @brief Clone a GPU tensor
 *
 * @param tensor Source tensor
 * @return New tensor with copied data
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_gpu_tensor_clone(const nimcp_gpu_tensor_t* tensor);

//=============================================================================
// GEMM Operations (General Matrix Multiply)
//=============================================================================

/**
 * @brief Perform general matrix multiplication: C = alpha * A @ B + beta * C
 *
 * @param ctx GPU context
 * @param A First matrix (M x K)
 * @param B Second matrix (K x N)
 * @param C Output matrix (M x N)
 * @param alpha Scalar multiplier for A @ B
 * @param beta Scalar multiplier for C
 * @param trans_a Transpose A if true
 * @param trans_b Transpose B if true
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_gemm(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* A,
    const nimcp_gpu_tensor_t* B,
    nimcp_gpu_tensor_t* C,
    float alpha,
    float beta,
    bool trans_a,
    bool trans_b
);

/**
 * @brief Matrix-vector multiplication: y = alpha * A @ x + beta * y
 *
 * @param ctx GPU context
 * @param A Matrix (M x N)
 * @param x Vector (N)
 * @param y Output vector (M)
 * @param alpha Scalar multiplier for A @ x
 * @param beta Scalar multiplier for y
 * @param trans_a Transpose A if true
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_gemv(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* A,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* y,
    float alpha,
    float beta,
    bool trans_a
);

/**
 * @brief Batched matrix multiplication
 *
 * Performs batch_size independent GEMM operations:
 * C[i] = alpha * A[i] @ B[i] + beta * C[i]
 *
 * @param ctx GPU context
 * @param A First batch of matrices (batch_size x M x K)
 * @param B Second batch of matrices (batch_size x K x N)
 * @param C Output batch of matrices (batch_size x M x N)
 * @param alpha Scalar multiplier
 * @param beta Scalar multiplier
 * @param trans_a Transpose A matrices if true
 * @param trans_b Transpose B matrices if true
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_gemm_batched(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* A,
    const nimcp_gpu_tensor_t* B,
    nimcp_gpu_tensor_t* C,
    float alpha,
    float beta,
    bool trans_a,
    bool trans_b
);

//=============================================================================
// Element-wise Operations
//=============================================================================

/**
 * @brief Element-wise addition: out = a + b
 */
NIMCP_EXPORT bool nimcp_gpu_add(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief Element-wise subtraction: out = a - b
 */
NIMCP_EXPORT bool nimcp_gpu_sub(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief Element-wise multiplication: out = a * b
 */
NIMCP_EXPORT bool nimcp_gpu_mul(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief Element-wise division: out = a / b
 */
NIMCP_EXPORT bool nimcp_gpu_div(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief Scalar addition: out = a + scalar
 */
NIMCP_EXPORT bool nimcp_gpu_add_scalar(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    float scalar,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief Scalar multiplication: out = a * scalar
 */
NIMCP_EXPORT bool nimcp_gpu_mul_scalar(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    float scalar,
    nimcp_gpu_tensor_t* out
);

//=============================================================================
// Activation Functions
//=============================================================================

/**
 * @brief ReLU activation: out = max(0, x)
 */
NIMCP_EXPORT bool nimcp_gpu_relu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief Leaky ReLU: out = x > 0 ? x : alpha * x
 */
NIMCP_EXPORT bool nimcp_gpu_leaky_relu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out,
    float alpha
);

/**
 * @brief Sigmoid activation: out = 1 / (1 + exp(-x))
 */
NIMCP_EXPORT bool nimcp_gpu_sigmoid(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief Tanh activation: out = tanh(x)
 */
NIMCP_EXPORT bool nimcp_gpu_tanh(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief GELU activation: out = x * Phi(x)
 * Uses approximate version: 0.5 * x * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3)))
 */
NIMCP_EXPORT bool nimcp_gpu_gelu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief SiLU/Swish activation: out = x * sigmoid(x)
 */
NIMCP_EXPORT bool nimcp_gpu_silu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief Softmax activation (along last dimension)
 * Numerically stable: out = exp(x - max(x)) / sum(exp(x - max(x)))
 */
NIMCP_EXPORT bool nimcp_gpu_softmax(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief Log-softmax (along last dimension)
 * out = x - max(x) - log(sum(exp(x - max(x))))
 */
NIMCP_EXPORT bool nimcp_gpu_log_softmax(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out
);

//=============================================================================
// Math Functions
//=============================================================================

/**
 * @brief Element-wise exponential: out = exp(x)
 */
NIMCP_EXPORT bool nimcp_gpu_exp(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief Element-wise natural logarithm: out = log(x)
 */
NIMCP_EXPORT bool nimcp_gpu_log(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief Element-wise square root: out = sqrt(x)
 */
NIMCP_EXPORT bool nimcp_gpu_sqrt(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief Element-wise power: out = x^exponent
 */
NIMCP_EXPORT bool nimcp_gpu_pow(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    float exponent,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief Element-wise absolute value: out = |x|
 */
NIMCP_EXPORT bool nimcp_gpu_abs(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief Element-wise clamp: out = clamp(x, min_val, max_val)
 */
NIMCP_EXPORT bool nimcp_gpu_clamp(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    float min_val,
    float max_val,
    nimcp_gpu_tensor_t* out
);

//=============================================================================
// Reduction Operations
//=============================================================================

/**
 * @brief Sum reduction along specified axis
 *
 * @param ctx GPU context
 * @param x Input tensor
 * @param out Output tensor (reduced along axis)
 * @param axis Axis to reduce (-1 for all)
 * @param keepdims Keep reduced dimension as size 1
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_sum(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out,
    int axis,
    bool keepdims
);

/**
 * @brief Mean reduction along specified axis
 */
NIMCP_EXPORT bool nimcp_gpu_mean(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out,
    int axis,
    bool keepdims
);

/**
 * @brief Max reduction along specified axis
 */
NIMCP_EXPORT bool nimcp_gpu_max(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out,
    int axis,
    bool keepdims
);

/**
 * @brief Min reduction along specified axis
 */
NIMCP_EXPORT bool nimcp_gpu_min(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out,
    int axis,
    bool keepdims
);

/**
 * @brief Argmax along specified axis
 *
 * @param ctx GPU context
 * @param x Input tensor
 * @param out Output tensor (int64 indices)
 * @param axis Axis to find max
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_argmax(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out,
    int axis
);

/**
 * @brief Argmin along specified axis
 */
NIMCP_EXPORT bool nimcp_gpu_argmin(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out,
    int axis
);

/**
 * @brief Variance along specified axis
 */
NIMCP_EXPORT bool nimcp_gpu_var(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out,
    int axis,
    bool keepdims,
    bool unbiased
);

/**
 * @brief Standard deviation along specified axis
 */
NIMCP_EXPORT bool nimcp_gpu_std(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out,
    int axis,
    bool keepdims,
    bool unbiased
);

//=============================================================================
// Norm Operations
//=============================================================================

/**
 * @brief L1 norm: ||x||_1 = sum(|x|)
 */
NIMCP_EXPORT bool nimcp_gpu_norm_l1(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    float* result
);

/**
 * @brief L2 norm: ||x||_2 = sqrt(sum(x^2))
 */
NIMCP_EXPORT bool nimcp_gpu_norm_l2(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    float* result
);

/**
 * @brief L-infinity norm: ||x||_inf = max(|x|)
 */
NIMCP_EXPORT bool nimcp_gpu_norm_linf(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    float* result
);

/**
 * @brief Frobenius norm for matrices: ||A||_F = sqrt(sum(A_ij^2))
 */
NIMCP_EXPORT bool nimcp_gpu_norm_frobenius(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    float* result
);

//=============================================================================
// Comparison Operations
//=============================================================================

/**
 * @brief Element-wise equality: out = (a == b)
 */
NIMCP_EXPORT bool nimcp_gpu_eq(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief Element-wise greater than: out = (a > b)
 */
NIMCP_EXPORT bool nimcp_gpu_gt(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief Element-wise less than: out = (a < b)
 */
NIMCP_EXPORT bool nimcp_gpu_lt(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief Element-wise greater than or equal: out = (a >= b)
 */
NIMCP_EXPORT bool nimcp_gpu_ge(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief Element-wise less than or equal: out = (a <= b)
 */
NIMCP_EXPORT bool nimcp_gpu_le(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief Element-wise where: out = cond ? a : b
 */
NIMCP_EXPORT bool nimcp_gpu_where(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* cond,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* out
);

//=============================================================================
// Memory Operations
//=============================================================================

/**
 * @brief Fill tensor with value
 */
NIMCP_EXPORT bool nimcp_gpu_fill(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* tensor,
    float value
);

/**
 * @brief Fill tensor with zeros
 */
NIMCP_EXPORT bool nimcp_gpu_zeros(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* tensor
);

/**
 * @brief Fill tensor with ones
 */
NIMCP_EXPORT bool nimcp_gpu_ones(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* tensor
);

/**
 * @brief Copy tensor: dst = src
 */
NIMCP_EXPORT bool nimcp_gpu_copy(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* src,
    nimcp_gpu_tensor_t* dst
);

/**
 * @brief Transpose tensor (swap last two dimensions)
 */
NIMCP_EXPORT bool nimcp_gpu_transpose(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief Reshape tensor (change dims without data copy if possible)
 */
NIMCP_EXPORT bool nimcp_gpu_reshape(
    nimcp_gpu_tensor_t* tensor,
    const size_t* new_dims,
    uint32_t new_ndim
);

//=============================================================================
// FFT Operations (Audio Processing Support)
//=============================================================================

/**
 * @brief 1D FFT (Fast Fourier Transform)
 *
 * @param ctx GPU context
 * @param x Input tensor (real or complex)
 * @param out Output tensor (complex)
 * @param inverse true for inverse FFT
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_fft_1d(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out,
    bool inverse
);

/**
 * @brief 2D FFT (for spectrogram images)
 */
NIMCP_EXPORT bool nimcp_gpu_fft_2d(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out,
    bool inverse
);

/**
 * @brief Real-to-complex FFT (optimized for real input)
 */
NIMCP_EXPORT bool nimcp_gpu_rfft(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief Complex-to-real inverse FFT
 */
NIMCP_EXPORT bool nimcp_gpu_irfft(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out
);

//=============================================================================
// CPU-GPU Tensor Integration
//=============================================================================
// These functions integrate with the existing nimcp tensor library
// (utils/tensor/nimcp_tensor.h) to provide seamless CPU<->GPU transfers.

/**
 * @brief Convert nimcp_dtype_t to nimcp_gpu_precision_t
 *
 * WHAT: Map CPU tensor dtype to GPU precision type
 * WHY:  Enable seamless CPU-GPU tensor conversion
 * HOW:  Direct mapping of compatible types
 *
 * @param dtype CPU tensor data type
 * @return GPU precision type
 */
NIMCP_EXPORT nimcp_gpu_precision_t nimcp_dtype_to_gpu_precision(nimcp_dtype_t dtype);

/**
 * @brief Convert nimcp_gpu_precision_t to nimcp_dtype_t
 *
 * @param precision GPU precision type
 * @return CPU tensor data type
 */
NIMCP_EXPORT nimcp_dtype_t nimcp_gpu_precision_to_dtype(nimcp_gpu_precision_t precision);

/**
 * @brief Create GPU tensor from CPU tensor
 *
 * WHAT: Copy CPU tensor data to GPU memory
 * WHY:  Enable GPU-accelerated operations on CPU tensors
 * HOW:  Allocate GPU memory and copy data via cudaMemcpy
 *
 * @param ctx GPU context
 * @param cpu_tensor Source CPU tensor (nimcp_tensor_t from tensor library)
 * @return GPU tensor with copied data, or NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_gpu_tensor_from_cpu(
    nimcp_gpu_context_t* ctx,
    const nimcp_tensor_t* cpu_tensor
);

/**
 * @brief Create CPU tensor from GPU tensor
 *
 * WHAT: Copy GPU tensor data back to CPU memory
 * WHY:  Enable CPU operations on GPU-computed results
 * HOW:  Allocate CPU tensor and copy data via cudaMemcpy
 *
 * @param gpu_tensor Source GPU tensor
 * @return CPU tensor (nimcp_tensor_t) with copied data, or NULL on failure
 */
NIMCP_EXPORT nimcp_tensor_t* nimcp_cpu_tensor_from_gpu(
    const nimcp_gpu_tensor_t* gpu_tensor
);

/**
 * @brief Copy GPU tensor data into existing CPU tensor
 *
 * WHAT: Copy GPU data to pre-allocated CPU tensor
 * WHY:  Avoid allocation when destination already exists
 * HOW:  cudaMemcpy to existing tensor's data buffer
 *
 * @param gpu_tensor Source GPU tensor
 * @param cpu_tensor Destination CPU tensor (must have matching shape/dtype)
 * @return true on success, false on failure
 */
NIMCP_EXPORT bool nimcp_gpu_tensor_copy_to_cpu(
    const nimcp_gpu_tensor_t* gpu_tensor,
    nimcp_tensor_t* cpu_tensor
);

/**
 * @brief Copy CPU tensor data into existing GPU tensor
 *
 * WHAT: Copy CPU data to pre-allocated GPU tensor
 * WHY:  Avoid allocation when destination already exists
 * HOW:  cudaMemcpy from CPU tensor's data buffer
 *
 * @param cpu_tensor Source CPU tensor
 * @param gpu_tensor Destination GPU tensor (must have matching shape/precision)
 * @return true on success, false on failure
 */
NIMCP_EXPORT bool nimcp_cpu_tensor_copy_to_gpu(
    const nimcp_tensor_t* cpu_tensor,
    nimcp_gpu_tensor_t* gpu_tensor
);

/**
 * @brief Execute tensor operation on GPU, return result as CPU tensor
 *
 * WHAT: Convenience function for CPU->GPU->operate->CPU workflow
 * WHY:  Simplify common pattern of GPU-accelerated operations
 * HOW:  Upload, execute, download in one call
 *
 * Example:
 * @code
 * nimcp_tensor_t* result = nimcp_gpu_accelerate_matmul(ctx, a, b);
 * // result is a CPU tensor with GPU-computed matmul result
 * @endcode
 *
 * @param ctx GPU context
 * @param a First CPU tensor
 * @param b Second CPU tensor
 * @return Result as CPU tensor, or NULL on failure
 */
NIMCP_EXPORT nimcp_tensor_t* nimcp_gpu_accelerate_matmul(
    nimcp_gpu_context_t* ctx,
    const nimcp_tensor_t* a,
    const nimcp_tensor_t* b
);

/**
 * @brief Check if GPU acceleration is available for tensor operations
 *
 * @return true if CUDA is available and context can be created
 */
NIMCP_EXPORT bool nimcp_gpu_tensor_available(void);

/**
 * @brief Get GPU memory info for tensor operations
 *
 * @param ctx GPU context
 * @param free_bytes Output: available GPU memory
 * @param total_bytes Output: total GPU memory
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_tensor_memory_info(
    nimcp_gpu_context_t* ctx,
    size_t* free_bytes,
    size_t* total_bytes
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_TENSOR_GPU_H
