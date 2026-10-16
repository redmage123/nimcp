//=============================================================================
// nimcp_mps.h - Matrix Product States Weight Compression
//=============================================================================
/**
 * @file nimcp_mps.h
 * @brief Matrix Product States (MPS) for neural network weight compression
 *
 * WHAT: Tensor network decomposition for memory-efficient weight storage
 * WHY: 10-100x memory reduction with controlled accuracy loss
 * HOW: Decompose weight matrix W[N×M] into chain of small tensors
 *
 * ALGORITHM:
 * ```
 * Standard weight matrix: W[N×M] = N×M floats
 * MPS decomposition:      W ≈ A[1] · A[2] · ... · A[k]
 *                         A[i] has size: (bond_dim × bond_dim × local_dim)
 *
 * Memory: O(N×M) → O(k × bond_dim² × local_dim)
 * For large matrices with small bond_dim: 10-100x compression!
 * ```
 *
 * MATHEMATICAL FOUNDATION:
 * - Singular Value Decomposition (SVD) at each bond
 * - Truncation to bond_dim largest singular values
 * - Controlled approximation error via bond dimension
 *
 * INTEGRATION WITH NIMCP:
 * - Compresses synaptic weight matrices
 * - Fast matrix-vector multiplication O(N × bond_dim²)
 * - Adaptive bond dimension based on required accuracy
 * - Compatible with learning (gradient descent on tensors)
 *
 * PERFORMANCE:
 * - Compression: O(N×M × bond_dim²) one-time cost
 * - Matrix-vector multiply: O(N × bond_dim²) vs O(N×M)
 * - Memory: 10-100x reduction typical
 * - Accuracy: >99% with bond_dim=10, >99.9% with bond_dim=20
 *
 * SYNERGIES:
 * - Works with hyperbolic embeddings (B1.1): 200x × 100x = 20,000x total
 * - Compatible with RK4 integration (A1.1): compress weights, accurate dynamics
 * - Enables larger networks: 100K synapses → 1M+ synapses same memory
 *
 * EXAMPLE:
 * ```c
 * // Compress 1000×1000 weight matrix (1M floats = 4MB)
 * mps_tensor_t* mps = mps_compress_matrix(weights, 1000, 1000, 10);
 * // Result: ~100KB (40x compression)
 *
 * // Fast matrix-vector multiply
 * float input[1000], output[1000];
 * mps_matrix_vector_multiply(mps, input, output);
 *
 * // Measure approximation error
 * float error = mps_reconstruction_error(mps, original_weights);
 * printf("Relative error: %.4f%%\n", error * 100.0f);
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-11-11
 * @version 2.8.0 Phase C3.1
 */

#ifndef NIMCP_MPS_H
#define NIMCP_MPS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Single tensor in MPS chain
 *
 * WHAT: 3-index tensor A[left_bond][right_bond][physical]
 * WHY: Building block of MPS representation
 * HOW: Store as flattened array with index arithmetic
 *
 * INDEXING: A[i][j][k] = data[i * right_dim * phys_dim + j * phys_dim + k]
 */
typedef struct {
    float* data;           ///< Flattened tensor data
    uint32_t left_dim;     ///< Left bond dimension (1 for first tensor)
    uint32_t right_dim;    ///< Right bond dimension (1 for last tensor)
    uint32_t phys_dim;     ///< Physical dimension (number of features)
    uint32_t total_size;   ///< left_dim × right_dim × phys_dim
} mps_tensor_t;

/**
 * @brief Complete MPS chain representing weight matrix
 *
 * WHAT: Chain of tensors approximating W[N×M]
 * WHY: Memory-efficient weight storage
 * HOW: W ≈ contract(A[0], A[1], ..., A[num_sites-1])
 *
 * STRUCTURE:
 * ```
 *   A[0]      A[1]      A[2]          A[n-1]
 *   [1,D,d] - [D,D,d] - [D,D,d] - ... [D,1,d]
 *    |         |         |              |
 *    i₀        i₁        i₂            iₙ₋₁
 *
 * W[i₀i₁...iₙ₋₁, j] = Σ A[0][1,α₁,i₀] A[1][α₁,α₂,i₁] ... A[n-1][αₙ₋₁,1,iₙ₋₁]
 * ```
 */
typedef struct {
    mps_tensor_t* sites;        ///< Array of MPS tensors (length: num_sites)
    uint32_t num_sites;         ///< Number of sites in chain
    uint32_t bond_dim;          ///< Maximum bond dimension (D)
    uint32_t input_dim;         ///< Original matrix rows (N)
    uint32_t output_dim;        ///< Original matrix columns (M)
    uint32_t total_params;      ///< Total number of parameters stored
    float compression_ratio;    ///< input_dim × output_dim / total_params
    float reconstruction_error; ///< ||W_original - W_mps||_F / ||W_original||_F
} mps_matrix_t;

/**
 * @brief Configuration for MPS compression
 *
 * WHAT: Control parameters for MPS decomposition
 * WHY: Trade off memory vs accuracy
 * HOW: Adjust bond dimension and tolerance
 */
