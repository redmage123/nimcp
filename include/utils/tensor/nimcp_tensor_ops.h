//=============================================================================
// nimcp_tensor_ops.h - Enhanced Tensor Operations
//=============================================================================
/**
 * @file nimcp_tensor_ops.h
 * @brief High-Performance Tensor Operations for Neural Networks
 *
 * WHAT: Comprehensive tensor operations for neural network computations
 * WHY:  Foundation for attention, convolution, and gradient computations
 * HOW:  Optimized SIMD operations with automatic memory management
 *
 * THEORETICAL FOUNDATIONS:
 * - Einstein summation convention for tensor contractions
 * - Broadcast semantics following NumPy conventions
 * - Automatic differentiation support via gradient tape
 *
 * OPERATION CATEGORIES:
 * 1. CREATION: zeros, ones, randn, eye, from_data
 * 2. ELEMENT-WISE: add, sub, mul, div, pow, exp, log, tanh
 * 3. REDUCTION: sum, mean, max, min, norm
 * 4. LINEAR ALGEBRA: matmul, transpose, einsum
 * 5. RESHAPE: view, reshape, squeeze, unsqueeze, permute
 * 6. ATTENTION: softmax, scaled_dot_product, flash_attention
 *
 * MEMORY MANAGEMENT:
 * - Reference counting for shared tensors
 * - Copy-on-write for efficient cloning
 * - Contiguous vs strided storage
 * - NIMCP memory tracking integration
 *
 * BIO-ASYNC INTEGRATION:
 * - Async tensor operations via bio-async futures
 * - Gradient computation messages
 * - Memory allocation events
 *
 * CODING STANDARDS:
 * - Guard clauses (no nested ifs)
 * - Helper functions (<50 lines)
 * - WHAT-WHY-HOW documentation
 * - Single Responsibility Principle
 *
 * @author NIMCP Development Team
 * @date 2025-12-10
 * @version 1.0.0
 */

#ifndef NIMCP_TENSOR_OPS_H
#define NIMCP_TENSOR_OPS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of tensor dimensions */
#define NIMCP_TENSOR_MAX_DIMS 8

/** Maximum einsum equation length */
#define NIMCP_TENSOR_MAX_EINSUM_LEN 128

/** Default alignment for tensor data */
#define NIMCP_TENSOR_ALIGNMENT 64

//=============================================================================
// Error Codes (Negative codes for module-local consistency)
//=============================================================================

/**
 * @brief Tensor operations error codes
 *
 * DESIGN RATIONALE:
 * - Uses negative codes for module-local error handling, consistent with
 *   nimcp_tensor.h (nimcp_tensor_error_t) which uses -1 to -11
 * - Does NOT use core NIMCP_ERROR_* codes (1000-9999 range)
 * - Enables fast, type-safe error checking within tensor operations
 *
 * ERROR CODE CONVENTION:
 * - 0 = success
 * - Negative values = error conditions
 * - Match nimcp_tensor_error_t semantics where applicable
 */
#define NIMCP_TENSOR_SUCCESS              0
#define NIMCP_TENSOR_ERROR_NULL_PARAM     (-1)   /**< NULL pointer argument (matches NIMCP_TENSOR_ERR_NULL) */
#define NIMCP_TENSOR_ERROR_INVALID_SHAPE  (-2)   /**< Shape mismatch/invalid (matches NIMCP_TENSOR_ERR_SHAPE) */
#define NIMCP_TENSOR_ERROR_INVALID_DIM    (-3)   /**< Invalid dimension (matches NIMCP_TENSOR_ERR_RANK) */
#define NIMCP_TENSOR_ERROR_ALLOC_FAILED   (-4)   /**< Memory allocation failed (matches NIMCP_TENSOR_ERR_ALLOC) */
#define NIMCP_TENSOR_ERROR_SHAPE_MISMATCH (-5)   /**< Incompatible shapes (matches NIMCP_TENSOR_ERR_BROADCAST) */
#define NIMCP_TENSOR_ERROR_BROADCAST_FAIL (-5)   /**< Alias for SHAPE_MISMATCH */
#define NIMCP_TENSOR_ERROR_INVALID_DTYPE  (-7)   /**< Invalid data type (matches NIMCP_TENSOR_ERR_DTYPE) */
#define NIMCP_TENSOR_ERROR_NOT_CONTIGUOUS (-8)   /**< Requires contiguous (matches NIMCP_TENSOR_ERR_CONTIGUOUS) */
#define NIMCP_TENSOR_ERROR_EINSUM_INVALID (-6)   /**< Invalid einsum (matches NIMCP_TENSOR_ERR_EINSUM) */
#define NIMCP_TENSOR_ERROR_GRAD_DISABLED  (-10)  /**< Gradient error (matches NIMCP_TENSOR_ERR_GRAD) */
#define NIMCP_TENSOR_ERROR_OVERFLOW       (-12)  /**< Element count overflow */

