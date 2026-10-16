//=============================================================================
// nimcp_tensor.h - Comprehensive Tensor Library with Calculus Support
//=============================================================================
/**
 * @file nimcp_tensor.h
 * @brief N-dimensional Tensor Library with Tensor Calculus Algorithms
 *
 * WHAT: Complete tensor library for neural networks and scientific computing
 * WHY:  Foundation for emotion tensors, swarm coordination, and neural ops
 * HOW:  N-dimensional arrays with calculus, contractions, and autodiff
 *
 * MATHEMATICAL FOUNDATIONS:
 * - Tensor algebra: Contractions, outer products, inner products
 * - Tensor calculus: Gradients, Jacobians, Hessians, divergence, curl
 * - Differential geometry: Metric tensors, Christoffel symbols, covariant derivatives
 * - Einstein summation: General tensor operations via index notation
 *
 * FEATURES:
 * 1. N-DIMENSIONAL: Arbitrary rank tensors (scalars, vectors, matrices, higher)
 * 2. MEMORY: Reference counting, CoW, contiguous/strided storage
 * 3. OPERATIONS: Element-wise, reductions, linear algebra, broadcasting
 * 4. CALCULUS: Automatic differentiation, numerical derivatives
 * 5. INTEGRATION: Bio-async, emotion tensor, swarm systems
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

#ifndef NIMCP_TENSOR_H
#define NIMCP_TENSOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum tensor dimensions (rank) */
#define NIMCP_TENSOR_MAX_RANK 8

/** Maximum number of tensor elements to prevent overflow (1 billion) */
#define NIMCP_TENSOR_MAX_ELEMENTS ((size_t)1000000000)

/** Maximum einsum equation length */
#define NIMCP_TENSOR_MAX_EINSUM 256

/** Default memory alignment */
#define NIMCP_TENSOR_ALIGN 64

/* P2 fix: Compile-time verification that alignment is power of 2
 * WHY:  posix_memalign() requires alignment to be power of 2 and >= sizeof(void*)
 *       Invalid alignment causes undefined behavior or allocation failures.
 */
#ifdef __cplusplus
static_assert((NIMCP_TENSOR_ALIGN & (NIMCP_TENSOR_ALIGN - 1)) == 0,
              "NIMCP_TENSOR_ALIGN must be a power of 2");
static_assert(NIMCP_TENSOR_ALIGN >= sizeof(void*),
              "NIMCP_TENSOR_ALIGN must be >= sizeof(void*)");
#else
_Static_assert((NIMCP_TENSOR_ALIGN & (NIMCP_TENSOR_ALIGN - 1)) == 0,
               "NIMCP_TENSOR_ALIGN must be a power of 2");
_Static_assert(NIMCP_TENSOR_ALIGN >= sizeof(void*),
               "NIMCP_TENSOR_ALIGN must be >= sizeof(void*)");
#endif

/** Magic number for validation */
#define NIMCP_TENSOR_MAGIC 0x54454E53  /* "TENS" */

//=============================================================================
// Error Codes
//=============================================================================

/**
 * @brief Tensor library error codes
 *
 * DESIGN RATIONALE:
 * - Uses negative codes (-1 to -11) for module-local error handling
 * - Does NOT use core NIMCP_ERROR_* codes (1000-9999 range)
 * - Enables fast, type-safe error checking within tensor operations
 * - See docs/ERROR_CODE_STRATEGY.md for full error code architecture
 *
 * ERROR HANDLING PATTERN:
 * @code
 * nimcp_tensor_t* t = nimcp_tensor_create(dims, rank, NIMCP_DTYPE_F32);
 * if (!t) {
 *     // Creation failed - check logs for details
 *     // Possible errors: NIMCP_TENSOR_ERR_RANK, NIMCP_TENSOR_ERR_ALLOC
 * }
 * @endcode
 *
 * MEMORY SAFETY:
 * - NULL returns indicate allocation failure (NIMCP_TENSOR_ERR_ALLOC)
 * - nimcp_tensor_destroy() is IDEMPOTENT (safe to call multiple times)
 * - nimcp_tensor_destroy() handles partially initialized tensors
 * - All creation functions clean up on failure (no leaks)
 */
typedef enum {
    NIMCP_TENSOR_OK = 0,              /**< Success */
    NIMCP_TENSOR_ERR_NULL = -1,       /**< NULL pointer argument */
    NIMCP_TENSOR_ERR_SHAPE = -2,      /**< Shape mismatch or invalid shape */
    NIMCP_TENSOR_ERR_RANK = -3,       /**< Rank exceeds NIMCP_TENSOR_MAX_RANK */
    NIMCP_TENSOR_ERR_ALLOC = -4,      /**< Memory allocation failed */
    NIMCP_TENSOR_ERR_BROADCAST = -5,  /**< Incompatible shapes for broadcasting */
    NIMCP_TENSOR_ERR_EINSUM = -6,     /**< Invalid einsum equation */
    NIMCP_TENSOR_ERR_DTYPE = -7,      /**< Unsupported or mismatched data type */
    NIMCP_TENSOR_ERR_CONTIGUOUS = -8, /**< Operation requires contiguous memory */
    NIMCP_TENSOR_ERR_INDEX = -9,      /**< Index out of bounds */
    NIMCP_TENSOR_ERR_GRAD = -10,      /**< Gradient computation error */
    NIMCP_TENSOR_ERR_INVALID = -11,   /**< Invalid tensor (corrupted magic) */
    NIMCP_TENSOR_ERR_OVERFLOW = -12   /**< Element count overflow (exceeds NIMCP_TENSOR_MAX_ELEMENTS) */
} nimcp_tensor_error_t;

