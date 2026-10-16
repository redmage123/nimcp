//=============================================================================
// nimcp_sparse_gpu.h - GPU Sparse Tensor Operations using cuSPARSE
//=============================================================================
/**
 * @file nimcp_sparse_gpu.h
 * @brief GPU-accelerated sparse tensor operations with cuSPARSE integration
 *
 * WHAT: Comprehensive sparse tensor system with multiple format support
 * WHY:  Enable memory-efficient neural network operations with sparse weights
 * HOW:  Uses cuSPARSE for optimized SpMM/SpMV, custom kernels for pruning
 *
 * ARCHITECTURE:
 *
 *   +----------------------------------------------------------+
 *   |              SPARSE TENSOR GPU SYSTEM                     |
 *   |                                                          |
 *   |  +--------------+  +--------------+  +--------------+    |
 *   |  |   CSR/CSC    |  |     COO      |  |   BSR/ELL    |    |
 *   |  |   Format     |  |    Format    |  |   Formats    |    |
 *   |  +--------------+  +--------------+  +--------------+    |
 *   |         |                |                 |             |
 *   |         +--------+-------+---------+-------+             |
 *   |                  |                 |                     |
 *   |        +------------------+  +------------------+        |
 *   |        |    cuSPARSE      |  |  Custom Kernels  |        |
 *   |        | (SpMM, SpMV,    |  | (Prune, Mask,    |        |
 *   |        |  Add, Convert)   |  |  Attention)      |        |
 *   |        +------------------+  +------------------+        |
 *   +----------------------------------------------------------+
 *
 * SUPPORTED SPARSE FORMATS:
 * - CSR (Compressed Sparse Row): Best for row-major operations
 * - CSC (Compressed Sparse Column): Best for column-major operations
 * - COO (Coordinate): Best for construction and format conversion
 * - BSR (Block Sparse Row): Best for structured sparsity (N:M)
 * - ELL (ELLPACK): Best for uniform sparsity patterns
 *
 * PERFORMANCE CHARACTERISTICS:
 * - SpMM (Sparse x Dense): O(nnz * N) vs O(M * K * N) for dense
 * - SpMV (Sparse x Vector): O(nnz) vs O(M * N) for dense
 * - Memory: O(nnz) vs O(M * N) for dense
 * - Best speedup at >70% sparsity
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_SPARSE_GPU_H
#define NIMCP_SPARSE_GPU_H

// Include GPU context BEFORE extern "C" block - it brings in CUDA headers
// which contain C++ code that cannot have C linkage
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

#ifdef NIMCP_ENABLE_CUDA
#include <cusparse.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

//=============================================================================
// Forward Declarations
//=============================================================================

#ifndef NIMCP_ENABLE_CUDA
typedef void* cusparseHandle_t;
typedef void* cusparseSpMatDescr_t;
typedef void* cusparseDnMatDescr_t;
typedef void* cusparseDnVecDescr_t;
#endif

//=============================================================================
// Sparse Format Enumeration
//=============================================================================

/**
 * @brief Supported sparse tensor formats
 */
typedef enum {
    SPARSE_FORMAT_CSR = 0,      /**< Compressed Sparse Row */
    SPARSE_FORMAT_CSC = 1,      /**< Compressed Sparse Column */
    SPARSE_FORMAT_COO = 2,      /**< Coordinate format */
    SPARSE_FORMAT_BSR = 3,      /**< Block Sparse Row */
    SPARSE_FORMAT_ELL = 4       /**< ELLPACK (fixed nnz per row) */
} nimcp_sparse_format_t;

/**
 * @brief Sparse matrix transpose operation
 */
typedef enum {
    SPARSE_OP_NON_TRANSPOSE = 0,
    SPARSE_OP_TRANSPOSE = 1,
    SPARSE_OP_CONJUGATE_TRANSPOSE = 2
} nimcp_sparse_operation_t;

//=============================================================================
// CSR Sparse Tensor (Compressed Sparse Row)
//=============================================================================