//=============================================================================
// Data Types
//=============================================================================

/**
 * @brief Tensor data types
 *
 * WHAT: Supported numeric types for tensor storage
 * WHY:  Trade-off between precision and memory/speed
 * HOW:  Tagged union approach for type safety
 */
typedef enum {
    NIMCP_DTYPE_FLOAT32 = 0,   /**< 32-bit float (default) */
    NIMCP_DTYPE_FLOAT64 = 1,   /**< 64-bit double */
    NIMCP_DTYPE_FLOAT16 = 2,   /**< 16-bit half precision */
    NIMCP_DTYPE_BFLOAT16 = 3,  /**< Brain floating point */
    NIMCP_DTYPE_INT32 = 4,     /**< 32-bit integer */
    NIMCP_DTYPE_INT64 = 5,     /**< 64-bit integer */
    NIMCP_DTYPE_INT8 = 6,      /**< 8-bit integer (quantized) */
    NIMCP_DTYPE_UINT8 = 7,     /**< 8-bit unsigned */
    NIMCP_DTYPE_BOOL = 8,      /**< Boolean (stored as uint8) */
    NIMCP_DTYPE_COUNT = 9
} nimcp_dtype_t;

/**
 * @brief Tensor device location
 *
 * WHAT: Where tensor data is stored
 * WHY:  Support CPU/GPU computation
 * HOW:  Tag for memory management
 */
typedef enum {
    NIMCP_DEVICE_CPU = 0,      /**< CPU memory */
    NIMCP_DEVICE_GPU = 1,      /**< GPU memory (future) */
    NIMCP_DEVICE_COUNT = 2
} nimcp_device_t;

/**
 * @brief Tensor memory layout
 *
 * WHAT: How multi-dimensional data is laid out in memory
 * WHY:  Optimize for different access patterns
 * HOW:  Row-major (C) or column-major (Fortran)
 */
typedef enum {
    NIMCP_LAYOUT_ROW_MAJOR = 0,  /**< C-style row-major (default) */
    NIMCP_LAYOUT_COL_MAJOR = 1,  /**< Fortran-style column-major */
} nimcp_layout_t;

//=============================================================================
// Core Structures
//=============================================================================

/**
 * @brief Tensor shape descriptor
 *
 * WHAT: Multi-dimensional shape information
 * WHY:  Separate from data for reshape without copy
 * HOW:  Dims array with dimension count
 */
typedef struct {
    uint32_t ndim;                         /**< Number of dimensions */
    uint32_t dims[NIMCP_TENSOR_MAX_DIMS];  /**< Size of each dimension */
    int64_t strides[NIMCP_TENSOR_MAX_DIMS]; /**< Stride for each dimension */
} nimcp_tensor_shape_t;

/**
 * @brief Tensor configuration
 *
 * WHAT: Creation parameters for tensors
 * WHY:  Control dtype, device, gradient tracking
 * HOW:  Passed to creation functions
 */
typedef struct {
    nimcp_dtype_t dtype;           /**< Data type */
    nimcp_device_t device;         /**< Device location */
    nimcp_layout_t layout;         /**< Memory layout */
    bool requires_grad;            /**< Enable gradient computation */
    bool is_contiguous;            /**< Force contiguous storage */
} nimcp_tensor_config_t;

