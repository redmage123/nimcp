/**
 * @file nimcp_lie_group.c
 * @brief Lie Group Operations Implementation
 *
 * SO(3) rotation group with Rodrigues' formula, matrix exponential/logarithm,
 * geodesic interpolation (SLERP), and projection onto SO(3).
 */

#include "utils/geometry/nimcp_lie_group.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <math.h>
#include <float.h>

#define LOG_MOD "LIE_GROUP"

/*=============================================================================
 * Internal Helpers
 *===========================================================================*/

static inline float clamp_lie(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/** 3x3 matrix multiply C = A * B (row-major) */
static void mat3_mul(const float* A, const float* B, float* C) {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            float sum = 0.0f;
            for (int k = 0; k < 3; k++) {
                sum += A[i * 3 + k] * B[k * 3 + j];
            }
            C[i * 3 + j] = sum;
        }
    }
}

/** Skew-symmetric matrix from 3D vector */
static void skew(const float* v, float* S) {
    /* S = [ 0  -v2  v1]
     *     [ v2  0  -v0]
     *     [-v1  v0  0 ] */
    S[0] = 0.0f;  S[1] = -v[2]; S[2] = v[1];
    S[3] = v[2];  S[4] = 0.0f;  S[5] = -v[0];
    S[6] = -v[1]; S[7] = v[0];  S[8] = 0.0f;
}

/** 3x3 matrix trace */
static float mat3_trace(const float* M) {
    return M[0] + M[4] + M[8];
}

/*=============================================================================
 * SO(3) Implementation
 *===========================================================================*/

so3_rotation_t so3_identity(void) {
    so3_rotation_t R;
    memset(R.m, 0, sizeof(R.m));
    R.m[0] = R.m[4] = R.m[8] = 1.0f;
    return R;
}

so3_rotation_t so3_from_axis_angle(const float* axis, float angle) {
    so3_algebra_t omega;
    float norm = sqrtf(axis[0]*axis[0] + axis[1]*axis[1] + axis[2]*axis[2]);
    if (norm < LIE_EPSILON) return so3_identity();

    omega.v[0] = axis[0] / norm * angle;
    omega.v[1] = axis[1] / norm * angle;
    omega.v[2] = axis[2] / norm * angle;
    return so3_exp(&omega);
}

so3_rotation_t so3_exp(const so3_algebra_t* omega) {
    if (!omega) return so3_identity();

    float theta = sqrtf(omega->v[0]*omega->v[0] +
                        omega->v[1]*omega->v[1] +
                        omega->v[2]*omega->v[2]);

    if (theta < LIE_EPSILON) {
        /* First-order approximation: exp(omega) ≈ I + omega_hat */
        so3_rotation_t R = so3_identity();
        float S[9];
        skew(omega->v, S);
        for (int i = 0; i < 9; i++) R.m[i] += S[i];
        return R;
    }

    /* Rodrigues' formula:
     * R = I + sin(theta)/theta * K + (1-cos(theta))/theta^2 * K^2
     * where K is skew-symmetric matrix of omega */
    float K[9], K2[9];
    skew(omega->v, K);
    mat3_mul(K, K, K2);

    float sin_t = sinf(theta);
    float cos_t = cosf(theta);
    float a = sin_t / theta;
    float b = (1.0f - cos_t) / (theta * theta);

    so3_rotation_t R;
    R.m[0] = 1.0f + a * K[0] + b * K2[0];
    R.m[1] = a * K[1] + b * K2[1];
    R.m[2] = a * K[2] + b * K2[2];
    R.m[3] = a * K[3] + b * K2[3];
    R.m[4] = 1.0f + a * K[4] + b * K2[4];
    R.m[5] = a * K[5] + b * K2[5];
    R.m[6] = a * K[6] + b * K2[6];
    R.m[7] = a * K[7] + b * K2[7];
    R.m[8] = 1.0f + a * K[8] + b * K2[8];

    return R;
}