/**
 * @brief CSR sparse tensor structure
 *
 * WHAT: Compressed Sparse Row format for efficient row-based operations
 * WHY:  Optimal for SpMV and row-slicing operations
 * HOW:  Store values, column indices, and row pointers
 *
 * MEMORY LAYOUT:
 * values[nnz]       - Non-zero values (device memory)
 * col_indices[nnz]  - Column index for each value
 * row_ptrs[rows+1]  - Cumulative count of non-zeros per row
 *
 * EXAMPLE (3x4 matrix, 5 non-zeros):
 *   [1 0 2 0]
 *   [0 3 0 4]    values:      [1, 2, 3, 4, 5]
 *   [5 0 0 0]    col_indices: [0, 2, 1, 3, 0]
 *                row_ptrs:    [0, 2, 4, 5]
 */
typedef struct nimcp_sparse_csr {
    float* values;              /**< Non-zero values [nnz] */
    int* col_indices;           /**< Column indices [nnz] */
    int* row_ptrs;              /**< Row pointers [rows+1] */
    int rows;                   /**< Number of rows */
    int cols;                   /**< Number of columns */
    int nnz;                    /**< Number of non-zeros */
    float sparsity;             /**< Sparsity ratio (1 - nnz/total) */
} nimcp_sparse_csr_t;

//=============================================================================
// CSC Sparse Tensor (Compressed Sparse Column)
//=============================================================================

/**
 * @brief CSC sparse tensor structure
 *
 * WHAT: Compressed Sparse Column format
 * WHY:  Optimal for column-based operations and some linear solvers
 * HOW:  Store values, row indices, and column pointers
 */
typedef struct nimcp_sparse_csc {
    float* values;              /**< Non-zero values [nnz] */
    int* row_indices;           /**< Row indices [nnz] */
    int* col_ptrs;              /**< Column pointers [cols+1] */
    int rows;                   /**< Number of rows */
    int cols;                   /**< Number of columns */
    int nnz;                    /**< Number of non-zeros */
    float sparsity;             /**< Sparsity ratio */
} nimcp_sparse_csc_t;

//=============================================================================
// COO Sparse Tensor (Coordinate Format)
//=============================================================================

/**
 * @brief COO sparse tensor structure
 *
 * WHAT: Coordinate format for flexible sparse tensor construction
 * WHY:  Easy to construct and convert to other formats
 * HOW:  Store row, column, and value for each non-zero element
 */
typedef struct nimcp_sparse_coo {
    float* values;              /**< Non-zero values [nnz] */
    int* row_indices;           /**< Row indices [nnz] */
    int* col_indices;           /**< Column indices [nnz] */
    int rows;                   /**< Number of rows */
    int cols;                   /**< Number of columns */
    int nnz;                    /**< Number of non-zeros */
} nimcp_sparse_coo_t;

//=============================================================================
// BSR Sparse Tensor (Block Sparse Row)
//=============================================================================

/**
 * @brief BSR sparse tensor structure
 *
 * WHAT: Block Sparse Row format for structured sparsity
 * WHY:  Efficient for N:M sparsity patterns (e.g., 2:4 for Ampere GPUs)
 * HOW:  Store blocks instead of individual values
 *
 * MEMORY LAYOUT:
 * values[nnz_blocks * block_size^2] - Non-zero blocks (row-major within blocks)
 * col_indices[nnz_blocks]           - Block column indices
 * row_ptrs[block_rows+1]            - Block row pointers
 */
typedef struct nimcp_sparse_bsr {
    float* values;              /**< Non-zero blocks [nnz_blocks * block_size^2] */
    int* col_indices;           /**< Block column indices */
    int* row_ptrs;              /**< Block row pointers */
    int rows;                   /**< Number of block rows */
    int cols;                   /**< Number of block columns */
    int block_size;             /**< Block dimension (e.g., 4 for 4x4 blocks) */
    int nnz_blocks;             /**< Number of non-zero blocks */
} nimcp_sparse_bsr_t;

//=============================================================================
// ELL Sparse Tensor (ELLPACK)
//=============================================================================

/**
 * @brief ELL sparse tensor structure
 *
 * WHAT: ELLPACK format with fixed non-zeros per row
 * WHY:  Optimal for regular sparsity patterns (uniform nnz per row)
 * HOW:  Pad shorter rows with zeros, store in column-major
 */