/** Opaque tensor handle */
#ifndef NIMCP_TENSOR_T_DEFINED
#define NIMCP_TENSOR_T_DEFINED
typedef struct nimcp_tensor_s nimcp_tensor_t;
#endif

/** Opaque gradient tape handle for automatic differentiation */
typedef struct nimcp_grad_tape_s nimcp_grad_tape_t;

//=============================================================================
// Statistics Structure
//=============================================================================

/**
 * @brief Tensor operation statistics
 *
 * WHAT: Performance and memory metrics
 * WHY:  Enable monitoring and profiling
 * HOW:  Track operations, memory, timings
 */
typedef struct {
    uint64_t total_ops;                /**< Total operations performed */
    uint64_t total_allocs;             /**< Total allocations */
    uint64_t total_frees;              /**< Total deallocations */
    size_t current_memory_bytes;       /**< Current memory usage */
    size_t peak_memory_bytes;          /**< Peak memory usage */
    float avg_op_time_us;              /**< Average operation time */
    uint64_t matmul_count;             /**< Matrix multiply count */
    uint64_t elemwise_count;           /**< Element-wise op count */
    uint64_t reduction_count;          /**< Reduction op count */
} nimcp_tensor_stats_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Get default tensor configuration
 *
 * WHAT: Return sensible defaults for tensor creation
 * WHY:  Convenient starting point
 * HOW:  float32, CPU, row-major, no grad
 *
 * @return Default configuration
 */
nimcp_tensor_config_t nimcp_tensor_default_config(void);

//=============================================================================
// Tensor Creation
//=============================================================================

/**
 * @brief Create tensor with specified shape and config
 *
 * WHAT: Allocate tensor with uninitialized data
 * WHY:  Base creation function
 * HOW:  Allocate header and data arrays
 *
 * @param shape Shape descriptor
 * @param config Creation configuration
 * @return Tensor handle, or NULL on failure
 */
nimcp_tensor_t* nimcp_tensor_create(
    const nimcp_tensor_shape_t* shape,
    const nimcp_tensor_config_t* config
);

/**
 * @brief Create tensor initialized to zeros
 *
 * WHAT: Allocate and zero-initialize tensor
 * WHY:  Common initialization pattern
 * HOW:  Create + memset to 0
 *
 * @param dims Dimension sizes array
 * @param ndim Number of dimensions
 * @param config Configuration (can be NULL for defaults)
 * @return Tensor handle, or NULL on failure
 */
nimcp_tensor_t* nimcp_tensor_zeros(
    const uint32_t* dims,
    uint32_t ndim,
    const nimcp_tensor_config_t* config
);

/**
 * @brief Create tensor initialized to ones
 *
 * WHAT: Allocate and initialize tensor to 1.0
 * WHY:  Common initialization pattern
 * HOW:  Create + fill with 1.0
 *
 * @param dims Dimension sizes array
 * @param ndim Number of dimensions
 * @param config Configuration (can be NULL for defaults)
 * @return Tensor handle, or NULL on failure
 */
nimcp_tensor_t* nimcp_tensor_ones(
    const uint32_t* dims,
    uint32_t ndim,
    const nimcp_tensor_config_t* config
);

/**
 * @brief Create tensor with random normal values
 *
 * WHAT: Initialize with N(mean, std) distribution
 * WHY:  Weight initialization
 * HOW:  Box-Muller transform or library RNG
 *
 * @param dims Dimension sizes array
 * @param ndim Number of dimensions
 * @param mean Distribution mean
 * @param std Distribution standard deviation
 * @param config Configuration
 * @return Tensor handle, or NULL on failure
 */
nimcp_tensor_t* nimcp_tensor_randn(
    const uint32_t* dims,
    uint32_t ndim,
    float mean,
    float std,
    const nimcp_tensor_config_t* config
);

/**
 * @brief Create tensor with uniform random values
 *
 * WHAT: Initialize with U(low, high) distribution
 * WHY:  Weight initialization
 * HOW:  Scale uniform RNG to range
 *
 * @param dims Dimension sizes array
 * @param ndim Number of dimensions
 * @param low Lower bound
 * @param high Upper bound
 * @param config Configuration
 * @return Tensor handle, or NULL on failure
 */
