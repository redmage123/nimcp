/**
 * @file nimcp_multivariate.c
 * @brief Multivariate Analysis Implementation
 *
 * WHAT: Implementation of PCA, ICA, Factor Analysis, LDA/QDA, and CCA
 * WHY:  Dimensionality reduction and pattern analysis for neural computation
 * HOW:  SVD-based numerical stability, LAPACK integration, GPU acceleration
 *
 * @author NIMCP Development Team
 * @date 2026-01-30
 */

#include "utils/statistics/nimcp_multivariate.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_math_constants.h"

//=============================================================================
// MODULE IDENTIFICATION
//=============================================================================

#define LOG_MODULE_NAME "multivariate"

//=============================================================================
// LAPACK Function Declarations
//=============================================================================

#ifdef NIMCP_USE_LAPACK
extern void sgesdd_(const char* jobz, const int* m, const int* n, float* a,
                    const int* lda, float* s, float* u, const int* ldu,
                    float* vt, const int* ldvt, float* work, const int* lwork,
                    int* iwork, int* info);

extern void ssyevd_(const char* jobz, const char* uplo, const int* n, float* a,
                    const int* lda, float* w, float* work, const int* lwork,
                    int* iwork, const int* liwork, int* info);

extern void sgemm_(const char* transa, const char* transb, const int* m,
                   const int* n, const int* k, const float* alpha, const float* a,
                   const int* lda, const float* b, const int* ldb, const float* beta,
                   float* c, const int* ldc);

extern void sgesv_(const int* n, const int* nrhs, float* a, const int* lda,
                   int* ipiv, float* b, const int* ldb, int* info);

extern void sgeqrf_(const int* m, const int* n, float* a, const int* lda,
                    float* tau, float* work, const int* lwork, int* info);

extern void sorgqr_(const int* m, const int* n, const int* k, float* a,
                    const int* lda, const float* tau, float* work,
                    const int* lwork, int* info);
#endif

//=============================================================================
// Constants
//=============================================================================

#define PI NIMCP_PI_F

//=============================================================================
// Helper Function Declarations
//=============================================================================

static void transpose_matrix(const float* src, float* dst, uint32_t rows, uint32_t cols);
static float vector_norm(const float* v, uint32_t n);
static void vector_normalize(float* v, uint32_t n);
static float vector_dot(const float* a, const float* b, uint32_t n);
static void matrix_multiply_simple(const float* A, const float* B, float* C,
                                   uint32_t m, uint32_t k, uint32_t n);

//=============================================================================
// Configuration
//=============================================================================

nimcp_mv_config_t nimcp_mv_default_config(void) {
    nimcp_mv_config_t config = {
        .use_gpu = false,
        .center_data = true,
        .scale_data = false,
        .random_seed = 42,
        .tolerance = 1e-6f,
        .max_iterations = 200
    };
    return config;
}

//=============================================================================
// Error Handling
//=============================================================================

const char* nimcp_mv_error_string(nimcp_mv_result_t result) {
    switch (result) {
        case NIMCP_MV_OK:           return "Success";
        case NIMCP_MV_ERROR_NULL:   return "NULL pointer argument";
        case NIMCP_MV_ERROR_SIZE:   return "Invalid size";
        case NIMCP_MV_ERROR_MEMORY: return "Memory allocation failed";
        case NIMCP_MV_ERROR_PARAMS: return "Invalid parameters";
        case NIMCP_MV_ERROR_CONVERGE: return "Algorithm did not converge";
        case NIMCP_MV_ERROR_SINGULAR: return "Singular matrix";
        case NIMCP_MV_ERROR_NOT_FIT: return "Model not fitted";
        case NIMCP_MV_ERROR_DIMS:   return "Dimension mismatch";
        case NIMCP_MV_ERROR_GPU:    return "GPU operation failed";
        case NIMCP_MV_ERROR_LAPACK: return "LAPACK error";
        case NIMCP_MV_ERROR_LABELS: return "Invalid class labels";
        default: return "Unknown error";
    }
}

//=============================================================================
// Helper Functions
//=============================================================================

static void transpose_matrix(const float* src, float* dst, uint32_t rows, uint32_t cols) {
    for (uint32_t i = 0; i < rows; i++) {
        for (uint32_t j = 0; j < cols; j++) {
            dst[j * rows + i] = src[i * cols + j];
        }
    }
}

static float vector_norm(const float* v, uint32_t n) {
    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        sum += v[i] * v[i];
    }
    return sqrtf(sum);
}

static void vector_normalize(float* v, uint32_t n) {
    float norm = vector_norm(v, n);
    if (norm > NIMCP_MV_EPSILON) {
        for (uint32_t i = 0; i < n; i++) {
            v[i] /= norm;
        }
    }
}

static float vector_dot(const float* a, const float* b, uint32_t n) {
    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

static void matrix_multiply_simple(const float* A, const float* B, float* C,
                                   uint32_t m, uint32_t k, uint32_t n) {
    memset(C, 0, m * n * sizeof(float));
    for (uint32_t i = 0; i < m; i++) {
        for (uint32_t j = 0; j < n; j++) {
            float sum = 0.0f;
            for (uint32_t l = 0; l < k; l++) {
                sum += A[i * k + l] * B[l * n + j];
            }
            C[i * n + j] = sum;
        }
    }
}

//=============================================================================
// Utility Functions
//=============================================================================

nimcp_mv_result_t nimcp_mv_center(
    const float* X,
    uint32_t n_samples,
    uint32_t n_features,
    float* X_centered,
    float* mean)
{
    if (!X || !X_centered) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL pointer in nimcp_mv_center");
        return NIMCP_MV_ERROR_NULL;
    }
    if (n_samples == 0 || n_features == 0) {
        return NIMCP_MV_ERROR_SIZE;
    }

    float* local_mean = mean;
    bool allocated_mean = false;
    if (!local_mean) {
        local_mean = (float*)nimcp_malloc(n_features * sizeof(float));
        if (!local_mean) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate mean in center");
            return NIMCP_MV_ERROR_MEMORY;
        }
        allocated_mean = true;
    }

    // Compute mean
    memset(local_mean, 0, n_features * sizeof(float));
    for (uint32_t i = 0; i < n_samples; i++) {
        for (uint32_t j = 0; j < n_features; j++) {
            local_mean[j] += X[i * n_features + j];
        }
    }
    for (uint32_t j = 0; j < n_features; j++) {
        local_mean[j] /= (float)n_samples;
    }

    // Center data
    for (uint32_t i = 0; i < n_samples; i++) {
        for (uint32_t j = 0; j < n_features; j++) {
            X_centered[i * n_features + j] = X[i * n_features + j] - local_mean[j];
        }
    }

    if (allocated_mean) {
        nimcp_free(local_mean);
    }

    return NIMCP_MV_OK;
}

nimcp_mv_result_t nimcp_mv_standardize(
    const float* X,
    uint32_t n_samples,
    uint32_t n_features,
    float* X_standardized,
    float* mean,
    float* std)
{
    if (!X || !X_standardized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL pointer in standardize");
        return NIMCP_MV_ERROR_NULL;
    }
    if (n_samples < 2 || n_features == 0) {
        return NIMCP_MV_ERROR_SIZE;
    }

    float* local_mean = mean ? mean : (float*)nimcp_malloc(n_features * sizeof(float));
    float* local_std = std ? std : (float*)nimcp_malloc(n_features * sizeof(float));
    bool alloc_mean = (mean == NULL);
    bool alloc_std = (std == NULL);

    if (!local_mean || !local_std) {
        if (alloc_mean && local_mean) nimcp_free(local_mean);
        if (alloc_std && local_std) nimcp_free(local_std);
        return NIMCP_MV_ERROR_MEMORY;
    }

    // Compute mean
    memset(local_mean, 0, n_features * sizeof(float));
    for (uint32_t i = 0; i < n_samples; i++) {
        for (uint32_t j = 0; j < n_features; j++) {
            local_mean[j] += X[i * n_features + j];
        }
    }
    for (uint32_t j = 0; j < n_features; j++) {
        local_mean[j] /= (float)n_samples;
    }

    // Compute std using Welford
    memset(local_std, 0, n_features * sizeof(float));
    for (uint32_t i = 0; i < n_samples; i++) {
        for (uint32_t j = 0; j < n_features; j++) {
            float diff = X[i * n_features + j] - local_mean[j];
            local_std[j] += diff * diff;
        }
    }
    for (uint32_t j = 0; j < n_features; j++) {
        local_std[j] = sqrtf(local_std[j] / (float)(n_samples - 1));
        if (local_std[j] < NIMCP_MV_EPSILON) {
            local_std[j] = 1.0f; // Avoid division by zero
        }
    }

    // Standardize
    for (uint32_t i = 0; i < n_samples; i++) {
        for (uint32_t j = 0; j < n_features; j++) {
            X_standardized[i * n_features + j] =
                (X[i * n_features + j] - local_mean[j]) / local_std[j];
        }
    }

    if (alloc_mean) nimcp_free(local_mean);
    if (alloc_std) nimcp_free(local_std);

    return NIMCP_MV_OK;
}

nimcp_mv_result_t nimcp_mv_covariance(
    const float* X,
    uint32_t n_samples,
    uint32_t n_features,
    float* cov,
    uint32_t ddof)
{
    if (!X || !cov) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL in covariance");
        return NIMCP_MV_ERROR_NULL;
    }
    if (n_samples <= ddof || n_features == 0) {
        return NIMCP_MV_ERROR_SIZE;
    }

    // X should already be centered
    float denom = (float)(n_samples - ddof);

    // Compute X^T @ X / (n - ddof)
    for (uint32_t i = 0; i < n_features; i++) {
        for (uint32_t j = i; j < n_features; j++) {
            float sum = 0.0f;
            for (uint32_t k = 0; k < n_samples; k++) {
                sum += X[k * n_features + i] * X[k * n_features + j];
            }
            cov[i * n_features + j] = sum / denom;
            cov[j * n_features + i] = cov[i * n_features + j]; // Symmetric
        }
    }

    return NIMCP_MV_OK;
}

//=============================================================================
// SVD Implementation (CPU fallback)
//=============================================================================

// Simple power iteration SVD for when LAPACK not available
static nimcp_mv_result_t svd_power_iteration(
    const float* A,
    uint32_t m,
    uint32_t n,
    float* U,
    float* S,
    float* Vt,
    uint32_t k)
{
    uint32_t min_mn = (m < n) ? m : n;
    if (k > min_mn) k = min_mn;

    float* A_work = (float*)nimcp_malloc(m * n * sizeof(float));
    float* v = (float*)nimcp_malloc(n * sizeof(float));
    float* u = (float*)nimcp_malloc(m * sizeof(float));
    float* Av = (float*)nimcp_malloc(m * sizeof(float));
    float* Atu = (float*)nimcp_malloc(n * sizeof(float));

    if (!A_work || !v || !u || !Av || !Atu) {
        nimcp_free(A_work); nimcp_free(v); nimcp_free(u); nimcp_free(Av); nimcp_free(Atu);
        return NIMCP_MV_ERROR_MEMORY;
    }

    memcpy(A_work, A, m * n * sizeof(float));

    for (uint32_t i = 0; i < k; i++) {
        // Initialize v randomly
        for (uint32_t j = 0; j < n; j++) {
            v[j] = (float)nimcp_tl_rand() / (float)RAND_MAX - 0.5f;
        }
        vector_normalize(v, n);

        float sigma = 0.0f;
        float prev_sigma = -1.0f;

        // Power iteration
        for (uint32_t iter = 0; iter < 100; iter++) {
            // u = A @ v
            memset(Av, 0, m * sizeof(float));
            for (uint32_t row = 0; row < m; row++) {
                for (uint32_t col = 0; col < n; col++) {
                    Av[row] += A_work[row * n + col] * v[col];
                }
            }

            sigma = vector_norm(Av, m);
            if (sigma < NIMCP_MV_EPSILON) break;

            for (uint32_t j = 0; j < m; j++) {
                u[j] = Av[j] / sigma;
            }

            // v = A^T @ u
            memset(Atu, 0, n * sizeof(float));
            for (uint32_t col = 0; col < n; col++) {
                for (uint32_t row = 0; row < m; row++) {
                    Atu[col] += A_work[row * n + col] * u[row];
                }
            }

            vector_normalize(Atu, n);
            memcpy(v, Atu, n * sizeof(float));

            if (fabsf(sigma - prev_sigma) < NIMCP_MV_EPSILON * sigma) {
                break;
            }
            prev_sigma = sigma;
        }

        S[i] = sigma;
        for (uint32_t j = 0; j < m; j++) U[j * k + i] = u[j];
        for (uint32_t j = 0; j < n; j++) Vt[i * n + j] = v[j];

        // Deflate: A = A - sigma * u @ v^T
        for (uint32_t row = 0; row < m; row++) {
            for (uint32_t col = 0; col < n; col++) {
                A_work[row * n + col] -= sigma * u[row] * v[col];
            }
        }
    }

    nimcp_free(A_work); nimcp_free(v); nimcp_free(u); nimcp_free(Av); nimcp_free(Atu);
    return NIMCP_MV_OK;
}

