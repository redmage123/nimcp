#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_svd_simple.c - Simple SVD Implementation for TT-SVD
//=============================================================================
/**
 * @file nimcp_svd_simple.c
 * @brief Lightweight SVD implementation for tensor decomposition
 *
 * WHAT: Singular Value Decomposition via power iteration + Jacobi
 * WHY:  Needed for optimal TT-SVD compression in MPS
 * HOW:  Power iteration for large matrices, Jacobi for small ones
 *
 * @author NIMCP Development Team
 * @date 2025-11-17
 */

#include "utils/tensor_networks/nimcp_svd_simple.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <float.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(svd_simple)

//=============================================================================
// INTERNAL HELPERS
//=============================================================================

/**
 * WHAT: Compute Frobenius norm of matrix
 * WHY:  Needed for normalization and error computation
 * HOW:  √(Σᵢⱼ aᵢⱼ²)
 *
 * NUMERICAL STABILITY:
 * Uses double accumulator to prevent overflow for large matrices.
 * For float32 data, accumulating in float32 can overflow when:
 *   - Matrix has many elements (n*m > ~10^7 with typical values)
 *   - Elements have magnitude ~1000, squares ~10^6, sum overflows float32 max (~3.4e38)
 * Using double accumulator (max ~1.8e308) safely handles matrices up to ~10^300 elements.
 */
static float frobenius_norm(const float* A, uint32_t m, uint32_t n) {
    double sum = 0.0;  /* Use double accumulator to prevent overflow */
    for (uint32_t i = 0; i < (uint32_t)(m * n); i++) {
        double val = (double)A[i];
        sum += val * val;
    }
    return (float)sqrt(sum);
}

/**
 * WHAT: Normalize vector to unit length
 * WHY:  Power iteration requires normalized eigenvectors
 * HOW:  v ← v / ||v||₂
 */
static void normalize_vector(float* v, uint32_t n) {
    float norm = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        norm += v[i] * v[i];
    }
    norm = sqrtf(norm);

    if (norm > 1e-10f) {
        for (uint32_t i = 0; i < n; i++) {
            v[i] /= norm;
        }
    }
}

/**
 * WHAT: Matrix-vector multiply: y = A·x
 * WHY:  Core operation for power iteration
 * HOW:  Standard matrix-vector product
 */
static void matvec_multiply(const float* A, const float* x, float* y,
                           uint32_t m, uint32_t n) {
    for (uint32_t i = 0; i < m; i++) {
        float sum = 0.0f;
        for (uint32_t j = 0; j < n; j++) {
            sum += A[i * n + j] * x[j];
        }
        y[i] = sum;
    }
}

/**
 * WHAT: Transpose matrix-vector multiply: y = Aᵀ·x
 * WHY:  Needed for computing AᵀA eigenvectors
 * HOW:  Transpose indexing during multiply
 */
static void matvec_transpose_multiply(const float* A, const float* x, float* y,
                                      uint32_t m, uint32_t n) {
    for (uint32_t j = 0; j < n; j++) {
        float sum = 0.0f;
        for (uint32_t i = 0; i < m; i++) {
            sum += A[i * n + j] * x[i];
        }
        y[j] = sum;
    }
}

/**
 * WHAT: Compute dominant singular triplet via power iteration
 * WHY:  Find largest singular value and vectors
 * HOW:  Iterate v ← AᵀAv, u ← Av/||Av||, σ ← ||Av||
 */