so3_algebra_t so3_log(const so3_rotation_t* R) {
    so3_algebra_t omega = {{ 0.0f, 0.0f, 0.0f }};
    if (!R) return omega;

    float trace = mat3_trace(R->m);
    float cos_theta = clamp_lie((trace - 1.0f) / 2.0f, -1.0f, 1.0f);
    float theta = acosf(cos_theta);

    if (theta < LIE_EPSILON) {
        /* Near identity: omega ≈ (R - R^T) / 2 */
        omega.v[0] = (R->m[7] - R->m[5]) / 2.0f;
        omega.v[1] = (R->m[2] - R->m[6]) / 2.0f;
        omega.v[2] = (R->m[3] - R->m[1]) / 2.0f;
        return omega;
    }

    if (fabsf(theta - (float)M_PI) < LIE_EPSILON) {
        /* Near pi: special handling */
        /* Find column of R+I with largest norm */
        float diag[3] = { R->m[0] + 1.0f, R->m[4] + 1.0f, R->m[8] + 1.0f };
        int max_idx = 0;
        if (diag[1] > diag[max_idx]) max_idx = 1;
        if (diag[2] > diag[max_idx]) max_idx = 2;

        float col[3];
        col[0] = R->m[0 + max_idx] + (max_idx == 0 ? 1.0f : 0.0f);
        col[1] = R->m[3 + max_idx] + (max_idx == 1 ? 1.0f : 0.0f);
        col[2] = R->m[6 + max_idx] + (max_idx == 2 ? 1.0f : 0.0f);

        float norm = sqrtf(col[0]*col[0] + col[1]*col[1] + col[2]*col[2]);
        if (norm > LIE_EPSILON) {
            omega.v[0] = (float)M_PI * col[0] / norm;
            omega.v[1] = (float)M_PI * col[1] / norm;
            omega.v[2] = (float)M_PI * col[2] / norm;
        }
        return omega;
    }

    /* General case: omega = theta / (2*sin(theta)) * (R - R^T) */
    float factor = theta / (2.0f * sinf(theta));
    omega.v[0] = factor * (R->m[7] - R->m[5]);
    omega.v[1] = factor * (R->m[2] - R->m[6]);
    omega.v[2] = factor * (R->m[3] - R->m[1]);

    return omega;
}

so3_rotation_t so3_multiply(const so3_rotation_t* R1, const so3_rotation_t* R2) {
    so3_rotation_t result;
    if (!R1 || !R2) return so3_identity();
    mat3_mul(R1->m, R2->m, result.m);
    return result;
}

so3_rotation_t so3_transpose(const so3_rotation_t* R) {
    so3_rotation_t Rt;
    if (!R) return so3_identity();

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            Rt.m[i * 3 + j] = R->m[j * 3 + i];
        }
    }
    return Rt;
}

void so3_rotate_vector(const so3_rotation_t* R, const float* x, float* y) {
    if (!R || !x || !y) return;

    for (int i = 0; i < 3; i++) {
        y[i] = R->m[i * 3 + 0] * x[0] +
               R->m[i * 3 + 1] * x[1] +
               R->m[i * 3 + 2] * x[2];
    }
}

float so3_distance(const so3_rotation_t* R1, const so3_rotation_t* R2) {
    if (!R1 || !R2) return 0.0f;

    /* d(R1, R2) = ||log(R1^T * R2)|| */
    so3_rotation_t R1t = so3_transpose(R1);
    so3_rotation_t delta = so3_multiply(&R1t, R2);
    so3_algebra_t omega = so3_log(&delta);

    return sqrtf(omega.v[0]*omega.v[0] +
                 omega.v[1]*omega.v[1] +
                 omega.v[2]*omega.v[2]);
}

so3_rotation_t so3_slerp(
    const so3_rotation_t* R1, const so3_rotation_t* R2, float t)
{
    if (!R1 || !R2) return so3_identity();
    t = clamp_lie(t, 0.0f, 1.0f);

    /* SLERP via Lie algebra:
     * R(t) = R1 * exp(t * log(R1^T * R2)) */
    so3_rotation_t R1t = so3_transpose(R1);
    so3_rotation_t delta = so3_multiply(&R1t, R2);
    so3_algebra_t omega = so3_log(&delta);

    /* Scale by t */
    so3_algebra_t scaled = {{ omega.v[0] * t, omega.v[1] * t, omega.v[2] * t }};

    so3_rotation_t delta_t = so3_exp(&scaled);
    return so3_multiply(R1, &delta_t);
}

so3_rotation_t so3_project(const float* matrix_3x3) {
    if (!matrix_3x3) return so3_identity();

    /* Simple projection: Gram-Schmidt orthogonalization + det correction */
    float R[9];
    memcpy(R, matrix_3x3, 9 * sizeof(float));

    /* Normalize first column */
    float n0 = sqrtf(R[0]*R[0] + R[3]*R[3] + R[6]*R[6]);
    if (n0 < LIE_EPSILON) return so3_identity();
    R[0] /= n0; R[3] /= n0; R[6] /= n0;

    /* Orthogonalize second column */
    float d01 = R[0]*R[1] + R[3]*R[4] + R[6]*R[7];
    R[1] -= d01 * R[0]; R[4] -= d01 * R[3]; R[7] -= d01 * R[6];
    float n1 = sqrtf(R[1]*R[1] + R[4]*R[4] + R[7]*R[7]);
    if (n1 < LIE_EPSILON) return so3_identity();
    R[1] /= n1; R[4] /= n1; R[7] /= n1;

    /* Third column = cross product of first two */
    R[2] = R[3]*R[7] - R[6]*R[4];
    R[5] = R[6]*R[1] - R[0]*R[7];
    R[8] = R[0]*R[4] - R[3]*R[1];

    /* Ensure det = +1 */
    float det = R[0]*(R[4]*R[8]-R[5]*R[7]) - R[1]*(R[3]*R[8]-R[5]*R[6]) + R[2]*(R[3]*R[7]-R[4]*R[6]);
    if (det < 0.0f) {
        R[2] = -R[2]; R[5] = -R[5]; R[8] = -R[8];
    }

    so3_rotation_t result;
    memcpy(result.m, R, 9 * sizeof(float));
    return result;
}