nimcp_mv_result_t nimcp_mv_svd(
    const float* A,
    uint32_t m,
    uint32_t n,
    float* U,
    float* S,
    float* Vt,
    bool full_matrices)
{
    if (!A || !S) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL in SVD");
        return NIMCP_MV_ERROR_NULL;
    }
    if (m == 0 || n == 0) {
        return NIMCP_MV_ERROR_SIZE;
    }

#ifdef NIMCP_USE_LAPACK
    int M = (int)m;
    int N = (int)n;
    int lda = N;
    int min_mn = (M < N) ? M : N;
    int ldu = M;
    int ldvt = full_matrices ? N : min_mn;
    nimcp_mv_result_t result = NIMCP_MV_OK;

    // LAPACK expects column-major, so transpose
    float* A_col = NULL;
    int* iwork = NULL;
    float* U_col = NULL;
    float* Vt_col = NULL;
    float* work = NULL;

    A_col = (float*)nimcp_malloc(m * n * sizeof(float));
    if (!A_col) { result = NIMCP_MV_ERROR_MEMORY; goto svd_cleanup; }
    transpose_matrix(A, A_col, m, n);

    char jobz = full_matrices ? 'A' : 'S';

    // Query workspace
    int lwork = -1;
    float work_query;
    int info;

    iwork = (int*)nimcp_malloc(8 * min_mn * sizeof(int));
    if (!iwork) { result = NIMCP_MV_ERROR_MEMORY; goto svd_cleanup; }

    U_col = U ? (float*)nimcp_malloc(m * (full_matrices ? m : min_mn) * sizeof(float)) : NULL;
    if (U && !U_col) { result = NIMCP_MV_ERROR_MEMORY; goto svd_cleanup; }

    Vt_col = Vt ? (float*)nimcp_malloc((full_matrices ? n : min_mn) * n * sizeof(float)) : NULL;
    if (Vt && !Vt_col) { result = NIMCP_MV_ERROR_MEMORY; goto svd_cleanup; }

    sgesdd_(&jobz, &M, &N, A_col, &lda, S, U_col, &ldu, Vt_col, &ldvt,
            &work_query, &lwork, iwork, &info);

    lwork = (int)work_query;
    work = (float*)nimcp_malloc(lwork * sizeof(float));
    if (!work) { result = NIMCP_MV_ERROR_MEMORY; goto svd_cleanup; }

    sgesdd_(&jobz, &M, &N, A_col, &lda, S, U_col, &ldu, Vt_col, &ldvt,
            work, &lwork, iwork, &info);

    if (info != 0) {
        result = NIMCP_MV_ERROR_LAPACK;
        goto svd_cleanup;
    }

    // Transpose results back to row-major
    if (U && U_col) {
        uint32_t u_cols = full_matrices ? m : min_mn;
        transpose_matrix(U_col, U, u_cols, m);
    }
    if (Vt && Vt_col) {
        uint32_t vt_rows = full_matrices ? n : min_mn;
        transpose_matrix(Vt_col, Vt, n, vt_rows);
    }

svd_cleanup:
    nimcp_free(work);
    nimcp_free(Vt_col);
    nimcp_free(U_col);
    nimcp_free(iwork);
    nimcp_free(A_col);

    return result;
#else
    // Fallback to power iteration
    uint32_t min_mn = (m < n) ? m : n;
    return svd_power_iteration(A, m, n, U, S, Vt, min_mn);
#endif
}

nimcp_mv_result_t nimcp_mv_eigh(
    const float* A,
    uint32_t n,
    float* eigenvalues,
    float* eigenvectors)
{
    if (!A || !eigenvalues) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL in eigh");
        return NIMCP_MV_ERROR_NULL;
    }
    if (n == 0) {
        return NIMCP_MV_ERROR_SIZE;
    }

#ifdef NIMCP_USE_LAPACK
    int N = (int)n;
    int lda = N;
    char jobz = eigenvectors ? 'V' : 'N';
    char uplo = 'U';
    int info;

    if (n > 0 && (size_t)n * n > SIZE_MAX / sizeof(float)) {
        return NIMCP_MV_ERROR_MEMORY;
    }
    float* A_work = NULL;
    float* work = NULL;
    int* iwork = NULL;
    nimcp_mv_result_t result = NIMCP_MV_OK;

    A_work = (float*)nimcp_malloc((size_t)n * n * sizeof(float));
    if (!A_work) { result = NIMCP_MV_ERROR_MEMORY; goto eigh_cleanup; }

    // LAPACK expects column-major, transpose symmetric matrix
    transpose_matrix(A, A_work, n, n);

    // Query workspace
    int lwork = -1;
    int liwork = -1;
    float work_query;
    int iwork_query;

    ssyevd_(&jobz, &uplo, &N, A_work, &lda, eigenvalues,
            &work_query, &lwork, &iwork_query, &liwork, &info);

    lwork = (int)work_query;
    liwork = iwork_query;
    work = (float*)nimcp_malloc(lwork * sizeof(float));
    iwork = (int*)nimcp_malloc(liwork * sizeof(int));

    if (!work || !iwork) { result = NIMCP_MV_ERROR_MEMORY; goto eigh_cleanup; }

    ssyevd_(&jobz, &uplo, &N, A_work, &lda, eigenvalues,
            work, &lwork, iwork, &liwork, &info);

    if (info != 0) {
        result = NIMCP_MV_ERROR_LAPACK;
        goto eigh_cleanup;
    }

    if (eigenvectors) {
        transpose_matrix(A_work, eigenvectors, n, n);
    }

eigh_cleanup:
    nimcp_free(iwork);
    nimcp_free(work);
    nimcp_free(A_work);
    return result;
#else
    // Simple power iteration for largest eigenvalue (fallback)
    // For full eigendecomposition without LAPACK, this is very slow
    return NIMCP_MV_ERROR_LAPACK; // Indicate LAPACK needed
#endif
}

nimcp_mv_result_t nimcp_mv_gemm(
    const float* A,
    const float* B,
    float* C,
    uint32_t m,
    uint32_t k,
    uint32_t n,
    float alpha,
    float beta,
    bool trans_a,
    bool trans_b)
{
    if (!A || !B || !C) {
        return NIMCP_MV_ERROR_NULL;
    }

#ifdef NIMCP_USE_LAPACK
    int M = (int)m;
    int N = (int)n;
    int K = (int)k;
    char ta = trans_a ? 'T' : 'N';
    char tb = trans_b ? 'T' : 'N';
    int lda = trans_a ? M : K;
    int ldb = trans_b ? K : N;
    int ldc = N;

    // Convert to column-major
    float* A_col = (float*)nimcp_malloc((trans_a ? k*m : m*k) * sizeof(float));
    float* B_col = (float*)nimcp_malloc((trans_b ? n*k : k*n) * sizeof(float));
    float* C_col = (float*)nimcp_malloc(m * n * sizeof(float));

    if (!A_col || !B_col || !C_col) {
        nimcp_free(A_col); nimcp_free(B_col); nimcp_free(C_col);
        return NIMCP_MV_ERROR_MEMORY;
    }

    if (trans_a) {
        transpose_matrix(A, A_col, k, m);
    } else {
        transpose_matrix(A, A_col, m, k);
    }
    if (trans_b) {
        transpose_matrix(B, B_col, n, k);
    } else {
        transpose_matrix(B, B_col, k, n);
    }

    if (beta != 0.0f) {
        transpose_matrix(C, C_col, m, n);
    }

    sgemm_(&tb, &ta, &N, &M, &K, &alpha, B_col, &ldb, A_col, &lda, &beta, C_col, &ldc);

    transpose_matrix(C_col, C, n, m);

    nimcp_free(A_col); nimcp_free(B_col); nimcp_free(C_col);
    return NIMCP_MV_OK;
#else
    // Simple fallback
    if (beta == 0.0f) {
        memset(C, 0, m * n * sizeof(float));
    } else if (beta != 1.0f) {
        for (uint32_t i = 0; i < m * n; i++) {
            C[i] *= beta;
        }
    }

    for (uint32_t i = 0; i < m; i++) {
        for (uint32_t j = 0; j < n; j++) {
            float sum = 0.0f;
            for (uint32_t l = 0; l < k; l++) {
                float a_val = trans_a ? A[l * m + i] : A[i * k + l];
                float b_val = trans_b ? B[j * k + l] : B[l * n + j];
                sum += a_val * b_val;
            }
            C[i * n + j] += alpha * sum;
        }
    }
    return NIMCP_MV_OK;
#endif
}

nimcp_mv_result_t nimcp_mv_solve(
    const float* A,
    const float* B,
    uint32_t n,
    uint32_t nrhs,
    float* X)
{
    if (!A || !B || !X) {
        return NIMCP_MV_ERROR_NULL;
    }

#ifdef NIMCP_USE_LAPACK
    int N = (int)n;
    int NRHS = (int)nrhs;
    int lda = N;
    int ldb = NRHS;
    int info;

    if (n > 0 && ((size_t)n * n > SIZE_MAX / sizeof(float) ||
                   (size_t)n * nrhs > SIZE_MAX / sizeof(float))) {
        return NIMCP_MV_ERROR_MEMORY;
    }
    float* A_col = (float*)nimcp_malloc((size_t)n * n * sizeof(float));
    float* B_col = (float*)nimcp_malloc((size_t)n * nrhs * sizeof(float));
    int* ipiv = (int*)nimcp_malloc(n * sizeof(int));

    if (!A_col || !B_col || !ipiv) {
        nimcp_free(A_col); nimcp_free(B_col); nimcp_free(ipiv);
        return NIMCP_MV_ERROR_MEMORY;
    }

    transpose_matrix(A, A_col, n, n);
    transpose_matrix(B, B_col, n, nrhs);

    sgesv_(&N, &NRHS, A_col, &lda, ipiv, B_col, &ldb, &info);

    if (info != 0) {
        nimcp_free(A_col); nimcp_free(B_col); nimcp_free(ipiv);
        return info < 0 ? NIMCP_MV_ERROR_PARAMS : NIMCP_MV_ERROR_SINGULAR;
    }

    transpose_matrix(B_col, X, nrhs, n);

    nimcp_free(A_col); nimcp_free(B_col); nimcp_free(ipiv);
    return NIMCP_MV_OK;
#else
    return NIMCP_MV_ERROR_LAPACK;
#endif
}

nimcp_mv_result_t nimcp_mv_pinv(
    const float* A,
    uint32_t m,
    uint32_t n,
    float* A_pinv,
    float rcond)
{
    if (!A || !A_pinv) {
        return NIMCP_MV_ERROR_NULL;
    }

    uint32_t min_mn = (m < n) ? m : n;

    float* U = (float*)nimcp_malloc(m * min_mn * sizeof(float));
    float* S = (float*)nimcp_malloc(min_mn * sizeof(float));
    float* Vt = (float*)nimcp_malloc(min_mn * n * sizeof(float));

    if (!U || !S || !Vt) {
        nimcp_free(U); nimcp_free(S); nimcp_free(Vt);
        return NIMCP_MV_ERROR_MEMORY;
    }

    nimcp_mv_result_t res = nimcp_mv_svd(A, m, n, U, S, Vt, false);
    if (res != NIMCP_MV_OK) {
        nimcp_free(U); nimcp_free(S); nimcp_free(Vt);
        return res;
    }

    // Compute threshold
    float threshold = rcond * S[0];

    // S_inv = 1/S for S > threshold, else 0
    float* S_inv = (float*)nimcp_calloc(min_mn, sizeof(float));
    if (!S_inv) {
        nimcp_free(U); nimcp_free(S); nimcp_free(Vt);
        return NIMCP_MV_ERROR_MEMORY;
    }

    for (uint32_t i = 0; i < min_mn; i++) {
        if (S[i] > threshold) {
            S_inv[i] = 1.0f / S[i];
        }
    }

    // A_pinv = V @ diag(S_inv) @ U^T = Vt^T @ diag(S_inv) @ U^T
    // First: Vt^T @ diag(S_inv) = (n x min_mn) @ diag = scale columns of Vt^T
    float* V_Sinv = (float*)nimcp_malloc(n * min_mn * sizeof(float));
    if (!V_Sinv) {
        nimcp_free(U); nimcp_free(S); nimcp_free(Vt); nimcp_free(S_inv);
        return NIMCP_MV_ERROR_MEMORY;
    }

    // V_Sinv = V @ diag(S_inv) where V = Vt^T
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < min_mn; j++) {
            V_Sinv[i * min_mn + j] = Vt[j * n + i] * S_inv[j];
        }
    }

    // A_pinv = V_Sinv @ U^T  (n x min_mn) @ (min_mn x m)
    // U^T[j,k] = U[k,j] where U is (m x min_mn)
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < m; j++) {
            float sum = 0.0f;
            for (uint32_t k = 0; k < min_mn; k++) {
                sum += V_Sinv[i * min_mn + k] * U[j * min_mn + k];
            }
            A_pinv[i * m + j] = sum;
        }
    }

    nimcp_free(U); nimcp_free(S); nimcp_free(Vt); nimcp_free(S_inv); nimcp_free(V_Sinv);
    return NIMCP_MV_OK;
}

