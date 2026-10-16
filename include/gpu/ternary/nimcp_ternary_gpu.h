/**
 * @file nimcp_ternary_gpu.h
 * @brief GPU-accelerated Ternary Tensor Operations
 *
 * WHAT: CUDA kernels for ternary {-1, 0, +1} tensor operations
 * WHY:  20x memory reduction, 2-3x faster GEMM (no multiplication needed)
 * HOW:  Specialized kernels exploiting ternary structure
 *
 * ARCHITECTURE:
 *
 *   +----------------------------------------------------------+
 *   |              TERNARY GPU SYSTEM                          |
 *   |                                                          |
 *   |  +------------------+  +------------------+              |
 *   |  | Ternary Tensor   |  | Ternary GEMM     |              |
 *   |  | (Packed Storage) |  | (No Multiply)    |              |
 *   |  +------------------+  +------------------+              |
 *   |           |                    |                         |
 *   |           v                    v                         |
 *   |  +------------------+  +------------------+              |
 *   |  | 2-bit Packing    |  | Add/Sub/Zero     |              |
 *   |  | (4 trits/byte)   |  | Operations Only  |              |
 *   |  +------------------+  +------------------+              |
 *   +----------------------------------------------------------+
 *
 * STORAGE FORMATS:
 * - UNPACKED: 1 trit per int8 {-1, 0, +1} - Fastest access
 * - PACKED_2BIT: 4 trits per byte - 4x memory compression
 * - PACKED_BASE243: 5 trits per byte - 5x compression (slower)
 *
 * CONSUMERS:
 * - LNN: Ternary sparse matrices
 * - SNN: Ternary synaptic weights
 * - Plasticity: Ternary weight updates
 * - Swarm: Ternary voting consensus
 * - Attention: Ternary gating
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_TERNARY_GPU_H
#define NIMCP_TERNARY_GPU_H

#include "gpu/context/nimcp_gpu_context.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include <stdbool.h>
#include <stdint.h>

//=============================================================================
// Constants
//=============================================================================

#define NIMCP_TERNARY_MAGIC 0x54524954  /**< 'TRIT' magic number */

/** Ternary values */
#define NIMCP_TRIT_NEG  (-1)
#define NIMCP_TRIT_ZERO (0)
#define NIMCP_TRIT_POS  (1)

//=============================================================================
// Ternary Storage Format
//=============================================================================

/**
 * @brief Ternary packing format
 */
typedef enum {
    TERNARY_PACK_NONE = 0,      /**< Unpacked: 1 trit per int8 */
    TERNARY_PACK_2BIT = 1,      /**< 2-bit packed: 4 trits per byte */
    TERNARY_PACK_BASE243 = 2    /**< Base-243 packed: 5 trits per byte */
} nimcp_ternary_pack_t;

/**
 * @brief Ternary tensor structure
 *
 * WHAT: GPU-resident ternary tensor with optional packing
 * WHY:  Memory-efficient storage of {-1, 0, +1} values
 * HOW:  Stores raw data + metadata for unpacking
 */
typedef struct nimcp_ternary_tensor {
    uint32_t magic;                 /**< Magic number for validation */
    void* data;                     /**< Device memory pointer */
    int64_t* dims;                  /**< Dimension sizes [rank] */
    int rank;                       /**< Number of dimensions */
    int64_t numel;                  /**< Total number of trits */
    size_t packed_size;             /**< Size of packed data in bytes */
    nimcp_ternary_pack_t pack_mode; /**< Packing format */
    float sparsity;                 /**< Fraction of zeros (0-1) */
    nimcp_gpu_context_t* ctx;       /**< GPU context */
    bool owns_data;                 /**< Whether tensor owns its data */
} nimcp_ternary_tensor_t;

/**
 * @brief Ternary quantization configuration
 */
typedef struct nimcp_ternary_quant_config {
    float threshold;                /**< Quantization threshold (default 0.3) */
    bool symmetric;                 /**< Use symmetric thresholds */
    bool adaptive;                  /**< Compute threshold from data statistics */
    float adaptive_percentile;      /**< Percentile for adaptive threshold (0-1) */
} nimcp_ternary_quant_config_t;