//=============================================================================
// Data Types
//=============================================================================

/**
 * @brief Supported tensor data types
 */
typedef enum {
    NIMCP_DTYPE_F32 = 0,    /**< 32-bit float (default) */
    NIMCP_DTYPE_F64 = 1,    /**< 64-bit double */
    NIMCP_DTYPE_F16 = 2,    /**< 16-bit half precision */
    NIMCP_DTYPE_BF16 = 3,   /**< Brain float 16 */
    NIMCP_DTYPE_I32 = 4,    /**< 32-bit signed integer */
    NIMCP_DTYPE_I64 = 5,    /**< 64-bit signed integer */
    NIMCP_DTYPE_I8 = 6,     /**< 8-bit signed integer */
    NIMCP_DTYPE_U8 = 7,     /**< 8-bit unsigned integer */
    NIMCP_DTYPE_BOOL = 8,   /**< Boolean */
    NIMCP_DTYPE_C64 = 9,    /**< Complex float (2x32-bit) */
    NIMCP_DTYPE_C128 = 10,  /**< Complex double (2x64-bit) */
    NIMCP_DTYPE_COUNT = 11
} nimcp_dtype_t;

/**
 * @brief Tensor index type for advanced indexing
 */
typedef enum {
    NIMCP_IDX_SCALAR = 0,   /**< Single index: t[3] */
    NIMCP_IDX_SLICE = 1,    /**< Slice: t[1:5] */
    NIMCP_IDX_ARRAY = 2,    /**< Integer array indexing */
    NIMCP_IDX_BOOL = 3,     /**< Boolean mask indexing */
    NIMCP_IDX_NEWAXIS = 4,  /**< Add new axis */
    NIMCP_IDX_ELLIPSIS = 5  /**< Ellipsis (...) */
} nimcp_index_type_t;

//=============================================================================
// Core Structures
//=============================================================================

/**
 * @brief Tensor shape descriptor
 *
 * WHAT: Describes tensor dimensions and memory layout
 * WHY:  Separate metadata from data for efficient views
 * HOW:  Dims array with strides for arbitrary layout
 */
typedef struct {
    uint32_t rank;                      /**< Number of dimensions */
    uint32_t dims[NIMCP_TENSOR_MAX_RANK];   /**< Size of each dimension */
    int64_t strides[NIMCP_TENSOR_MAX_RANK]; /**< Stride for each dimension (bytes) */
    size_t numel;                       /**< Total number of elements */
    size_t nbytes;                      /**< Total bytes of data */
} nimcp_tensor_shape_t;

/**
 * @brief Slice specification for indexing
 */
typedef struct {
    int64_t start;    /**< Start index (negative = from end) */
    int64_t stop;     /**< Stop index (exclusive) */
    int64_t step;     /**< Step size */
} nimcp_slice_t;

/**
 * @brief Index specification (tagged union)
 */
typedef struct {
    nimcp_index_type_t type;
    union {
        int64_t scalar;
        nimcp_slice_t slice;
        struct {
            const int64_t* indices;
            size_t count;
        } array;
        struct {
            const bool* mask;
            size_t count;
        } boolean;
    } data;
} nimcp_index_t;

/** Opaque tensor handle */
#ifndef NIMCP_TENSOR_T_DEFINED
#define NIMCP_TENSOR_T_DEFINED
typedef struct nimcp_tensor_s nimcp_tensor_t;
#endif

/** Opaque autodiff context */
typedef struct nimcp_autodiff_ctx_s nimcp_autodiff_ctx_t;

//=============================================================================
// Tensor Statistics
//=============================================================================

/**
 * @brief Global tensor operation statistics
 */
typedef struct {
    uint64_t tensors_created;
    uint64_t tensors_destroyed;
    uint64_t operations_total;
    uint64_t ops_elementwise;
    uint64_t ops_reduction;
    uint64_t ops_matmul;
    uint64_t ops_contraction;
    uint64_t ops_calculus;
    size_t memory_current;
    size_t memory_peak;
    double time_compute_ms;
    double time_alloc_ms;
} nimcp_tensor_stats_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Initialize tensor subsystem
 *
 * WHAT: Set up global state, thread pools, memory pools
 * WHY:  Required before any tensor operations
 * HOW:  Call once at startup
 *
 * @return NIMCP_TENSOR_OK on success
 */
int nimcp_tensor_init(void);

/**
 * @brief Shutdown tensor subsystem
 *
 * WHAT: Free global resources
 * WHY:  Clean shutdown
 * HOW:  Call once at exit
 */
void nimcp_tensor_shutdown(void);

//=============================================================================
// Tensor Creation
//=============================================================================

/**
 * @brief Create tensor with uninitialized data
 *
 * WHAT: Allocate tensor with given shape
 * WHY:  Basic creation for filling with computed values
 * HOW:  Allocate header + aligned data buffer
 *
 * MEMORY SAFETY:
 * - Returns NULL on failure (no partial allocations leaked)
 * - Cleanup on failure: mutex destroyed, struct freed
 * - Data is UNINITIALIZED (use nimcp_tensor_zeros() for zero-init)
 * - Safe to destroy: nimcp_tensor_destroy(NULL) is no-op
 *
 * ERROR CONDITIONS:
 * - NULL: rank > NIMCP_TENSOR_MAX_RANK (logged as NIMCP_TENSOR_ERR_RANK)
 * - NULL: Allocation failed (logged as NIMCP_TENSOR_ERR_ALLOC)
 *
 * EXAMPLE:
 * @code
 * uint32_t dims[] = {3, 4};
 * nimcp_tensor_t* t = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
 * if (!t) {
 *     // Check logs for specific error
 *     return;
 * }
 * // ... use tensor ...
 * nimcp_tensor_destroy(t);  // Always safe, even if creation partial
 * @endcode
 *
 * @param dims Array of dimension sizes
 * @param rank Number of dimensions (must be <= NIMCP_TENSOR_MAX_RANK)
 * @param dtype Data type
 * @return Tensor handle or NULL on failure
 */