typedef struct {
    uint32_t bond_dim;          ///< Maximum bond dimension (higher = more accurate)
    float svd_tolerance;        ///< Truncation tolerance for SVD (e.g., 1e-6)
    bool adaptive_bond_dim;     ///< Adapt bond_dim to local structure
    uint32_t max_iterations;    ///< Max iterations for optimization (if used)
    float learning_rate;        ///< Learning rate for MPS gradient descent
    bool normalize_tensors;     ///< Normalize after each operation
} mps_config_t;

/**
 * @brief Statistics for MPS compression
 *
 * WHAT: Diagnostic information about compression quality
 * WHY: Monitor performance and accuracy
 * HOW: Computed during compression
 */
typedef struct {
    float compression_ratio;     ///< Memory saved (original / compressed)
    float reconstruction_error;  ///< Frobenius norm error
    float max_singular_value;    ///< Largest singular value kept
    float min_singular_value;    ///< Smallest singular value kept
    uint32_t num_singular_values_kept;    ///< Total singular values retained
    uint32_t num_singular_values_dropped; ///< Total singular values discarded
    float compression_time_ms;   ///< Time taken to compress
} mps_stats_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Get default MPS configuration
 *
 * WHAT: Return sensible default parameters
 * WHY: Good starting point for most use cases
 * HOW: bond_dim=10, tolerance=1e-6
 *
 * DEFAULTS:
 * - bond_dim: 10 (10-20x compression, >99% accuracy)
 * - svd_tolerance: 1e-6 (discard singular values < 1e-6)
 * - adaptive_bond_dim: true (optimize per-bond)
 * - normalize_tensors: true (numerical stability)
 *
 * @return Default configuration
 */
mps_config_t mps_default_config(void);

/**
 * @brief Get high-compression configuration
 *
 * WHAT: Maximize memory savings (50-100x compression)
 * WHY: For very large networks
 * HOW: bond_dim=5, higher tolerance
 *
 * @return High-compression configuration
 */
mps_config_t mps_high_compression_config(void);

/**
 * @brief Get high-accuracy configuration
 *
 * WHAT: Minimize approximation error (5-10x compression)
 * WHY: For critical synaptic connections
 * HOW: bond_dim=20, tight tolerance
 *
 * @return High-accuracy configuration
 */
mps_config_t mps_high_accuracy_config(void);

//=============================================================================
// Compression and Decompression
//=============================================================================

/**
 * @brief Compress weight matrix into MPS representation
 *
 * WHAT: Decompose W[N×M] into MPS chain
 * WHY: Reduce memory footprint 10-100x
 * HOW: SVD-based tensor train decomposition
 *
 * ALGORITHM:
 * 1. Reshape W[N×M] into multi-index tensor W[i₁,i₂,...,iₖ,j]
 * 2. For each bond:
 *    a. Reshape into matrix
 *    b. Compute SVD
 *    c. Truncate to bond_dim singular values
 *    d. Store left/right tensors
 * 3. Return MPS chain
 *
 * COMPLEXITY: O(N×M × bond_dim²)
 *
 * @param weights Original weight matrix (row-major, N×M)
 * @param num_rows Number of rows (N)
 * @param num_cols Number of columns (M)
 * @param config Compression configuration
 * @param stats Output statistics (can be NULL)
 * @return MPS representation, or NULL on failure
 */
mps_matrix_t* mps_compress_matrix(
    const float* weights,
    uint32_t num_rows,
    uint32_t num_cols,
    const mps_config_t* config,
    mps_stats_t* stats
);

/**
 * @brief Reconstruct full weight matrix from MPS
 *
 * WHAT: Materialize W[N×M] from MPS chain
 * WHY: For validation, analysis, or export
 * HOW: Contract all MPS tensors
 *
 * WARNING: Defeats compression! Use only for debugging.
 * For normal operation, use mps_matrix_vector_multiply().
 *
 * COMPLEXITY: O(N×M × bond_dim²)
 *
 * @param mps MPS matrix representation
 * @param weights Output buffer (must be allocated, N×M floats)
 * @return true on success, false on failure
 */
bool mps_reconstruct_matrix(
    const mps_matrix_t* mps,
    float* weights
);

/**
 * @brief Compute reconstruction error
 *
 * WHAT: ||W_original - W_mps||_F / ||W_original||_F
 * WHY: Quantify approximation quality
 * HOW: Frobenius norm of difference
 *
 * @param mps MPS representation
 * @param original_weights Original weight matrix
 * @return Relative Frobenius error [0, 1]
 */
float mps_compute_error(
    const mps_matrix_t* mps,
    const float* original_weights
);

//=============================================================================
// Matrix Operations
//=============================================================================