//=============================================================================
// PCA Implementation
//=============================================================================

nimcp_pca_t* nimcp_pca_create(
    uint32_t n_components,
    nimcp_pca_whiten_t whiten,
    nimcp_pca_solver_t solver)
{
    nimcp_pca_t* pca = (nimcp_pca_t*)nimcp_calloc(1, sizeof(nimcp_pca_t));
    if (!pca) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate PCA context");
        return NULL;
    }

    pca->n_components = n_components;
    pca->whiten = whiten;
    pca->solver = solver;
    pca->is_fitted = false;
    pca->gpu_ctx = NULL;

    return pca;
}

void nimcp_pca_destroy(nimcp_pca_t* pca) {
    if (!pca) return;

    nimcp_free(pca->components);
    nimcp_free(pca->explained_variance);
    nimcp_free(pca->explained_variance_ratio);
    nimcp_free(pca->singular_values);
    nimcp_free(pca->mean);
    nimcp_free(pca->std);
    nimcp_free(pca);
}

nimcp_mv_result_t nimcp_pca_fit(
    nimcp_pca_t* pca,
    const float* X,
    uint32_t n_samples,
    uint32_t n_features)
{
    if (!pca || !X) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL in PCA fit");
        return NIMCP_MV_ERROR_NULL;
    }
    if (n_samples < 2 || n_features == 0) {
        return NIMCP_MV_ERROR_SIZE;
    }

    // Determine number of components
    uint32_t n_comp = pca->n_components;
    uint32_t max_comp = (n_samples < n_features) ? n_samples : n_features;
    if (n_comp == 0 || n_comp > max_comp) {
        n_comp = max_comp;
    }
    pca->n_components = n_comp;
    pca->n_features = n_features;
    pca->n_samples_seen = n_samples;

    // Allocate arrays
    pca->mean = (float*)nimcp_malloc(n_features * sizeof(float));
    pca->components = (float*)nimcp_malloc(n_comp * n_features * sizeof(float));
    pca->explained_variance = (float*)nimcp_malloc(n_comp * sizeof(float));
    pca->explained_variance_ratio = (float*)nimcp_malloc(n_comp * sizeof(float));
    pca->singular_values = (float*)nimcp_malloc(n_comp * sizeof(float));

    if (!pca->mean || !pca->components || !pca->explained_variance ||
        !pca->explained_variance_ratio || !pca->singular_values) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "PCA allocation failed");
        return NIMCP_MV_ERROR_MEMORY;
    }

    // Center data
    float* X_centered = (float*)nimcp_malloc(n_samples * n_features * sizeof(float));
    if (!X_centered) {
        return NIMCP_MV_ERROR_MEMORY;
    }

    nimcp_mv_result_t res = nimcp_mv_center(X, n_samples, n_features, X_centered, pca->mean);
    if (res != NIMCP_MV_OK) {
        nimcp_free(X_centered);
        return res;
    }

    // SVD of centered data
    uint32_t min_mn = (n_samples < n_features) ? n_samples : n_features;
    float* U = (float*)nimcp_malloc(n_samples * min_mn * sizeof(float));
    float* S = (float*)nimcp_malloc(min_mn * sizeof(float));
    float* Vt = (float*)nimcp_malloc(min_mn * n_features * sizeof(float));

    if (!U || !S || !Vt) {
        nimcp_free(X_centered); nimcp_free(U); nimcp_free(S); nimcp_free(Vt);
        return NIMCP_MV_ERROR_MEMORY;
    }

    res = nimcp_mv_svd(X_centered, n_samples, n_features, U, S, Vt, false);
    nimcp_free(X_centered);

    if (res != NIMCP_MV_OK) {
        nimcp_free(U); nimcp_free(S); nimcp_free(Vt);
        return res;
    }

    // Copy top k components (rows of Vt)
    for (uint32_t i = 0; i < n_comp; i++) {
        for (uint32_t j = 0; j < n_features; j++) {
            pca->components[i * n_features + j] = Vt[i * n_features + j];
        }
        pca->singular_values[i] = S[i];
    }

    // Compute explained variance
    float total_var = 0.0f;
    for (uint32_t i = 0; i < min_mn; i++) {
        total_var += S[i] * S[i];
    }
    pca->total_variance = total_var / (float)(n_samples - 1);

    for (uint32_t i = 0; i < n_comp; i++) {
        pca->explained_variance[i] = (S[i] * S[i]) / (float)(n_samples - 1);
        pca->explained_variance_ratio[i] = (S[i] * S[i]) / total_var;
    }

    // Noise variance (from discarded components)
    float noise_var = 0.0f;
    for (uint32_t i = n_comp; i < min_mn; i++) {
        noise_var += S[i] * S[i];
    }
    if (min_mn > n_comp) {
        pca->noise_variance = noise_var / ((float)(n_samples - 1) * (min_mn - n_comp));
    } else {
        pca->noise_variance = 0.0f;
    }

    nimcp_free(U); nimcp_free(S); nimcp_free(Vt);
    pca->is_fitted = true;

    return NIMCP_MV_OK;
}

nimcp_mv_result_t nimcp_pca_transform(
    const nimcp_pca_t* pca,
    const float* X,
    uint32_t n_samples,
    float* X_transformed)
{
    if (!pca || !X || !X_transformed) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL in PCA transform");
        return NIMCP_MV_ERROR_NULL;
    }
    if (!pca->is_fitted) {
        return NIMCP_MV_ERROR_NOT_FIT;
    }

    // Center and project: X_t = (X - mean) @ components.T
    for (uint32_t i = 0; i < n_samples; i++) {
        for (uint32_t j = 0; j < pca->n_components; j++) {
            float sum = 0.0f;
            for (uint32_t k = 0; k < pca->n_features; k++) {
                float centered = X[i * pca->n_features + k] - pca->mean[k];
                sum += centered * pca->components[j * pca->n_features + k];
            }

            // Apply whitening if requested
            if (pca->whiten == NIMCP_PCA_WHITEN_UNIT && pca->singular_values[j] > NIMCP_MV_EPSILON) {
                sum /= pca->singular_values[j] / sqrtf((float)(pca->n_samples_seen - 1));
            }

            X_transformed[i * pca->n_components + j] = sum;
        }
    }

    return NIMCP_MV_OK;
}

nimcp_mv_result_t nimcp_pca_inverse_transform(
    const nimcp_pca_t* pca,
    const float* X_transformed,
    uint32_t n_samples,
    float* X_reconstructed)
{
    if (!pca || !X_transformed || !X_reconstructed) {
        return NIMCP_MV_ERROR_NULL;
    }
    if (!pca->is_fitted) {
        return NIMCP_MV_ERROR_NOT_FIT;
    }

    // X_rec = X_t @ components + mean
    for (uint32_t i = 0; i < n_samples; i++) {
        for (uint32_t j = 0; j < pca->n_features; j++) {
            float sum = pca->mean[j];
            for (uint32_t k = 0; k < pca->n_components; k++) {
                float val = X_transformed[i * pca->n_components + k];

                // Reverse whitening if applied
                if (pca->whiten == NIMCP_PCA_WHITEN_UNIT) {
                    val *= pca->singular_values[k] / sqrtf((float)(pca->n_samples_seen - 1));
                }

                sum += val * pca->components[k * pca->n_features + j];
            }
            X_reconstructed[i * pca->n_features + j] = sum;
        }
    }

    return NIMCP_MV_OK;
}

nimcp_mv_result_t nimcp_pca_fit_transform(
    nimcp_pca_t* pca,
    const float* X,
    uint32_t n_samples,
    uint32_t n_features,
    float* X_transformed)
{
    nimcp_mv_result_t res = nimcp_pca_fit(pca, X, n_samples, n_features);
    if (res != NIMCP_MV_OK) {
        return res;
    }
    return nimcp_pca_transform(pca, X, n_samples, X_transformed);
}

nimcp_mv_result_t nimcp_pca_explained_variance(
    const nimcp_pca_t* pca,
    float* variance)
{
    if (!pca || !variance) {
        return NIMCP_MV_ERROR_NULL;
    }
    if (!pca->is_fitted) {
        return NIMCP_MV_ERROR_NOT_FIT;
    }

    memcpy(variance, pca->explained_variance, pca->n_components * sizeof(float));
    return NIMCP_MV_OK;
}

nimcp_mv_result_t nimcp_pca_get_components(
    const nimcp_pca_t* pca,
    float* components)
{
    if (!pca || !components) {
        return NIMCP_MV_ERROR_NULL;
    }
    if (!pca->is_fitted) {
        return NIMCP_MV_ERROR_NOT_FIT;
    }

    memcpy(components, pca->components, pca->n_components * pca->n_features * sizeof(float));
    return NIMCP_MV_OK;
}

float nimcp_pca_score(
    const nimcp_pca_t* pca,
    const float* X,
    uint32_t n_samples)
{
    if (!pca || !X || !pca->is_fitted) {
        return NAN;
    }

    float* X_t = (float*)nimcp_malloc(n_samples * pca->n_components * sizeof(float));
    float* X_rec = (float*)nimcp_malloc(n_samples * pca->n_features * sizeof(float));

    if (!X_t || !X_rec) {
        nimcp_free(X_t); nimcp_free(X_rec);
        return NAN;
    }

    nimcp_pca_transform(pca, X, n_samples, X_t);
    nimcp_pca_inverse_transform(pca, X_t, n_samples, X_rec);

    // Compute MSE
    float mse = 0.0f;
    for (uint32_t i = 0; i < n_samples * pca->n_features; i++) {
        float diff = X[i] - X_rec[i];
        mse += diff * diff;
    }
    mse /= (float)(n_samples * pca->n_features);

    nimcp_free(X_t); nimcp_free(X_rec);
    return mse;
}

nimcp_mv_result_t nimcp_pca_partial_fit(
    nimcp_pca_t* pca,
    const float* X,
    uint32_t n_samples,
    uint32_t n_features)
{
    // Simplified incremental PCA - just refit for now
    // Full incremental SVD would use Ross et al. algorithm
    return nimcp_pca_fit(pca, X, n_samples, n_features);
}

//=============================================================================
// ICA Implementation
//=============================================================================

// FastICA nonlinearity functions
static void ica_g_logcosh(const float* u, float* g, float* g_prime, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        float th = tanhf(u[i]);
        g[i] = th;
        g_prime[i] = 1.0f - th * th;
    }
}

static void ica_g_exp(const float* u, float* g, float* g_prime, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        float u2 = u[i] * u[i];
        float exp_u2 = expf(-u2 / 2.0f);
        g[i] = u[i] * exp_u2;
        g_prime[i] = (1.0f - u2) * exp_u2;
    }
}

static void ica_g_cube(const float* u, float* g, float* g_prime, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        g[i] = u[i] * u[i] * u[i];
        g_prime[i] = 3.0f * u[i] * u[i];
    }
}

nimcp_ica_t* nimcp_ica_create(
    uint32_t n_components,
    nimcp_ica_fun_t fun,
    nimcp_ica_algorithm_t algorithm)
{
    if (n_components == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_ica_create: n_components is zero");
        return NULL;
    }

    nimcp_ica_t* ica = (nimcp_ica_t*)nimcp_calloc(1, sizeof(nimcp_ica_t));
    if (!ica) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate ICA context");
        return NULL;
    }

    ica->n_components = n_components;
    ica->fun = fun;
    ica->algorithm = algorithm;
    ica->tolerance = NIMCP_MV_ICA_TOLERANCE;
    ica->max_iter = NIMCP_MV_ICA_MAX_ITERATIONS;
    ica->random_state = 42;
    ica->is_fitted = false;

    return ica;
}

void nimcp_ica_destroy(nimcp_ica_t* ica) {
    if (!ica) return;

    nimcp_free(ica->mixing);
    nimcp_free(ica->unmixing);
    nimcp_free(ica->components);
    nimcp_free(ica->mean);
    nimcp_free(ica->whitening);
    nimcp_free(ica);
}