nimcp_tensor_t* nimcp_tensor_create(
    const uint32_t* dims,
    uint32_t rank,
    nimcp_dtype_t dtype
);

/**
 * @brief Create 1D tensor (vector) convenience wrapper
 *
 * WHAT: Create a 1-dimensional tensor
 * WHY:  Common pattern - simplifies code in VAE bridges
 * HOW:  Wraps nimcp_tensor_create with rank=1
 *
 * @param size Number of elements
 * @param dtype Data type
 * @return Tensor handle or NULL on failure
 */
static inline nimcp_tensor_t* nimcp_tensor_create_1d(
    uint32_t size,
    nimcp_dtype_t dtype
) {
    uint32_t dims[1] = {size};
    return nimcp_tensor_create(dims, 1, dtype);
}

/**
 * @brief Create 2D tensor (matrix) convenience wrapper
 *
 * WHAT: Create a 2-dimensional tensor
 * WHY:  Common pattern - simplifies matrix creation
 * HOW:  Wraps nimcp_tensor_create with rank=2
 *
 * @param rows Number of rows
 * @param cols Number of columns
 * @param dtype Data type
 * @return Tensor handle or NULL on failure
 */
static inline nimcp_tensor_t* nimcp_tensor_create_2d(
    uint32_t rows,
    uint32_t cols,
    nimcp_dtype_t dtype
) {
    uint32_t dims[2] = {rows, cols};
    return nimcp_tensor_create(dims, 2, dtype);
}

/**
 * @brief Create tensor initialized to zeros
 */
nimcp_tensor_t* nimcp_tensor_zeros(
    const uint32_t* dims,
    uint32_t rank,
    nimcp_dtype_t dtype
);

/**
 * @brief Create tensor initialized to ones
 */
nimcp_tensor_t* nimcp_tensor_ones(
    const uint32_t* dims,
    uint32_t rank,
    nimcp_dtype_t dtype
);

/**
 * @brief Create tensor with constant fill value
 */
nimcp_tensor_t* nimcp_tensor_full(
    const uint32_t* dims,
    uint32_t rank,
    nimcp_dtype_t dtype,
    double fill_value
);

/**
 * @brief Create tensor from existing data
 *
 * @param data Source data pointer
 * @param dims Dimension sizes
 * @param rank Number of dimensions
 * @param dtype Data type
 * @param copy If true, copy data; if false, reference (data must outlive tensor)
 * @return Tensor handle
 */
nimcp_tensor_t* nimcp_tensor_from_data(
    const void* data,
    const uint32_t* dims,
    uint32_t rank,
    nimcp_dtype_t dtype,
    bool copy
);

/**
 * @brief Create identity matrix
 */
nimcp_tensor_t* nimcp_tensor_eye(uint32_t n, nimcp_dtype_t dtype);

/**
 * @brief Create tensor with random normal values
 */
nimcp_tensor_t* nimcp_tensor_randn(
    const uint32_t* dims,
    uint32_t rank,
    nimcp_dtype_t dtype,
    double mean,
    double std
);

/**
 * @brief Create tensor with uniform random values
 */
nimcp_tensor_t* nimcp_tensor_rand(
    const uint32_t* dims,
    uint32_t rank,
    nimcp_dtype_t dtype,
    double low,
    double high
);

/**
 * @brief Create range tensor [start, stop) with step
 */
nimcp_tensor_t* nimcp_tensor_arange(
    double start,
    double stop,
    double step,
    nimcp_dtype_t dtype
);

/**
 * @brief Create linearly spaced tensor
 */
nimcp_tensor_t* nimcp_tensor_linspace(
    double start,
    double stop,
    uint32_t num,
    nimcp_dtype_t dtype
);

/**
 * @brief Clone tensor (deep copy)
 */
nimcp_tensor_t* nimcp_tensor_clone(const nimcp_tensor_t* t);

/**
 * @brief Destroy tensor and free resources
 *
 * WHAT: Free tensor memory and decrement reference count
 * WHY:  Clean resource management with refcounting
 * HOW:  Thread-safe cleanup, idempotent (safe to call multiple times)
 *
 * MEMORY SAFETY GUARANTEES:
 * 1. **Idempotent**: Safe to call multiple times (magic check prevents double-free)
 * 2. **NULL-safe**: nimcp_tensor_destroy(NULL) is a no-op
 * 3. **Partial cleanup**: Safe even if tensor creation failed partway
 * 4. **Refcounting**: Only frees when refcount reaches 0
 * 5. **Gradient cleanup**: Recursively destroys gradient tensor
 * 6. **No double-free**: Sets data/grad to NULL after freeing
 *
 * THREAD SAFETY:
 * - Uses tensor's mutex lock for refcount decrement
 * - Updates global stats under stats_lock
 * - Safe to call from multiple threads on same tensor (refcount protected)
 *
 * EXAMPLE:
 * @code
 * nimcp_tensor_t* t = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
 * if (!t) return;  // Safe: destroy not needed on NULL
 *
 * // ... use tensor ...
 *
 * nimcp_tensor_destroy(t);  // Decrements refcount, frees if 0
 * nimcp_tensor_destroy(t);  // Safe: magic invalidated, early return
 * @endcode
 *
 * @param t Tensor to destroy (NULL is safe, no-op)
 */