nimcp_tensor_t* nimcp_tensor_uniform(
    const uint32_t* dims,
    uint32_t ndim,
    float low,
    float high,
    const nimcp_tensor_config_t* config
);

/**
 * @brief Create identity matrix tensor
 *
 * WHAT: Square matrix with 1s on diagonal
 * WHY:  Linear algebra operations
 * HOW:  Fill diagonal elements
 *
 * @param size Matrix size (n x n)
 * @param config Configuration
 * @return Tensor handle, or NULL on failure
 */
nimcp_tensor_t* nimcp_tensor_eye(
    uint32_t size,
    const nimcp_tensor_config_t* config
);

/**
 * @brief Create tensor from existing data
 *
 * WHAT: Wrap existing data array as tensor
 * WHY:  Avoid copy when data already exists
 * HOW:  Reference data pointer (no copy)
 *
 * @param data Source data (not copied, must outlive tensor)
 * @param dims Dimension sizes array
 * @param ndim Number of dimensions
 * @param config Configuration
 * @param copy If true, copy data; if false, reference
 * @return Tensor handle, or NULL on failure
 */
nimcp_tensor_t* nimcp_tensor_from_data(
    const void* data,
    const uint32_t* dims,
    uint32_t ndim,
    const nimcp_tensor_config_t* config,
    bool copy
);

/**
 * @brief Clone tensor (deep copy)
 *
 * WHAT: Create independent copy of tensor
 * WHY:  Safe modification without affecting original
 * HOW:  Copy all data and metadata
 *
 * @param tensor Source tensor
 * @return New tensor, or NULL on failure
 */
nimcp_tensor_t* nimcp_tensor_clone(const nimcp_tensor_t* tensor);

/**
 * @brief Destroy tensor and free memory
 *
 * WHAT: Deallocate tensor resources
 * WHY:  Prevent memory leaks
 * HOW:  Free data and header, decrement refcount
 *
 * @param tensor Tensor to destroy (NULL-safe)
 */
void nimcp_tensor_destroy(nimcp_tensor_t* tensor);

//=============================================================================
// Tensor Properties
//=============================================================================

/**
 * @brief Get tensor shape
 *
 * WHAT: Query dimension information
 * WHY:  Needed for operations and allocation
 * HOW:  Return pointer to shape struct
 *
 * @param tensor Tensor to query
 * @return Shape pointer (do not free)
 */
const nimcp_tensor_shape_t* nimcp_tensor_get_shape(const nimcp_tensor_t* tensor);

/**
 * @brief Get total number of elements
 *
 * WHAT: Product of all dimensions
 * WHY:  Buffer size calculation
 * HOW:  Multiply all dims
 *
 * @param tensor Tensor to query
 * @return Number of elements
 */
size_t nimcp_tensor_numel(const nimcp_tensor_t* tensor);

/**
 * @brief Get data type
 *
 * @param tensor Tensor to query
 * @return Data type enum
 */
nimcp_dtype_t nimcp_tensor_dtype(const nimcp_tensor_t* tensor);

/**
 * @brief Get raw data pointer
 *
 * WHAT: Direct access to underlying data
 * WHY:  Interop with other libraries
 * HOW:  Return internal pointer (use with care)
 *
 * @param tensor Tensor to query
 * @return Data pointer (do not free)
 */
void* nimcp_tensor_data(nimcp_tensor_t* tensor);

/**
 * @brief Get const data pointer
 *
 * @param tensor Tensor to query
 * @return Const data pointer
 */
const void* nimcp_tensor_data_const(const nimcp_tensor_t* tensor);

/**
 * @brief Check if tensor is contiguous
 *
 * WHAT: Test if data is stored contiguously
 * WHY:  Some ops require contiguous storage
 * HOW:  Check stride consistency
 *
 * @param tensor Tensor to query
 * @return true if contiguous
 */
bool nimcp_tensor_is_contiguous(const nimcp_tensor_t* tensor);

/**
 * @brief Check if gradient tracking is enabled
 *
 * @param tensor Tensor to query
 * @return true if requires_grad
 */
bool nimcp_tensor_requires_grad(const nimcp_tensor_t* tensor);