nimcp_mv_result_t nimcp_ica_fit(
    nimcp_ica_t* ica,
    const float* X,
    uint32_t n_samples,
    uint32_t n_features)
{
    if (!ica || !X) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL in ICA fit");
        return NIMCP_MV_ERROR_NULL;
    }
    if (n_samples < ica->n_components || n_features < ica->n_components) {
        return NIMCP_MV_ERROR_SIZE;
    }

    uint32_t n_comp = ica->n_components;
    ica->n_features = n_features;

    // Allocate storage
    ica->mean = (float*)nimcp_malloc(n_features * sizeof(float));
    ica->mixing = (float*)nimcp_malloc(n_features * n_comp * sizeof(float));
    ica->unmixing = (float*)nimcp_malloc(n_comp * n_features * sizeof(float));
    ica->whitening = (float*)nimcp_malloc(n_comp * n_features * sizeof(float));

    if (!ica->mean || !ica->mixing || !ica->unmixing || !ica->whitening) {
        return NIMCP_MV_ERROR_MEMORY;
    }

    // Center data
    float* X_centered = (float*)nimcp_malloc(n_samples * n_features * sizeof(float));
    if (!X_centered) return NIMCP_MV_ERROR_MEMORY;

    nimcp_mv_center(X, n_samples, n_features, X_centered, ica->mean);

    // Whiten using PCA
    nimcp_pca_t* pca = nimcp_pca_create(n_comp, NIMCP_PCA_WHITEN_UNIT, NIMCP_PCA_SOLVER_AUTO);
    if (!pca) {
        nimcp_free(X_centered);
        return NIMCP_MV_ERROR_MEMORY;
    }

    float* X_white = (float*)nimcp_malloc(n_samples * n_comp * sizeof(float));
    if (!X_white) {
        nimcp_pca_destroy(pca);
        nimcp_free(X_centered);
        return NIMCP_MV_ERROR_MEMORY;
    }

    nimcp_mv_result_t res = nimcp_pca_fit_transform(pca, X_centered, n_samples, n_features, X_white);
    if (res != NIMCP_MV_OK) {
        nimcp_pca_destroy(pca);
        nimcp_free(X_centered);
        nimcp_free(X_white);
        return res;
    }

    // Store whitening matrix
    nimcp_pca_get_components(pca, ica->whitening);

    // Initialize W randomly
    float* W = (float*)nimcp_malloc(n_comp * n_comp * sizeof(float));
    if (!W) {
        nimcp_pca_destroy(pca);
        nimcp_free(X_centered);
        nimcp_free(X_white);
        return NIMCP_MV_ERROR_MEMORY;
    }

    /* srand() removed: thread-unsafe global PRNG state; nimcp_tl_rand() has its own seeding */
    for (uint32_t i = 0; i < n_comp * n_comp; i++) {
        W[i] = (float)nimcp_tl_rand() / (float)RAND_MAX - 0.5f;
    }

    // Orthogonalize W using symmetric decorrelation: W = (W @ W^T)^(-1/2) @ W
    float* WWT = (float*)nimcp_malloc(n_comp * n_comp * sizeof(float));
    float* eigenvalues = (float*)nimcp_malloc(n_comp * sizeof(float));
    float* eigenvectors = (float*)nimcp_malloc(n_comp * n_comp * sizeof(float));
    float* g = (float*)nimcp_malloc(n_samples * sizeof(float));
    float* g_prime = (float*)nimcp_malloc(n_samples * sizeof(float));
    float* wx = (float*)nimcp_malloc(n_samples * sizeof(float));
    float* w_new = (float*)nimcp_malloc(n_comp * sizeof(float));

    if (!WWT || !eigenvalues || !eigenvectors || !g || !g_prime || !wx || !w_new) {
        nimcp_free(W); nimcp_free(WWT); nimcp_free(eigenvalues); nimcp_free(eigenvectors);
        nimcp_free(g); nimcp_free(g_prime); nimcp_free(wx); nimcp_free(w_new);
        nimcp_pca_destroy(pca);
        nimcp_free(X_centered);
        nimcp_free(X_white);
        return NIMCP_MV_ERROR_MEMORY;
    }

    // Select nonlinearity function
    void (*g_func)(const float*, float*, float*, uint32_t);
    switch (ica->fun) {
        case NIMCP_ICA_FUN_EXP:  g_func = ica_g_exp; break;
        case NIMCP_ICA_FUN_CUBE: g_func = ica_g_cube; break;
        default: g_func = ica_g_logcosh; break;
    }

    // FastICA iteration (parallel/symmetric)
    for (uint32_t iter = 0; iter < ica->max_iter; iter++) {
        float max_change = 0.0f;

        for (uint32_t p = 0; p < n_comp; p++) {
            // wx = X_white @ w_p
            for (uint32_t i = 0; i < n_samples; i++) {
                float sum = 0.0f;
                for (uint32_t j = 0; j < n_comp; j++) {
                    sum += X_white[i * n_comp + j] * W[p * n_comp + j];
                }
                wx[i] = sum;
            }

            // Apply nonlinearity
            g_func(wx, g, g_prime, n_samples);

            // w_new = E{X * g(w^T X)} - E{g'(w^T X)} * w
            float mean_g_prime = 0.0f;
            for (uint32_t i = 0; i < n_samples; i++) {
                mean_g_prime += g_prime[i];
            }
            mean_g_prime /= (float)n_samples;

            for (uint32_t j = 0; j < n_comp; j++) {
                float mean_xg = 0.0f;
                for (uint32_t i = 0; i < n_samples; i++) {
                    mean_xg += X_white[i * n_comp + j] * g[i];
                }
                mean_xg /= (float)n_samples;

                w_new[j] = mean_xg - mean_g_prime * W[p * n_comp + j];
            }

            // Gram-Schmidt orthogonalization
            for (uint32_t q = 0; q < p; q++) {
                float dot = 0.0f;
                for (uint32_t j = 0; j < n_comp; j++) {
                    dot += w_new[j] * W[q * n_comp + j];
                }
                for (uint32_t j = 0; j < n_comp; j++) {
                    w_new[j] -= dot * W[q * n_comp + j];
                }
            }

            // Normalize
            vector_normalize(w_new, n_comp);

            // Check convergence
            float change = 0.0f;
            for (uint32_t j = 0; j < n_comp; j++) {
                float diff = fabsf(w_new[j]) - fabsf(W[p * n_comp + j]);
                change += diff * diff;
            }
            change = sqrtf(change);
            if (change > max_change) max_change = change;

            // Update W
            for (uint32_t j = 0; j < n_comp; j++) {
                W[p * n_comp + j] = w_new[j];
            }
        }

        if (max_change < ica->tolerance) {
            ica->n_iter = iter + 1;
            break;
        }
        ica->n_iter = iter + 1;
    }

    // Compute unmixing matrix: W_full = W @ whitening
    // W is (n_comp x n_comp), whitening is (n_comp x n_features)
    for (uint32_t i = 0; i < n_comp; i++) {
        for (uint32_t j = 0; j < n_features; j++) {
            float sum = 0.0f;
            for (uint32_t k = 0; k < n_comp; k++) {
                sum += W[i * n_comp + k] * ica->whitening[k * n_features + j];
            }
            ica->unmixing[i * n_features + j] = sum;
        }
    }

    // Mixing matrix is pseudo-inverse of unmixing
    nimcp_mv_pinv(ica->unmixing, n_comp, n_features, ica->mixing, NIMCP_MV_MIN_EIGENVALUE);

    // Cleanup
    nimcp_free(W); nimcp_free(WWT); nimcp_free(eigenvalues); nimcp_free(eigenvectors);
    nimcp_free(g); nimcp_free(g_prime); nimcp_free(wx); nimcp_free(w_new);
    nimcp_pca_destroy(pca);
    nimcp_free(X_centered);
    nimcp_free(X_white);

    ica->is_fitted = true;
    return NIMCP_MV_OK;
}

nimcp_mv_result_t nimcp_ica_transform(
    const nimcp_ica_t* ica,
    const float* X,
    uint32_t n_samples,
    float* S)
{
    if (!ica || !X || !S) {
        return NIMCP_MV_ERROR_NULL;
    }
    if (!ica->is_fitted) {
        return NIMCP_MV_ERROR_NOT_FIT;
    }

    // S = (X - mean) @ W^T
    for (uint32_t i = 0; i < n_samples; i++) {
        for (uint32_t j = 0; j < ica->n_components; j++) {
            float sum = 0.0f;
            for (uint32_t k = 0; k < ica->n_features; k++) {
                float centered = X[i * ica->n_features + k] - ica->mean[k];
                sum += centered * ica->unmixing[j * ica->n_features + k];
            }
            S[i * ica->n_components + j] = sum;
        }
    }

    return NIMCP_MV_OK;
}

nimcp_mv_result_t nimcp_ica_get_mixing_matrix(
    const nimcp_ica_t* ica,
    float* A)
{
    if (!ica || !A) return NIMCP_MV_ERROR_NULL;
    if (!ica->is_fitted) return NIMCP_MV_ERROR_NOT_FIT;

    memcpy(A, ica->mixing, ica->n_features * ica->n_components * sizeof(float));
    return NIMCP_MV_OK;
}

nimcp_mv_result_t nimcp_ica_get_unmixing_matrix(
    const nimcp_ica_t* ica,
    float* W)
{
    if (!ica || !W) return NIMCP_MV_ERROR_NULL;
    if (!ica->is_fitted) return NIMCP_MV_ERROR_NOT_FIT;

    memcpy(W, ica->unmixing, ica->n_components * ica->n_features * sizeof(float));
    return NIMCP_MV_OK;
}

nimcp_mv_result_t nimcp_ica_fit_transform(
    nimcp_ica_t* ica,
    const float* X,
    uint32_t n_samples,
    uint32_t n_features,
    float* S)
{
    nimcp_mv_result_t res = nimcp_ica_fit(ica, X, n_samples, n_features);
    if (res != NIMCP_MV_OK) return res;
    return nimcp_ica_transform(ica, X, n_samples, S);
}

//=============================================================================
// Factor Analysis Implementation
//=============================================================================

nimcp_factor_t* nimcp_factor_create(
    uint32_t n_factors,
    nimcp_factor_rotation_t rotation)
{
    if (n_factors == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_factor_create: n_factors is zero");
        return NULL;
    }

    nimcp_factor_t* fa = (nimcp_factor_t*)nimcp_calloc(1, sizeof(nimcp_factor_t));
    if (!fa) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate Factor context");
        return NULL;
    }

    fa->n_factors = n_factors;
    fa->rotation = rotation;
    fa->tolerance = NIMCP_MV_EPSILON;
    fa->max_iter = NIMCP_MV_FA_MAX_ITERATIONS;
    fa->is_fitted = false;

    return fa;
}

void nimcp_factor_destroy(nimcp_factor_t* fa) {
    if (!fa) return;

    nimcp_free(fa->loadings);
    nimcp_free(fa->communalities);
    nimcp_free(fa->uniquenesses);
    nimcp_free(fa->factor_variance);
    nimcp_free(fa->mean);
    nimcp_free(fa->rotation_matrix);
    nimcp_free(fa->factor_correlation);
    nimcp_free(fa);
}

// Varimax rotation implementation
static void varimax_rotation(float* loadings, uint32_t n_vars, uint32_t n_factors,
                             float* rotation_matrix, uint32_t max_iter, float tol) {
    // Initialize rotation matrix to identity
    for (uint32_t i = 0; i < n_factors; i++) {
        for (uint32_t j = 0; j < n_factors; j++) {
            rotation_matrix[i * n_factors + j] = (i == j) ? 1.0f : 0.0f;
        }
    }

    float* rotated = (float*)nimcp_malloc(n_vars * n_factors * sizeof(float));
    if (!rotated) return;
    memcpy(rotated, loadings, n_vars * n_factors * sizeof(float));

    for (uint32_t iter = 0; iter < max_iter; iter++) {
        float max_change = 0.0f;

        // Rotate pairs of factors
        for (uint32_t p = 0; p < n_factors - 1; p++) {
            for (uint32_t q = p + 1; q < n_factors; q++) {
                // Compute rotation angle using varimax criterion
                float num = 0.0f, denom = 0.0f;

                for (uint32_t i = 0; i < n_vars; i++) {
                    float x = rotated[i * n_factors + p];
                    float y = rotated[i * n_factors + q];
                    float u = x * x - y * y;
                    float v = 2.0f * x * y;
                    num += u * v;
                    denom += u * u - v * v;
                }

                float phi = 0.25f * atan2f(2.0f * num, denom);
                float c = cosf(phi);
                float s = sinf(phi);

                if (fabsf(phi) > max_change) max_change = fabsf(phi);

                // Apply rotation
                for (uint32_t i = 0; i < n_vars; i++) {
                    float x = rotated[i * n_factors + p];
                    float y = rotated[i * n_factors + q];
                    rotated[i * n_factors + p] = c * x + s * y;
                    rotated[i * n_factors + q] = -s * x + c * y;
                }

                // Update rotation matrix
                for (uint32_t i = 0; i < n_factors; i++) {
                    float x = rotation_matrix[i * n_factors + p];
                    float y = rotation_matrix[i * n_factors + q];
                    rotation_matrix[i * n_factors + p] = c * x + s * y;
                    rotation_matrix[i * n_factors + q] = -s * x + c * y;
                }
            }
        }

        if (max_change < tol) break;
    }

    memcpy(loadings, rotated, n_vars * n_factors * sizeof(float));
    nimcp_free(rotated);
}