typedef struct nimcp_sparse_ell {
    float* values;              /**< Values [rows * max_nnz_per_row], column-major */
    int* col_indices;           /**< Column indices [rows * max_nnz_per_row] */
    int rows;                   /**< Number of rows */
    int cols;                   /**< Number of columns */
    int max_nnz_per_row;        /**< Maximum non-zeros in any row */
    int nnz;                    /**< Total non-zeros */
} nimcp_sparse_ell_t;

//=============================================================================
// Unified Sparse Tensor Handle
//=============================================================================

/**
 * @brief Unified sparse tensor handle supporting all formats
 *
 * WHAT: Single handle type for any sparse format
 * WHY:  Unified API across formats with format-specific optimizations
 * HOW:  Union of format-specific structures with common metadata
 */
typedef struct nimcp_sparse_tensor {
    nimcp_sparse_format_t format;   /**< Current sparse format */

    union {
        nimcp_sparse_csr_t csr;
        nimcp_sparse_csc_t csc;
        nimcp_sparse_coo_t coo;
        nimcp_sparse_bsr_t bsr;
        nimcp_sparse_ell_t ell;
    } data;

#ifdef NIMCP_ENABLE_CUDA
    cusparseSpMatDescr_t cusparse_desc;  /**< cuSPARSE descriptor */
#else
    void* cusparse_desc;
#endif

    bool on_device;                  /**< Data resides on GPU */
    nimcp_gpu_context_t* ctx;        /**< GPU context */
    bool owns_data;                  /**< Whether tensor owns its data */
} nimcp_sparse_tensor_t;

//=============================================================================
// Sparse Context
//=============================================================================

/**
 * @brief Sparse operations context with cuSPARSE handle
 *
 * WHAT: Context for sparse GPU operations
 * WHY:  Manages cuSPARSE handle and workspace allocation
 * HOW:  Wraps cuSPARSE handle with memory management
 */
typedef struct nimcp_sparse_ctx {
    nimcp_gpu_context_t* gpu_ctx;   /**< Associated GPU context */
#ifdef NIMCP_ENABLE_CUDA
    cusparseHandle_t cusparse_handle; /**< cuSPARSE library handle */
#else
    void* cusparse_handle;
#endif
    void* workspace;                 /**< cuSPARSE workspace buffer */
    size_t workspace_size;           /**< Current workspace size */
    size_t workspace_capacity;       /**< Allocated workspace capacity */
} nimcp_sparse_ctx_t;

//=============================================================================
// Sparsity Statistics
//=============================================================================

/**
 * @brief Sparsity statistics for a sparse tensor
 */
typedef struct nimcp_sparsity_stats {
    float sparsity_ratio;           /**< nnz / total (0 = all zeros, 1 = all non-zero) */
    float density_ratio;            /**< 1 - sparsity_ratio */
    int nnz;                        /**< Number of non-zeros */
    int total_elements;             /**< Total elements (rows * cols) */
    float avg_nnz_per_row;          /**< Average non-zeros per row */
    float max_nnz_per_row;          /**< Maximum non-zeros in any row */
    float min_nnz_per_row;          /**< Minimum non-zeros in any row */
    float std_nnz_per_row;          /**< Standard deviation of nnz per row */
    size_t dense_memory_bytes;      /**< Memory if stored as dense */
    size_t sparse_memory_bytes;     /**< Actual sparse memory usage */
    float memory_savings_percent;   /**< Memory savings percentage */
} nimcp_sparsity_stats_t;

//=============================================================================
// Context Lifecycle API
//=============================================================================

/**
 * @brief Create sparse operations context
 *
 * WHAT: Initialize cuSPARSE handle and workspace
 * WHY:  Required for all sparse GPU operations
 * HOW:  Creates cuSPARSE handle bound to GPU context
 *
 * @param gpu_ctx GPU context (must be valid)
 * @return Sparse context on success, NULL on failure
 *
 * EXAMPLE:
 *   nimcp_gpu_context_t* gpu_ctx = nimcp_gpu_context_create(0);
 *   nimcp_sparse_ctx_t* sparse_ctx = nimcp_sparse_ctx_create(gpu_ctx);
 */
NIMCP_EXPORT nimcp_sparse_ctx_t* nimcp_sparse_ctx_create(nimcp_gpu_context_t* gpu_ctx);