//=============================================================================
// Element-wise Operations
//=============================================================================

/**
 * @brief Add two tensors element-wise
 *
 * WHAT: out = a + b (with broadcasting)
 * WHY:  Core arithmetic operation
 * HOW:  Broadcast then add
 *
 * @param a First tensor
 * @param b Second tensor
 * @return Result tensor, or NULL on failure
 */
nimcp_tensor_t* nimcp_tensor_add(
    const nimcp_tensor_t* a,
    const nimcp_tensor_t* b
);

/**
 * @brief Subtract two tensors element-wise
 *
 * @param a First tensor
 * @param b Second tensor
 * @return Result tensor (a - b)
 */
nimcp_tensor_t* nimcp_tensor_sub(
    const nimcp_tensor_t* a,
    const nimcp_tensor_t* b
);

/**
 * @brief Multiply two tensors element-wise
 *
 * @param a First tensor
 * @param b Second tensor
 * @return Result tensor (a * b)
 */
nimcp_tensor_t* nimcp_tensor_mul(
    const nimcp_tensor_t* a,
    const nimcp_tensor_t* b
);

/**
 * @brief Divide two tensors element-wise
 *
 * @param a First tensor (numerator)
 * @param b Second tensor (denominator)
 * @return Result tensor (a / b)
 */
nimcp_tensor_t* nimcp_tensor_div(
    const nimcp_tensor_t* a,
    const nimcp_tensor_t* b
);

/**
 * @brief Power operation element-wise
 *
 * @param a Base tensor
 * @param b Exponent tensor
 * @return Result tensor (a^b)
 */
nimcp_tensor_t* nimcp_tensor_pow(
    const nimcp_tensor_t* a,
    const nimcp_tensor_t* b
);

/**
 * @brief Scale tensor by scalar
 *
 * @param tensor Input tensor
 * @param scale Scalar multiplier
 * @return Result tensor
 */
nimcp_tensor_t* nimcp_tensor_scale(
    const nimcp_tensor_t* tensor,
    float scale
);

/**
 * @brief Add scalar to tensor
 *
 * @param tensor Input tensor
 * @param scalar Value to add
 * @return Result tensor
 */
nimcp_tensor_t* nimcp_tensor_add_scalar(
    const nimcp_tensor_t* tensor,
    float scalar
);

//=============================================================================
// Unary Operations
//=============================================================================

/**
 * @brief Exponential element-wise
 */
nimcp_tensor_t* nimcp_tensor_exp(const nimcp_tensor_t* tensor);

/**
 * @brief Natural logarithm element-wise
 */
nimcp_tensor_t* nimcp_tensor_log(const nimcp_tensor_t* tensor);

/**
 * @brief Square root element-wise
 */
nimcp_tensor_t* nimcp_tensor_sqrt(const nimcp_tensor_t* tensor);

/**
 * @brief Hyperbolic tangent element-wise
 */
nimcp_tensor_t* nimcp_tensor_tanh(const nimcp_tensor_t* tensor);

/**
 * @brief Sigmoid activation element-wise
 */
nimcp_tensor_t* nimcp_tensor_sigmoid(const nimcp_tensor_t* tensor);

/**
 * @brief ReLU activation element-wise
 */
nimcp_tensor_t* nimcp_tensor_relu(const nimcp_tensor_t* tensor);

/**
 * @brief GELU activation element-wise
 */
nimcp_tensor_t* nimcp_tensor_gelu(const nimcp_tensor_t* tensor);

/**
 * @brief Negate tensor element-wise
 */
nimcp_tensor_t* nimcp_tensor_neg(const nimcp_tensor_t* tensor);

/**
 * @brief Absolute value element-wise
 */
nimcp_tensor_t* nimcp_tensor_abs(const nimcp_tensor_t* tensor);

//=============================================================================
// Reduction Operations
//=============================================================================

/**
 * @brief Sum all elements
 *
 * @param tensor Input tensor
 * @return Scalar result as tensor
 */
nimcp_tensor_t* nimcp_tensor_sum(const nimcp_tensor_t* tensor);