nimcp_mv_result_t nimcp_factor_fit(
    nimcp_factor_t* fa,
    const float* X,
    uint32_t n_samples,
    uint32_t n_features)
{
    if (!fa || !X) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL in Factor fit");
        return NIMCP_MV_ERROR_NULL;
    }
    if (n_samples < fa->n_factors + 2 || n_features < fa->n_factors) {
        return NIMCP_MV_ERROR_SIZE;
    }

    fa->n_features = n_features;

    // Allocate
    fa->loadings = (float*)nimcp_malloc(n_features * fa->n_factors * sizeof(float));
    fa->communalities = (float*)nimcp_malloc(n_features * sizeof(float));
    fa->uniquenesses = (float*)nimcp_malloc(n_features * sizeof(float));
    fa->factor_variance = (float*)nimcp_malloc(fa->n_factors * sizeof(float));
    fa->mean = (float*)nimcp_malloc(n_features * sizeof(float));
    fa->rotation_matrix = (float*)nimcp_malloc(fa->n_factors * fa->n_factors * sizeof(float));

    if (!fa->loadings || !fa->communalities || !fa->uniquenesses ||
        !fa->factor_variance || !fa->mean || !fa->rotation_matrix) {
        return NIMCP_MV_ERROR_MEMORY;
    }

    // Center data
    float* X_centered = (float*)nimcp_malloc(n_samples * n_features * sizeof(float));
    if (!X_centered) return NIMCP_MV_ERROR_MEMORY;

    nimcp_mv_center(X, n_samples, n_features, X_centered, fa->mean);

    // Compute covariance matrix
    float* cov = (float*)nimcp_malloc(n_features * n_features * sizeof(float));
    if (!cov) {
        nimcp_free(X_centered);
        return NIMCP_MV_ERROR_MEMORY;
    }
    nimcp_mv_covariance(X_centered, n_samples, n_features, cov, 1);

    // Initialize uniquenesses from diagonal (inverse of communalities)
    for (uint32_t i = 0; i < n_features; i++) {
        fa->uniquenesses[i] = cov[i * n_features + i] * 0.5f; // Start with half variance
    }

    // EM iterations for factor analysis
    float* eigenvalues = (float*)nimcp_malloc(n_features * sizeof(float));
    float* eigenvectors = (float*)nimcp_malloc(n_features * n_features * sizeof(float));
    float* reduced_cov = (float*)nimcp_malloc(n_features * n_features * sizeof(float));

    if (!eigenvalues || !eigenvectors || !reduced_cov) {
        nimcp_free(X_centered); nimcp_free(cov); nimcp_free(eigenvalues);
        nimcp_free(eigenvectors); nimcp_free(reduced_cov);
        return NIMCP_MV_ERROR_MEMORY;
    }

    for (uint32_t iter = 0; iter < fa->max_iter; iter++) {
        // Reduced covariance: cov - diag(uniquenesses)
        memcpy(reduced_cov, cov, n_features * n_features * sizeof(float));
        for (uint32_t i = 0; i < n_features; i++) {
            reduced_cov[i * n_features + i] -= fa->uniquenesses[i];
        }

        // Eigendecomposition
        nimcp_mv_result_t res = nimcp_mv_eigh(reduced_cov, n_features, eigenvalues, eigenvectors);
        if (res != NIMCP_MV_OK) {
            nimcp_free(X_centered); nimcp_free(cov); nimcp_free(eigenvalues);
            nimcp_free(eigenvectors); nimcp_free(reduced_cov);
            return res;
        }

        // Extract top n_factors (eigenvalues are ascending, so take from end)
        for (uint32_t i = 0; i < fa->n_factors; i++) {
            uint32_t idx = n_features - 1 - i;
            float eigenval = eigenvalues[idx];
            if (eigenval < 0) eigenval = 0;

            for (uint32_t j = 0; j < n_features; j++) {
                fa->loadings[j * fa->n_factors + i] =
                    eigenvectors[j * n_features + idx] * sqrtf(eigenval);
            }
        }

        // Update communalities and uniquenesses
        float max_change = 0.0f;
        for (uint32_t i = 0; i < n_features; i++) {
            float comm = 0.0f;
            for (uint32_t j = 0; j < fa->n_factors; j++) {
                comm += fa->loadings[i * fa->n_factors + j] * fa->loadings[i * fa->n_factors + j];
            }
            fa->communalities[i] = comm;

            float new_uniq = cov[i * n_features + i] - comm;
            if (new_uniq < NIMCP_MV_EPSILON) new_uniq = NIMCP_MV_EPSILON;

            float change = fabsf(new_uniq - fa->uniquenesses[i]);
            if (change > max_change) max_change = change;
            fa->uniquenesses[i] = new_uniq;
        }

        fa->n_iter = iter + 1;
        if (max_change < fa->tolerance) break;
    }

    // Compute factor variance
    for (uint32_t j = 0; j < fa->n_factors; j++) {
        float var = 0.0f;
        for (uint32_t i = 0; i < n_features; i++) {
            var += fa->loadings[i * fa->n_factors + j] * fa->loadings[i * fa->n_factors + j];
        }
        fa->factor_variance[j] = var;
    }

    // Apply rotation if requested
    if (fa->rotation == NIMCP_FACTOR_ROTATION_VARIMAX) {
        varimax_rotation(fa->loadings, n_features, fa->n_factors,
                         fa->rotation_matrix, 100, fa->tolerance);
    }

    nimcp_free(X_centered); nimcp_free(cov); nimcp_free(eigenvalues);
    nimcp_free(eigenvectors); nimcp_free(reduced_cov);

    fa->is_fitted = true;
    return NIMCP_MV_OK;
}

nimcp_mv_result_t nimcp_factor_get_loadings(
    const nimcp_factor_t* fa,
    float* loadings)
{
    if (!fa || !loadings) return NIMCP_MV_ERROR_NULL;
    if (!fa->is_fitted) return NIMCP_MV_ERROR_NOT_FIT;

    memcpy(loadings, fa->loadings, fa->n_features * fa->n_factors * sizeof(float));
    return NIMCP_MV_OK;
}

nimcp_mv_result_t nimcp_factor_get_communalities(
    const nimcp_factor_t* fa,
    float* communalities)
{
    if (!fa || !communalities) return NIMCP_MV_ERROR_NULL;
    if (!fa->is_fitted) return NIMCP_MV_ERROR_NOT_FIT;

    memcpy(communalities, fa->communalities, fa->n_features * sizeof(float));
    return NIMCP_MV_OK;
}

nimcp_mv_result_t nimcp_factor_scores(
    const nimcp_factor_t* fa,
    const float* X,
    uint32_t n_samples,
    float* scores)
{
    if (!fa || !X || !scores) return NIMCP_MV_ERROR_NULL;
    if (!fa->is_fitted) return NIMCP_MV_ERROR_NOT_FIT;

    // Regression method: scores = (X - mean) @ L @ (L^T L)^-1
    // Where L is loadings matrix

    // Compute L^T @ L
    float* LtL = (float*)nimcp_malloc(fa->n_factors * fa->n_factors * sizeof(float));
    if (!LtL) return NIMCP_MV_ERROR_MEMORY;

    for (uint32_t i = 0; i < fa->n_factors; i++) {
        for (uint32_t j = 0; j < fa->n_factors; j++) {
            float sum = 0.0f;
            for (uint32_t k = 0; k < fa->n_features; k++) {
                sum += fa->loadings[k * fa->n_factors + i] * fa->loadings[k * fa->n_factors + j];
            }
            LtL[i * fa->n_factors + j] = sum;
        }
    }

    // Invert L^T L
    float* LtL_inv = (float*)nimcp_malloc(fa->n_factors * fa->n_factors * sizeof(float));
    if (!LtL_inv) {
        nimcp_free(LtL);
        return NIMCP_MV_ERROR_MEMORY;
    }

    // Simple 2x2 or use pseudo-inverse for general case
    nimcp_mv_pinv(LtL, fa->n_factors, fa->n_factors, LtL_inv, NIMCP_MV_MIN_EIGENVALUE);

    // Compute scores
    for (uint32_t s = 0; s < n_samples; s++) {
        // temp = (X - mean) @ L
        float* temp = (float*)nimcp_malloc(fa->n_factors * sizeof(float));
        if (!temp) {
            nimcp_free(LtL); nimcp_free(LtL_inv);
            return NIMCP_MV_ERROR_MEMORY;
        }

        for (uint32_t j = 0; j < fa->n_factors; j++) {
            float sum = 0.0f;
            for (uint32_t k = 0; k < fa->n_features; k++) {
                float centered = X[s * fa->n_features + k] - fa->mean[k];
                sum += centered * fa->loadings[k * fa->n_factors + j];
            }
            temp[j] = sum;
        }

        // scores = temp @ LtL_inv
        for (uint32_t j = 0; j < fa->n_factors; j++) {
            float sum = 0.0f;
            for (uint32_t k = 0; k < fa->n_factors; k++) {
                sum += temp[k] * LtL_inv[k * fa->n_factors + j];
            }
            scores[s * fa->n_factors + j] = sum;
        }

        nimcp_free(temp);
    }

    nimcp_free(LtL); nimcp_free(LtL_inv);
    return NIMCP_MV_OK;
}

nimcp_mv_result_t nimcp_factor_rotate(
    nimcp_factor_t* fa,
    nimcp_factor_rotation_t rotation)
{
    if (!fa) return NIMCP_MV_ERROR_NULL;
    if (!fa->is_fitted) return NIMCP_MV_ERROR_NOT_FIT;

    if (rotation == NIMCP_FACTOR_ROTATION_VARIMAX) {
        varimax_rotation(fa->loadings, fa->n_features, fa->n_factors,
                         fa->rotation_matrix, 100, fa->tolerance);
    }

    fa->rotation = rotation;
    return NIMCP_MV_OK;
}

//=============================================================================
// LDA Implementation
//=============================================================================

nimcp_lda_t* nimcp_lda_create(
    nimcp_lda_solver_t solver,
    uint32_t n_components)
{
    nimcp_lda_t* lda = (nimcp_lda_t*)nimcp_calloc(1, sizeof(nimcp_lda_t));
    if (!lda) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate LDA context");
        return NULL;
    }

    lda->solver = solver;
    lda->n_components = n_components;
    lda->shrinkage = 0.0f;
    lda->is_fitted = false;

    return lda;
}

void nimcp_lda_destroy(nimcp_lda_t* lda) {
    if (!lda) return;

    nimcp_free(lda->means);
    nimcp_free(lda->priors);
    nimcp_free(lda->coef);
    nimcp_free(lda->intercept);
    nimcp_free(lda->scalings);
    nimcp_free(lda->explained_variance_ratio);
    nimcp_free(lda->xbar);
    nimcp_free(lda);
}