/**
 * @brief Ternary GEMM configuration
 */
typedef struct nimcp_ternary_gemm_config {
    bool use_sparse;                /**< Use sparse algorithm if sparsity > 50% */
    bool accumulate;                /**< Add to output (C = A*B + C) */
    float alpha;                    /**< Scale factor for A*B */
    float beta;                     /**< Scale factor for C */
} nimcp_ternary_gemm_config_t;

//=============================================================================
// Ternary Tensor Lifecycle
//=============================================================================

/**
 * @brief Create ternary tensor
 *
 * @param ctx GPU context
 * @param dims Dimension sizes
 * @param rank Number of dimensions
 * @param pack_mode Packing format
 * @return Ternary tensor or NULL on failure
 */
NIMCP_EXPORT nimcp_ternary_tensor_t* nimcp_ternary_tensor_create(
    nimcp_gpu_context_t* ctx,
    const int64_t* dims,
    int rank,
    nimcp_ternary_pack_t pack_mode
);

/**
 * @brief Create ternary tensor from FP32 tensor
 *
 * Quantizes float values to ternary using threshold.
 *
 * @param ctx GPU context
 * @param src Source float tensor
 * @param config Quantization configuration
 * @param pack_mode Packing format for output
 * @return Ternary tensor or NULL on failure
 */
NIMCP_EXPORT nimcp_ternary_tensor_t* nimcp_ternary_from_float(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* src,
    const nimcp_ternary_quant_config_t* config,
    nimcp_ternary_pack_t pack_mode
);

/**
 * @brief Create ternary tensor from host data
 *
 * @param ctx GPU context
 * @param data Host int8 data {-1, 0, +1}
 * @param dims Dimension sizes
 * @param rank Number of dimensions
 * @param pack_mode Packing format
 * @return Ternary tensor or NULL on failure
 */
NIMCP_EXPORT nimcp_ternary_tensor_t* nimcp_ternary_from_host(
    nimcp_gpu_context_t* ctx,
    const int8_t* data,
    const int64_t* dims,
    int rank,
    nimcp_ternary_pack_t pack_mode
);

/**
 * @brief Destroy ternary tensor
 */
NIMCP_EXPORT void nimcp_ternary_tensor_destroy(nimcp_ternary_tensor_t* tensor);

/**
 * @brief Clone ternary tensor
 */
NIMCP_EXPORT nimcp_ternary_tensor_t* nimcp_ternary_tensor_clone(
    nimcp_gpu_context_t* ctx,
    const nimcp_ternary_tensor_t* src
);

/**
 * @brief Check if ternary tensor is valid
 */
NIMCP_EXPORT bool nimcp_ternary_tensor_is_valid(const nimcp_ternary_tensor_t* tensor);

//=============================================================================
// Packing/Unpacking Operations
//=============================================================================

/**
 * @brief Pack ternary tensor to 2-bit format
 *
 * Converts unpacked tensor to 4 trits per byte.
 *
 * @param ctx GPU context
 * @param src Source unpacked tensor
 * @return Packed tensor or NULL on failure
 */
NIMCP_EXPORT nimcp_ternary_tensor_t* nimcp_ternary_pack_2bit(
    nimcp_gpu_context_t* ctx,
    const nimcp_ternary_tensor_t* src
);

/**
 * @brief Unpack ternary tensor from 2-bit format
 *
 * @param ctx GPU context
 * @param src Source packed tensor
 * @return Unpacked tensor or NULL on failure
 */
NIMCP_EXPORT nimcp_ternary_tensor_t* nimcp_ternary_unpack_2bit(
    nimcp_gpu_context_t* ctx,
    const nimcp_ternary_tensor_t* src
);