/**
 * @brief Destroy sparse operations context
 *
 * @param ctx Context to destroy (can be NULL)
 */
NIMCP_EXPORT void nimcp_sparse_ctx_destroy(nimcp_sparse_ctx_t* ctx);

/**
 * @brief Ensure workspace is at least specified size
 *
 * @param ctx Sparse context
 * @param required_size Minimum workspace size in bytes
 * @return true if workspace is sufficient, false on allocation failure
 */
NIMCP_EXPORT bool nimcp_sparse_ctx_ensure_workspace(
    nimcp_sparse_ctx_t* ctx,
    size_t required_size
);

//=============================================================================
// Sparse Tensor Creation API
//=============================================================================

/**
 * @brief Create sparse tensor from dense tensor
 *
 * WHAT: Convert dense tensor to sparse by pruning small values
 * WHY:  Create sparse representation for memory efficiency
 * HOW:  Values with |x| < threshold become zeros
 *
 * @param ctx Sparse context
 * @param dense Dense GPU tensor
 * @param format Target sparse format
 * @param threshold Values with |x| < threshold become zero
 * @return Sparse tensor on success, NULL on failure
 *
 * EXAMPLE:
 *   // Prune values below 0.01
 *   nimcp_sparse_tensor_t* sparse = nimcp_sparse_from_dense(
 *       ctx, dense_weights, SPARSE_FORMAT_CSR, 0.01f);
 */
NIMCP_EXPORT nimcp_sparse_tensor_t* nimcp_sparse_from_dense(
    nimcp_sparse_ctx_t* ctx,
    nimcp_gpu_tensor_t* dense,
    nimcp_sparse_format_t format,
    float threshold
);

/**
 * @brief Create sparse tensor from explicit COO data
 *
 * WHAT: Construct sparse tensor from coordinate arrays
 * WHY:  Allow direct construction from known sparse structure
 * HOW:  Upload COO data and optionally convert to target format
 *
 * @param ctx Sparse context
 * @param values Non-zero values array (host memory)
 * @param row_idx Row indices array (host memory)
 * @param col_idx Column indices array (host memory)
 * @param rows Number of rows
 * @param cols Number of columns
 * @param nnz Number of non-zeros
 * @param target_format Target sparse format
 * @return Sparse tensor on success, NULL on failure
 */
NIMCP_EXPORT nimcp_sparse_tensor_t* nimcp_sparse_from_coo(
    nimcp_sparse_ctx_t* ctx,
    const float* values,
    const int* row_idx,
    const int* col_idx,
    int rows,
    int cols,
    int nnz,
    nimcp_sparse_format_t target_format
);

/**
 * @brief Create sparse tensor from CSR data
 *
 * @param ctx Sparse context
 * @param values Non-zero values (host memory)
 * @param col_indices Column indices (host memory)
 * @param row_ptrs Row pointers (host memory)
 * @param rows Number of rows
 * @param cols Number of columns
 * @param nnz Number of non-zeros
 * @return Sparse CSR tensor on success, NULL on failure
 */
NIMCP_EXPORT nimcp_sparse_tensor_t* nimcp_sparse_from_csr(
    nimcp_sparse_ctx_t* ctx,
    const float* values,
    const int* col_indices,
    const int* row_ptrs,
    int rows,
    int cols,
    int nnz
);

/**
 * @brief Convert sparse tensor to different format
 *
 * WHAT: Convert between sparse formats
 * WHY:  Different operations are optimal in different formats
 * HOW:  Uses cuSPARSE conversion routines
 *
 * @param ctx Sparse context
 * @param src Source sparse tensor
 * @param target_format Target format
 * @return New sparse tensor in target format, or NULL on failure
 */
NIMCP_EXPORT nimcp_sparse_tensor_t* nimcp_sparse_convert(
    nimcp_sparse_ctx_t* ctx,
    nimcp_sparse_tensor_t* src,
    nimcp_sparse_format_t target_format
);