/**
 * @brief Sum along dimension
 *
 * @param tensor Input tensor
 * @param dim Dimension to reduce
 * @param keepdim Keep reduced dimension as size 1
 * @return Reduced tensor
 */
nimcp_tensor_t* nimcp_tensor_sum_dim(
    const nimcp_tensor_t* tensor,
    int dim,
    bool keepdim
);

/**
 * @brief Mean of all elements
 */
nimcp_tensor_t* nimcp_tensor_mean(const nimcp_tensor_t* tensor);

/**
 * @brief Mean along dimension
 */
nimcp_tensor_t* nimcp_tensor_mean_dim(
    const nimcp_tensor_t* tensor,
    int dim,
    bool keepdim
);

/**
 * @brief Maximum element
 */
nimcp_tensor_t* nimcp_tensor_max(const nimcp_tensor_t* tensor);

/**
 * @brief Maximum along dimension
 *
 * @param tensor Input tensor
 * @param dim Dimension to reduce
 * @param indices Optional output for argmax indices
 * @param keepdim Keep reduced dimension
 * @return Maximum values tensor
 */
nimcp_tensor_t* nimcp_tensor_max_dim(
    const nimcp_tensor_t* tensor,
    int dim,
    nimcp_tensor_t** indices,
    bool keepdim
);

/**
 * @brief Minimum element
 */
nimcp_tensor_t* nimcp_tensor_min(const nimcp_tensor_t* tensor);

/**
 * @brief Frobenius norm
 */
nimcp_tensor_t* nimcp_tensor_norm(const nimcp_tensor_t* tensor);

/**
 * @brief Variance along dimension
 */
nimcp_tensor_t* nimcp_tensor_var(
    const nimcp_tensor_t* tensor,
    int dim,
    bool unbiased,
    bool keepdim
);

//=============================================================================
// Linear Algebra Operations
//=============================================================================

/**
 * @brief Matrix multiplication
 *
 * WHAT: C = A @ B
 * WHY:  Core operation for neural networks
 * HOW:  BLAS-style GEMM for 2D, batched for higher dims
 *
 * @param a First tensor (... x M x K)
 * @param b Second tensor (... x K x N)
 * @return Result tensor (... x M x N)
 */
nimcp_tensor_t* nimcp_tensor_matmul(
    const nimcp_tensor_t* a,
    const nimcp_tensor_t* b
);

/**
 * @brief Batched matrix multiplication
 *
 * @param a First tensor (B x M x K)
 * @param b Second tensor (B x K x N)
 * @return Result tensor (B x M x N)
 */
nimcp_tensor_t* nimcp_tensor_bmm(
    const nimcp_tensor_t* a,
    const nimcp_tensor_t* b
);

/**
 * @brief Transpose last two dimensions
 *
 * @param tensor Input tensor
 * @return Transposed tensor
 */
nimcp_tensor_t* nimcp_tensor_transpose(const nimcp_tensor_t* tensor);

/**
 * @brief General transpose
 *
 * @param tensor Input tensor
 * @param dim0 First dimension to swap
 * @param dim1 Second dimension to swap
 * @return Transposed tensor
 */
nimcp_tensor_t* nimcp_tensor_transpose_dims(
    const nimcp_tensor_t* tensor,
    int dim0,
    int dim1
);

/**
 * @brief Permute dimensions
 *
 * @param tensor Input tensor
 * @param dims New dimension order
 * @param ndim Number of dimensions
 * @return Permuted tensor
 */
nimcp_tensor_t* nimcp_tensor_permute(
    const nimcp_tensor_t* tensor,
    const int* dims,
    uint32_t ndim
);

/**
 * @brief Einstein summation
 *
 * WHAT: General tensor contraction via string specification
 * WHY:  Flexible, expressive tensor operations
 * HOW:  Parse equation, determine contractions
 *
 * @param equation Einsum equation (e.g., "ij,jk->ik")
 * @param tensors Array of input tensors
 * @param num_tensors Number of input tensors
 * @return Result tensor
 */
nimcp_tensor_t* nimcp_tensor_einsum(
    const char* equation,
    nimcp_tensor_t* const* tensors,
    uint32_t num_tensors
);

