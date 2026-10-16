/**
 * @file nimcp_linear_algebra.c
 * @brief Dense linear algebra engine implementation
 */

#include "cognitive/math/nimcp_linear_algebra.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "LINALG"

/* ================================================================
 * LIFECYCLE
 * ================================================================ */

matrix_t* matrix_create(int rows, int cols) {
    if (rows <= 0 || cols <= 0 || rows > LINALG_MAX_DIM || cols > LINALG_MAX_DIM) {
        LOG_ERROR(LOG_TAG, "Invalid matrix dimensions %dx%d (max %d)", rows, cols, LINALG_MAX_DIM);
        return NULL;
    }
    matrix_t* m = (matrix_t*)nimcp_calloc(1, sizeof(matrix_t));
    if (!m) return NULL;
    m->rows = rows;
    m->cols = cols;
    m->data = (double*)nimcp_calloc((size_t)(rows * cols), sizeof(double));
    if (!m->data) { nimcp_free(m); return NULL; }
    return m;
}

matrix_t* matrix_create_identity(int n) {
    matrix_t* m = matrix_create(n, n);
    if (!m) return NULL;
    for (int i = 0; i < n; i++) m->data[i * n + i] = 1.0;
    return m;
}

matrix_t* matrix_clone(const matrix_t* src) {
    if (!src) return NULL;
    matrix_t* m = matrix_create(src->rows, src->cols);
    if (!m) return NULL;
    memcpy(m->data, src->data, (size_t)(src->rows * src->cols) * sizeof(double));
    return m;
}

void matrix_destroy(matrix_t* m) {
    if (!m) return;
    nimcp_free(m->data);
    nimcp_free(m);
}

/* ================================================================
 * BASIC OPERATIONS
 * ================================================================ */

matrix_t* matrix_add(const matrix_t* a, const matrix_t* b) {
    if (!a || !b || a->rows != b->rows || a->cols != b->cols) return NULL;
    matrix_t* r = matrix_create(a->rows, a->cols);
    if (!r) return NULL;
    int n = a->rows * a->cols;
    for (int i = 0; i < n; i++) r->data[i] = a->data[i] + b->data[i];
    return r;
}

matrix_t* matrix_multiply(const matrix_t* a, const matrix_t* b) {
    if (!a || !b || a->cols != b->rows) return NULL;
    matrix_t* r = matrix_create(a->rows, b->cols);
    if (!r) return NULL;
    for (int i = 0; i < a->rows; i++) {
        for (int k = 0; k < a->cols; k++) {
            double a_ik = a->data[i * a->cols + k];
            if (a_ik == 0.0) continue;
            for (int j = 0; j < b->cols; j++) {
                r->data[i * b->cols + j] += a_ik * b->data[k * b->cols + j];
            }
        }
    }
    return r;
}

matrix_t* matrix_transpose(const matrix_t* a) {
    if (!a) return NULL;
    matrix_t* r = matrix_create(a->cols, a->rows);
    if (!r) return NULL;
    for (int i = 0; i < a->rows; i++)
        for (int j = 0; j < a->cols; j++)
            r->data[j * a->rows + i] = a->data[i * a->cols + j];
    return r;
}

matrix_t* matrix_scale(const matrix_t* a, double s) {
    if (!a) return NULL;
    matrix_t* r = matrix_clone(a);
    if (!r) return NULL;
    int n = r->rows * r->cols;
    for (int i = 0; i < n; i++) r->data[i] *= s;
    return r;
}

double matrix_trace(const matrix_t* a) {
    if (!a || a->rows != a->cols) return 0.0;
    double tr = 0.0;
    for (int i = 0; i < a->rows; i++) tr += a->data[i * a->cols + i];
    return tr;
}

/* ================================================================
 * LU DECOMPOSITION (partial pivoting)
 * ================================================================ */