/**
 * @brief Convert sparse tensor to dense
 *
 * @param ctx Sparse context
 * @param sparse Source sparse tensor
 * @return Dense GPU tensor on success, NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_sparse_to_dense(
    nimcp_sparse_ctx_t* ctx,
    nimcp_sparse_tensor_t* sparse
);

/**
 * @brief Clone a sparse tensor
 *
 * @param ctx Sparse context
 * @param tensor Tensor to clone
 * @return New tensor with copied data
 */
NIMCP_EXPORT nimcp_sparse_tensor_t* nimcp_sparse_tensor_clone(
    nimcp_sparse_ctx_t* ctx,
    const nimcp_sparse_tensor_t* tensor
);

/**
 * @brief Destroy sparse tensor and free resources
 *
 * @param tensor Tensor to destroy (can be NULL)
 */
NIMCP_EXPORT void nimcp_sparse_tensor_destroy(nimcp_sparse_tensor_t* tensor);

//=============================================================================
// Sparse Matrix Operations (cuSPARSE wrappers)
//=============================================================================

/**
 * @brief Sparse x Dense Matrix Multiplication (SpMM)
 *
 * WHAT: Compute C = alpha * A_sparse * B_dense + beta * C
 * WHY:  Core operation for sparse neural network layers
 * HOW:  Uses cuSPARSE SpMM (cusparseSpMM)
 *
 * @param ctx Sparse context
 * @param A Sparse matrix [M, K]
 * @param B Dense matrix [K, N]
 * @param alpha Scalar multiplier for A * B
 * @param beta Scalar multiplier for C
 * @param C Dense matrix [M, N] (optional, created if NULL)
 * @return Output dense tensor C
 *
 * PERFORMANCE:
 * - Best speedup at >70% sparsity
 * - CSR format optimal for row-major operations
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_sparse_mm(
    nimcp_sparse_ctx_t* ctx,
    nimcp_sparse_tensor_t* A,
    nimcp_gpu_tensor_t* B,
    float alpha,
    float beta,
    nimcp_gpu_tensor_t* C
);

/**
 * @brief Sparse x Dense Vector (SpMV)
 *
 * WHAT: Compute y = alpha * A_sparse * x + beta * y
 * WHY:  Efficient for single-sample inference
 * HOW:  Uses cuSPARSE SpMV (cusparseSpMV)
 *
 * @param ctx Sparse context
 * @param A Sparse matrix [M, N]
 * @param x Dense vector [N]
 * @param alpha Scalar multiplier for A * x
 * @param beta Scalar multiplier for y
 * @param y Dense vector [M] (optional, created if NULL)
 * @return Output dense vector y
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_sparse_mv(
    nimcp_sparse_ctx_t* ctx,
    nimcp_sparse_tensor_t* A,
    nimcp_gpu_tensor_t* x,
    float alpha,
    float beta,
    nimcp_gpu_tensor_t* y
);

/**
 * @brief Transposed Sparse x Dense Matrix Multiplication
 *
 * WHAT: Compute C = alpha * A_sparse^T * B_dense + beta * C
 * WHY:  Needed for backward pass in sparse layers
 * HOW:  Uses cuSPARSE SpMM with transpose operation
 *
 * @param ctx Sparse context
 * @param A Sparse matrix [M, K] (will be transposed)
 * @param B Dense matrix [M, N]
 * @param alpha Scalar multiplier
 * @param beta Scalar multiplier
 * @param C Dense matrix [K, N] (optional)
 * @return Output dense tensor C
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_sparse_mm_transpose(
    nimcp_sparse_ctx_t* ctx,
    nimcp_sparse_tensor_t* A,
    nimcp_gpu_tensor_t* B,
    float alpha,
    float beta,
    nimcp_gpu_tensor_t* C
);

/**
 * @brief Sparse + Sparse Addition
 *
 * WHAT: Compute C = alpha * A + beta * B (both sparse)
 * WHY:  Combine sparse matrices efficiently
 * HOW:  Uses cuSPARSE sparse-sparse addition
 *
 * @param ctx Sparse context
 * @param A First sparse matrix
 * @param B Second sparse matrix (same dimensions as A)
 * @param alpha Scalar multiplier for A
 * @param beta Scalar multiplier for B
 * @return New sparse tensor C = alpha*A + beta*B
 */