nimcp_mv_result_t nimcp_lda_fit(
    nimcp_lda_t* lda,
    const float* X,
    const uint32_t* y,
    uint32_t n_samples,
    uint32_t n_features)
{
    if (!lda || !X || !y) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL in LDA fit");
        return NIMCP_MV_ERROR_NULL;
    }

    // Find number of classes
    uint32_t max_class = 0;
    for (uint32_t i = 0; i < n_samples; i++) {
        if (y[i] > max_class) max_class = y[i];
    }
    uint32_t n_classes = max_class + 1;

    if (n_classes < 2) {
        return NIMCP_MV_ERROR_LABELS;
    }

    lda->n_features = n_features;
    lda->n_classes = n_classes;

    // Number of discriminant components
    uint32_t max_components = (n_classes - 1 < n_features) ? n_classes - 1 : n_features;
    if (lda->n_components == 0 || lda->n_components > max_components) {
        lda->n_components = max_components;
    }

    // Count samples per class
    uint32_t* class_counts = (uint32_t*)nimcp_calloc(n_classes, sizeof(uint32_t));
    if (!class_counts) return NIMCP_MV_ERROR_MEMORY;

    for (uint32_t i = 0; i < n_samples; i++) {
        class_counts[y[i]]++;
    }

    // Allocate
    lda->means = (float*)nimcp_calloc(n_classes * n_features, sizeof(float));
    lda->priors = (float*)nimcp_malloc(n_classes * sizeof(float));
    lda->xbar = (float*)nimcp_calloc(n_features, sizeof(float));
    lda->scalings = (float*)nimcp_malloc(lda->n_components * n_features * sizeof(float));
    lda->coef = (float*)nimcp_malloc(n_classes * n_features * sizeof(float));
    lda->intercept = (float*)nimcp_malloc(n_classes * sizeof(float));
    lda->explained_variance_ratio = (float*)nimcp_malloc(lda->n_components * sizeof(float));

    if (!lda->means || !lda->priors || !lda->xbar || !lda->scalings ||
        !lda->coef || !lda->intercept || !lda->explained_variance_ratio) {
        nimcp_free(class_counts);
        return NIMCP_MV_ERROR_MEMORY;
    }

    // Compute class means
    for (uint32_t i = 0; i < n_samples; i++) {
        uint32_t c = y[i];
        for (uint32_t j = 0; j < n_features; j++) {
            lda->means[c * n_features + j] += X[i * n_features + j];
        }
    }
    for (uint32_t c = 0; c < n_classes; c++) {
        if (class_counts[c] > 0) {
            for (uint32_t j = 0; j < n_features; j++) {
                lda->means[c * n_features + j] /= (float)class_counts[c];
            }
        }
        lda->priors[c] = (float)class_counts[c] / (float)n_samples;
    }

    // Global mean
    for (uint32_t i = 0; i < n_samples; i++) {
        for (uint32_t j = 0; j < n_features; j++) {
            lda->xbar[j] += X[i * n_features + j];
        }
    }
    for (uint32_t j = 0; j < n_features; j++) {
        lda->xbar[j] /= (float)n_samples;
    }

    // Within-class scatter matrix Sw
    float* Sw = (float*)nimcp_calloc(n_features * n_features, sizeof(float));
    if (!Sw) {
        nimcp_free(class_counts);
        return NIMCP_MV_ERROR_MEMORY;
    }

    for (uint32_t i = 0; i < n_samples; i++) {
        uint32_t c = y[i];
        for (uint32_t j = 0; j < n_features; j++) {
            float dj = X[i * n_features + j] - lda->means[c * n_features + j];
            for (uint32_t k = j; k < n_features; k++) {
                float dk = X[i * n_features + k] - lda->means[c * n_features + k];
                Sw[j * n_features + k] += dj * dk;
            }
        }
    }
    // Symmetrize
    for (uint32_t j = 0; j < n_features; j++) {
        for (uint32_t k = j + 1; k < n_features; k++) {
            Sw[k * n_features + j] = Sw[j * n_features + k];
        }
    }

    // Regularization
    if (lda->shrinkage > 0.0f) {
        float trace = 0.0f;
        for (uint32_t j = 0; j < n_features; j++) {
            trace += Sw[j * n_features + j];
        }
        trace /= (float)n_features;
        for (uint32_t j = 0; j < n_features; j++) {
            Sw[j * n_features + j] += lda->shrinkage * trace;
        }
    }

    // Between-class scatter matrix Sb
    float* Sb = (float*)nimcp_calloc(n_features * n_features, sizeof(float));
    if (!Sb) {
        nimcp_free(class_counts); nimcp_free(Sw);
        return NIMCP_MV_ERROR_MEMORY;
    }

    for (uint32_t c = 0; c < n_classes; c++) {
        float nc = (float)class_counts[c];
        for (uint32_t j = 0; j < n_features; j++) {
            float dj = lda->means[c * n_features + j] - lda->xbar[j];
            for (uint32_t k = j; k < n_features; k++) {
                float dk = lda->means[c * n_features + k] - lda->xbar[k];
                Sb[j * n_features + k] += nc * dj * dk;
            }
        }
    }
    // Symmetrize
    for (uint32_t j = 0; j < n_features; j++) {
        for (uint32_t k = j + 1; k < n_features; k++) {
            Sb[k * n_features + j] = Sb[j * n_features + k];
        }
    }

    // Solve generalized eigenvalue problem: Sb @ v = lambda @ Sw @ v
    // Using Sw^-1 @ Sb if Sw is invertible
    float* Sw_inv_Sb = (float*)nimcp_malloc(n_features * n_features * sizeof(float));
    float* eigenvalues = (float*)nimcp_malloc(n_features * sizeof(float));
    float* eigenvectors = (float*)nimcp_malloc(n_features * n_features * sizeof(float));

    if (!Sw_inv_Sb || !eigenvalues || !eigenvectors) {
        nimcp_free(class_counts); nimcp_free(Sw); nimcp_free(Sb);
        nimcp_free(Sw_inv_Sb); nimcp_free(eigenvalues); nimcp_free(eigenvectors);
        return NIMCP_MV_ERROR_MEMORY;
    }

    // Sw^-1 @ Sb using solve
    nimcp_mv_result_t res = nimcp_mv_solve(Sw, Sb, n_features, n_features, Sw_inv_Sb);
    if (res != NIMCP_MV_OK) {
        // Fall back to pseudo-inverse
        float* Sw_pinv = (float*)nimcp_malloc(n_features * n_features * sizeof(float));
        if (Sw_pinv) {
            nimcp_mv_pinv(Sw, n_features, n_features, Sw_pinv, NIMCP_MV_MIN_EIGENVALUE);
            matrix_multiply_simple(Sw_pinv, Sb, Sw_inv_Sb, n_features, n_features, n_features);
            nimcp_free(Sw_pinv);
        }
    }

    // Eigendecomposition
    res = nimcp_mv_eigh(Sw_inv_Sb, n_features, eigenvalues, eigenvectors);

    // Extract top n_components eigenvectors (from end, ascending order)
    float total_eigenval = 0.0f;
    for (uint32_t i = 0; i < lda->n_components; i++) {
        uint32_t idx = n_features - 1 - i;
        if (eigenvalues[idx] > 0) total_eigenval += eigenvalues[idx];
    }

    for (uint32_t i = 0; i < lda->n_components; i++) {
        uint32_t idx = n_features - 1 - i;
        for (uint32_t j = 0; j < n_features; j++) {
            lda->scalings[i * n_features + j] = eigenvectors[j * n_features + idx];
        }
        lda->explained_variance_ratio[i] = (eigenvalues[idx] > 0) ?
            eigenvalues[idx] / total_eigenval : 0.0f;
    }

    // Compute classification coefficients
    // coef[c] = means[c] @ Sw_inv, intercept[c] = -0.5 * means[c] @ Sw_inv @ means[c] + log(prior[c])
    float* Sw_inv = (float*)nimcp_malloc(n_features * n_features * sizeof(float));
    if (Sw_inv) {
        nimcp_mv_pinv(Sw, n_features, n_features, Sw_inv, NIMCP_MV_MIN_EIGENVALUE);

        for (uint32_t c = 0; c < n_classes; c++) {
            // coef[c] = means[c] @ Sw_inv
            for (uint32_t j = 0; j < n_features; j++) {
                float sum = 0.0f;
                for (uint32_t k = 0; k < n_features; k++) {
                    sum += lda->means[c * n_features + k] * Sw_inv[k * n_features + j];
                }
                lda->coef[c * n_features + j] = sum;
            }

            // intercept[c] = -0.5 * means[c] @ coef[c] + log(prior[c])
            float dot = 0.0f;
            for (uint32_t j = 0; j < n_features; j++) {
                dot += lda->means[c * n_features + j] * lda->coef[c * n_features + j];
            }
            lda->intercept[c] = -0.5f * dot + logf(lda->priors[c] + NIMCP_MV_EPSILON);
        }
        nimcp_free(Sw_inv);
    }

    nimcp_free(class_counts); nimcp_free(Sw); nimcp_free(Sb);
    nimcp_free(Sw_inv_Sb); nimcp_free(eigenvalues); nimcp_free(eigenvectors);

    lda->is_fitted = true;
    return NIMCP_MV_OK;
}

nimcp_mv_result_t nimcp_lda_transform(
    const nimcp_lda_t* lda,
    const float* X,
    uint32_t n_samples,
    float* X_transformed)
{
    if (!lda || !X || !X_transformed) return NIMCP_MV_ERROR_NULL;
    if (!lda->is_fitted) return NIMCP_MV_ERROR_NOT_FIT;

    // X_t = (X - xbar) @ scalings^T
    for (uint32_t i = 0; i < n_samples; i++) {
        for (uint32_t j = 0; j < lda->n_components; j++) {
            float sum = 0.0f;
            for (uint32_t k = 0; k < lda->n_features; k++) {
                float centered = X[i * lda->n_features + k] - lda->xbar[k];
                sum += centered * lda->scalings[j * lda->n_features + k];
            }
            X_transformed[i * lda->n_components + j] = sum;
        }
    }

    return NIMCP_MV_OK;
}

nimcp_mv_result_t nimcp_lda_predict(
    const nimcp_lda_t* lda,
    const float* X,
    uint32_t n_samples,
    uint32_t* y_pred)
{
    if (!lda || !X || !y_pred) return NIMCP_MV_ERROR_NULL;
    if (!lda->is_fitted) return NIMCP_MV_ERROR_NOT_FIT;

    for (uint32_t i = 0; i < n_samples; i++) {
        float max_score = -FLT_MAX;
        uint32_t best_class = 0;

        for (uint32_t c = 0; c < lda->n_classes; c++) {
            float score = lda->intercept[c];
            for (uint32_t j = 0; j < lda->n_features; j++) {
                score += X[i * lda->n_features + j] * lda->coef[c * lda->n_features + j];
            }

            if (score > max_score) {
                max_score = score;
                best_class = c;
            }
        }

        y_pred[i] = best_class;
    }

    return NIMCP_MV_OK;
}

nimcp_mv_result_t nimcp_lda_predict_proba(
    const nimcp_lda_t* lda,
    const float* X,
    uint32_t n_samples,
    float* proba)
{
    if (!lda || !X || !proba) return NIMCP_MV_ERROR_NULL;
    if (!lda->is_fitted) return NIMCP_MV_ERROR_NOT_FIT;

    for (uint32_t i = 0; i < n_samples; i++) {
        // Compute log-likelihoods
        float max_log = -FLT_MAX;
        for (uint32_t c = 0; c < lda->n_classes; c++) {
            float log_like = lda->intercept[c];
            for (uint32_t j = 0; j < lda->n_features; j++) {
                log_like += X[i * lda->n_features + j] * lda->coef[c * lda->n_features + j];
            }
            proba[i * lda->n_classes + c] = log_like;
            if (log_like > max_log) max_log = log_like;
        }

        // Softmax normalization
        float sum = 0.0f;
        for (uint32_t c = 0; c < lda->n_classes; c++) {
            proba[i * lda->n_classes + c] = expf(proba[i * lda->n_classes + c] - max_log);
            sum += proba[i * lda->n_classes + c];
        }
        for (uint32_t c = 0; c < lda->n_classes; c++) {
            proba[i * lda->n_classes + c] /= sum;
        }
    }

    return NIMCP_MV_OK;
}

nimcp_mv_result_t nimcp_lda_decision_function(
    const nimcp_lda_t* lda,
    const float* X,
    uint32_t n_samples,
    float* log_likelihood)
{
    if (!lda || !X || !log_likelihood) return NIMCP_MV_ERROR_NULL;
    if (!lda->is_fitted) return NIMCP_MV_ERROR_NOT_FIT;

    for (uint32_t i = 0; i < n_samples; i++) {
        for (uint32_t c = 0; c < lda->n_classes; c++) {
            float score = lda->intercept[c];
            for (uint32_t j = 0; j < lda->n_features; j++) {
                score += X[i * lda->n_features + j] * lda->coef[c * lda->n_features + j];
            }
            log_likelihood[i * lda->n_classes + c] = score;
        }
    }

    return NIMCP_MV_OK;
}

//=============================================================================
// QDA Implementation
//=============================================================================

nimcp_qda_t* nimcp_qda_create(float reg_param) {
    nimcp_qda_t* qda = (nimcp_qda_t*)nimcp_calloc(1, sizeof(nimcp_qda_t));
    if (!qda) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate QDA context");
        return NULL;
    }

    qda->reg_param = (reg_param > 0) ? reg_param : NIMCP_MV_QDA_REG_DEFAULT;
    qda->is_fitted = false;

    return qda;
}

void nimcp_qda_destroy(nimcp_qda_t* qda) {
    if (!qda) return;

    nimcp_free(qda->means);
    nimcp_free(qda->priors);
    nimcp_free(qda->log_det);

    if (qda->covariances) {
        for (uint32_t c = 0; c < qda->n_classes; c++) {
            nimcp_free(qda->covariances[c]);
        }
        nimcp_free(qda->covariances);
    }
    if (qda->covariance_inv) {
        for (uint32_t c = 0; c < qda->n_classes; c++) {
            nimcp_free(qda->covariance_inv[c]);
        }
        nimcp_free(qda->covariance_inv);
    }

    nimcp_free(qda);
}