lu_decomp_t* matrix_lu(const matrix_t* a) {
    if (!a || a->rows != a->cols) return NULL;
    int n = a->rows;

    lu_decomp_t* lu = (lu_decomp_t*)nimcp_calloc(1, sizeof(lu_decomp_t));
    if (!lu) return NULL;
    lu->n = n;
    lu->sign = 1;
    lu->pivot = (int*)nimcp_calloc((size_t)n, sizeof(int));
    lu->L = matrix_create_identity(n);
    lu->U = matrix_clone(a);
    if (!lu->pivot || !lu->L || !lu->U) { lu_decomp_free(lu); return NULL; }

    for (int i = 0; i < n; i++) lu->pivot[i] = i;

    for (int k = 0; k < n; k++) {
        /* Partial pivoting: find max in column k below diagonal */
        double max_val = fabs(lu->U->data[k * n + k]);
        int max_row = k;
        for (int i = k + 1; i < n; i++) {
            double v = fabs(lu->U->data[i * n + k]);
            if (v > max_val) { max_val = v; max_row = i; }
        }

        if (max_val < LINALG_EPSILON) {
            LOG_WARN(LOG_TAG, "LU: near-singular matrix at pivot %d", k);
            continue;
        }

        if (max_row != k) {
            /* Swap rows in U */
            for (int j = 0; j < n; j++) {
                double tmp = lu->U->data[k * n + j];
                lu->U->data[k * n + j] = lu->U->data[max_row * n + j];
                lu->U->data[max_row * n + j] = tmp;
            }
            /* Swap rows in L (below diagonal only) */
            for (int j = 0; j < k; j++) {
                double tmp = lu->L->data[k * n + j];
                lu->L->data[k * n + j] = lu->L->data[max_row * n + j];
                lu->L->data[max_row * n + j] = tmp;
            }
            int tmp = lu->pivot[k]; lu->pivot[k] = lu->pivot[max_row]; lu->pivot[max_row] = tmp;
            lu->sign *= -1;
        }

        double pivot = lu->U->data[k * n + k];
        for (int i = k + 1; i < n; i++) {
            double factor = lu->U->data[i * n + k] / pivot;
            lu->L->data[i * n + k] = factor;
            for (int j = k; j < n; j++) {
                lu->U->data[i * n + j] -= factor * lu->U->data[k * n + j];
            }
        }
    }
    return lu;
}

void lu_decomp_free(lu_decomp_t* lu) {
    if (!lu) return;
    matrix_destroy(lu->L);
    matrix_destroy(lu->U);
    nimcp_free(lu->pivot);
    nimcp_free(lu);
}

/* ================================================================
 * QR DECOMPOSITION (modified Gram-Schmidt)
 * ================================================================ */

qr_decomp_t* matrix_qr(const matrix_t* a) {
    if (!a) return NULL;
    int m = a->rows, n = a->cols;

    qr_decomp_t* qr = (qr_decomp_t*)nimcp_calloc(1, sizeof(qr_decomp_t));
    if (!qr) return NULL;
    qr->Q = matrix_create(m, n);
    qr->R = matrix_create(n, n);
    if (!qr->Q || !qr->R) { qr_decomp_free(qr); return NULL; }

    /* Copy columns of A into Q */
    for (int i = 0; i < m; i++)
        for (int j = 0; j < n; j++)
            qr->Q->data[i * n + j] = a->data[i * a->cols + j];

    /* Modified Gram-Schmidt */
    for (int j = 0; j < n; j++) {
        /* Orthogonalize column j against all previous columns */
        for (int i = 0; i < j; i++) {
            double dot = 0.0;
            for (int k = 0; k < m; k++)
                dot += qr->Q->data[k * n + i] * qr->Q->data[k * n + j];
            qr->R->data[i * n + j] = dot;
            for (int k = 0; k < m; k++)
                qr->Q->data[k * n + j] -= dot * qr->Q->data[k * n + i];
        }
        /* Normalize */
        double norm = 0.0;
        for (int k = 0; k < m; k++)
            norm += qr->Q->data[k * n + j] * qr->Q->data[k * n + j];
        norm = sqrt(norm);
        qr->R->data[j * n + j] = norm;
        if (norm > LINALG_EPSILON) {
            for (int k = 0; k < m; k++)
                qr->Q->data[k * n + j] /= norm;
        }
    }
    return qr;
}