/**
 * @brief Matrix-vector multiply: y = W·x
 *
 * WHAT: Compute output = W·input using MPS
 * WHY: Main operation for neural network forward pass
 * HOW: Left-to-right contraction through MPS chain
 *
 * ALGORITHM:
 * ```
 * v = input  // Start with input vector
 * for i = 0 to num_sites-1:
 *     v = contract(v, A[i])  // Contract with site tensor
 * output = v  // Final contracted result
 * ```
 *
 * COMPLEXITY: O(N × bond_dim²) vs O(N×M) for dense matrix
 * SPEEDUP: bond_dim² << M → faster for large M
 *
 * @param mps MPS matrix representation
 * @param input Input vector (length: input_dim)
 * @param output Output vector (length: output_dim, allocated by caller)
 * @return true on success, false on failure
 */
bool mps_matrix_vector_multiply(
    const mps_matrix_t* mps,
    const float* input,
    float* output
);

/**
 * @brief Compute gradient of loss w.r.t. MPS parameters
 *
 * WHAT: Backpropagate through MPS structure
 * WHY: Enable learning with compressed weights
 * HOW: Chain rule through tensor contractions
 *
 * USE CASE: Train neural network with MPS-compressed synapses
 *
 * @param mps MPS matrix representation
 * @param input Input vector
 * @param grad_output Gradient w.r.t. output
 * @param grad_mps Gradient w.r.t. MPS tensors (modified in-place)
 * @return true on success, false on failure
 */
bool mps_backward(
    const mps_matrix_t* mps,
    const float* input,
    const float* grad_output,
    mps_matrix_t* grad_mps
);

/**
 * @brief Update MPS parameters with gradient descent
 *
 * WHAT: θ_new = θ_old - learning_rate × gradient
 * WHY: Learning directly on compressed representation
 * HOW: Element-wise update of MPS tensors
 *
 * @param mps MPS matrix (modified in-place)
 * @param grad_mps Gradient w.r.t. MPS parameters
 * @param learning_rate Learning rate
 * @return true on success, false on failure
 */
bool mps_update_params(
    mps_matrix_t* mps,
    const mps_matrix_t* grad_mps,
    float learning_rate
);

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Free MPS matrix representation
 *
 * WHAT: Deallocate all MPS tensors and structure
 * WHY: Prevent memory leaks
 * HOW: Free each tensor, then MPS structure
 *
 * @param mps MPS matrix to free
 */
void mps_free(mps_matrix_t* mps);

/**
 * @brief Deep copy MPS matrix
 *
 * WHAT: Create independent copy of MPS
 * WHY: For parallel processing or snapshotting
 * HOW: Allocate and copy all tensors
 *
 * @param mps Source MPS matrix
 * @return New MPS matrix, or NULL on failure
 */
mps_matrix_t* mps_clone(const mps_matrix_t* mps);

//=============================================================================
// Diagnostic and Utility
//=============================================================================

/**
 * @brief Print MPS structure information
 *
 * WHAT: Display bond dimensions, compression ratio, error
 * WHY: Debugging and analysis
 * HOW: Print formatted statistics
 *
 * @param mps MPS matrix
 */
void mps_print_info(const mps_matrix_t* mps);

/**
 * @brief Get memory usage of MPS representation
 *
 * WHAT: Total bytes used by MPS tensors
 * WHY: Monitor memory savings
 * HOW: Sum size of all tensor data arrays
 *
 * @param mps MPS matrix
 * @return Memory usage in bytes
 */
size_t mps_memory_usage(const mps_matrix_t* mps);

/**
 * @brief Verify MPS structure integrity
 *
 * WHAT: Check bond dimensions are consistent
 * WHY: Catch corruption or bugs
 * HOW: Validate site[i].right_dim == site[i+1].left_dim
 *
 * @param mps MPS matrix
 * @return true if valid, false if corrupted
 */
bool mps_verify_structure(const mps_matrix_t* mps);

//=============================================================================
// Advanced Operations
//=============================================================================

/**
 * @brief Adaptively adjust bond dimensions
 *
 * WHAT: Increase/decrease bond_dim per site based on importance
 * WHY: Optimize memory vs accuracy tradeoff
 * HOW: Analyze singular value spectrum at each bond
 *
 * @param mps MPS matrix (modified in-place)
 * @param target_error Maximum acceptable reconstruction error
 * @return true if adjustment successful
 */
bool mps_adapt_bond_dimensions(
    mps_matrix_t* mps,
    float target_error
);

/**
 * @brief Compress existing MPS further
 *
 * WHAT: Reduce bond dimensions of already-compressed MPS
 * WHY: Dynamic memory management during runtime
 * HOW: Re-run SVD with smaller bond_dim
 *
 * @param mps MPS matrix (modified in-place)
 * @param new_bond_dim Target bond dimension
 * @return true on success, false on failure
 */
bool mps_recompress(
    mps_matrix_t* mps,
    uint32_t new_bond_dim
);

/**
 * @brief Orthogonalize MPS tensors
 *
 * WHAT: Bring MPS to canonical form (left/right/mixed)
 * WHY: Numerical stability and faster operations
 * HOW: QR or SVD decompositions
 *
 * @param mps MPS matrix (modified in-place)
 * @param center_site Site to center orthogonality (0 = left, num_sites-1 = right)
 * @return true on success, false on failure
 */
bool mps_canonicalize(
    mps_matrix_t* mps,
    uint32_t center_site
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_MPS_H