NIMCP_EXPORT nimcp_sparse_tensor_t* nimcp_sparse_add(
    nimcp_sparse_ctx_t* ctx,
    nimcp_sparse_tensor_t* A,
    nimcp_sparse_tensor_t* B,
    float alpha,
    float beta
);

/**
 * @brief Scale sparse tensor values
 *
 * WHAT: Multiply all values by scalar
 * WHY:  Apply learning rate, normalize, etc.
 * HOW:  In-place scaling of values array
 *
 * @param ctx Sparse context
 * @param A Sparse tensor to scale
 * @param scale Scalar multiplier
 * @return Scaled sparse tensor (same as A, modified in-place)
 */
NIMCP_EXPORT nimcp_sparse_tensor_t* nimcp_sparse_scale(
    nimcp_sparse_ctx_t* ctx,
    nimcp_sparse_tensor_t* A,
    float scale
);

/**
 * @brief Batched Sparse x Dense Matrix Multiplication
 *
 * WHAT: Batch of SpMM operations with shared sparse matrix
 * WHY:  Efficient for batch inference with sparse weights
 * HOW:  Single sparse matrix applied to batch of dense matrices
 *
 * @param ctx Sparse context
 * @param A Sparse matrix [M, K]
 * @param B Dense batch [batch, K, N]
 * @param alpha Scalar multiplier
 * @param beta Scalar multiplier
 * @param C Dense batch [batch, M, N] (optional)
 * @return Output batch C
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_sparse_mm_batched(
    nimcp_sparse_ctx_t* ctx,
    nimcp_sparse_tensor_t* A,
    nimcp_gpu_tensor_t* B,
    float alpha,
    float beta,
    nimcp_gpu_tensor_t* C
);

//=============================================================================
// Custom CUDA Kernels for Sparse Operations
//=============================================================================

/**
 * @brief Sparse attention with mask
 *
 * WHAT: Compute sparse attention output = softmax(Q * K^T, mask) * V
 * WHY:  Memory-efficient attention for long sequences
 * HOW:  Only compute attention where mask is non-zero
 *
 * @param ctx Sparse context
 * @param Q Query tensor [batch, heads, seq_len, head_dim]
 * @param K Key tensor [batch, heads, seq_len, head_dim]
 * @param V Value tensor [batch, heads, seq_len, head_dim]
 * @param attention_mask Sparse attention mask
 * @param output Output tensor [batch, heads, seq_len, head_dim]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_sparse_attention(
    nimcp_sparse_ctx_t* ctx,
    nimcp_gpu_tensor_t* Q,
    nimcp_gpu_tensor_t* K,
    nimcp_gpu_tensor_t* V,
    nimcp_sparse_tensor_t* attention_mask,
    nimcp_gpu_tensor_t* output
);

/**
 * @brief Apply sparse mask to dense tensor
 *
 * WHAT: Zero out elements not in sparse mask
 * WHY:  Enforce sparsity pattern on dense tensors
 * HOW:  Keep only values at mask positions
 *
 * @param ctx Sparse context
 * @param dense_in Input dense tensor
 * @param mask Sparse mask (structure defines kept positions)
 * @param dense_out Output dense tensor (can be same as input)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_sparse_apply_mask(
    nimcp_sparse_ctx_t* ctx,
    nimcp_gpu_tensor_t* dense_in,
    nimcp_sparse_tensor_t* mask,
    nimcp_gpu_tensor_t* dense_out
);

/**
 * @brief Accumulate sparse gradients into dense gradient
 *
 * WHAT: Add sparse gradients to dense gradient tensor
 * WHY:  Efficient gradient accumulation for sparse layers
 * HOW:  Scatter-add sparse values to dense positions
 *
 * @param ctx Sparse context
 * @param sparse_grad Sparse gradient
 * @param dense_grad Dense gradient to accumulate into
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_sparse_grad_accumulate(
    nimcp_sparse_ctx_t* ctx,
    nimcp_sparse_tensor_t* sparse_grad,
    nimcp_gpu_tensor_t* dense_grad
);

//=============================================================================
// Sparse Neural Network Layer Operations
//=============================================================================

/**
 * @brief Sparse linear layer forward pass
 *
 * WHAT: Compute output = input @ weight_sparse^T + bias
 * WHY:  Memory-efficient fully connected layers
 * HOW:  SpMM with transposed sparse weights
 *
 * @param ctx Sparse context
 * @param weight Sparse weight matrix [out_features, in_features]
 * @param bias Dense bias vector [out_features] (can be NULL)
 * @param input Dense input [batch, in_features]
 * @return Output tensor [batch, out_features]
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_sparse_linear_forward(
    nimcp_sparse_ctx_t* ctx,
    nimcp_sparse_tensor_t* weight,
    nimcp_gpu_tensor_t* bias,
    nimcp_gpu_tensor_t* input
);

/**
 * @brief Sparse linear layer backward pass
 *
 * WHAT: Compute gradients for sparse linear layer
 * WHY:  Training sparse neural networks
 * HOW:  SpMM operations for gradient computation
 *
 * @param ctx Sparse context
 * @param weight Sparse weight matrix
 * @param input Forward pass input (saved for backward)
 * @param grad_output Gradient w.r.t. output
 * @param grad_weight Output: sparse weight gradient (same sparsity as weight)
 * @param grad_input Output: dense input gradient
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_sparse_linear_backward(
    nimcp_sparse_ctx_t* ctx,
    nimcp_sparse_tensor_t* weight,
    nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* grad_output,
    nimcp_sparse_tensor_t* grad_weight,
    nimcp_gpu_tensor_t* grad_input
);

/**
 * @brief Sparse synaptic forward pass (for SNN)
 *
 * WHAT: Compute post-synaptic activity from pre-synaptic spikes
 * WHY:  Efficient spiking neural network computation
 * HOW:  SpMV with sparse connectivity matrix
 *
 * @param ctx Sparse context
 * @param connectivity Sparse connectivity [post, pre]
 * @param pre_activity Dense pre-synaptic activity [pre]
 * @return Post-synaptic activity [post]
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_sparse_synapse_forward(
    nimcp_sparse_ctx_t* ctx,
    nimcp_sparse_tensor_t* connectivity,
    nimcp_gpu_tensor_t* pre_activity
);

//=============================================================================
// Pruning and Sparsification Utilities
//=============================================================================

/**
 * @brief Magnitude-based pruning
 *
 * WHAT: Create sparse tensor by keeping top-k% largest values
 * WHY:  Neural network compression via weight pruning
 * HOW:  Threshold based on magnitude percentile
 *
 * @param ctx Sparse context
 * @param dense Dense tensor to prune
 * @param sparsity_target Target sparsity (e.g., 0.9 = 90% zeros)
 * @return Sparse tensor with sparsity_target fraction of zeros
 */
