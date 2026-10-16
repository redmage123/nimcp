/**
 * @file nimcp_linear_algebra.h
 * @brief Dense linear algebra engine (up to 64x64)
 *
 * Matrix operations, LU/QR/SVD decompositions, eigenvalues,
 * solvers, norms, rank, null space.
 */

#ifndef NIMCP_LINEAR_ALGEBRA_H
#define NIMCP_LINEAR_ALGEBRA_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- constants ---------- */
#define LINALG_MAX_DIM        64
#define LINALG_EPSILON        1e-12
#define LINALG_QR_MAX_ITER    200
#define LINALG_SVD_MAX_ITER   200

/* ---------- matrix type ---------- */

typedef struct matrix_s {
    double* data;       /* row-major: data[i*cols + j] */
    int     rows;
    int     cols;
} matrix_t;

/* ---------- LU decomposition result ---------- */

typedef struct lu_decomp_s {
    matrix_t* L;
    matrix_t* U;
    int*      pivot;    /* permutation vector, length rows */
    int       n;
    int       sign;     /* +1 or -1 for determinant sign */
} lu_decomp_t;

/* ---------- QR decomposition result ---------- */

typedef struct qr_decomp_s {
    matrix_t* Q;
    matrix_t* R;
} qr_decomp_t;

/* ---------- SVD result ---------- */

typedef struct svd_result_s {
    matrix_t* U;        /* m x m */
    double*   sigma;    /* min(m,n) singular values */
    matrix_t* Vt;       /* n x n */
    int       n_sigma;
} svd_result_t;

/* ---------- eigenvalue result ---------- */

typedef struct eigen_result_s {
    double* eigenvalues;    /* n real eigenvalues (symmetric only) */
    matrix_t* eigenvectors; /* columns are eigenvectors */
    int     n;
} eigen_result_t;

/* ---------- lifecycle ---------- */
matrix_t* matrix_create(int rows, int cols);
matrix_t* matrix_create_identity(int n);
matrix_t* matrix_clone(const matrix_t* src);
void      matrix_destroy(matrix_t* m);

/* ---------- element access ---------- */
static inline double matrix_get(const matrix_t* m, int r, int c) {
    return m->data[r * m->cols + c];
}
static inline void matrix_set(matrix_t* m, int r, int c, double v) {
    m->data[r * m->cols + c] = v;
}

/* ---------- basic operations ---------- */
matrix_t* matrix_add(const matrix_t* a, const matrix_t* b);
matrix_t* matrix_multiply(const matrix_t* a, const matrix_t* b);
matrix_t* matrix_transpose(const matrix_t* a);
matrix_t* matrix_scale(const matrix_t* a, double s);
double    matrix_trace(const matrix_t* a);

/* ---------- decompositions ---------- */
lu_decomp_t*    matrix_lu(const matrix_t* a);
void            lu_decomp_free(lu_decomp_t* lu);

qr_decomp_t*   matrix_qr(const matrix_t* a);
void            qr_decomp_free(qr_decomp_t* qr);

eigen_result_t* matrix_eigen_symmetric(const matrix_t* a);
void            eigen_result_free(eigen_result_t* e);

svd_result_t*   matrix_svd(const matrix_t* a);
void            svd_result_free(svd_result_t* s);

/* ---------- determinant + inverse ---------- */
double    matrix_determinant(const matrix_t* a);
matrix_t* matrix_inverse(const matrix_t* a);

/* ---------- solvers ---------- */
/** Solve Ax=b via LU decomposition. Returns x (n x 1). */
matrix_t* matrix_solve_lu(const matrix_t* a, const matrix_t* b);

/** Least-squares solve via QR: min ||Ax - b||. Returns x. */
matrix_t* matrix_solve_least_squares(const matrix_t* a, const matrix_t* b);

/** Condition number = sigma_max / sigma_min */
double    matrix_condition_number(const matrix_t* a);

/* ---------- norms ---------- */
double matrix_norm_frobenius(const matrix_t* a);
double matrix_norm_1(const matrix_t* a);
double matrix_norm_inf(const matrix_t* a);
double matrix_norm_spectral(const matrix_t* a);

/* ---------- rank and null space ---------- */
int       matrix_rank(const matrix_t* a);
matrix_t* matrix_null_space(const matrix_t* a, int* null_dim);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LINEAR_ALGEBRA_H */