void nimcp_tensor_destroy(nimcp_tensor_t* t);

//=============================================================================
// Tensor Properties
//=============================================================================

/**
 * @brief Get tensor shape information
 */
const nimcp_tensor_shape_t* nimcp_tensor_shape(const nimcp_tensor_t* t);

/**
 * @brief Get tensor rank (number of dimensions)
 */
uint32_t nimcp_tensor_rank(const nimcp_tensor_t* t);

/**
 * @brief Get total number of elements
 */
size_t nimcp_tensor_numel(const nimcp_tensor_t* t);

/**
 * @brief Get total number of elements (alias for numel)
 *
 * WHAT: Alias for nimcp_tensor_numel
 * WHY:  Consistency with some code that uses 'size'
 * HOW:  Inline wrapper
 */
static inline size_t nimcp_tensor_size(const nimcp_tensor_t* t) {
    return nimcp_tensor_numel(t);
}

/**
 * @brief Get data type
 */
nimcp_dtype_t nimcp_tensor_dtype(const nimcp_tensor_t* t);

/**
 * @brief Get raw data pointer
 */
void* nimcp_tensor_data(nimcp_tensor_t* t);

/**
 * @brief Get const data pointer
 */
const void* nimcp_tensor_data_const(const nimcp_tensor_t* t);

/**
 * @brief Check if tensor is contiguous in memory
 */
bool nimcp_tensor_is_contiguous(const nimcp_tensor_t* t);

/**
 * @brief Make tensor contiguous (copy if needed)
 */
nimcp_tensor_t* nimcp_tensor_contiguous(const nimcp_tensor_t* t);

/**
 * @brief Check if tensor requires gradient
 */
bool nimcp_tensor_requires_grad(const nimcp_tensor_t* t);

/**
 * @brief Set requires_grad flag
 */
void nimcp_tensor_set_requires_grad(nimcp_tensor_t* t, bool requires_grad);

//=============================================================================
// Element Access
//=============================================================================

/**
 * @brief Get single element as double
 *
 * @param t Tensor
 * @param indices Array of indices (length = rank)
 * @return Element value converted to double
 */
double nimcp_tensor_get(const nimcp_tensor_t* t, const uint32_t* indices);

/**
 * @brief Set single element from double
 */
int nimcp_tensor_set(nimcp_tensor_t* t, const uint32_t* indices, double value);

/**
 * @brief Get element by flat index
 */
double nimcp_tensor_get_flat(const nimcp_tensor_t* t, size_t index);

/**
 * @brief Set element by flat index
 */
int nimcp_tensor_set_flat(nimcp_tensor_t* t, size_t index, double value);

//=============================================================================
// Shape Operations
//=============================================================================

/**
 * @brief Reshape tensor (returns view if possible)
 */
nimcp_tensor_t* nimcp_tensor_reshape(
    const nimcp_tensor_t* t,
    const uint32_t* new_dims,
    uint32_t new_rank
);

/**
 * @brief Transpose tensor (swap last two dimensions)
 */
nimcp_tensor_t* nimcp_tensor_transpose(const nimcp_tensor_t* t);

/**
 * @brief General transpose with dimension permutation
 */
nimcp_tensor_t* nimcp_tensor_permute(
    const nimcp_tensor_t* t,
    const uint32_t* perm,
    uint32_t rank
);

/**
 * @brief Remove dimensions of size 1
 */
nimcp_tensor_t* nimcp_tensor_squeeze(const nimcp_tensor_t* t);

/**
 * @brief Add dimension of size 1
 */
nimcp_tensor_t* nimcp_tensor_unsqueeze(const nimcp_tensor_t* t, int dim);

/**
 * @brief Flatten to 1D
 */
nimcp_tensor_t* nimcp_tensor_flatten(const nimcp_tensor_t* t);

/**
 * @brief Expand tensor to new shape (broadcast)
 */
nimcp_tensor_t* nimcp_tensor_expand(
    const nimcp_tensor_t* t,
    const uint32_t* new_dims,
    uint32_t new_rank
);

/**
 * @brief Concatenate tensors along dimension
 */
nimcp_tensor_t* nimcp_tensor_cat(
    nimcp_tensor_t* const* tensors,
    uint32_t count,
    int dim
);

/**
 * @brief Stack tensors along new dimension
 */
nimcp_tensor_t* nimcp_tensor_stack(
    nimcp_tensor_t* const* tensors,
    uint32_t count,
    int dim
);

/**
 * @brief Split tensor along dimension
 */
int nimcp_tensor_split(
    const nimcp_tensor_t* t,
    int dim,
    uint32_t num_splits,
    nimcp_tensor_t** outputs
);

//=============================================================================
// Element-wise Operations
//=============================================================================

/** Binary operations with broadcasting */
nimcp_tensor_t* nimcp_tensor_add(const nimcp_tensor_t* a, const nimcp_tensor_t* b);
nimcp_tensor_t* nimcp_tensor_sub(const nimcp_tensor_t* a, const nimcp_tensor_t* b);
nimcp_tensor_t* nimcp_tensor_mul(const nimcp_tensor_t* a, const nimcp_tensor_t* b);
nimcp_tensor_t* nimcp_tensor_div(const nimcp_tensor_t* a, const nimcp_tensor_t* b);
nimcp_tensor_t* nimcp_tensor_pow(const nimcp_tensor_t* a, const nimcp_tensor_t* b);
nimcp_tensor_t* nimcp_tensor_mod(const nimcp_tensor_t* a, const nimcp_tensor_t* b);
nimcp_tensor_t* nimcp_tensor_max_binary(const nimcp_tensor_t* a, const nimcp_tensor_t* b);
nimcp_tensor_t* nimcp_tensor_min_binary(const nimcp_tensor_t* a, const nimcp_tensor_t* b);

