#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_svd_lapack.c - LAPACK-based SVD for TT-SVD Tensor Decomposition
//=============================================================================
/**
 * @file nimcp_svd_lapack.c
 * @brief Professional SVD implementation using LAPACK for tensor decomposition
 *
 * WHAT: Singular Value Decomposition using LAPACK's dgesdd/sgesdd
 * WHY:  TT-SVD algorithm requires robust, accurate SVD for optimal compression
 * HOW:  FORTRAN interface to LAPACK with proper calling conventions
 *
 * ALGORITHM:
 * ```
 * SVD: A = U Σ Vᵀ
 * LAPACK dgesdd (divide-and-conquer) for optimal performance
 * Faster than dgesvd for most cases
 * ```
 *
 * LAPACK FUNCTIONS USED:
 * - sgesdd_: Single precision SVD (divide-and-conquer)
 * - sgemm_:  Matrix multiplication for reconstruction
 *
 * FORTRAN CALLING CONVENTION:
 * - All arguments passed by pointer
 * - Column-major matrix layout
 * - Trailing underscores in function names
 *
 * @author NIMCP Development Team
 * @date 2025-11-18
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
#include <stdio.h>

#ifdef NIMCP_ENABLE_LAPACK

//=============================================================================
// LAPACK/BLAS FORTRAN INTERFACE
//=============================================================================

// WHAT: FORTRAN interface declarations
// WHY:  C code calls FORTRAN LAPACK library
// HOW:  Extern "C" with trailing underscores, pass-by-pointer

extern void sgesdd_(
    const char* jobz,    // 'A'=all, 'S'=min(m,n), 'O'=overwrite, 'N'=none
    const int* m,        // Number of rows
    const int* n,        // Number of columns
    float* A,            // Input matrix (column-major) [m×n], overwritten
    const int* lda,      // Leading dimension of A
    float* S,            // Singular values [min(m,n)]
    float* U,            // Left singular vectors [m×m] or [m×min(m,n)]
    const int* ldu,      // Leading dimension of U
    float* VT,           // Right singular vectors transposed [n×n] or [min(m,n)×n]
    const int* ldvt,     // Leading dimension of VT
    float* work,         // Workspace
    const int* lwork,    // Size of work
    int* iwork,          // Integer workspace [8*min(m,n)]
    int* info            // Return code: 0=success, <0=illegal arg, >0=no convergence
);

extern void sgemm_(
    const char* transa,  // 'N'=no transpose, 'T'=transpose
    const char* transb,
    const int* m,        // Rows of A and C
    const int* n,        // Columns of B and C
    const int* k,        // Columns of A, rows of B
    const float* alpha,  // Scalar multiplier
    const float* A,      // Matrix A
    const int* lda,      // Leading dimension of A
    const float* B,      // Matrix B
    const int* ldb,      // Leading dimension of B
    const float* beta,   // Scalar multiplier for C
    float* C,            // Output matrix C = alpha*A*B + beta*C
    const int* ldc       // Leading dimension of C
);

//=============================================================================
// INTERNAL HELPERS
//=============================================================================

/**
 * WHAT: Convert row-major to column-major layout
 * WHY:  C uses row-major, FORTRAN/LAPACK uses column-major
 * HOW:  Transpose during copy
 */
static void row_to_col_major(const float* row_major, float* col_major,
                             int m, int n) {
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            col_major[j * m + i] = row_major[i * n + j];
        }
    }
}

/**
 * WHAT: Convert column-major to row-major layout
 * WHY:  Return result in C-style row-major format
 * HOW:  Transpose during copy
 */
static void col_to_row_major(const float* col_major, float* row_major,
                             int m, int n) {
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            row_major[i * n + j] = col_major[j * m + i];
        }
    }
}

/**
 * WHAT: Compute Frobenius norm of matrix
 * WHY:  Needed for error computation and normalization
 * HOW:  √(Σᵢⱼ aᵢⱼ²)
 */
static float frobenius_norm(const float* A, uint32_t m, uint32_t n) {
    float sum = 0.0f;
    for (uint32_t i = 0; i < m * n; i++) {
        sum += A[i] * A[i];
    }
    return sqrtf(sum);
}

//=============================================================================
// PUBLIC API
//=============================================================================

/**
 * WHAT: Compute truncated SVD using LAPACK sgesdd
 * WHY:  Optimal performance and accuracy for TT-SVD
 * HOW:  FORTRAN interface with proper layout conversion
 *
 * ALGORITHM:
 * 1. Convert A from row-major (C) to column-major (FORTRAN)
 * 2. Call LAPACK sgesdd for SVD computation
 * 3. Truncate to max_rank and tolerance
 * 4. Convert U and VT back to row-major
 * 5. Return result
 */