/**
 * @brief Convert ternary tensor to FP32
 *
 * @param ctx GPU context
 * @param src Ternary tensor
 * @return Float tensor or NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_ternary_to_float(
    nimcp_gpu_context_t* ctx,
    const nimcp_ternary_tensor_t* src
);

/**
 * @brief Convert ternary tensor to INT8
 *
 * @param ctx GPU context
 * @param src Ternary tensor
 * @return INT8 tensor or NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_ternary_to_int8(
    nimcp_gpu_context_t* ctx,
    const nimcp_ternary_tensor_t* src
);

//=============================================================================
// Ternary GEMM (No-Multiply Matrix Multiplication)
//=============================================================================

/**
 * @brief Get default GEMM configuration
 */
NIMCP_EXPORT nimcp_ternary_gemm_config_t nimcp_ternary_gemm_config_default(void);

/**
 * @brief Ternary matrix multiplication: C = A * B
 *
 * WHAT: Matrix multiply where A is ternary, B and C are float
 * WHY:  2-3x faster than float GEMM (no multiplication, only add/subtract)
 * HOW:  For each element: if trit==1 add, if trit==-1 subtract, if trit==0 skip
 *
 * @param ctx GPU context
 * @param A Ternary matrix [M, K]
 * @param B Float matrix [K, N]
 * @param C Output float matrix [M, N] (can be NULL to allocate)
 * @param config GEMM configuration
 * @return Output matrix C or NULL on failure
 *
 * COMPLEXITY: O(M * K * N) but with cheaper operations than float GEMM
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_ternary_gemm(
    nimcp_gpu_context_t* ctx,
    const nimcp_ternary_tensor_t* A,
    const nimcp_gpu_tensor_t* B,
    nimcp_gpu_tensor_t* C,
    const nimcp_ternary_gemm_config_t* config
);

/**
 * @brief Ternary matrix-vector multiplication: y = A * x
 *
 * @param ctx GPU context
 * @param A Ternary matrix [M, N]
 * @param x Float vector [N]
 * @param y Output float vector [M] (can be NULL to allocate)
 * @return Output vector y or NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_ternary_gemv(
    nimcp_gpu_context_t* ctx,
    const nimcp_ternary_tensor_t* A,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* y
);

/**
 * @brief Batched ternary GEMM: C[b] = A * B[b]
 *
 * Single ternary matrix applied to batch of float matrices.
 *
 * @param ctx GPU context
 * @param A Ternary matrix [M, K]
 * @param B Float batch [batch, K, N]
 * @param C Output float batch [batch, M, N]
 * @return Output batch C or NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_ternary_gemm_batched(
    nimcp_gpu_context_t* ctx,
    const nimcp_ternary_tensor_t* A,
    const nimcp_gpu_tensor_t* B,
    nimcp_gpu_tensor_t* C
);

//=============================================================================
// Sparse Ternary Operations (CSR Format)
//=============================================================================

/**
 * @brief Sparse ternary tensor (CSR format)
 *
 * Only stores non-zero trits (+1 or -1).
 * Further memory reduction for sparse ternary matrices.
 */
typedef struct nimcp_ternary_sparse {
    int* row_ptrs;                  /**< CSR row pointers [rows+1] */
    int* col_indices;               /**< Column indices [nnz] */
    int8_t* signs;                  /**< Signs (+1 or -1) [nnz] */
    int rows;                       /**< Number of rows */
    int cols;                       /**< Number of columns */
    int nnz;                        /**< Number of non-zeros */
    float sparsity;                 /**< Fraction of zeros */
    nimcp_gpu_context_t* ctx;       /**< GPU context */
} nimcp_ternary_sparse_t;

/**
 * @brief Create sparse ternary from dense ternary
 *
 * @param ctx GPU context
 * @param dense Dense ternary tensor
 * @return Sparse ternary tensor or NULL on failure
 */
NIMCP_EXPORT nimcp_ternary_sparse_t* nimcp_ternary_sparse_from_dense(
    nimcp_gpu_context_t* ctx,
    const nimcp_ternary_tensor_t* dense
);

/**
 * @brief Destroy sparse ternary tensor
 */
NIMCP_EXPORT void nimcp_ternary_sparse_destroy(nimcp_ternary_sparse_t* sparse);