//=============================================================================
// Shape Operations
//=============================================================================

/**
 * @brief Reshape tensor
 *
 * WHAT: Change shape without changing data
 * WHY:  Prepare for different operations
 * HOW:  Update shape metadata, may copy if not contiguous
 *
 * @param tensor Input tensor
 * @param new_dims New dimension sizes (-1 for inferred)
 * @param ndim Number of dimensions
 * @return Reshaped tensor
 */
nimcp_tensor_t* nimcp_tensor_reshape(
    const nimcp_tensor_t* tensor,
    const int* new_dims,
    uint32_t ndim
);

/**
 * @brief View tensor with new shape (no copy)
 *
 * @param tensor Input tensor (must be contiguous)
 * @param new_dims New dimension sizes
 * @param ndim Number of dimensions
 * @return View tensor (shares data)
 */
nimcp_tensor_t* nimcp_tensor_view(
    nimcp_tensor_t* tensor,
    const int* new_dims,
    uint32_t ndim
);

/**
 * @brief Remove dimensions of size 1
 *
 * @param tensor Input tensor
 * @return Squeezed tensor
 */
nimcp_tensor_t* nimcp_tensor_squeeze(const nimcp_tensor_t* tensor);

/**
 * @brief Add dimension of size 1
 *
 * @param tensor Input tensor
 * @param dim Position for new dimension
 * @return Unsqueezed tensor
 */
nimcp_tensor_t* nimcp_tensor_unsqueeze(
    const nimcp_tensor_t* tensor,
    int dim
);

/**
 * @brief Flatten tensor to 1D
 *
 * @param tensor Input tensor
 * @return Flattened tensor
 */
nimcp_tensor_t* nimcp_tensor_flatten(const nimcp_tensor_t* tensor);

/**
 * @brief Concatenate tensors along dimension
 *
 * @param tensors Array of tensors
 * @param num_tensors Number of tensors
 * @param dim Dimension to concatenate along
 * @return Concatenated tensor
 */
nimcp_tensor_t* nimcp_tensor_cat(
    nimcp_tensor_t* const* tensors,
    uint32_t num_tensors,
    int dim
);

/**
 * @brief Stack tensors along new dimension
 *
 * @param tensors Array of tensors
 * @param num_tensors Number of tensors
 * @param dim Dimension for stacking
 * @return Stacked tensor
 */
nimcp_tensor_t* nimcp_tensor_stack(
    nimcp_tensor_t* const* tensors,
    uint32_t num_tensors,
    int dim
);

//=============================================================================
// Attention Operations
//=============================================================================

/**
 * @brief Softmax normalization
 *
 * @param tensor Input tensor
 * @param dim Dimension for normalization
 * @return Normalized tensor
 */
nimcp_tensor_t* nimcp_tensor_softmax(
    const nimcp_tensor_t* tensor,
    int dim
);

/**
 * @brief Log-softmax
 *
 * @param tensor Input tensor
 * @param dim Dimension for normalization
 * @return Log-normalized tensor
 */
nimcp_tensor_t* nimcp_tensor_log_softmax(
    const nimcp_tensor_t* tensor,
    int dim
);

/**
 * @brief Scaled dot-product attention
 *
 * WHAT: attention(Q, K, V) = softmax(QK^T / sqrt(d)) * V
 * WHY:  Core transformer operation
 * HOW:  Compute attention weights and apply to values
 *
 * @param query Query tensor (B x H x L x D)
 * @param key Key tensor (B x H x S x D)
 * @param value Value tensor (B x H x S x D)
 * @param mask Optional attention mask
 * @param scale Scale factor (0 = sqrt(d))
 * @return Attention output (B x H x L x D)
 */
nimcp_tensor_t* nimcp_tensor_scaled_dot_product_attention(
    const nimcp_tensor_t* query,
    const nimcp_tensor_t* key,
    const nimcp_tensor_t* value,
    const nimcp_tensor_t* mask,
    float scale
);

/**
 * @brief Layer normalization
 *
 * @param tensor Input tensor
 * @param normalized_shape Shape of normalized dimensions
 * @param ndim Number of normalized dimensions
 * @param gamma Scale parameter (optional)
 * @param beta Bias parameter (optional)
 * @param eps Epsilon for numerical stability
 * @return Normalized tensor
 */