void qr_decomp_free(qr_decomp_t* qr) {
    if (!qr) return;
    matrix_destroy(qr->Q);
    matrix_destroy(qr->R);
    nimcp_free(qr);
}

/* ================================================================
 * EIGENVALUES (QR algorithm for symmetric matrices)
 * ================================================================ */

eigen_result_t* matrix_eigen_symmetric(const matrix_t* a) {
    if (!a || a->rows != a->cols) return NULL;
    int n = a->rows;

    eigen_result_t* e = (eigen_result_t*)nimcp_calloc(1, sizeof(eigen_result_t));
    if (!e) return NULL;
    e->n = n;
    e->eigenvalues = (double*)nimcp_calloc((size_t)n, sizeof(double));
    e->eigenvectors = matrix_create_identity(n);
    if (!e->eigenvalues || !e->eigenvectors) { eigen_result_free(e); return NULL; }

    matrix_t* work = matrix_clone(a);
    if (!work) { eigen_result_free(e); return NULL; }

    /* QR iteration */
    for (int iter = 0; iter < LINALG_QR_MAX_ITER; iter++) {
        /* Wilkinson shift: use bottom-right 2x2 eigenvalue closest to a[n-1][n-1] */
        double shift = 0.0;
        if (n >= 2) {
            double a11 = work->data[(n-2)*n + (n-2)];
            double a12 = work->data[(n-2)*n + (n-1)];
            double a22 = work->data[(n-1)*n + (n-1)];
            double d = (a11 - a22) / 2.0;
            double sign_d = (d >= 0) ? 1.0 : -1.0;
            shift = a22 - a12 * a12 / (d + sign_d * sqrt(d * d + a12 * a12));
        }

        /* Shift */
        for (int i = 0; i < n; i++) work->data[i * n + i] -= shift;

        qr_decomp_t* qr = matrix_qr(work);
        if (!qr) break;

        /* work = R * Q + shift * I */
        matrix_t* rq = matrix_multiply(qr->R, qr->Q);
        if (rq) {
            for (int i = 0; i < n; i++) rq->data[i * n + i] += shift;
            memcpy(work->data, rq->data, (size_t)(n * n) * sizeof(double));

            /* Accumulate eigenvectors: V = V * Q */
            matrix_t* vq = matrix_multiply(e->eigenvectors, qr->Q);
            if (vq) {
                memcpy(e->eigenvectors->data, vq->data, (size_t)(n * n) * sizeof(double));
                matrix_destroy(vq);
            }
            matrix_destroy(rq);
        }
        qr_decomp_free(qr);

        /* Check convergence: off-diagonal elements near zero */
        double off_diag = 0.0;
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                if (i != j) off_diag += fabs(work->data[i * n + j]);
        if (off_diag < LINALG_EPSILON * (double)n) {
            LOG_DEBUG(LOG_TAG, "QR eigenvalue converged in %d iterations", iter + 1);
            break;
        }
    }

    for (int i = 0; i < n; i++) e->eigenvalues[i] = work->data[i * n + i];
    matrix_destroy(work);
    return e;
}

void eigen_result_free(eigen_result_t* e) {
    if (!e) return;
    nimcp_free(e->eigenvalues);
    matrix_destroy(e->eigenvectors);
    nimcp_free(e);
}

/* ================================================================
 * SVD (via eigendecomposition of A^T A)
 * ================================================================ */