/**
 * @brief Sparse ternary matrix-vector multiply: y = A_sparse * x
 *
 * @param ctx GPU context
 * @param A Sparse ternary matrix
 * @param x Dense float vector [cols]
 * @param y Output float vector [rows]
 * @return Output vector y or NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_ternary_sparse_gemv(
    nimcp_gpu_context_t* ctx,
    const nimcp_ternary_sparse_t* A,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* y
);

//=============================================================================
// Quantization Operations
//=============================================================================

/**
 * @brief Get default quantization configuration
 */
NIMCP_EXPORT nimcp_ternary_quant_config_t nimcp_ternary_quant_config_default(void);

/**
 * @brief Quantize FP32 tensor to ternary (in-place output)
 *
 * @param ctx GPU context
 * @param src Source float tensor
 * @param dst Pre-allocated destination ternary tensor
 * @param config Quantization configuration
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_ternary_quantize(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* src,
    nimcp_ternary_tensor_t* dst,
    const nimcp_ternary_quant_config_t* config
);

/**
 * @brief Compute adaptive quantization threshold
 *
 * Uses tensor statistics to find optimal threshold.
 *
 * @param ctx GPU context
 * @param src Source float tensor
 * @param percentile Target percentile for threshold
 * @return Computed threshold
 */
NIMCP_EXPORT float nimcp_ternary_compute_threshold(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* src,
    float percentile
);

//=============================================================================
// Element-wise Operations
//=============================================================================

/**
 * @brief Ternary element-wise multiply: C = A * B (both ternary)
 *
 * Result is also ternary: (-1)*(-1)=1, (-1)*1=-1, 0*x=0
 *
 * @param ctx GPU context
 * @param A First ternary tensor
 * @param B Second ternary tensor (same shape)
 * @param C Output ternary tensor
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_ternary_mul(
    nimcp_gpu_context_t* ctx,
    const nimcp_ternary_tensor_t* A,
    const nimcp_ternary_tensor_t* B,
    nimcp_ternary_tensor_t* C
);

/**
 * @brief Ternary gating: output = gate * input (gate is ternary)
 *
 * @param ctx GPU context
 * @param gate Ternary gate tensor
 * @param input Float input tensor
 * @param output Float output tensor
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_ternary_gate(
    nimcp_gpu_context_t* ctx,
    const nimcp_ternary_tensor_t* gate,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output
);

/**
 * @brief Ternary masking: output = input where mask != 0
 *
 * @param ctx GPU context
 * @param mask Ternary mask tensor
 * @param input Float input tensor
 * @param output Float output tensor
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_ternary_mask(
    nimcp_gpu_context_t* ctx,
    const nimcp_ternary_tensor_t* mask,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Compute sparsity of ternary tensor
 *
 * @param tensor Ternary tensor
 * @return Fraction of zeros (0-1)
 */
NIMCP_EXPORT float nimcp_ternary_compute_sparsity(
    const nimcp_ternary_tensor_t* tensor
);

/**
 * @brief Count non-zeros in ternary tensor
 *
 * @param tensor Ternary tensor
 * @return Number of non-zero elements
 */
NIMCP_EXPORT int64_t nimcp_ternary_count_nonzero(
    const nimcp_ternary_tensor_t* tensor
);

/**
 * @brief Get memory size of ternary tensor
 *
 * @param tensor Ternary tensor
 * @return Memory size in bytes
 */
NIMCP_EXPORT size_t nimcp_ternary_memory_size(
    const nimcp_ternary_tensor_t* tensor
);

/**
 * @brief Compare memory savings vs float tensor
 *
 * @param tensor Ternary tensor
 * @return Compression ratio (float_size / ternary_size)
 */
NIMCP_EXPORT float nimcp_ternary_compression_ratio(
    const nimcp_ternary_tensor_t* tensor
);

/**
 * @brief Copy ternary tensor to host (as int8)
 *
 * @param tensor Ternary tensor
 * @param host_data Pre-allocated host buffer [numel]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_ternary_to_host(
    const nimcp_ternary_tensor_t* tensor,
    int8_t* host_data
);

/**
 * @brief Print ternary tensor info
 */
NIMCP_EXPORT void nimcp_ternary_print_info(
    const nimcp_ternary_tensor_t* tensor
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_TERNARY_GPU_H