NIMCP_EXPORT nimcp_sparse_tensor_t* nimcp_magnitude_prune(
    nimcp_sparse_ctx_t* ctx,
    nimcp_gpu_tensor_t* dense,
    float sparsity_target
);

/**
 * @brief Structured N:M pruning
 *
 * WHAT: Keep N largest values per M consecutive elements
 * WHY:  Ampere GPU acceleration requires 2:4 sparsity
 * HOW:  For each group of M elements, keep N largest
 *
 * @param ctx Sparse context
 * @param dense Dense tensor to prune
 * @param N Number of values to keep
 * @param M Group size (must be 2, 4, 8, or 16)
 * @return Sparse tensor with N:M structured sparsity
 *
 * EXAMPLE:
 *   // 2:4 sparsity for Ampere Tensor Core acceleration
 *   nimcp_sparse_tensor_t* sparse = nimcp_structured_prune(ctx, dense, 2, 4);
 */
NIMCP_EXPORT nimcp_sparse_tensor_t* nimcp_structured_prune(
    nimcp_sparse_ctx_t* ctx,
    nimcp_gpu_tensor_t* dense,
    int N,
    int M
);

/**
 * @brief Threshold-based pruning
 *
 * WHAT: Zero out values with absolute value below threshold
 * WHY:  Simple sparsification for model compression
 * HOW:  Direct threshold comparison
 *
 * @param ctx Sparse context
 * @param dense Dense tensor
 * @param threshold Absolute threshold value
 * @return Sparse tensor with values below threshold removed
 */
