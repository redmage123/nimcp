//=============================================================================
// nimcp_lnn_ternary.h - Ternary Weight Support for Liquid Neural Networks
//=============================================================================
/**
 * @file nimcp_lnn_ternary.h
 * @brief Ternary weight matrices and sparse operations for LNN
 * @version 1.0.0
 * @date 2025-12-31
 *
 * WHAT: Ternary weight representation for LNN recurrent and input matrices
 * WHY:  20x memory savings with minimal accuracy loss for sparse LNNs
 * HOW:  Ternary weights {-1, 0, +1} with specialized sparse matmul
 *
 * BIOLOGICAL BASIS:
 * - Synaptic weights naturally cluster around discrete values
 * - Strong/weak/absent connections map to +1/0/-1
 * - Ternary quantization preserves network topology
 *
 * MEMORY SAVINGS:
 * | Network     | Float32    | Ternary Base-243 | Savings |
 * |-------------|------------|------------------|---------|
 * | 1K neurons  | 4 MB       | 200 KB           | 20x     |
 * | 10K neurons | 400 MB     | 20 MB            | 20x     |
 *
 * USAGE:
 * ```c
 * // Create layer with ternary recurrent weights
 * lnn_layer_config_t config;
 * lnn_layer_config_default(&config);
 * config.use_ternary_weights = true;
 * config.ternary_pack_mode = TERNARY_PACK_BASE243;
 *
 * lnn_layer_t* layer = lnn_layer_create(&config, n_inputs);
 *
 * // Quantize existing float weights to ternary
 * lnn_layer_quantize_weights(layer, 0.3f);  // threshold = 0.3
 *
 * // Forward pass uses ternary sparse matmul automatically
 * lnn_layer_forward(layer, input, output, dt);
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_LNN_TERNARY_H
#define NIMCP_LNN_TERNARY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "lnn/nimcp_lnn_types.h"
#include "utils/ternary/nimcp_ternary.h"
#include "utils/tensor/nimcp_tensor.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Default quantization threshold for ternary conversion */
#define LNN_TERNARY_DEFAULT_THRESHOLD 0.3f

/** Minimum threshold value */
#define LNN_TERNARY_MIN_THRESHOLD 0.01f

/** Maximum threshold value */
#define LNN_TERNARY_MAX_THRESHOLD 0.99f

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Ternary weight configuration for LNN layer
 *
 * WHAT: Configuration for ternary weight representation
 * WHY:  Control ternary quantization behavior
 * HOW:  Threshold-based quantization with optional scaling
 */
typedef struct {
    bool enabled;                    /**< Enable ternary weights */
    ternary_pack_mode_t pack_mode;   /**< Packing mode for storage */
    float threshold;                 /**< Quantization threshold |w| < threshold => 0 */
    bool use_learned_threshold;      /**< Learn threshold during training */
    float scale_factor;              /**< Optional scaling for dequantization */
    bool apply_to_W_in;              /**< Apply ternary to input weights */
    bool apply_to_W_rec;             /**< Apply ternary to recurrent weights */
    bool apply_to_W_tau;             /**< Apply ternary to tau weights */
} lnn_ternary_config_t;

//=============================================================================
// Ternary Weight Matrix Structure
//=============================================================================

/**
 * @brief Ternary sparse weight matrix for LNN
 *
 * WHAT: Compressed ternary weight matrix with CSR-like sparse representation
 * WHY:  Efficient storage and fast sparse matmul for recurrent connections
 * HOW:  Stores only non-zero (+1/-1) connections with implicit zeros
 *
 * SPARSE FORMAT:
 * - row_ptr[i] = start index of row i in col_idx/signs
 * - col_idx[j] = column index of non-zero element j
 * - signs[j]   = sign of element j (+1 or -1)
 * - Zero elements are implicit (not stored)
 */
typedef struct {
    uint32_t magic;                  /**< Validation: LNN_TERNARY_MAGIC */
    uint32_t rows;                   /**< Number of rows (output neurons) */
    uint32_t cols;                   /**< Number of columns (input neurons) */
    uint32_t nnz;                    /**< Number of non-zero elements */

    /* CSR sparse format for non-zeros only */
    uint32_t* row_ptr;               /**< Row pointers [rows + 1] */
    uint32_t* col_idx;               /**< Column indices [nnz] */
    trit_t* signs;                   /**< Signs (+1 or -1) [nnz] */

    /* Dense ternary matrix (alternative representation) */
    trit_matrix_t* dense;            /**< Dense ternary matrix (if not sparse) */

    /* Metadata */
    float threshold;                 /**< Threshold used for quantization */
    float scale_factor;              /**< Dequantization scale factor */
    float sparsity;                  /**< Fraction of zero weights */
    ternary_pack_mode_t pack_mode;   /**< Packing mode for dense storage */
    bool is_sparse;                  /**< Using sparse representation */
} lnn_ternary_matrix_t;