svd_result_t* matrix_svd(const matrix_t* a) {
    if (!a) return NULL;

    matrix_t* at = matrix_transpose(a);
    if (!at) return NULL;
    matrix_t* ata = matrix_multiply(at, a);
    matrix_destroy(at);
    if (!ata) return NULL;

    eigen_result_t* eig = matrix_eigen_symmetric(ata);
    matrix_destroy(ata);
    if (!eig) return NULL;

    int n_sigma = (a->rows < a->cols) ? a->rows : a->cols;
    svd_result_t* s = (svd_result_t*)nimcp_calloc(1, sizeof(svd_result_t));
    if (!s) { eigen_result_free(eig); return NULL; }
    s->n_sigma = n_sigma;
    s->sigma = (double*)nimcp_calloc((size_t)n_sigma, sizeof(double));
    s->Vt = matrix_transpose(eig->eigenvectors);
    if (!s->sigma || !s->Vt) { svd_result_free(s); eigen_result_free(eig); return NULL; }

    /* Singular values = sqrt of eigenvalues (sorted descending) */
    for (int i = 0; i < n_sigma; i++) {
        double ev = (i < eig->n) ? eig->eigenvalues[i] : 0.0;
        s->sigma[i] = (ev > 0.0) ? sqrt(ev) : 0.0;
    }
    /* Sort descending with corresponding columns of V */
    for (int i = 0; i < n_sigma - 1; i++) {
        for (int j = i + 1; j < n_sigma; j++) {
            if (s->sigma[j] > s->sigma[i]) {
                double tmp = s->sigma[i]; s->sigma[i] = s->sigma[j]; s->sigma[j] = tmp;
                /* Swap rows of Vt */
                if (s->Vt) {
                    for (int c = 0; c < s->Vt->cols; c++) {
                        double t = s->Vt->data[i * s->Vt->cols + c];
                        s->Vt->data[i * s->Vt->cols + c] = s->Vt->data[j * s->Vt->cols + c];
                        s->Vt->data[j * s->Vt->cols + c] = t;
                    }
                }
            }
        }
    }

    /* U = A * V * Sigma^{-1} */
    matrix_t* v = matrix_transpose(s->Vt);
    int U_cols = (a->rows < a->cols) ? a->rows : a->cols;
    s->U = matrix_create(a->rows, U_cols);
    if (v && s->U) {
        matrix_t* av = matrix_multiply(a, v);
        if (av) {
            for (int j = 0; j < n_sigma && j < U_cols; j++) {
                if (s->sigma[j] > LINALG_EPSILON) {
                    for (int i = 0; i < a->rows; i++)
                        s->U->data[i * U_cols + j] = av->data[i * av->cols + j] / s->sigma[j];
                }
            }
            matrix_destroy(av);
        }
    }
    matrix_destroy(v);
    eigen_result_free(eig);
    return s;
}

void svd_result_free(svd_result_t* s) {
    if (!s) return;
    matrix_destroy(s->U);
    nimcp_free(s->sigma);
    matrix_destroy(s->Vt);
    nimcp_free(s);
}

/* ================================================================
 * DETERMINANT + INVERSE
 * ================================================================ */

double matrix_determinant(const matrix_t* a) {
    if (!a || a->rows != a->cols) return 0.0;
    lu_decomp_t* lu = matrix_lu(a);
    if (!lu) return 0.0;
    double det = (double)lu->sign;
    for (int i = 0; i < lu->n; i++) det *= lu->U->data[i * lu->n + i];
    lu_decomp_free(lu);
    return det;
}

matrix_t* matrix_inverse(const matrix_t* a) {
    if (!a || a->rows != a->cols) return NULL;
    int n = a->rows;

    /* Gauss-Jordan elimination on [A | I] */
    matrix_t* aug = matrix_create(n, 2 * n);
    if (!aug) return NULL;

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) aug->data[i * 2 * n + j] = a->data[i * n + j];
        aug->data[i * 2 * n + n + i] = 1.0;
    }

    for (int k = 0; k < n; k++) {
        /* Partial pivot */
        int max_row = k;
        double max_val = fabs(aug->data[k * 2 * n + k]);
        for (int i = k + 1; i < n; i++) {
            double v = fabs(aug->data[i * 2 * n + k]);
            if (v > max_val) { max_val = v; max_row = i; }
        }
        if (max_val < LINALG_EPSILON) {
            LOG_WARN(LOG_TAG, "Matrix is singular, cannot invert");
            matrix_destroy(aug);
            return NULL;
        }
        if (max_row != k) {
            for (int j = 0; j < 2 * n; j++) {
                double tmp = aug->data[k * 2 * n + j];
                aug->data[k * 2 * n + j] = aug->data[max_row * 2 * n + j];
                aug->data[max_row * 2 * n + j] = tmp;
            }
        }

        double pivot = aug->data[k * 2 * n + k];
        for (int j = 0; j < 2 * n; j++) aug->data[k * 2 * n + j] /= pivot;

        for (int i = 0; i < n; i++) {
            if (i == k) continue;
            double factor = aug->data[i * 2 * n + k];
            for (int j = 0; j < 2 * n; j++)
                aug->data[i * 2 * n + j] -= factor * aug->data[k * 2 * n + j];
        }
    }

    matrix_t* inv = matrix_create(n, n);
    if (inv) {
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                inv->data[i * n + j] = aug->data[i * 2 * n + n + j];
    }
    matrix_destroy(aug);
    return inv;
}