svd_result_t svd_compute(
    const float* A,
    uint32_t m,
    uint32_t n,
    uint32_t max_rank,
    float tolerance)
{
    svd_result_t result = {0};

    // Guard: NULL checks
    if (!A || m == 0 || n == 0) {
        return result;
    }

    // WHAT: Determine actual rank
    // WHY:  Limit to matrix dimensions and requested rank
    uint32_t min_dim = (m < n) ? m : n;
    uint32_t rank = (max_rank == 0 || max_rank > min_dim) ? min_dim : max_rank;

    // WHAT: Convert input to column-major for LAPACK
    // WHY:  FORTRAN uses column-major, C uses row-major
    float* A_col = NULL;
    float* S_full = NULL;
    float* U_full = NULL;
    float* VT_full = NULL;
    int* iwork = NULL;
    float* work = NULL;

    A_col = (float*)nimcp_malloc(m * n * sizeof(float));
    if (!A_col) goto svd_fail;

    row_to_col_major(A, A_col, m, n);

    // WHAT: Allocate workspace for LAPACK
    // WHY:  sgesdd requires singular values and vectors storage
    S_full = (float*)nimcp_malloc(min_dim * sizeof(float));
    U_full = (float*)nimcp_malloc(m * min_dim * sizeof(float));
    VT_full = (float*)nimcp_malloc(min_dim * n * sizeof(float));

    if (!S_full || !U_full || !VT_full) goto svd_fail;

    // WHAT: Query optimal workspace size
    // WHY:  LAPACK needs temporary buffers for computation
    int m_int = (int)m;
    int n_int = (int)n;
    int min_dim_int = (int)min_dim;
    int lwork = -1;
    float work_query;
    int info;
    char jobz = 'S';  // Compute min(m,n) singular vectors

    iwork = (int*)nimcp_malloc(8 * min_dim * sizeof(int));
    if (!iwork) goto svd_fail;

    // Workspace query
    sgesdd_(&jobz, &m_int, &n_int, A_col, &m_int,
            S_full, U_full, &m_int, VT_full, &min_dim_int,
            &work_query, &lwork, iwork, &info);

    if (info != 0) goto svd_fail;

    // WHAT: Allocate optimal workspace
    // WHY:  Use LAPACK's recommended size for best performance
    lwork = (int)work_query;
    work = (float*)nimcp_malloc(lwork * sizeof(float));
    if (!work) goto svd_fail;

    // WHAT: Compute full SVD using LAPACK
    // WHY:  Divide-and-conquer algorithm for optimal performance
    sgesdd_(&jobz, &m_int, &n_int, A_col, &m_int,
            S_full, U_full, &m_int, VT_full, &min_dim_int,
            work, &lwork, iwork, &info);

    nimcp_free(work); work = NULL;
    nimcp_free(iwork); iwork = NULL;
    nimcp_free(A_col); A_col = NULL;

    if (info != 0) {
        if (info < 0) {
            fprintf(stderr, "LAPACK sgesdd: illegal argument %d\n", -info);
        } else {
            fprintf(stderr, "LAPACK sgesdd: convergence failure\n");
        }
        goto svd_fail;
    }

    // WHAT: Determine actual rank based on tolerance
    // WHY:  Adaptive truncation - keep only significant singular values
    float A_norm = S_full[0];  // Largest singular value
    float abs_tolerance = tolerance * A_norm;
    uint32_t actual_rank = 0;

    for (uint32_t i = 0; i < min_dim && i < rank; i++) {
        if (S_full[i] >= abs_tolerance) {
            actual_rank++;
        } else {
            break;  // Singular values are sorted descending
        }
    }

    // Ensure at least one singular value
    if (actual_rank == 0 && min_dim > 0) {
        actual_rank = 1;
    }

    // WHAT: Allocate result arrays with actual rank
    // WHY:  Return only the truncated SVD
    result.U = (float*)nimcp_malloc(m * actual_rank * sizeof(float));
    result.S = (float*)nimcp_malloc(actual_rank * sizeof(float));
    result.Vt = (float*)nimcp_malloc(actual_rank * n * sizeof(float));

    if (!result.U || !result.S || !result.Vt) {
        svd_free(&result);
        goto svd_fail;
    }

    // WHAT: Copy truncated singular values
    // WHY:  Store only significant components
    memcpy(result.S, S_full, actual_rank * sizeof(float));

    // WHAT: Convert U from column-major to row-major and truncate
    // WHY:  U is stored in FORTRAN layout [m×min_dim], need [m×actual_rank] row-major
    // NOTE: U is stored column-by-column in U_full
    for (uint32_t i = 0; i < m; i++) {
        for (uint32_t k = 0; k < actual_rank; k++) {
            // Column-major U_full: U[i,k] = U_full[k*m + i]
            // Row-major result.U: U[i,k] = result.U[i*actual_rank + k]
            result.U[i * actual_rank + k] = U_full[k * m + i];
        }
    }

    // WHAT: Copy VT (already transposed) and truncate
    // WHY:  VT is [min_dim×n] column-major, need [actual_rank×n] row-major
    for (uint32_t k = 0; k < actual_rank; k++) {
        for (uint32_t j = 0; j < n; j++) {
            // Column-major VT_full: VT[k,j] = VT_full[j*min_dim + k]
            // Row-major result.Vt: VT[k,j] = result.Vt[k*n + j]
            result.Vt[k * n + j] = VT_full[j * min_dim + k];
        }
    }

    // WHAT: Store dimensions
    result.m = m;
    result.n = n;
    result.rank = actual_rank;

    // WHAT: Compute reconstruction error BEFORE freeing S_full
    // WHY:  Quantify approximation quality using truncated singular values
    // NOTE: Must compute before freeing S_full to avoid use-after-free
    result.reconstruction_error = 0.0f;
    if (actual_rank < min_dim) {
        // Error from truncated singular values
        float truncated_norm = 0.0f;
        for (uint32_t i = actual_rank; i < min_dim; i++) {
            truncated_norm += S_full[i] * S_full[i];
        }
        result.reconstruction_error = sqrtf(truncated_norm) /
                                     sqrtf(truncated_norm +
                                           (result.S[0] * result.S[0] * actual_rank));
    }

svd_fail:
    // WHAT: Free temporary LAPACK workspace
    // WHY:  Cleanup after SVD computation and truncation
    // NOTE: nimcp_free(NULL) is safe
    nimcp_free(VT_full);
    nimcp_free(U_full);
    nimcp_free(S_full);
    nimcp_free(iwork);
    nimcp_free(work);
    nimcp_free(A_col);

    return result;
}