nimcp_tensor_t* nimcp_tensor_layer_norm(
    const nimcp_tensor_t* tensor,
    const uint32_t* normalized_shape,
    uint32_t ndim,
    const nimcp_tensor_t* gamma,
    const nimcp_tensor_t* beta,
    float eps
);

//=============================================================================
// Gradient Operations (Automatic Differentiation)
//=============================================================================

/**
 * @brief Create gradient tape for automatic differentiation
 *
 * WHAT: Record operations for backpropagation
 * WHY:  Compute gradients automatically
 * HOW:  Build computation graph during forward pass
 *
 * @return Gradient tape handle
 */
nimcp_grad_tape_t* nimcp_grad_tape_create(void);

/**
 * @brief Destroy gradient tape
 *
 * @param tape Tape to destroy
 */
void nimcp_grad_tape_destroy(nimcp_grad_tape_t* tape);

/**
 * @brief Start recording operations
 *
 * @param tape Gradient tape
 * @return NIMCP_TENSOR_SUCCESS or error code
 */
int nimcp_grad_tape_start(nimcp_grad_tape_t* tape);

/**
 * @brief Stop recording operations
 *
 * @param tape Gradient tape
 * @return NIMCP_TENSOR_SUCCESS or error code
 */
int nimcp_grad_tape_stop(nimcp_grad_tape_t* tape);

/**
 * @brief Compute gradients via backpropagation
 *
 * WHAT: Compute d(loss)/d(source) via chain rule
 * WHY:  Training neural networks
 * HOW:  Reverse-mode automatic differentiation
 *
 * @param tape Gradient tape
 * @param loss Loss tensor (scalar)
 * @param sources Tensors to compute gradients for
 * @param num_sources Number of source tensors
 * @param gradients Output gradients (same order as sources)
 * @return NIMCP_TENSOR_SUCCESS or error code
 */
int nimcp_grad_tape_gradient(
    nimcp_grad_tape_t* tape,
    nimcp_tensor_t* loss,
    nimcp_tensor_t* const* sources,
    uint32_t num_sources,
    nimcp_tensor_t** gradients
);

/**
 * @brief Get accumulated gradient for tensor
 *
 * @param tensor Tensor with requires_grad=true
 * @return Gradient tensor, or NULL if none
 */
nimcp_tensor_t* nimcp_tensor_grad(nimcp_tensor_t* tensor);

/**
 * @brief Zero out gradient
 *
 * @param tensor Tensor to zero gradient
 * @return NIMCP_TENSOR_SUCCESS or error code
 */
int nimcp_tensor_zero_grad(nimcp_tensor_t* tensor);

//=============================================================================
// Statistics and Diagnostics
//=============================================================================

/**
 * @brief Get tensor operation statistics
 *
 * @param stats Output statistics structure
 * @return NIMCP_TENSOR_SUCCESS or error code
 */
int nimcp_tensor_get_stats(nimcp_tensor_stats_t* stats);

/**
 * @brief Reset tensor statistics
 *
 * @return NIMCP_TENSOR_SUCCESS or error code
 */
int nimcp_tensor_reset_stats(void);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Convert dtype to string
 *
 * @param dtype Data type enum
 * @return String name
 */
const char* nimcp_dtype_to_string(nimcp_dtype_t dtype);

/**
 * @brief Get size of dtype in bytes
 *
 * @param dtype Data type enum
 * @return Size in bytes
 */
size_t nimcp_dtype_size(nimcp_dtype_t dtype);

/**
 * @brief Print tensor information
 *
 * @param tensor Tensor to print
 * @param name Optional name for display
 */
void nimcp_tensor_print(const nimcp_tensor_t* tensor, const char* name);

/**
 * @brief Print tensor data (first N elements)
 *
 * @param tensor Tensor to print
 * @param max_elements Maximum elements to show
 */
void nimcp_tensor_print_data(const nimcp_tensor_t* tensor, uint32_t max_elements);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_TENSOR_OPS_H