static bool compute_dominant_singular_triplet(
    const float* A, uint32_t m, uint32_t n,
    float* u_out, float* sigma_out, float* v_out,
    uint32_t max_iterations)
{
    // WHAT: Allocate workspace
    // WHY:  Need temporary vectors for iteration
    float* v = nimcp_calloc(n, sizeof(float));
    float* Av = nimcp_calloc(m, sizeof(float));
    float* AtAv = nimcp_calloc(n, sizeof(float));

    if (!v || !Av || !AtAv) {
        nimcp_free(v);
        nimcp_free(Av);
        nimcp_free(AtAv);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "compute_dominant_singular_triplet: required parameter is NULL (v, Av, AtAv)");
        return false;
    }

    // WHAT: Initialize v randomly (using simple pattern)
    // WHY:  Power iteration needs non-zero start
    for (uint32_t i = 0; i < n; i++) {
        v[i] = sinf((float)i * 0.1f);
    }
    normalize_vector(v, n);

    // WHAT: Power iteration for AᵀA eigenvector
    // WHY:  Dominant eigenvector of AᵀA is right singular vector
    float prev_sigma = 0.0f;
    for (uint32_t iter = 0; iter < max_iterations; iter++) {
        // Compute Av
        matvec_multiply(A, v, Av, m, n);

        // Compute AᵀAv
        matvec_transpose_multiply(A, Av, AtAv, m, n);

        // Normalize: v ← AᵀAv / ||AᵀAv||
        normalize_vector(AtAv, n);
        memcpy(v, AtAv, n * sizeof(float));

        // Compute σ = ||Av||
        float sigma = 0.0f;
        matvec_multiply(A, v, Av, m, n);
        for (uint32_t i = 0; i < m; i++) {
            sigma += Av[i] * Av[i];
        }
        sigma = sqrtf(sigma);

        // Check convergence - fixed to avoid division issues
        // Use absolute tolerance if sigma is very small
        float tol = (sigma > 1e-6f) ? (1e-6f * sigma) : 1e-9f;
        if (iter > 0 && fabsf(sigma - prev_sigma) < tol) {
            break;
        }

        // Early exit if sigma becomes negligible (matrix is rank deficient)
        if (sigma < 1e-10f) {
            break;
        }

        prev_sigma = sigma;
    }

    // WHAT: Compute final u and σ
    // WHY:  u = Av / ||Av||, σ = ||Av||
    matvec_multiply(A, v, Av, m, n);
    float sigma = 0.0f;
    for (uint32_t i = 0; i < m; i++) {
        sigma += Av[i] * Av[i];
    }
    sigma = sqrtf(sigma);

    if (sigma > 1e-10f) {
        for (uint32_t i = 0; i < m; i++) {
            u_out[i] = Av[i] / sigma;
        }
    } else {
        memset(u_out, 0, m * sizeof(float));
    }

    *sigma_out = sigma;
    memcpy(v_out, v, n * sizeof(float));

    nimcp_free(v);
    nimcp_free(Av);
    nimcp_free(AtAv);
    return true;
}

/**
 * WHAT: Deflate matrix: A ← A - σ u vᵀ
 * WHY:  Remove found singular component to find next
 * HOW:  Rank-1 update
 */
static void deflate_matrix(float* A, uint32_t m, uint32_t n,
                          float sigma, const float* u, const float* v) {
    for (uint32_t i = 0; i < m; i++) {
        for (uint32_t j = 0; j < n; j++) {
            A[i * n + j] -= sigma * u[i] * v[j];
        }
    }
}

//=============================================================================
// PUBLIC API
//=============================================================================

/**
 * WHAT: Compute truncated SVD via power iteration + deflation
 * WHY:  Core decomposition for TT-SVD compression
 * HOW:  Iteratively find singular triplets and deflate
 */
svd_result_t svd_compute(
    const float* A,
    uint32_t m,
    uint32_t n,
    uint32_t max_rank,
    float tolerance)
{
    svd_result_t result = {0};

    // WHAT: Validate inputs
    // WHY:  Guard clause - prevent invalid computation
    if (!A || m == 0 || n == 0) {
        return result;
    }

    // WHAT: Determine actual rank
    // WHY:  Limit to matrix dimensions and requested rank
    uint32_t rank = (max_rank == 0) ? (m < n ? m : n) : max_rank;
    if (rank > m) rank = m;
    if (rank > n) rank = n;

    // WHAT: Allocate result arrays
    // WHY:  Store U[m×rank], S[rank], Vt[rank×n]
    result.U = nimcp_calloc(m * rank, sizeof(float));
    result.S = nimcp_calloc(rank, sizeof(float));
    result.Vt = nimcp_calloc(rank * n, sizeof(float));

    if (!result.U || !result.S || !result.Vt) {
        svd_free(&result);
        return result;
    }

    // WHAT: Copy A for deflation
    // WHY:  Don't modify original matrix
    float* A_work = nimcp_malloc(m * n * sizeof(float));
    if (!A_work) {
        svd_free(&result);
        return result;
    }
    memcpy(A_work, A, m * n * sizeof(float));

    // WHAT: Compute Frobenius norm for tolerance
    // WHY:  Relative truncation threshold
    float A_norm = frobenius_norm(A, m, n);
    float abs_tolerance = tolerance * A_norm;

    // WHAT: Iteratively find singular triplets
    // WHY:  Each iteration extracts next largest component
    uint32_t actual_rank = 0;
    for (uint32_t k = 0; k < rank; k++) {
        float* u = result.U + k * m;
        float* v = result.Vt + k * n;
        float sigma;

        // Find dominant singular triplet of deflated matrix
        if (!compute_dominant_singular_triplet(A_work, m, n, u, &sigma, v, 100)) {
            break;
        }

        result.S[k] = sigma;
        actual_rank++;

        // WHAT: Check truncation tolerance
        // WHY:  Adaptive rank - stop if singular value too small
        if (sigma < abs_tolerance) {
            break;
        }

        // Deflate matrix for next iteration
        deflate_matrix(A_work, m, n, sigma, u, v);
    }

    nimcp_free(A_work);

    // WHAT: Store final dimensions
    // WHY:  Return actual rank found
    result.m = m;
    result.n = n;
    result.rank = actual_rank;

    // WHAT: Compute reconstruction error
    // WHY:  Quantify approximation quality
    if (actual_rank > 0) {
        float* A_recon = nimcp_malloc(m * n * sizeof(float));
        if (A_recon) {
            svd_reconstruct(&result, A_recon);

            float error_sum = 0.0f;
            for (uint32_t i = 0; i < m * n; i++) {
                float diff = A[i] - A_recon[i];
                error_sum += diff * diff;
            }
            result.reconstruction_error = sqrtf(error_sum) / A_norm;

            nimcp_free(A_recon);
        }
    }

    return result;
}