/** Magic number for validation */
#define LNN_TERNARY_MAGIC 0x4C4E5454  /* "LNTT" */

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create ternary matrix from float tensor
 *
 * WHAT: Quantize float weights to ternary values
 * WHY:  Convert trained weights to memory-efficient ternary form
 * HOW:  Threshold-based quantization: |w| < threshold => 0, else sign(w)
 *
 * ALGORITHM:
 * 1. For each weight w:
 *    - If |w| < threshold: trit = 0
 *    - If w >= threshold: trit = +1
 *    - If w <= -threshold: trit = -1
 * 2. Store scale_factor = max(|w|) for dequantization
 * 3. Choose sparse vs dense based on sparsity
 *
 * @param float_weights Source float tensor [rows x cols]
 * @param threshold Quantization threshold
 * @param pack_mode Storage packing mode
 * @param use_sparse Use sparse representation if sparsity > 0.5
 * @return Ternary matrix or NULL on failure
 */
lnn_ternary_matrix_t* lnn_ternary_matrix_from_float(
    const nimcp_tensor_t* float_weights,
    float threshold,
    ternary_pack_mode_t pack_mode,
    bool use_sparse
);

/**
 * @brief Create empty ternary matrix
 *
 * WHAT: Allocate ternary matrix with given dimensions
 * WHY:  Pre-allocate before population
 * HOW:  Allocate storage arrays based on mode
 *
 * @param rows Number of rows
 * @param cols Number of columns
 * @param pack_mode Packing mode
 * @param use_sparse Use sparse representation
 * @return Ternary matrix or NULL on failure
 */
lnn_ternary_matrix_t* lnn_ternary_matrix_create(
    uint32_t rows,
    uint32_t cols,
    ternary_pack_mode_t pack_mode,
    bool use_sparse
);

/**
 * @brief Destroy ternary matrix
 *
 * WHAT: Free all matrix memory
 * WHY:  Clean resource release
 * HOW:  Free sparse arrays or dense matrix
 *
 * @param mat Matrix to destroy
 */
void lnn_ternary_matrix_destroy(lnn_ternary_matrix_t* mat);

/**
 * @brief Clone ternary matrix
 *
 * WHAT: Create deep copy of matrix
 * WHY:  Needed for state checkpointing
 * HOW:  Duplicate all storage
 *
 * @param src Source matrix
 * @return Cloned matrix or NULL on failure
 */
lnn_ternary_matrix_t* lnn_ternary_matrix_clone(const lnn_ternary_matrix_t* src);

//=============================================================================
// Matrix Operations
//=============================================================================

/**
 * @brief Ternary sparse matrix-vector multiply
 *
 * WHAT: Compute y = W_ternary * x efficiently
 * WHY:  Core operation for LNN recurrent computation
 * HOW:  Sparse CSR matmul using sign-only multiplication
 *
 * ALGORITHM (sparse):
 * ```
 * for each row i:
 *     sum = 0
 *     for j in row_ptr[i]..row_ptr[i+1]:
 *         col = col_idx[j]
 *         sum += signs[j] * x[col]  // sign multiply = conditional negate
 *     y[i] = scale_factor * sum
 * ```
 *
 * PERFORMANCE:
 * - O(nnz) for sparse, O(rows * cols) for dense
 * - Sign multiply is branchless add/subtract
 * - Cache-friendly CSR access pattern
 *
 * @param mat Ternary weight matrix [rows x cols]
 * @param x Input vector [cols]
 * @param y Output vector [rows] (pre-allocated)
 * @return 0 on success, negative on error
 */
int lnn_ternary_matmul(
    const lnn_ternary_matrix_t* mat,
    const nimcp_tensor_t* x,
    nimcp_tensor_t* y
);

/**
 * @brief Ternary matrix-vector multiply returning integer sums
 *
 * WHAT: Compute integer sums without scaling
 * WHY:  Useful for exact ternary arithmetic
 * HOW:  Accumulate signed counts
 *
 * @param mat Ternary weight matrix
 * @param x Input vector
 * @param out Output integer array (pre-allocated)
 * @return 0 on success, negative on error
 */
int lnn_ternary_matmul_int(
    const lnn_ternary_matrix_t* mat,
    const nimcp_tensor_t* x,
    int32_t* out
);

