//=============================================================================
// nimcp_svd_simple.h - Simple SVD Implementation for TT-SVD
//=============================================================================
/**
 * @file nimcp_svd_simple.h
 * @brief Lightweight SVD implementation for tensor decomposition
 *
 * WHAT: Singular Value Decomposition (SVD) utilities for MPS/TT compression
 * WHY:  TT-SVD algorithm requires SVD at each bond for optimal compression
 * HOW:  Power iteration method for dominant singular values + Jacobi for small matrices
 *
 * ALGORITHM:
 * ```
 * SVD: A = U Σ Vᵀ
 * Power iteration finds largest singular values/vectors iteratively
 * Deflation removes found components to find next largest
 * ```
 *
 * PERFORMANCE:
 * - Small matrices (<100×100): O(n³) Jacobi SVD
 * - Large matrices: O(k×n²) truncated power iteration (k=rank)
 * - Accuracy: ~1e-6 relative error for well-conditioned matrices
 *
 * USAGE:
 * ```c
 * float* A = ...; // m×n matrix
 * svd_result_t svd = svd_compute(A, m, n, max_rank, tolerance);
 * // svd.U[m×k], svd.S[k], svd.Vt[k×n]
 * svd_free(&svd);
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-11-17
 */

#ifndef NIMCP_SVD_SIMPLE_H
#define NIMCP_SVD_SIMPLE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief SVD result structure
 *
 * WHAT: Decomposition A = U Σ Vᵀ
 * WHY:  Store all SVD components
 * HOW:  U[m×k], S[k], Vt[k×n]
 */
typedef struct {
    float* U;              ///< Left singular vectors [m × rank]
    float* S;              ///< Singular values [rank] (descending order)
    float* Vt;             ///< Right singular vectors transposed [rank × n]
    uint32_t m;            ///< Number of rows in original matrix
    uint32_t n;            ///< Number of columns in original matrix
    uint32_t rank;         ///< Number of singular values kept
    float reconstruction_error; ///< ||A - USVᵀ||_F / ||A||_F
} svd_result_t;

//=============================================================================
// SVD Computation
//=============================================================================

/**
 * @brief Compute truncated SVD of matrix
 *
 * WHAT: Decompose A[m×n] ≈ U[m×k] Σ[k×k] Vᵀ[k×n]
 * WHY:  Core operation for TT-SVD compression
 * HOW:  Power iteration for dominant components + deflation
 *
 * ALGORITHM:
 * 1. For each singular value i = 1 to max_rank:
 *    a. Power iteration: vᵢ = dominant eigenvector of AᵀA
 *    b. Compute uᵢ = Avᵢ / ||Avᵢ||
 *    c. Compute σᵢ = ||Avᵢ||
 *    d. If σᵢ < tolerance × σ₁: stop (adaptive rank)
 *    e. Deflate: A ← A - σᵢ uᵢ vᵢᵀ
 * 2. Return U, Σ, Vᵀ
 *
 * @param A Input matrix [m × n] (row-major, not modified)
 * @param m Number of rows
 * @param n Number of columns
 * @param max_rank Maximum number of singular values (0 = full rank)
 * @param tolerance Truncation tolerance (0 = keep all)
 * @return SVD result (must be freed with svd_free)
 */
svd_result_t svd_compute(
    const float* A,
    uint32_t m,
    uint32_t n,
    uint32_t max_rank,
    float tolerance
);

/**
 * @brief Free SVD result
 *
 * @param svd SVD result to free
 */
void svd_free(svd_result_t* svd);

/**
 * @brief Reconstruct matrix from SVD: A = U Σ Vᵀ
 *
 * @param svd SVD components
 * @param A Output matrix [m × n] (allocated by caller)
 * @return true on success
 */
bool svd_reconstruct(const svd_result_t* svd, float* A);

/**
 * @brief Compute reconstruction error
 *
 * @param svd SVD components
 * @param A_original Original matrix [m × n]
 * @return Relative Frobenius error [0, 1]
 */
float svd_compute_error(const svd_result_t* svd, const float* A_original);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SVD_SIMPLE_H