/*=============================================================================
 * General Matrix Exponential
 *===========================================================================*/

int matrix_exp(const float* A, uint32_t n, float* result) {
    if (!A || !result || n == 0 || n > LIE_MAX_DIM) return -1;

    size_t nn = (size_t)n * n;

    /* Pade [4/4] approximation with scaling-and-squaring.
     *
     * 1. Scale: s = max(0, ceil(log2(||A||_inf)))
     * 2. Compute B = A / 2^s
     * 3. Pade: exp(B) ≈ (I - B/2 + B^2/12)^{-1} * (I + B/2 + B^2/12)
     *    (simplified [2/2] Pade for efficiency)
     * 4. Square: exp(A) = (exp(B))^{2^s}
     */

    /* Compute ||A||_inf */
    float norm_inf = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float row_sum = 0.0f;
        for (uint32_t j = 0; j < n; j++) {
            row_sum += fabsf(A[i * n + j]);
        }
        if (row_sum > norm_inf) norm_inf = row_sum;
    }

    /* Determine scaling factor */
    int s = 0;
    if (norm_inf > 0.5f) {
        s = (int)ceilf(log2f(norm_inf + 1e-10f));
        if (s < 0) s = 0;
    }

    float scale = 1.0f / (float)(1 << s);

    /* Allocate working matrices */
    float* B = nimcp_calloc(nn, sizeof(float));
    float* B2 = nimcp_calloc(nn, sizeof(float));
    float* N_mat = nimcp_calloc(nn, sizeof(float)); /* Numerator */
    float* D_mat = nimcp_calloc(nn, sizeof(float)); /* Denominator */
    float* temp = nimcp_calloc(nn, sizeof(float));

    if (!B || !B2 || !N_mat || !D_mat || !temp) {
        nimcp_free(B); nimcp_free(B2); nimcp_free(N_mat);
        nimcp_free(D_mat); nimcp_free(temp);
        return -1;
    }

    /* B = A * scale */
    for (size_t i = 0; i < nn; i++) {
        B[i] = A[i] * scale;
    }

    /* B^2 */
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            float sum = 0.0f;
            for (uint32_t k = 0; k < n; k++) {
                sum += B[i * n + k] * B[k * n + j];
            }
            B2[i * n + j] = sum;
        }
    }

    /* [2/2] Pade: N = I + B/2 + B^2/12, D = I - B/2 + B^2/12 */
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            float eye = (i == j) ? 1.0f : 0.0f;
            float b_val = B[i * n + j];
            float b2_val = B2[i * n + j];

            N_mat[i * n + j] = eye + 0.5f * b_val + b2_val / 12.0f;
            D_mat[i * n + j] = eye - 0.5f * b_val + b2_val / 12.0f;
        }
    }

    /* Solve D * result = N => result = D^{-1} * N */
    /* Column by column */
    for (uint32_t col = 0; col < n; col++) {
        float* rhs = nimcp_calloc(n, sizeof(float));
        float* sol = nimcp_calloc(n, sizeof(float));
        if (!rhs || !sol) {
            nimcp_free(rhs); nimcp_free(sol);
            nimcp_free(B); nimcp_free(B2); nimcp_free(N_mat);
            nimcp_free(D_mat); nimcp_free(temp);
            return -1;
        }

        for (uint32_t row = 0; row < n; row++) {
            rhs[row] = N_mat[row * n + col];
        }

        /* Gauss elimination */
        float* aug = nimcp_calloc(n * (n + 1), sizeof(float));
        if (!aug) {
            nimcp_free(rhs); nimcp_free(sol);
            nimcp_free(B); nimcp_free(B2); nimcp_free(N_mat);
            nimcp_free(D_mat); nimcp_free(temp);
            return -1;
        }
        for (uint32_t i = 0; i < n; i++) {
            for (uint32_t j = 0; j < n; j++) aug[i*(n+1)+j] = D_mat[i*n+j];
            aug[i*(n+1)+n] = rhs[i];
        }

        for (uint32_t c = 0; c < n; c++) {
            uint32_t pivot = c;
            for (uint32_t r = c+1; r < n; r++)
                if (fabsf(aug[r*(n+1)+c]) > fabsf(aug[pivot*(n+1)+c])) pivot = r;
            if (pivot != c)
                for (uint32_t j = 0; j <= n; j++) {
                    float t = aug[c*(n+1)+j]; aug[c*(n+1)+j] = aug[pivot*(n+1)+j]; aug[pivot*(n+1)+j] = t;
                }
            if (fabsf(aug[c*(n+1)+c]) < LIE_EPSILON) { aug[c*(n+1)+c] = LIE_EPSILON; }
            for (uint32_t r = c+1; r < n; r++) {
                float f = aug[r*(n+1)+c] / aug[c*(n+1)+c];
                for (uint32_t j = c; j <= n; j++) aug[r*(n+1)+j] -= f * aug[c*(n+1)+j];
            }
        }
        for (int i = (int)n-1; i >= 0; i--) {
            sol[i] = aug[i*(n+1)+n];
            for (uint32_t j = (uint32_t)i+1; j < n; j++) sol[i] -= aug[i*(n+1)+j]*sol[j];
            sol[i] /= aug[i*(n+1)+i];
        }

        for (uint32_t row = 0; row < n; row++) {
            result[row * n + col] = sol[row];
        }

        nimcp_free(aug);
        nimcp_free(rhs);
        nimcp_free(sol);
    }

    /* Repeated squaring: result = result^{2^s} */
    for (int i = 0; i < s; i++) {
        for (uint32_t r = 0; r < n; r++) {
            for (uint32_t c = 0; c < n; c++) {
                float sum = 0.0f;
                for (uint32_t k = 0; k < n; k++) {
                    sum += result[r * n + k] * result[k * n + c];
                }
                temp[r * n + c] = sum;
            }
        }
        memcpy(result, temp, nn * sizeof(float));
    }

    nimcp_free(B); nimcp_free(B2); nimcp_free(N_mat);
    nimcp_free(D_mat); nimcp_free(temp);
    return 0;
}