//=============================================================================
// COMMON API (works with both LAPACK and fallback)
//=============================================================================

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
 * HOW:  Matrix multiplication A = (U * diag(S)) * VT
 */
bool svd_reconstruct(const svd_result_t* svd, float* A) {
    if (!svd || !A || !svd->U || !svd->S || !svd->Vt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "svd_reconstruct: invalid parameters");

            return false;
    }

    // WHAT: Initialize output to zero
    memset(A, 0, svd->m * svd->n * sizeof(float));

    // WHAT: Compute A = Σₖ σₖ uₖ vₖᵀ (sum of rank-1 matrices)
    // WHY:  Direct implementation of SVD reconstruction
    for (uint32_t k = 0; k < svd->rank; k++) {
        float sigma = svd->S[k];
        for (uint32_t i = 0; i < svd->m; i++) {
            for (uint32_t j = 0; j < svd->n; j++) {
                // U is [m×rank] row-major: U[i,k] = U[i*rank + k]
                // Vt is [rank×n] row-major: Vt[k,j] = Vt[k*n + j]
                A[i * svd->n + j] += sigma * svd->U[i * svd->rank + k] *
                                    svd->Vt[k * svd->n + j];
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
    float* A_recon = (float*)nimcp_malloc(svd->m * svd->n * sizeof(float));
    if (!A_recon) {
        return 1.0f;
    }

    if (!svd_reconstruct(svd, A_recon)) {
        nimcp_free(A_recon);
        return 1.0f;
    }

    // WHAT: Compute ||A_original - A_recon||_F
    float error_sum = 0.0f;
    for (uint32_t i = 0; i < svd->m * svd->n; i++) {
        float diff = A_original[i] - A_recon[i];
        error_sum += diff * diff;
    }

    // WHAT: Normalize by ||A_original||_F
    float A_norm = frobenius_norm(A_original, svd->m, svd->n);
    float rel_error = (A_norm > 1e-10f) ? sqrtf(error_sum) / A_norm : 0.0f;

    nimcp_free(A_recon);
    return rel_error;
}

#else  // !NIMCP_ENABLE_LAPACK

// WHAT: Fallback to simplified SVD when LAPACK unavailable
// WHY:  Maintain functionality without external dependencies
// HOW:  Include the simple implementation

#include "nimcp_svd_simple.c"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(svd_lapack)

#endif  // NIMCP_ENABLE_LAPACK