/** Scalar operations */
nimcp_tensor_t* nimcp_tensor_add_scalar(const nimcp_tensor_t* t, double s);
nimcp_tensor_t* nimcp_tensor_mul_scalar(const nimcp_tensor_t* t, double s);
nimcp_tensor_t* nimcp_tensor_pow_scalar(const nimcp_tensor_t* t, double s);

/** Unary operations */
nimcp_tensor_t* nimcp_tensor_neg(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_abs(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_sign(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_ceil(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_floor(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_round(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_sqrt(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_rsqrt(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_square(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_exp(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_log(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_log2(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_log10(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_sin(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_cos(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_tan(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_asin(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_acos(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_atan(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_sinh(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_cosh(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_tanh(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_sigmoid(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_relu(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_gelu(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_silu(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_softplus(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_erf(const nimcp_tensor_t* t);

/** In-place operations (modify tensor directly) */
int nimcp_tensor_add_(nimcp_tensor_t* t, const nimcp_tensor_t* other);
int nimcp_tensor_sub_(nimcp_tensor_t* t, const nimcp_tensor_t* other);
int nimcp_tensor_mul_(nimcp_tensor_t* t, const nimcp_tensor_t* other);
int nimcp_tensor_div_(nimcp_tensor_t* t, const nimcp_tensor_t* other);
int nimcp_tensor_mul_scalar_(nimcp_tensor_t* t, double s);
int nimcp_tensor_add_scalar_(nimcp_tensor_t* t, double s);

//=============================================================================
// Reduction Operations
//=============================================================================

nimcp_tensor_t* nimcp_tensor_sum(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_sum_dim(const nimcp_tensor_t* t, int dim, bool keepdim);
nimcp_tensor_t* nimcp_tensor_mean(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_mean_dim(const nimcp_tensor_t* t, int dim, bool keepdim);
nimcp_tensor_t* nimcp_tensor_prod(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_prod_dim(const nimcp_tensor_t* t, int dim, bool keepdim);
nimcp_tensor_t* nimcp_tensor_max(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_max_dim(const nimcp_tensor_t* t, int dim, bool keepdim);
nimcp_tensor_t* nimcp_tensor_min(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_min_dim(const nimcp_tensor_t* t, int dim, bool keepdim);
nimcp_tensor_t* nimcp_tensor_argmax(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_argmax_dim(const nimcp_tensor_t* t, int dim, bool keepdim);
nimcp_tensor_t* nimcp_tensor_argmin(const nimcp_tensor_t* t);
nimcp_tensor_t* nimcp_tensor_argmin_dim(const nimcp_tensor_t* t, int dim, bool keepdim);
nimcp_tensor_t* nimcp_tensor_var(const nimcp_tensor_t* t, bool unbiased);
nimcp_tensor_t* nimcp_tensor_var_dim(const nimcp_tensor_t* t, int dim, bool unbiased, bool keepdim);
nimcp_tensor_t* nimcp_tensor_std(const nimcp_tensor_t* t, bool unbiased);
nimcp_tensor_t* nimcp_tensor_std_dim(const nimcp_tensor_t* t, int dim, bool unbiased, bool keepdim);

//=============================================================================
// Linear Algebra Operations
//=============================================================================

/**
 * @brief Matrix multiplication
 *
 * WHAT: C = A @ B
 * WHY:  Core neural network operation
 * HOW:  Optimized BLAS-style GEMM, batched for higher dims
 */
nimcp_tensor_t* nimcp_tensor_matmul(const nimcp_tensor_t* a, const nimcp_tensor_t* b);

/**
 * @brief Batched matrix multiplication
 */
nimcp_tensor_t* nimcp_tensor_bmm(const nimcp_tensor_t* a, const nimcp_tensor_t* b);

/**
 * @brief Inner product (dot product)
 */
nimcp_tensor_t* nimcp_tensor_dot(const nimcp_tensor_t* a, const nimcp_tensor_t* b);

/**
 * @brief Outer product
 */
nimcp_tensor_t* nimcp_tensor_outer(const nimcp_tensor_t* a, const nimcp_tensor_t* b);

/**
 * @brief Matrix-vector product
 */
nimcp_tensor_t* nimcp_tensor_mv(const nimcp_tensor_t* mat, const nimcp_tensor_t* vec);

/**
 * @brief Vector-matrix product
 */
nimcp_tensor_t* nimcp_tensor_vm(const nimcp_tensor_t* vec, const nimcp_tensor_t* mat);

/**
 * @brief Kronecker product (tensor product)
 */
nimcp_tensor_t* nimcp_tensor_kron(const nimcp_tensor_t* a, const nimcp_tensor_t* b);

/**
 * @brief Matrix trace
 */
double nimcp_tensor_trace(const nimcp_tensor_t* t);

/**
 * @brief Matrix determinant
 */
double nimcp_tensor_det(const nimcp_tensor_t* t);

/**
 * @brief Matrix inverse
 */
nimcp_tensor_t* nimcp_tensor_inv(const nimcp_tensor_t* t);

/**
 * @brief Frobenius norm
 */
double nimcp_tensor_norm_fro(const nimcp_tensor_t* t);

/**
 * @brief L-p norm
 */
double nimcp_tensor_norm_p(const nimcp_tensor_t* t, double p);

/**
 * @brief Vector/matrix norms along dimension
 */
nimcp_tensor_t* nimcp_tensor_norm_dim(const nimcp_tensor_t* t, double p, int dim, bool keepdim);

//=============================================================================
// Tensor Contraction and Einstein Summation
//=============================================================================

/**
 * @brief Tensor contraction over specified indices
 *
 * WHAT: Contract two tensors over paired indices
 * WHY:  General tensor operations (matmul is special case)
 * HOW:  Sum over contracted indices
 *
 * @param a First tensor
 * @param b Second tensor
 * @param dims_a Contraction dimensions in a
 * @param dims_b Contraction dimensions in b
 * @param num_dims Number of dimensions to contract
 * @return Contracted tensor
 */
nimcp_tensor_t* nimcp_tensor_contract(
    const nimcp_tensor_t* a,
    const nimcp_tensor_t* b,
    const uint32_t* dims_a,
    const uint32_t* dims_b,
    uint32_t num_dims
);

/**
 * @brief Einstein summation
 *
 * WHAT: General tensor operation via index notation
 * WHY:  Express any tensor operation concisely
 * HOW:  Parse equation string, determine contractions
 *
 * Examples:
 * - "ij,jk->ik" : matrix multiply
 * - "ij->ji" : transpose
 * - "ii->" : trace
 * - "ijk,ikl->ijl" : batched matmul
 *
 * @param equation Einsum equation string
 * @param tensors Array of input tensors
 * @param num_tensors Number of input tensors
 * @return Result tensor
 */
nimcp_tensor_t* nimcp_tensor_einsum(
    const char* equation,
    nimcp_tensor_t* const* tensors,
    uint32_t num_tensors
);

/**
 * @brief Tensor inner product (full contraction)
 *
 * @param a First tensor
 * @param b Second tensor (must have same shape)
 * @return Scalar result as 0-rank tensor
 */
nimcp_tensor_t* nimcp_tensor_inner(const nimcp_tensor_t* a, const nimcp_tensor_t* b);

/**
 * @brief Tensor mode-n product with matrix
 *
 * WHAT: Contract tensor with matrix along mode n
 * WHY:  Tucker decomposition, multilinear algebra
 * HOW:  T x_n M contracts mode n of T with rows of M
 */
nimcp_tensor_t* nimcp_tensor_mode_product(
    const nimcp_tensor_t* t,
    const nimcp_tensor_t* m,
    uint32_t mode
);

//=============================================================================
// TENSOR CALCULUS - Derivatives and Gradients
//=============================================================================

/**
 * @brief Numerical gradient (finite differences)
 *
 * WHAT: Compute numerical gradient df/dx
 * WHY:  Gradient checking, simple differentiation
 * HOW:  Central differences: (f(x+h) - f(x-h)) / 2h
 *
 * @param f Function pointer: f(x) -> scalar
 * @param x Input tensor
 * @param h Step size (default 1e-5)
 * @param ctx User context passed to f
 * @return Gradient tensor (same shape as x)
 */
typedef double (*nimcp_scalar_fn)(const nimcp_tensor_t* x, void* ctx);

nimcp_tensor_t* nimcp_tensor_numerical_gradient(
    nimcp_scalar_fn f,
    const nimcp_tensor_t* x,
    double h,
    void* ctx
);

/**
 * @brief Jacobian matrix
 *
 * WHAT: Compute Jacobian df/dx for vector function f: R^n -> R^m
 * WHY:  Sensitivity analysis, Newton methods
 * HOW:  J[i][j] = df_i/dx_j
 *
 * @param f Vector function pointer
 * @param x Input tensor (1D, length n)
 * @param h Step size
 * @param ctx User context
 * @return Jacobian matrix (m x n)
 */
typedef nimcp_tensor_t* (*nimcp_vector_fn)(const nimcp_tensor_t* x, void* ctx);

nimcp_tensor_t* nimcp_tensor_jacobian(
    nimcp_vector_fn f,
    const nimcp_tensor_t* x,
    double h,
    void* ctx
);

/**
 * @brief Hessian matrix
 *
 * WHAT: Second derivative matrix d²f/dxdx
 * WHY:  Optimization (Newton's method), curvature analysis
 * HOW:  H[i][j] = d²f/(dx_i dx_j)
 *
 * @param f Scalar function
 * @param x Input tensor (1D)
 * @param h Step size
 * @param ctx User context
 * @return Hessian matrix (n x n)
 */
nimcp_tensor_t* nimcp_tensor_hessian(
    nimcp_scalar_fn f,
    const nimcp_tensor_t* x,
    double h,
    void* ctx
);

/**
 * @brief Gradient along tensor dimension
 *
 * WHAT: Compute gradient along axis (like np.gradient)
 * WHY:  Numerical differentiation of sampled data
 * HOW:  Second-order central differences with edge handling
 *
 * @param t Input tensor
 * @param dim Dimension along which to differentiate
 * @param spacing Sample spacing (default 1.0)
 * @return Gradient tensor
 */
nimcp_tensor_t* nimcp_tensor_gradient_dim(
    const nimcp_tensor_t* t,
    int dim,
    double spacing
);

/**
 * @brief Divergence of vector field
 *
 * WHAT: div(F) = dF_x/dx + dF_y/dy + ...
 * WHY:  Fluid dynamics, electromagnetic theory
 * HOW:  Sum of partial derivatives
 *
 * @param components Array of component tensors [F_x, F_y, ...]
 * @param num_dims Number of spatial dimensions
 * @param spacing Grid spacing per dimension
 * @return Scalar divergence field
 */
nimcp_tensor_t* nimcp_tensor_divergence(
    nimcp_tensor_t* const* components,
    uint32_t num_dims,
    const double* spacing
);

/**
 * @brief Curl of 3D vector field
 *
 * WHAT: curl(F) = (dF_z/dy - dF_y/dz, dF_x/dz - dF_z/dx, dF_y/dx - dF_x/dy)
 * WHY:  Fluid vorticity, electromagnetic fields
 * HOW:  Cross product of nabla with F
 *
 * @param fx X component tensor
 * @param fy Y component tensor
 * @param fz Z component tensor
 * @param spacing Grid spacing [dx, dy, dz]
 * @param curl_out Output: array of 3 tensors [curl_x, curl_y, curl_z]
 * @return NIMCP_TENSOR_OK on success
 */
int nimcp_tensor_curl(
    const nimcp_tensor_t* fx,
    const nimcp_tensor_t* fy,
    const nimcp_tensor_t* fz,
    const double* spacing,
    nimcp_tensor_t** curl_out
);

/**
 * @brief Laplacian
 *
 * WHAT: Laplacian operator: sum of second derivatives
 * WHY:  Heat equation, wave equation, potential theory
 * HOW:  div(grad(f)) = sum(d²f/dx_i²)
 *
 * @param t Input scalar field
 * @param spacing Grid spacing per dimension (NULL = 1.0)
 * @return Laplacian tensor
 */
nimcp_tensor_t* nimcp_tensor_laplacian(
    const nimcp_tensor_t* t,
    const double* spacing
);

//=============================================================================
// TENSOR CALCULUS - Differential Geometry
//=============================================================================

/**
 * @brief Metric tensor (first fundamental form)
 *
 * WHAT: Compute metric tensor g_ij for parameterized surface
 * WHY:  Length, area, geodesics on curved surfaces
 * HOW:  g_ij = (dX/du_i) . (dX/du_j)
 *
 * @param surface Surface parameterization X(u, v, ...)
 * @param params Parameter tensor
 * @param h Step size for numerical derivatives
 * @return Metric tensor g[i][j]
 */
nimcp_tensor_t* nimcp_tensor_metric(
    nimcp_vector_fn surface,
    const nimcp_tensor_t* params,
    double h,
    void* ctx
);

/**
 * @brief Christoffel symbols of the second kind
 *
 * WHAT: Connection coefficients Γ^k_ij
 * WHY:  Geodesic equations, parallel transport
 * HOW:  Γ^k_ij = (1/2) g^kl (∂g_il/∂x^j + ∂g_jl/∂x^i - ∂g_ij/∂x^l)
 *
 * @param metric Metric tensor g[i][j]
 * @param coords Coordinate tensor
 * @param h Step size
 * @return Christoffel tensor Γ[k][i][j]
 */
nimcp_tensor_t* nimcp_tensor_christoffel(
    const nimcp_tensor_t* metric,
    const nimcp_tensor_t* coords,
    double h
);

/**
 * @brief Covariant derivative
 *
 * WHAT: ∇_j V^i = ∂V^i/∂x^j + Γ^i_jk V^k
 * WHY:  Derivative that accounts for curved space
 * HOW:  Add Christoffel correction to ordinary derivative
 *
 * @param vector Contravariant vector field V^i
 * @param christoffel Christoffel symbols Γ^k_ij
 * @param direction Covariant direction j
 * @param h Step size
 * @return Covariant derivative tensor
 */
nimcp_tensor_t* nimcp_tensor_covariant_derivative(
    const nimcp_tensor_t* vector,
    const nimcp_tensor_t* christoffel,
    uint32_t direction,
    double h
);

/**
 * @brief Riemann curvature tensor
 *
 * WHAT: R^l_ijk curvature tensor
 * WHY:  Measure intrinsic curvature of space
 * HOW:  From Christoffel symbols and their derivatives
 *
 * @param christoffel Christoffel symbols
 * @param coords Coordinates
 * @param h Step size
 * @return Riemann tensor R[l][i][j][k]
 */
nimcp_tensor_t* nimcp_tensor_riemann(
    const nimcp_tensor_t* christoffel,
    const nimcp_tensor_t* coords,
    double h
);

/**
 * @brief Ricci tensor
 *
 * WHAT: R_ij = R^k_ikj contraction of Riemann
 * WHY:  Einstein field equations, scalar curvature
 *
 * @param riemann Riemann tensor
 * @return Ricci tensor R[i][j]
 */
nimcp_tensor_t* nimcp_tensor_ricci(const nimcp_tensor_t* riemann);

/**
 * @brief Scalar curvature
 *
 * WHAT: R = g^ij R_ij trace of Ricci
 * WHY:  Single measure of total curvature
 */
double nimcp_tensor_scalar_curvature(
    const nimcp_tensor_t* ricci,
    const nimcp_tensor_t* metric_inverse
);

//=============================================================================
// Automatic Differentiation
//=============================================================================

/**
 * @brief Create autodiff context for gradient computation
 */
nimcp_autodiff_ctx_t* nimcp_autodiff_create(void);

/**
 * @brief Destroy autodiff context
 */
void nimcp_autodiff_destroy(nimcp_autodiff_ctx_t* ctx);

/**
 * @brief Start recording operations for autodiff
 */
int nimcp_autodiff_start(nimcp_autodiff_ctx_t* ctx);

/**
 * @brief Stop recording
 */
int nimcp_autodiff_stop(nimcp_autodiff_ctx_t* ctx);

/**
 * @brief Compute gradients via backpropagation
 *
 * @param ctx Autodiff context
 * @param output Loss/output tensor (must be scalar)
 * @param inputs Array of input tensors
 * @param num_inputs Number of inputs
 * @param gradients Output gradients (same order as inputs)
 * @return NIMCP_TENSOR_OK on success
 */
int nimcp_autodiff_backward(
    nimcp_autodiff_ctx_t* ctx,
    nimcp_tensor_t* output,
    nimcp_tensor_t* const* inputs,
    uint32_t num_inputs,
    nimcp_tensor_t** gradients
);

/**
 * @brief Get accumulated gradient for tensor
 */
nimcp_tensor_t* nimcp_tensor_grad(nimcp_tensor_t* t);

/**
 * @brief Zero accumulated gradients
 */
int nimcp_tensor_zero_grad(nimcp_tensor_t* t);

//=============================================================================
// Neural Network Operations
//=============================================================================

/**
 * @brief Softmax normalization
 */
nimcp_tensor_t* nimcp_tensor_softmax(const nimcp_tensor_t* t, int dim);

/**
 * @brief Log-softmax
 */
nimcp_tensor_t* nimcp_tensor_log_softmax(const nimcp_tensor_t* t, int dim);

/**
 * @brief Layer normalization
 */
nimcp_tensor_t* nimcp_tensor_layer_norm(
    const nimcp_tensor_t* t,
    const nimcp_tensor_t* gamma,
    const nimcp_tensor_t* beta,
    double eps
);

/**
 * @brief Batch normalization
 */
nimcp_tensor_t* nimcp_tensor_batch_norm(
    const nimcp_tensor_t* t,
    const nimcp_tensor_t* mean,
    const nimcp_tensor_t* var,
    const nimcp_tensor_t* gamma,
    const nimcp_tensor_t* beta,
    double eps
);

/**
 * @brief Scaled dot-product attention
 *
 * attention(Q, K, V) = softmax(QK^T / sqrt(d)) * V
 */
nimcp_tensor_t* nimcp_tensor_attention(
    const nimcp_tensor_t* query,
    const nimcp_tensor_t* key,
    const nimcp_tensor_t* value,
    const nimcp_tensor_t* mask,
    double scale
);

/**
 * @brief Dropout (training mode)
 */
nimcp_tensor_t* nimcp_tensor_dropout(
    const nimcp_tensor_t* t,
    double p,
    bool training
);

/**
 * @brief Cross-entropy loss
 */
nimcp_tensor_t* nimcp_tensor_cross_entropy(
    const nimcp_tensor_t* logits,
    const nimcp_tensor_t* targets
);

/**
 * @brief Mean squared error loss
 */
nimcp_tensor_t* nimcp_tensor_mse_loss(
    const nimcp_tensor_t* pred,
    const nimcp_tensor_t* target
);

//=============================================================================
// Comparison Operations
//=============================================================================

nimcp_tensor_t* nimcp_tensor_eq(const nimcp_tensor_t* a, const nimcp_tensor_t* b);
nimcp_tensor_t* nimcp_tensor_ne(const nimcp_tensor_t* a, const nimcp_tensor_t* b);
nimcp_tensor_t* nimcp_tensor_lt(const nimcp_tensor_t* a, const nimcp_tensor_t* b);
nimcp_tensor_t* nimcp_tensor_le(const nimcp_tensor_t* a, const nimcp_tensor_t* b);
nimcp_tensor_t* nimcp_tensor_gt(const nimcp_tensor_t* a, const nimcp_tensor_t* b);
nimcp_tensor_t* nimcp_tensor_ge(const nimcp_tensor_t* a, const nimcp_tensor_t* b);

/**
 * @brief Check if tensors are approximately equal
 */
bool nimcp_tensor_allclose(
    const nimcp_tensor_t* a,
    const nimcp_tensor_t* b,
    double rtol,
    double atol
);

/**
 * @brief Where (conditional selection)
 */
nimcp_tensor_t* nimcp_tensor_where(
    const nimcp_tensor_t* condition,
    const nimcp_tensor_t* x,
    const nimcp_tensor_t* y
);

//=============================================================================
// Statistics and Utilities
//=============================================================================

/**
 * @brief Get global tensor statistics
 */
int nimcp_tensor_get_stats(nimcp_tensor_stats_t* stats);

/**
 * @brief Reset global statistics
 */
void nimcp_tensor_reset_stats(void);

/**
 * @brief Get dtype name string
 */
const char* nimcp_dtype_name(nimcp_dtype_t dtype);

/**
 * @brief Get dtype size in bytes
 */
size_t nimcp_dtype_size(nimcp_dtype_t dtype);

/**
 * @brief Print tensor info (shape, dtype, etc.)
 */
void nimcp_tensor_print_info(const nimcp_tensor_t* t, const char* name);

/**
 * @brief Print tensor data (first N elements)
 */
void nimcp_tensor_print_data(const nimcp_tensor_t* t, uint32_t max_elements);

/**
 * @brief Get error message for error code
 */
const char* nimcp_tensor_error_string(nimcp_tensor_error_t err);

//=============================================================================
// Persistence (save/load)
//=============================================================================

/**
 * @brief Save tensor to a FILE stream
 *
 * WHAT: Serialize tensor shape, dtype, and data to binary
 * WHY:  Checkpoint persistence for network weights
 * HOW:  Write magic + rank + dims + dtype + raw data
 *
 * @param tensor Tensor to save (may be NULL — writes a null marker)
 * @param file   Open FILE stream for binary writing
 * @return 0 on success, -1 on error
 */
int nimcp_tensor_save(const nimcp_tensor_t* tensor, FILE* file);

/**
 * @brief Load tensor from a FILE stream
 *
 * WHAT: Deserialize tensor from binary
 * WHY:  Restore checkpointed weights
 * HOW:  Read magic + rank + dims + dtype, create tensor, read data
 *
 * @param file Open FILE stream for binary reading
 * @return Tensor handle, or NULL on error or if a null marker was saved
 */
nimcp_tensor_t* nimcp_tensor_load(FILE* file);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TENSOR_H */