/* ================================================================
 * SOLVERS
 * ================================================================ */

matrix_t* matrix_solve_lu(const matrix_t* a, const matrix_t* b) {
    if (!a || !b || a->rows != a->cols || a->rows != b->rows) return NULL;
    int n = a->rows;

    lu_decomp_t* lu = matrix_lu(a);
    if (!lu) return NULL;

    matrix_t* x = matrix_create(n, 1);
    if (!x) { lu_decomp_free(lu); return NULL; }

    /* Permute b */
    double* pb = (double*)nimcp_calloc((size_t)n, sizeof(double));
    if (!pb) { matrix_destroy(x); lu_decomp_free(lu); return NULL; }
    for (int i = 0; i < n; i++) pb[i] = b->data[lu->pivot[i] * b->cols];

    /* Forward substitution: Ly = Pb */
    double* y = (double*)nimcp_calloc((size_t)n, sizeof(double));
    if (!y) { nimcp_free(pb); matrix_destroy(x); lu_decomp_free(lu); return NULL; }
    for (int i = 0; i < n; i++) {
        y[i] = pb[i];
        for (int j = 0; j < i; j++) y[i] -= lu->L->data[i * n + j] * y[j];
    }

    /* Back substitution: Ux = y */
    for (int i = n - 1; i >= 0; i--) {
        x->data[i] = y[i];
        for (int j = i + 1; j < n; j++) x->data[i] -= lu->U->data[i * n + j] * x->data[j];
        double diag = lu->U->data[i * n + i];
        if (fabs(diag) > LINALG_EPSILON) x->data[i] /= diag;
    }

    nimcp_free(y);
    nimcp_free(pb);
    lu_decomp_free(lu);
    return x;
}

matrix_t* matrix_solve_least_squares(const matrix_t* a, const matrix_t* b) {
    if (!a || !b) return NULL;
    /* x = (A^T A)^{-1} A^T b via QR: R x = Q^T b */
    qr_decomp_t* qr = matrix_qr(a);
    if (!qr) return NULL;

    matrix_t* qt = matrix_transpose(qr->Q);
    matrix_t* qtb = matrix_multiply(qt, b);
    matrix_destroy(qt);

    if (!qtb) { qr_decomp_free(qr); return NULL; }

    int n = qr->R->rows;
    matrix_t* x = matrix_create(n, 1);
    if (!x) { matrix_destroy(qtb); qr_decomp_free(qr); return NULL; }

    /* Back substitution with R */
    for (int i = n - 1; i >= 0; i--) {
        x->data[i] = qtb->data[i];
        for (int j = i + 1; j < n; j++) x->data[i] -= qr->R->data[i * n + j] * x->data[j];
        double diag = qr->R->data[i * n + i];
        if (fabs(diag) > LINALG_EPSILON) x->data[i] /= diag;
    }

    matrix_destroy(qtb);
    qr_decomp_free(qr);
    return x;
}