/**
 * WHAT: Free SVD result structure
 * WHY:  Prevent memory leaks
 * HOW:  Free each component
 */
void svd_free(svd_result_t* svd) {
    if (!svd) return;

    nimcp_free(svd->U);
    nimcp_free(svd->S);
    nimcp_free(svd->Vt);

    memset(svd, 0, sizeof(svd_result_t));
}

/**
 * WHAT: Reconstruct matrix from SVD: A = U Σ Vᵀ
 * WHY:  Validate decomposition quality
 * HOW:  Matrix multiplication through singular values
 */
bool svd_reconstruct(const svd_result_t* svd, float* A) {
    if (!svd || !A || !svd->U || !svd->S || !svd->Vt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "svd_reconstruct: invalid parameters");

            return false;
    }

    // WHAT: Initialize output to zero
    // WHY:  Accumulate rank-1 updates
    memset(A, 0, svd->m * svd->n * sizeof(float));

    // WHAT: A = Σₖ σₖ uₖ vₖᵀ
    // WHY:  Sum of rank-1 matrices
    for (uint32_t k = 0; k < svd->rank; k++) {
        float sigma = svd->S[k];
        const float* u = svd->U + k * svd->m;
        const float* v = svd->Vt + k * svd->n;

        for (uint32_t i = 0; i < svd->m; i++) {
            for (uint32_t j = 0; j < svd->n; j++) {
                A[i * svd->n + j] += sigma * u[i] * v[j];
            }
        }
    }

    return true;
}

/**
 * WHAT: Compute reconstruction error ||A_original - A_svd||_F / ||A_original||_F
 * WHY:  Quantify approximation quality
 * HOW:  Frobenius norm of difference
 */
float svd_compute_error(const svd_result_t* svd, const float* A_original) {
    if (!svd || !A_original) {
        return 1.0f;  // Maximum error
    }

    // WHAT: Reconstruct matrix
    // WHY:  Need full matrix to compute error
    float* A_recon = nimcp_malloc(svd->m * svd->n * sizeof(float));
    if (!A_recon) {
        return 1.0f;
    }

    if (!svd_reconstruct(svd, A_recon)) {
        nimcp_free(A_recon);
        return 1.0f;
    }

    // WHAT: Compute ||A_original - A_recon||_F
    // WHY:  Measure reconstruction accuracy
    float error_sum = 0.0f;
    for (uint32_t i = 0; i < svd->m * svd->n; i++) {
        float diff = A_original[i] - A_recon[i];
        error_sum += diff * diff;
    }

    // WHAT: Normalize by ||A_original||_F
    // WHY:  Relative error is scale-invariant
    float A_norm = frobenius_norm(A_original, svd->m, svd->n);
    float rel_error = (A_norm > 1e-10f) ? sqrtf(error_sum) / A_norm : 0.0f;

    nimcp_free(A_recon);
    return rel_error;
}