int matrix_log(const float* A, uint32_t n, float* result) {
    if (!A || !result || n == 0 || n > LIE_MAX_DIM) return -1;

    size_t nn = (size_t)n * n;

    /* For matrices close to identity: log(A) ≈ (A - I) - (A - I)^2/2 + (A - I)^3/3
     * Use inverse scaling-and-squaring for general case.
     *
     * Simplified approach: log(A) via series for A near I.
     * First compute X = A - I, then log(I + X) ≈ X - X^2/2 + X^3/3 - X^4/4
     */

    float* X = nimcp_calloc(nn, sizeof(float));
    float* Xk = nimcp_calloc(nn, sizeof(float));  /* X^k */
    float* temp = nimcp_calloc(nn, sizeof(float));
    if (!X || !Xk || !temp) {
        nimcp_free(X); nimcp_free(Xk); nimcp_free(temp);
        return -1;
    }

    /* X = A - I */
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            X[i * n + j] = A[i * n + j] - (i == j ? 1.0f : 0.0f);
        }
    }

    /* Initialize: result = 0, Xk = I */
    memset(result, 0, nn * sizeof(float));
    for (uint32_t i = 0; i < n; i++) Xk[i * n + i] = 1.0f;

    /* Series: log(I+X) = sum_{k=1}^{K} (-1)^{k+1} X^k / k */
    uint32_t max_terms = 20;
    for (uint32_t k = 1; k <= max_terms; k++) {
        /* Xk = Xk * X */
        for (uint32_t i = 0; i < n; i++) {
            for (uint32_t j = 0; j < n; j++) {
                float sum = 0.0f;
                for (uint32_t m = 0; m < n; m++) {
                    sum += Xk[i * n + m] * X[m * n + j];
                }
                temp[i * n + j] = sum;
            }
        }
        memcpy(Xk, temp, nn * sizeof(float));

        float sign = (k % 2 == 1) ? 1.0f : -1.0f;
        float coeff = sign / (float)k;

        float term_norm = 0.0f;
        for (size_t i = 0; i < nn; i++) {
            result[i] += coeff * Xk[i];
            term_norm += fabsf(coeff * Xk[i]);
        }

        /* Convergence check */
        if (term_norm < LIE_EPSILON * (float)nn) break;
    }

    nimcp_free(X);
    nimcp_free(Xk);
    nimcp_free(temp);
    return 0;
}