nimcp_mv_result_t nimcp_qda_fit(
    nimcp_qda_t* qda,
    const float* X,
    const uint32_t* y,
    uint32_t n_samples,
    uint32_t n_features)
{
    if (!qda || !X || !y) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL in QDA fit");
        return NIMCP_MV_ERROR_NULL;
    }

    // Find number of classes
    uint32_t max_class = 0;
    for (uint32_t i = 0; i < n_samples; i++) {
        if (y[i] > max_class) max_class = y[i];
    }
    uint32_t n_classes = max_class + 1;

    qda->n_features = n_features;
    qda->n_classes = n_classes;

    // Count per class
    uint32_t* class_counts = (uint32_t*)nimcp_calloc(n_classes, sizeof(uint32_t));
    if (!class_counts) return NIMCP_MV_ERROR_MEMORY;

    for (uint32_t i = 0; i < n_samples; i++) {
        class_counts[y[i]]++;
    }

    // Allocate
    qda->means = (float*)nimcp_calloc(n_classes * n_features, sizeof(float));
    qda->priors = (float*)nimcp_malloc(n_classes * sizeof(float));
    qda->log_det = (float*)nimcp_malloc(n_classes * sizeof(float));
    qda->covariances = (float**)nimcp_calloc(n_classes, sizeof(float*));
    qda->covariance_inv = (float**)nimcp_calloc(n_classes, sizeof(float*));

    if (!qda->means || !qda->priors || !qda->log_det ||
        !qda->covariances || !qda->covariance_inv) {
        nimcp_free(class_counts);
        return NIMCP_MV_ERROR_MEMORY;
    }

    for (uint32_t c = 0; c < n_classes; c++) {
        qda->covariances[c] = (float*)nimcp_calloc(n_features * n_features, sizeof(float));
        qda->covariance_inv[c] = (float*)nimcp_malloc(n_features * n_features * sizeof(float));
        if (!qda->covariances[c] || !qda->covariance_inv[c]) {
            nimcp_free(class_counts);
            return NIMCP_MV_ERROR_MEMORY;
        }
    }

    // Compute class means
    for (uint32_t i = 0; i < n_samples; i++) {
        uint32_t c = y[i];
        for (uint32_t j = 0; j < n_features; j++) {
            qda->means[c * n_features + j] += X[i * n_features + j];
        }
    }
    for (uint32_t c = 0; c < n_classes; c++) {
        if (class_counts[c] > 0) {
            for (uint32_t j = 0; j < n_features; j++) {
                qda->means[c * n_features + j] /= (float)class_counts[c];
            }
        }
        qda->priors[c] = (float)class_counts[c] / (float)n_samples;
    }

    // Compute class covariances
    for (uint32_t i = 0; i < n_samples; i++) {
        uint32_t c = y[i];
        for (uint32_t j = 0; j < n_features; j++) {
            float dj = X[i * n_features + j] - qda->means[c * n_features + j];
            for (uint32_t k = j; k < n_features; k++) {
                float dk = X[i * n_features + k] - qda->means[c * n_features + k];
                qda->covariances[c][j * n_features + k] += dj * dk;
            }
        }
    }

    // Normalize and regularize covariances, compute inverses
    for (uint32_t c = 0; c < n_classes; c++) {
        float denom = (class_counts[c] > 1) ? (float)(class_counts[c] - 1) : 1.0f;

        for (uint32_t j = 0; j < n_features; j++) {
            for (uint32_t k = j; k < n_features; k++) {
                qda->covariances[c][j * n_features + k] /= denom;
                qda->covariances[c][k * n_features + j] = qda->covariances[c][j * n_features + k];
            }
            // Regularization
            qda->covariances[c][j * n_features + j] += qda->reg_param;
        }

        // Compute inverse and log determinant
        float* eigenvalues = (float*)nimcp_malloc(n_features * sizeof(float));
        float* eigenvectors = (float*)nimcp_malloc(n_features * n_features * sizeof(float));

        if (eigenvalues && eigenvectors) {
            nimcp_mv_eigh(qda->covariances[c], n_features, eigenvalues, eigenvectors);

            // Log determinant
            float log_det = 0.0f;
            for (uint32_t j = 0; j < n_features; j++) {
                if (eigenvalues[j] > NIMCP_MV_EPSILON) {
                    log_det += logf(eigenvalues[j]);
                }
            }
            qda->log_det[c] = log_det;

            // Compute pseudo-inverse: V @ diag(1/eigenvalues) @ V^T
            for (uint32_t j = 0; j < n_features; j++) {
                for (uint32_t k = 0; k < n_features; k++) {
                    float sum = 0.0f;
                    for (uint32_t l = 0; l < n_features; l++) {
                        if (eigenvalues[l] > NIMCP_MV_EPSILON) {
                            sum += eigenvectors[j * n_features + l] *
                                   (1.0f / eigenvalues[l]) *
                                   eigenvectors[k * n_features + l];
                        }
                    }
                    qda->covariance_inv[c][j * n_features + k] = sum;
                }
            }
        }

        nimcp_free(eigenvalues);
        nimcp_free(eigenvectors);
    }

    nimcp_free(class_counts);
    qda->is_fitted = true;
    return NIMCP_MV_OK;
}

nimcp_mv_result_t nimcp_qda_predict(
    const nimcp_qda_t* qda,
    const float* X,
    uint32_t n_samples,
    uint32_t* y_pred)
{
    if (!qda || !X || !y_pred) return NIMCP_MV_ERROR_NULL;
    if (!qda->is_fitted) return NIMCP_MV_ERROR_NOT_FIT;

    float* diff = (float*)nimcp_malloc(qda->n_features * sizeof(float));
    if (!diff) return NIMCP_MV_ERROR_MEMORY;

    for (uint32_t i = 0; i < n_samples; i++) {
        float max_score = -FLT_MAX;
        uint32_t best_class = 0;

        for (uint32_t c = 0; c < qda->n_classes; c++) {
            // diff = x - mean[c]
            for (uint32_t j = 0; j < qda->n_features; j++) {
                diff[j] = X[i * qda->n_features + j] - qda->means[c * qda->n_features + j];
            }

            // Mahalanobis distance: diff @ cov_inv @ diff
            float mahal = 0.0f;
            for (uint32_t j = 0; j < qda->n_features; j++) {
                float sum = 0.0f;
                for (uint32_t k = 0; k < qda->n_features; k++) {
                    sum += qda->covariance_inv[c][j * qda->n_features + k] * diff[k];
                }
                mahal += diff[j] * sum;
            }

            // Log posterior: -0.5 * (log_det + mahal) + log(prior)
            float score = -0.5f * (qda->log_det[c] + mahal) +
                          logf(qda->priors[c] + NIMCP_MV_EPSILON);

            if (score > max_score) {
                max_score = score;
                best_class = c;
            }
        }

        y_pred[i] = best_class;
    }

    nimcp_free(diff);
    return NIMCP_MV_OK;
}

nimcp_mv_result_t nimcp_qda_predict_proba(
    const nimcp_qda_t* qda,
    const float* X,
    uint32_t n_samples,
    float* proba)
{
    if (!qda || !X || !proba) return NIMCP_MV_ERROR_NULL;
    if (!qda->is_fitted) return NIMCP_MV_ERROR_NOT_FIT;

    float* diff = (float*)nimcp_malloc(qda->n_features * sizeof(float));
    if (!diff) return NIMCP_MV_ERROR_MEMORY;

    for (uint32_t i = 0; i < n_samples; i++) {
        float max_log = -FLT_MAX;

        for (uint32_t c = 0; c < qda->n_classes; c++) {
            for (uint32_t j = 0; j < qda->n_features; j++) {
                diff[j] = X[i * qda->n_features + j] - qda->means[c * qda->n_features + j];
            }

            float mahal = 0.0f;
            for (uint32_t j = 0; j < qda->n_features; j++) {
                float sum = 0.0f;
                for (uint32_t k = 0; k < qda->n_features; k++) {
                    sum += qda->covariance_inv[c][j * qda->n_features + k] * diff[k];
                }
                mahal += diff[j] * sum;
            }

            float log_prob = -0.5f * (qda->log_det[c] + mahal) +
                             logf(qda->priors[c] + NIMCP_MV_EPSILON);
            proba[i * qda->n_classes + c] = log_prob;
            if (log_prob > max_log) max_log = log_prob;
        }

        // Softmax
        float sum = 0.0f;
        for (uint32_t c = 0; c < qda->n_classes; c++) {
            proba[i * qda->n_classes + c] = expf(proba[i * qda->n_classes + c] - max_log);
            sum += proba[i * qda->n_classes + c];
        }
        for (uint32_t c = 0; c < qda->n_classes; c++) {
            proba[i * qda->n_classes + c] /= sum;
        }
    }

    nimcp_free(diff);
    return NIMCP_MV_OK;
}

//=============================================================================
// CCA Implementation
//=============================================================================

nimcp_cca_t* nimcp_cca_create(uint32_t n_components, bool scale) {
    if (n_components == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_cca_create: n_components is zero");
        return NULL;
    }

    nimcp_cca_t* cca = (nimcp_cca_t*)nimcp_calloc(1, sizeof(nimcp_cca_t));
    if (!cca) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate CCA context");
        return NULL;
    }

    cca->n_components = n_components;
    cca->scale = scale;
    cca->tolerance = NIMCP_MV_EPSILON;
    cca->is_fitted = false;

    return cca;
}

void nimcp_cca_destroy(nimcp_cca_t* cca) {
    if (!cca) return;

    nimcp_free(cca->x_weights);
    nimcp_free(cca->y_weights);
    nimcp_free(cca->x_loadings);
    nimcp_free(cca->y_loadings);
    nimcp_free(cca->correlations);
    nimcp_free(cca->x_mean);
    nimcp_free(cca->y_mean);
    nimcp_free(cca->x_std);
    nimcp_free(cca->y_std);
    nimcp_free(cca);
}