NIMCP_EXPORT nimcp_sparse_tensor_t* nimcp_threshold_prune(
    nimcp_sparse_ctx_t* ctx,
    nimcp_gpu_tensor_t* dense,
    float threshold
);

/**
 * @brief Random sparsity pattern
 *
 * WHAT: Create sparse tensor with random non-zero positions
 * WHY:  Initialize sparse networks, test sparse operations
 * HOW:  Random sampling of positions
 *
 * @param ctx Sparse context
 * @param rows Number of rows
 * @param cols Number of columns
 * @param density Fraction of non-zeros (0-1)
 * @param format Target sparse format
 * @return Sparse tensor with random pattern
 */
NIMCP_EXPORT nimcp_sparse_tensor_t* nimcp_sparse_random(
    nimcp_sparse_ctx_t* ctx,
    int rows,
    int cols,
    float density,
    nimcp_sparse_format_t format
);

//=============================================================================
// Sparsity Statistics and Analysis
//=============================================================================

/**
 * @brief Get sparsity statistics for sparse tensor
 *
 * @param tensor Sparse tensor to analyze
 * @return Statistics structure
 */
NIMCP_EXPORT nimcp_sparsity_stats_t nimcp_sparse_get_stats(
    nimcp_sparse_tensor_t* tensor
);

/**
 * @brief Compute sparsity of dense tensor without conversion
 *
 * WHAT: Count near-zero elements in dense tensor
 * WHY:  Decide whether sparsification is beneficial
 * HOW:  Count elements with |x| < threshold
 *
 * @param ctx Sparse context
 * @param dense Dense tensor to analyze
 * @param threshold Threshold for "near-zero"
 * @return Sparsity ratio (0-1)
 */
NIMCP_EXPORT float nimcp_sparse_compute_density(
    nimcp_sparse_ctx_t* ctx,
    nimcp_gpu_tensor_t* dense,
    float threshold
);

/**
 * @brief Print sparse tensor info to stdout
 *
 * @param tensor Sparse tensor
 * @param verbose Include detailed statistics
 */
NIMCP_EXPORT void nimcp_sparse_print_info(
    nimcp_sparse_tensor_t* tensor,
    bool verbose
);

/**
 * @brief Validate sparse tensor structure
 *
 * WHAT: Check that sparse tensor is well-formed
 * WHY:  Debug sparse tensor construction
 * HOW:  Verify indices are in bounds, sorted, etc.
 *
 * @param tensor Tensor to validate
 * @return true if valid, false if corrupted
 */
NIMCP_EXPORT bool nimcp_sparse_validate(nimcp_sparse_tensor_t* tensor);

//=============================================================================
// Host-Device Transfer
//=============================================================================

/**
 * @brief Copy sparse tensor to host memory
 *
 * @param tensor GPU sparse tensor
 * @param values Output values array (must be pre-allocated)
 * @param indices Output indices arrays (format-specific)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_sparse_to_host_csr(
    nimcp_sparse_tensor_t* tensor,
    float* values,
    int* col_indices,
    int* row_ptrs
);

/**
 * @brief Copy sparse tensor to host memory (COO format)
 */
NIMCP_EXPORT bool nimcp_sparse_to_host_coo(
    nimcp_sparse_tensor_t* tensor,
    float* values,
    int* row_indices,
    int* col_indices
);

//=============================================================================
// Format-Specific Utilities
//=============================================================================

/**
 * @brief Get number of rows
 */
NIMCP_EXPORT int nimcp_sparse_rows(const nimcp_sparse_tensor_t* tensor);

/**
 * @brief Get number of columns
 */
NIMCP_EXPORT int nimcp_sparse_cols(const nimcp_sparse_tensor_t* tensor);

/**
 * @brief Get number of non-zeros
 */
NIMCP_EXPORT int nimcp_sparse_nnz(const nimcp_sparse_tensor_t* tensor);

/**
 * @brief Get sparsity ratio
 */
NIMCP_EXPORT float nimcp_sparse_sparsity(const nimcp_sparse_tensor_t* tensor);

/**
 * @brief Get format string for sparse tensor
 */
NIMCP_EXPORT const char* nimcp_sparse_format_name(nimcp_sparse_format_t format);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SPARSE_GPU_H