double matrix_condition_number(const matrix_t* a) {
    svd_result_t* s = matrix_svd(a);
    if (!s || s->n_sigma == 0) { svd_result_free(s); return INFINITY; }
    double smax = s->sigma[0];
    double smin = s->sigma[s->n_sigma - 1];
    svd_result_free(s);
    if (smin < LINALG_EPSILON) return INFINITY;
    return smax / smin;
}

/* ================================================================
 * NORMS
 * ================================================================ */

double matrix_norm_frobenius(const matrix_t* a) {
    if (!a) return 0.0;
    double sum = 0.0;
    int n = a->rows * a->cols;
    for (int i = 0; i < n; i++) sum += a->data[i] * a->data[i];
    return sqrt(sum);
}

double matrix_norm_1(const matrix_t* a) {
    if (!a) return 0.0;
    double max_col = 0.0;
    for (int j = 0; j < a->cols; j++) {
        double col_sum = 0.0;
        for (int i = 0; i < a->rows; i++) col_sum += fabs(a->data[i * a->cols + j]);
        if (col_sum > max_col) max_col = col_sum;
    }
    return max_col;
}

double matrix_norm_inf(const matrix_t* a) {
    if (!a) return 0.0;
    double max_row = 0.0;
    for (int i = 0; i < a->rows; i++) {
        double row_sum = 0.0;
        for (int j = 0; j < a->cols; j++) row_sum += fabs(a->data[i * a->cols + j]);
        if (row_sum > max_row) max_row = row_sum;
    }
    return max_row;
}

double matrix_norm_spectral(const matrix_t* a) {
    svd_result_t* s = matrix_svd(a);
    if (!s || s->n_sigma == 0) { svd_result_free(s); return 0.0; }
    double smax = s->sigma[0];
    svd_result_free(s);
    return smax;
}

/* ================================================================
 * RANK + NULL SPACE
 * ================================================================ */

int matrix_rank(const matrix_t* a) {
    svd_result_t* s = matrix_svd(a);
    if (!s) return 0;
    int rank = 0;
    double tol = LINALG_EPSILON * (double)((a->rows > a->cols) ? a->rows : a->cols) *
                 ((s->n_sigma > 0) ? s->sigma[0] : 0.0);
    for (int i = 0; i < s->n_sigma; i++) {
        if (s->sigma[i] > tol) rank++;
    }
    svd_result_free(s);
    return rank;
}

matrix_t* matrix_null_space(const matrix_t* a, int* null_dim) {
    if (!a || !null_dim) return NULL;
    svd_result_t* s = matrix_svd(a);
    if (!s) { *null_dim = 0; return NULL; }

    double tol = LINALG_EPSILON * (double)((a->rows > a->cols) ? a->rows : a->cols) *
                 ((s->n_sigma > 0) ? s->sigma[0] : 0.0);

    int rank = 0;
    for (int i = 0; i < s->n_sigma; i++) {
        if (s->sigma[i] > tol) rank++;
    }
    *null_dim = a->cols - rank;
    if (*null_dim <= 0) { svd_result_free(s); *null_dim = 0; return NULL; }

    /* Null space = last (cols - rank) rows of Vt, transposed to columns */
    matrix_t* ns = matrix_create(a->cols, *null_dim);
    if (ns && s->Vt) {
        for (int j = 0; j < *null_dim; j++) {
            int row_idx = rank + j;
            for (int i = 0; i < a->cols; i++) {
                ns->data[i * (*null_dim) + j] = s->Vt->data[row_idx * s->Vt->cols + i];
            }
        }
    }
    svd_result_free(s);
    return ns;
}

/* ============================================================================
 * W14 (2026-04-24): KG runtime emit for linear-algebra results.
 * Callers that hold a brain_t invoke this after matrix_solve / eigen /
 * SVD / rank / determinant computations to register the result in the
 * cog_math_algebra KG root.
 * ============================================================================ */
#include "cognitive/kg/nimcp_wave14_math_genius_kg.h"
void linear_algebra_wave14_kg_emit(
    struct brain_struct* brain,
    const char* operation_label,
    float confidence)
{
    if (!brain) return;
    wave14_math_emit_proof(brain, "algebra", operation_label, confidence);
}