nimcp_mv_result_t nimcp_cca_fit(
    nimcp_cca_t* cca,
    const float* X,
    const float* Y,
    uint32_t n_samples,
    uint32_t n_features_x,
    uint32_t n_features_y)
{
    if (!cca || !X || !Y) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL in CCA fit");
        return NIMCP_MV_ERROR_NULL;
    }

    uint32_t min_feat = (n_features_x < n_features_y) ? n_features_x : n_features_y;
    if (cca->n_components > min_feat) {
        cca->n_components = min_feat;
    }
    if (n_samples < cca->n_components + 2) {
        return NIMCP_MV_ERROR_SIZE;
    }

    cca->n_features_x = n_features_x;
    cca->n_features_y = n_features_y;

    // Allocate
    cca->x_weights = (float*)nimcp_malloc(n_features_x * cca->n_components * sizeof(float));
    cca->y_weights = (float*)nimcp_malloc(n_features_y * cca->n_components * sizeof(float));
    cca->correlations = (float*)nimcp_malloc(cca->n_components * sizeof(float));
    cca->x_mean = (float*)nimcp_malloc(n_features_x * sizeof(float));
    cca->y_mean = (float*)nimcp_malloc(n_features_y * sizeof(float));

    if (!cca->x_weights || !cca->y_weights || !cca->correlations ||
        !cca->x_mean || !cca->y_mean) {
        return NIMCP_MV_ERROR_MEMORY;
    }

    if (cca->scale) {
        cca->x_std = (float*)nimcp_malloc(n_features_x * sizeof(float));
        cca->y_std = (float*)nimcp_malloc(n_features_y * sizeof(float));
    }

    // Center (and optionally scale) data
    float* X_c = (float*)nimcp_malloc(n_samples * n_features_x * sizeof(float));
    float* Y_c = (float*)nimcp_malloc(n_samples * n_features_y * sizeof(float));

    if (!X_c || !Y_c) {
        nimcp_free(X_c); nimcp_free(Y_c);
        return NIMCP_MV_ERROR_MEMORY;
    }

    if (cca->scale) {
        nimcp_mv_standardize(X, n_samples, n_features_x, X_c, cca->x_mean, cca->x_std);
        nimcp_mv_standardize(Y, n_samples, n_features_y, Y_c, cca->y_mean, cca->y_std);
    } else {
        nimcp_mv_center(X, n_samples, n_features_x, X_c, cca->x_mean);
        nimcp_mv_center(Y, n_samples, n_features_y, Y_c, cca->y_mean);
    }

    // Compute covariance matrices
    float* Cxx = (float*)nimcp_calloc(n_features_x * n_features_x, sizeof(float));
    float* Cyy = (float*)nimcp_calloc(n_features_y * n_features_y, sizeof(float));
    float* Cxy = (float*)nimcp_calloc(n_features_x * n_features_y, sizeof(float));

    if (!Cxx || !Cyy || !Cxy) {
        nimcp_free(X_c); nimcp_free(Y_c); nimcp_free(Cxx); nimcp_free(Cyy); nimcp_free(Cxy);
        return NIMCP_MV_ERROR_MEMORY;
    }

    float denom = (float)(n_samples - 1);

    // Cxx = X^T @ X / (n-1)
    nimcp_mv_covariance(X_c, n_samples, n_features_x, Cxx, 1);

    // Cyy = Y^T @ Y / (n-1)
    nimcp_mv_covariance(Y_c, n_samples, n_features_y, Cyy, 1);

    // Cxy = X^T @ Y / (n-1)
    for (uint32_t i = 0; i < n_features_x; i++) {
        for (uint32_t j = 0; j < n_features_y; j++) {
            float sum = 0.0f;
            for (uint32_t k = 0; k < n_samples; k++) {
                sum += X_c[k * n_features_x + i] * Y_c[k * n_features_y + j];
            }
            Cxy[i * n_features_y + j] = sum / denom;
        }
    }

    // Regularize
    for (uint32_t i = 0; i < n_features_x; i++) {
        Cxx[i * n_features_x + i] += NIMCP_MV_EPSILON;
    }
    for (uint32_t i = 0; i < n_features_y; i++) {
        Cyy[i * n_features_y + i] += NIMCP_MV_EPSILON;
    }

    // Compute Cxx^{-1/2} and Cyy^{-1/2} via eigen decomposition
    float* Cxx_eig = (float*)nimcp_malloc(n_features_x * sizeof(float));
    float* Cxx_vec = (float*)nimcp_malloc(n_features_x * n_features_x * sizeof(float));
    float* Cyy_eig = (float*)nimcp_malloc(n_features_y * sizeof(float));
    float* Cyy_vec = (float*)nimcp_malloc(n_features_y * n_features_y * sizeof(float));

    if (!Cxx_eig || !Cxx_vec || !Cyy_eig || !Cyy_vec) {
        nimcp_free(X_c); nimcp_free(Y_c); nimcp_free(Cxx); nimcp_free(Cyy); nimcp_free(Cxy);
        nimcp_free(Cxx_eig); nimcp_free(Cxx_vec); nimcp_free(Cyy_eig); nimcp_free(Cyy_vec);
        return NIMCP_MV_ERROR_MEMORY;
    }

    nimcp_mv_eigh(Cxx, n_features_x, Cxx_eig, Cxx_vec);
    nimcp_mv_eigh(Cyy, n_features_y, Cyy_eig, Cyy_vec);

    // Cxx^{-1/2} = V @ diag(1/sqrt(eigenvalues)) @ V^T
    float* Cxx_inv_sqrt = (float*)nimcp_calloc(n_features_x * n_features_x, sizeof(float));
    float* Cyy_inv_sqrt = (float*)nimcp_calloc(n_features_y * n_features_y, sizeof(float));

    if (!Cxx_inv_sqrt || !Cyy_inv_sqrt) {
        nimcp_free(X_c); nimcp_free(Y_c); nimcp_free(Cxx); nimcp_free(Cyy); nimcp_free(Cxy);
        nimcp_free(Cxx_eig); nimcp_free(Cxx_vec); nimcp_free(Cyy_eig); nimcp_free(Cyy_vec);
        nimcp_free(Cxx_inv_sqrt); nimcp_free(Cyy_inv_sqrt);
        return NIMCP_MV_ERROR_MEMORY;
    }

    for (uint32_t i = 0; i < n_features_x; i++) {
        for (uint32_t j = 0; j < n_features_x; j++) {
            float sum = 0.0f;
            for (uint32_t k = 0; k < n_features_x; k++) {
                if (Cxx_eig[k] > NIMCP_MV_EPSILON) {
                    sum += Cxx_vec[i * n_features_x + k] *
                           (1.0f / sqrtf(Cxx_eig[k])) *
                           Cxx_vec[j * n_features_x + k];
                }
            }
            Cxx_inv_sqrt[i * n_features_x + j] = sum;
        }
    }

    for (uint32_t i = 0; i < n_features_y; i++) {
        for (uint32_t j = 0; j < n_features_y; j++) {
            float sum = 0.0f;
            for (uint32_t k = 0; k < n_features_y; k++) {
                if (Cyy_eig[k] > NIMCP_MV_EPSILON) {
                    sum += Cyy_vec[i * n_features_y + k] *
                           (1.0f / sqrtf(Cyy_eig[k])) *
                           Cyy_vec[j * n_features_y + k];
                }
            }
            Cyy_inv_sqrt[i * n_features_y + j] = sum;
        }
    }

    // M = Cxx^{-1/2} @ Cxy @ Cyy^{-1/2}
    float* temp = (float*)nimcp_malloc(n_features_x * n_features_y * sizeof(float));
    float* M = (float*)nimcp_malloc(n_features_x * n_features_y * sizeof(float));

    if (!temp || !M) {
        nimcp_free(X_c); nimcp_free(Y_c); nimcp_free(Cxx); nimcp_free(Cyy); nimcp_free(Cxy);
        nimcp_free(Cxx_eig); nimcp_free(Cxx_vec); nimcp_free(Cyy_eig); nimcp_free(Cyy_vec);
        nimcp_free(Cxx_inv_sqrt); nimcp_free(Cyy_inv_sqrt); nimcp_free(temp); nimcp_free(M);
        return NIMCP_MV_ERROR_MEMORY;
    }

    // temp = Cxx^{-1/2} @ Cxy
    matrix_multiply_simple(Cxx_inv_sqrt, Cxy, temp, n_features_x, n_features_x, n_features_y);

    // M = temp @ Cyy^{-1/2}
    matrix_multiply_simple(temp, Cyy_inv_sqrt, M, n_features_x, n_features_y, n_features_y);

    // SVD of M
    uint32_t min_dim = (n_features_x < n_features_y) ? n_features_x : n_features_y;
    float* U = (float*)nimcp_malloc(n_features_x * min_dim * sizeof(float));
    float* S = (float*)nimcp_malloc(min_dim * sizeof(float));
    float* Vt = (float*)nimcp_malloc(min_dim * n_features_y * sizeof(float));

    if (!U || !S || !Vt) {
        nimcp_free(X_c); nimcp_free(Y_c); nimcp_free(Cxx); nimcp_free(Cyy); nimcp_free(Cxy);
        nimcp_free(Cxx_eig); nimcp_free(Cxx_vec); nimcp_free(Cyy_eig); nimcp_free(Cyy_vec);
        nimcp_free(Cxx_inv_sqrt); nimcp_free(Cyy_inv_sqrt); nimcp_free(temp); nimcp_free(M);
        nimcp_free(U); nimcp_free(S); nimcp_free(Vt);
        return NIMCP_MV_ERROR_MEMORY;
    }

    nimcp_mv_svd(M, n_features_x, n_features_y, U, S, Vt, false);

    // Store canonical correlations
    for (uint32_t i = 0; i < cca->n_components; i++) {
        cca->correlations[i] = (i < min_dim) ? S[i] : 0.0f;
    }

    // x_weights = Cxx^{-1/2} @ U
    // y_weights = Cyy^{-1/2} @ V (V = Vt^T)
    for (uint32_t i = 0; i < n_features_x; i++) {
        for (uint32_t j = 0; j < cca->n_components; j++) {
            float sum = 0.0f;
            for (uint32_t k = 0; k < n_features_x; k++) {
                float u_val = (j < min_dim) ? U[k * min_dim + j] : 0.0f;
                sum += Cxx_inv_sqrt[i * n_features_x + k] * u_val;
            }
            cca->x_weights[i * cca->n_components + j] = sum;
        }
    }

    for (uint32_t i = 0; i < n_features_y; i++) {
        for (uint32_t j = 0; j < cca->n_components; j++) {
            float sum = 0.0f;
            for (uint32_t k = 0; k < n_features_y; k++) {
                float v_val = (j < min_dim) ? Vt[j * n_features_y + k] : 0.0f;
                sum += Cyy_inv_sqrt[i * n_features_y + k] * v_val;
            }
            cca->y_weights[i * cca->n_components + j] = sum;
        }
    }

    // Cleanup
    nimcp_free(X_c); nimcp_free(Y_c); nimcp_free(Cxx); nimcp_free(Cyy); nimcp_free(Cxy);
    nimcp_free(Cxx_eig); nimcp_free(Cxx_vec); nimcp_free(Cyy_eig); nimcp_free(Cyy_vec);
    nimcp_free(Cxx_inv_sqrt); nimcp_free(Cyy_inv_sqrt); nimcp_free(temp); nimcp_free(M);
    nimcp_free(U); nimcp_free(S); nimcp_free(Vt);

    cca->is_fitted = true;
    return NIMCP_MV_OK;
}

nimcp_mv_result_t nimcp_cca_transform(
    const nimcp_cca_t* cca,
    const float* X,
    const float* Y,
    uint32_t n_samples,
    float* X_c,
    float* Y_c)
{
    if (!cca || !X || !Y || !X_c || !Y_c) return NIMCP_MV_ERROR_NULL;
    if (!cca->is_fitted) return NIMCP_MV_ERROR_NOT_FIT;

    // X_c = (X - mean) @ weights (optionally with scaling)
    for (uint32_t i = 0; i < n_samples; i++) {
        for (uint32_t j = 0; j < cca->n_components; j++) {
            float sum_x = 0.0f;
            float sum_y = 0.0f;

            for (uint32_t k = 0; k < cca->n_features_x; k++) {
                float val = X[i * cca->n_features_x + k] - cca->x_mean[k];
                if (cca->scale && cca->x_std) {
                    val /= cca->x_std[k];
                }
                sum_x += val * cca->x_weights[k * cca->n_components + j];
            }

            for (uint32_t k = 0; k < cca->n_features_y; k++) {
                float val = Y[i * cca->n_features_y + k] - cca->y_mean[k];
                if (cca->scale && cca->y_std) {
                    val /= cca->y_std[k];
                }
                sum_y += val * cca->y_weights[k * cca->n_components + j];
            }

            X_c[i * cca->n_components + j] = sum_x;
            Y_c[i * cca->n_components + j] = sum_y;
        }
    }

    return NIMCP_MV_OK;
}

nimcp_mv_result_t nimcp_cca_correlations(
    const nimcp_cca_t* cca,
    float* correlations)
{
    if (!cca || !correlations) return NIMCP_MV_ERROR_NULL;
    if (!cca->is_fitted) return NIMCP_MV_ERROR_NOT_FIT;

    memcpy(correlations, cca->correlations, cca->n_components * sizeof(float));
    return NIMCP_MV_OK;
}

nimcp_mv_result_t nimcp_cca_fit_transform(
    nimcp_cca_t* cca,
    const float* X,
    const float* Y,
    uint32_t n_samples,
    uint32_t n_features_x,
    uint32_t n_features_y,
    float* X_c,
    float* Y_c)
{
    nimcp_mv_result_t res = nimcp_cca_fit(cca, X, Y, n_samples, n_features_x, n_features_y);
    if (res != NIMCP_MV_OK) return res;
    return nimcp_cca_transform(cca, X, Y, n_samples, X_c, Y_c);
}

nimcp_mv_result_t nimcp_cca_cross_loadings(
    const nimcp_cca_t* cca,
    const float* X,
    const float* Y,
    uint32_t n_samples,
    float* x_cross_loadings,
    float* y_cross_loadings)
{
    if (!cca || !X || !Y || !x_cross_loadings || !y_cross_loadings) {
        return NIMCP_MV_ERROR_NULL;
    }
    if (!cca->is_fitted) return NIMCP_MV_ERROR_NOT_FIT;

    // Transform to canonical space
    float* X_c = (float*)nimcp_malloc(n_samples * cca->n_components * sizeof(float));
    float* Y_c = (float*)nimcp_malloc(n_samples * cca->n_components * sizeof(float));

    if (!X_c || !Y_c) {
        nimcp_free(X_c); nimcp_free(Y_c);
        return NIMCP_MV_ERROR_MEMORY;
    }

    nimcp_cca_transform(cca, X, Y, n_samples, X_c, Y_c);

    // x_cross_loadings[j, k] = corr(X[:, j], Y_c[:, k])
    for (uint32_t j = 0; j < cca->n_features_x; j++) {
        float x_mean = cca->x_mean[j];
        float x_var = 0.0f;
        for (uint32_t i = 0; i < n_samples; i++) {
            float diff = X[i * cca->n_features_x + j] - x_mean;
            x_var += diff * diff;
        }
        x_var = sqrtf(x_var / (n_samples - 1));

        for (uint32_t k = 0; k < cca->n_components; k++) {
            float cov = 0.0f;
            float yc_var = 0.0f;
            float yc_mean = 0.0f;

            for (uint32_t i = 0; i < n_samples; i++) {
                yc_mean += Y_c[i * cca->n_components + k];
            }
            yc_mean /= n_samples;

            for (uint32_t i = 0; i < n_samples; i++) {
                float x_diff = X[i * cca->n_features_x + j] - x_mean;
                float y_diff = Y_c[i * cca->n_components + k] - yc_mean;
                cov += x_diff * y_diff;
                yc_var += y_diff * y_diff;
            }
            cov /= (n_samples - 1);
            yc_var = sqrtf(yc_var / (n_samples - 1));

            if (x_var > NIMCP_MV_EPSILON && yc_var > NIMCP_MV_EPSILON) {
                x_cross_loadings[j * cca->n_components + k] = cov / (x_var * yc_var);
            } else {
                x_cross_loadings[j * cca->n_components + k] = 0.0f;
            }
        }
    }

    // Similarly for y_cross_loadings
    for (uint32_t j = 0; j < cca->n_features_y; j++) {
        float y_mean = cca->y_mean[j];
        float y_var = 0.0f;
        for (uint32_t i = 0; i < n_samples; i++) {
            float diff = Y[i * cca->n_features_y + j] - y_mean;
            y_var += diff * diff;
        }
        y_var = sqrtf(y_var / (n_samples - 1));

        for (uint32_t k = 0; k < cca->n_components; k++) {
            float cov = 0.0f;
            float xc_var = 0.0f;
            float xc_mean = 0.0f;

            for (uint32_t i = 0; i < n_samples; i++) {
                xc_mean += X_c[i * cca->n_components + k];
            }
            xc_mean /= n_samples;

            for (uint32_t i = 0; i < n_samples; i++) {
                float y_diff = Y[i * cca->n_features_y + j] - y_mean;
                float x_diff = X_c[i * cca->n_components + k] - xc_mean;
                cov += y_diff * x_diff;
                xc_var += x_diff * x_diff;
            }
            cov /= (n_samples - 1);
            xc_var = sqrtf(xc_var / (n_samples - 1));

            if (y_var > NIMCP_MV_EPSILON && xc_var > NIMCP_MV_EPSILON) {
                y_cross_loadings[j * cca->n_components + k] = cov / (y_var * xc_var);
            } else {
                y_cross_loadings[j * cca->n_components + k] = 0.0f;
            }
        }
    }

    nimcp_free(X_c); nimcp_free(Y_c);
    return NIMCP_MV_OK;
}