/**
 * @brief Convert ternary matrix back to float tensor
 *
 * WHAT: Dequantize ternary to float representation
 * WHY:  Needed for gradient computation and model export
 * HOW:  Multiply signs by scale_factor
 *
 * @param mat Ternary matrix
 * @return Float tensor or NULL on failure
 */
nimcp_tensor_t* lnn_ternary_matrix_to_float(const lnn_ternary_matrix_t* mat);

/**
 * @brief Get element at (row, col)
 *
 * WHAT: Access individual matrix element
 * WHY:  Element-wise inspection
 * HOW:  Binary search in sparse, direct access in dense
 *
 * @param mat Ternary matrix
 * @param row Row index
 * @param col Column index
 * @return Trit value at (row, col)
 */
trit_t lnn_ternary_matrix_get(
    const lnn_ternary_matrix_t* mat,
    uint32_t row,
    uint32_t col
);

/**
 * @brief Set element at (row, col)
 *
 * WHAT: Modify individual matrix element
 * WHY:  Weight updates during training
 * HOW:  Update sparse structure or dense matrix
 *
 * NOTE: For sparse matrices, setting a zero element to non-zero
 *       or vice versa may require structure rebuilding.
 *
 * @param mat Ternary matrix
 * @param row Row index
 * @param col Column index
 * @param value New trit value
 * @return 0 on success, negative on error
 */
int lnn_ternary_matrix_set(
    lnn_ternary_matrix_t* mat,
    uint32_t row,
    uint32_t col,
    trit_t value
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get matrix statistics
 *
 * WHAT: Compute statistics about ternary matrix
 * WHY:  Monitor quantization quality
 * HOW:  Count positive/negative/zero elements
 *
 * @param mat Ternary matrix
 * @param n_positive Output count of +1 elements
 * @param n_zero Output count of 0 elements
 * @param n_negative Output count of -1 elements
 */
void lnn_ternary_matrix_stats(
    const lnn_ternary_matrix_t* mat,
    uint32_t* n_positive,
    uint32_t* n_zero,
    uint32_t* n_negative
);

/**
 * @brief Compute effective sparsity
 *
 * WHAT: Calculate fraction of zero elements
 * WHY:  Measure compression ratio
 * HOW:  Count zeros / total elements
 *
 * @param mat Ternary matrix
 * @return Sparsity in [0, 1]
 */
float lnn_ternary_matrix_sparsity(const lnn_ternary_matrix_t* mat);

//=============================================================================
// Configuration Helpers
//=============================================================================

/**
 * @brief Get default ternary configuration
 *
 * WHAT: Initialize config with sensible defaults
 * WHY:  Convenient starting point
 * HOW:  Set threshold=0.3, enable for W_rec only
 *
 * @param config Configuration to initialize
 */
void lnn_ternary_config_default(lnn_ternary_config_t* config);

/**
 * @brief Validate ternary configuration
 *
 * WHAT: Check configuration for valid values
 * WHY:  Catch errors early
 * HOW:  Validate threshold range, etc.
 *
 * @param config Configuration to validate
 * @return 0 if valid, negative on error
 */
int lnn_ternary_config_validate(const lnn_ternary_config_t* config);

//=============================================================================
// Layer Integration
//=============================================================================

/**
 * @brief Quantize layer weights to ternary
 *
 * WHAT: Convert layer's float weights to ternary representation
 * WHY:  Reduce memory after training
 * HOW:  Apply threshold quantization to configured weight matrices
 *
 * @param layer LNN layer
 * @param config Ternary configuration
 * @return 0 on success, negative on error
 */
int lnn_layer_quantize_to_ternary(
    lnn_layer_t* layer,
    const lnn_ternary_config_t* config
);

/**
 * @brief Check if layer uses ternary weights
 *
 * WHAT: Query layer's ternary status
 * WHY:  Conditional code paths
 * HOW:  Check layer configuration flag
 *
 * @param layer LNN layer
 * @return true if ternary weights enabled
 */
bool lnn_layer_is_ternary(const lnn_layer_t* layer);

/**
 * @brief Get layer's ternary recurrent matrix
 *
 * WHAT: Access the ternary W_rec matrix
 * WHY:  Direct access for inspection/modification
 * HOW:  Return pointer to layer's ternary structure
 *
 * @param layer LNN layer
 * @return Ternary matrix or NULL if not ternary
 */
const lnn_ternary_matrix_t* lnn_layer_get_ternary_W_rec(const lnn_layer_t* layer);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LNN_TERNARY_H */
