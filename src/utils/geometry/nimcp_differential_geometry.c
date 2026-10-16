/**
 * @file nimcp_differential_geometry.c
 * @brief Differential Geometry Operations Implementation
 *
 * Core differential geometry primitives: Riemannian metrics, Christoffel symbols,
 * curvature tensors, parallel transport, and geodesic computation.
 *
 * All matrix operations are implemented directly (no LAPACK dependency)
 * for portability. For dim <= 64, this is sufficient.
 */

#include "utils/geometry/nimcp_differential_geometry.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <float.h>

#define LOG_MOD "DIFFGEO"

/*=============================================================================
 * Internal Helpers
 *===========================================================================*/

static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/** Matrix-vector multiply: y = A*x (row-major A [n*n]) */
static void mat_vec_mul(const float* A, const float* x, float* y, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        float sum = 0.0f;
        for (uint32_t j = 0; j < n; j++) {
            sum += A[i * n + j] * x[j];
        }
        y[i] = sum;
    }
}

/** Solve Ax = b via Gauss elimination with partial pivoting */
static int solve_linear(const float* A_in, const float* b_in, float* x, uint32_t n) {
    /* Augmented matrix [A | b] */
    float* aug = nimcp_calloc(n * (n + 1), sizeof(float));
    if (!aug) return -1;

    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            aug[i * (n + 1) + j] = A_in[i * n + j];
        }
        aug[i * (n + 1) + n] = b_in[i];
    }

    /* Forward elimination with partial pivoting */
    for (uint32_t col = 0; col < n; col++) {
        /* Find pivot */
        uint32_t pivot = col;
        float max_val = fabsf(aug[col * (n + 1) + col]);
        for (uint32_t row = col + 1; row < n; row++) {
            float val = fabsf(aug[row * (n + 1) + col]);
            if (val > max_val) { max_val = val; pivot = row; }
        }
        if (max_val < DIFFGEO_EPSILON) {
            nimcp_free(aug);
            return -1; /* Singular */
        }

        /* Swap rows */
        if (pivot != col) {
            for (uint32_t j = 0; j <= n; j++) {
                float tmp = aug[col * (n + 1) + j];
                aug[col * (n + 1) + j] = aug[pivot * (n + 1) + j];
                aug[pivot * (n + 1) + j] = tmp;
            }
        }

        /* Eliminate below */
        float diag = aug[col * (n + 1) + col];
        for (uint32_t row = col + 1; row < n; row++) {
            float factor = aug[row * (n + 1) + col] / diag;
            for (uint32_t j = col; j <= n; j++) {
                aug[row * (n + 1) + j] -= factor * aug[col * (n + 1) + j];
            }
        }
    }

    /* Back substitution */
    for (int i = (int)n - 1; i >= 0; i--) {
        x[i] = aug[i * (n + 1) + n];
        for (uint32_t j = (uint32_t)i + 1; j < n; j++) {
            x[i] -= aug[i * (n + 1) + j] * x[j];
        }
        if (fabsf(aug[i * (n + 1) + i]) < DIFFGEO_EPSILON) {
            nimcp_free(aug);
            return -1;
        }
        x[i] /= aug[i * (n + 1) + i];
    }

    nimcp_free(aug);
    return 0;
}

/** Compute matrix inverse via column-by-column solve */
static int mat_inverse(const float* A, float* A_inv, uint32_t n) {
    float* e = nimcp_calloc(n, sizeof(float));
    if (!e) return -1;

    for (uint32_t col = 0; col < n; col++) {
        memset(e, 0, n * sizeof(float));
        e[col] = 1.0f;
        float* col_result = A_inv + col; /* Column-major access for result */
        float* tmp = nimcp_calloc(n, sizeof(float));
        if (!tmp) { nimcp_free(e); return -1; }

        if (solve_linear(A, e, tmp, n) != 0) {
            nimcp_free(tmp);
            nimcp_free(e);
            return -1;
        }
        /* Store in row-major A_inv */
        for (uint32_t row = 0; row < n; row++) {
            A_inv[row * n + col] = tmp[row];
        }
        nimcp_free(tmp);
    }

    nimcp_free(e);
    return 0;
}

/** Compute determinant via LU decomposition */
static float mat_determinant(const float* A, uint32_t n) {
    if (n == 1) return A[0];
    if (n == 2) return A[0] * A[3] - A[1] * A[2];

    /* LU decomposition (simplified, no pivoting for det) */
    float* U = nimcp_calloc(n * n, sizeof(float));
    if (!U) return 0.0f;
    memcpy(U, A, n * n * sizeof(float));

    float det = 1.0f;
    for (uint32_t col = 0; col < n; col++) {
        if (fabsf(U[col * n + col]) < DIFFGEO_EPSILON) {
            nimcp_free(U);
            return 0.0f;
        }
        det *= U[col * n + col];
        for (uint32_t row = col + 1; row < n; row++) {
            float factor = U[row * n + col] / U[col * n + col];
            for (uint32_t j = col; j < n; j++) {
                U[row * n + j] -= factor * U[col * n + j];
            }
        }
    }

    nimcp_free(U);
    return det;
}

/*=============================================================================
 * Riemannian Metric
 *===========================================================================*/

riemannian_metric_t* riemannian_metric_create(uint32_t dim) {
    if (dim == 0 || dim > DIFFGEO_MAX_DIM) return NULL;

    riemannian_metric_t* m = nimcp_calloc(1, sizeof(riemannian_metric_t));
    if (!m) return NULL;

    m->dim = dim;
    m->g = nimcp_calloc(dim * dim, sizeof(float));
    m->g_inv = nimcp_calloc(dim * dim, sizeof(float));

    if (!m->g || !m->g_inv) {
        nimcp_free(m->g);
        nimcp_free(m->g_inv);
        nimcp_free(m);
        return NULL;
    }

    /* Initialize to identity (flat Euclidean metric) */
    for (uint32_t i = 0; i < dim; i++) {
        m->g[i * dim + i] = 1.0f;
        m->g_inv[i * dim + i] = 1.0f;
    }
    m->det = 1.0f;
    m->sqrt_det = 1.0f;
    m->inv_valid = true;

    return m;
}

void riemannian_metric_destroy(riemannian_metric_t* metric) {
    if (!metric) return;
    nimcp_free(metric->g);
    nimcp_free(metric->g_inv);
    nimcp_free(metric);
}

diffgeo_error_t riemannian_metric_set(
    riemannian_metric_t* metric,
    const float* g_data)
{
    if (!metric || !g_data) return DIFFGEO_ERR_NULL_PTR;

    uint32_t n = metric->dim;
    memcpy(metric->g, g_data, n * n * sizeof(float));

    /* Compute determinant */
    metric->det = mat_determinant(metric->g, n);
    metric->sqrt_det = sqrtf(fabsf(metric->det));
    metric->inv_valid = false;

    return DIFFGEO_OK;
}

diffgeo_error_t riemannian_metric_invert(riemannian_metric_t* metric) {
    if (!metric) return DIFFGEO_ERR_NULL_PTR;

    if (mat_inverse(metric->g, metric->g_inv, metric->dim) != 0) {
        NIMCP_LOGGING_WARN("riemannian_metric_invert: singular metric");
        return DIFFGEO_ERR_SINGULAR;
    }

    metric->inv_valid = true;
    return DIFFGEO_OK;
}

float riemannian_inner_product(
    const riemannian_metric_t* metric,
    const float* u,
    const float* v)
{
    if (!metric || !u || !v) return 0.0f;

    uint32_t n = metric->dim;
    float result = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            result += metric->g[i * n + j] * u[i] * v[j];
        }
    }
    return result;
}

float riemannian_norm(
    const riemannian_metric_t* metric,
    const float* v)
{
    float ip = riemannian_inner_product(metric, v, v);
    return (ip > 0.0f) ? sqrtf(ip) : 0.0f;
}

diffgeo_error_t riemannian_raise_index(
    const riemannian_metric_t* metric,
    const float* v_lower,
    float* v_upper)
{
    if (!metric || !v_lower || !v_upper) return DIFFGEO_ERR_NULL_PTR;
    if (!metric->inv_valid) return DIFFGEO_ERR_NOT_INIT;

    mat_vec_mul(metric->g_inv, v_lower, v_upper, metric->dim);
    return DIFFGEO_OK;
}

diffgeo_error_t riemannian_lower_index(
    const riemannian_metric_t* metric,
    const float* v_upper,
    float* v_lower)
{
    if (!metric || !v_upper || !v_lower) return DIFFGEO_ERR_NULL_PTR;

    mat_vec_mul(metric->g, v_upper, v_lower, metric->dim);
    return DIFFGEO_OK;
}

/*=============================================================================
 * Christoffel Symbols
 *===========================================================================*/

christoffel_symbols_t* christoffel_create(uint32_t dim) {
    if (dim == 0 || dim > DIFFGEO_MAX_DIM) return NULL;

    christoffel_symbols_t* c = nimcp_calloc(1, sizeof(christoffel_symbols_t));
    if (!c) return NULL;

    c->dim = dim;
    c->gamma = nimcp_calloc((size_t)dim * dim * dim, sizeof(float));
    if (!c->gamma) {
        nimcp_free(c);
        return NULL;
    }

    return c;
}

void christoffel_destroy(christoffel_symbols_t* chris) {
    if (!chris) return;
    nimcp_free(chris->gamma);
    nimcp_free(chris);
}

diffgeo_error_t christoffel_compute(
    christoffel_symbols_t* chris,
    const riemannian_metric_t* metric,
    const float* dg_dx)
{
    if (!chris || !metric || !dg_dx) return DIFFGEO_ERR_NULL_PTR;
    if (!metric->inv_valid) return DIFFGEO_ERR_NOT_INIT;

    uint32_t n = chris->dim;
    if (n != metric->dim) return DIFFGEO_ERR_INVALID_DIM;

    /* Gamma^k_{ij} = (1/2) g^{kl} (dg_{li}/dx^j + dg_{lj}/dx^i - dg_{ij}/dx^l)
     *
     * dg_dx[m * n*n + i * n + j] = dg_{ij}/dx^m
     */
    for (uint32_t k = 0; k < n; k++) {
        for (uint32_t i = 0; i < n; i++) {
            for (uint32_t j = 0; j <= i; j++) { /* Symmetric in i,j */
                float sum = 0.0f;
                for (uint32_t l = 0; l < n; l++) {
                    /* dg_{li}/dx^j */
                    float dg_li_j = dg_dx[j * n * n + l * n + i];
                    /* dg_{lj}/dx^i */
                    float dg_lj_i = dg_dx[i * n * n + l * n + j];
                    /* dg_{ij}/dx^l */
                    float dg_ij_l = dg_dx[l * n * n + i * n + j];

                    sum += metric->g_inv[k * n + l] * (dg_li_j + dg_lj_i - dg_ij_l);
                }
                float gamma_val = 0.5f * sum;
                chris->gamma[k * n * n + i * n + j] = gamma_val;
                chris->gamma[k * n * n + j * n + i] = gamma_val; /* Torsion-free */
            }
        }
    }

    return DIFFGEO_OK;
}

float christoffel_get(
    const christoffel_symbols_t* chris,
    uint32_t k, uint32_t i, uint32_t j)
{
    if (!chris || k >= chris->dim || i >= chris->dim || j >= chris->dim) {
        return 0.0f;
    }
    return chris->gamma[k * chris->dim * chris->dim + i * chris->dim + j];
}

/*=============================================================================
 * Curvature
 *===========================================================================*/

curvature_data_t* curvature_create(uint32_t dim) {
    if (dim == 0 || dim > DIFFGEO_MAX_DIM) return NULL;

    curvature_data_t* c = nimcp_calloc(1, sizeof(curvature_data_t));
    if (!c) return NULL;

    c->dim = dim;
    c->ricci = nimcp_calloc(dim * dim, sizeof(float));
    c->sectional = nimcp_calloc(dim * dim, sizeof(float));
    /* Riemann tensor is dim^4 — only allocate for small dim */
    if (dim <= 16) {
        c->riemann = nimcp_calloc((size_t)dim * dim * dim * dim, sizeof(float));
    }

    if (!c->ricci || !c->sectional) {
        curvature_destroy(c);
        return NULL;
    }

    return c;
}

void curvature_destroy(curvature_data_t* curv) {
    if (!curv) return;
    nimcp_free(curv->riemann);
    nimcp_free(curv->ricci);
    nimcp_free(curv->sectional);
    nimcp_free(curv);
}

diffgeo_error_t curvature_compute_ricci(
    curvature_data_t* curv,
    const christoffel_symbols_t* chris,
    const float* dgamma_dx)
{
    if (!curv || !chris || !dgamma_dx) return DIFFGEO_ERR_NULL_PTR;

    uint32_t n = curv->dim;
    if (n != chris->dim) return DIFFGEO_ERR_INVALID_DIM;

    /* R_{ij} = d_k Gamma^k_{ij} - d_j Gamma^k_{ik}
     *        + Gamma^k_{kl} Gamma^l_{ij} - Gamma^k_{jl} Gamma^l_{ik}
     *
     * dgamma_dx[m * n^3 + k * n^2 + i * n + j] = d(Gamma^k_{ij})/dx^m
     */
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            float R_ij = 0.0f;

            for (uint32_t k = 0; k < n; k++) {
                /* d_k Gamma^k_{ij} */
                R_ij += dgamma_dx[k * n * n * n + k * n * n + i * n + j];
                /* -d_j Gamma^k_{ik} */
                R_ij -= dgamma_dx[j * n * n * n + k * n * n + i * n + k];

                for (uint32_t l = 0; l < n; l++) {
                    /* Gamma^k_{kl} Gamma^l_{ij} */
                    R_ij += christoffel_get(chris, k, k, l) *
                            christoffel_get(chris, l, i, j);
                    /* -Gamma^k_{jl} Gamma^l_{ik} */
                    R_ij -= christoffel_get(chris, k, j, l) *
                            christoffel_get(chris, l, i, k);
                }
            }

            curv->ricci[i * n + j] = R_ij;
        }
    }

    return DIFFGEO_OK;
}

diffgeo_error_t curvature_compute_scalar(
    curvature_data_t* curv,
    const riemannian_metric_t* metric)
{
    if (!curv || !metric) return DIFFGEO_ERR_NULL_PTR;
    if (!metric->inv_valid) return DIFFGEO_ERR_NOT_INIT;
    if (!curv->ricci) return DIFFGEO_ERR_NOT_INIT;

    uint32_t n = curv->dim;
    float R = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            R += metric->g_inv[i * n + j] * curv->ricci[i * n + j];
        }
    }
    curv->scalar_curvature = R;

    return DIFFGEO_OK;
}

float curvature_sectional(
    const curvature_data_t* curv,
    const riemannian_metric_t* metric,
    const float* u,
    const float* v)
{
    if (!curv || !metric || !u || !v) return 0.0f;
    if (!curv->riemann) return 0.0f; /* Need full Riemann tensor */

    uint32_t n = curv->dim;

    /* R(u,v,v,u) = R^l_{ijk} u^i v^j v^k u_l */
    /* Simplified: use Ricci approximation for now */
    /* K(u,v) approx= R_{ij} u^i v^j / (g(u,u)g(v,v) - g(u,v)^2) */
    float R_uv = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            R_uv += curv->ricci[i * n + j] * u[i] * v[j];
        }
    }

    float g_uu = riemannian_inner_product(metric, u, u);
    float g_vv = riemannian_inner_product(metric, v, v);
    float g_uv = riemannian_inner_product(metric, u, v);
    float denom = g_uu * g_vv - g_uv * g_uv;

    if (fabsf(denom) < DIFFGEO_EPSILON) return 0.0f;

    return R_uv / denom;
}

/*=============================================================================
 * Parallel Transport
 *===========================================================================*/

diffgeo_error_t parallel_transport_along_curve(
    const christoffel_symbols_t* chris,
    const float* curve,
    uint32_t num_points,
    uint32_t dim,
    const float* v_initial,
    float* v_transported)
{
    if (!chris || !curve || !v_initial || !v_transported) return DIFFGEO_ERR_NULL_PTR;
    if (num_points < 2 || dim == 0) return DIFFGEO_ERR_INVALID_DIM;
    if (dim != chris->dim) return DIFFGEO_ERR_INVALID_DIM;

    /* Working vectors */
    float* v = nimcp_calloc(dim, sizeof(float));
    float* k1 = nimcp_calloc(dim, sizeof(float));
    float* k2 = nimcp_calloc(dim, sizeof(float));
    float* v_mid = nimcp_calloc(dim, sizeof(float));
    if (!v || !k1 || !k2 || !v_mid) {
        nimcp_free(v); nimcp_free(k1); nimcp_free(k2); nimcp_free(v_mid);
        return DIFFGEO_ERR_NO_MEMORY;
    }

    memcpy(v, v_initial, dim * sizeof(float));

    /* RK2 (midpoint method) integration of transport equation:
     * dv^k/dt = -Gamma^k_{ij} dx^i/dt v^j */
    for (uint32_t step = 0; step < num_points - 1; step++) {
        const float* x_cur = curve + step * dim;
        const float* x_next = curve + (step + 1) * dim;

        /* Tangent vector (finite difference) */
        float dx[DIFFGEO_MAX_DIM];
        for (uint32_t d = 0; d < dim; d++) {
            dx[d] = x_next[d] - x_cur[d];
        }

        /* k1 = -Gamma^k_{ij} dx^i v^j */
        for (uint32_t k = 0; k < dim; k++) {
            float sum = 0.0f;
            for (uint32_t i = 0; i < dim; i++) {
                for (uint32_t j = 0; j < dim; j++) {
                    sum += christoffel_get(chris, k, i, j) * dx[i] * v[j];
                }
            }
            k1[k] = -sum;
        }

        /* Midpoint: v_mid = v + 0.5 * k1 */
        for (uint32_t d = 0; d < dim; d++) {
            v_mid[d] = v[d] + 0.5f * k1[d];
        }

        /* k2 = -Gamma^k_{ij} dx^i v_mid^j */
        for (uint32_t k = 0; k < dim; k++) {
            float sum = 0.0f;
            for (uint32_t i = 0; i < dim; i++) {
                for (uint32_t j = 0; j < dim; j++) {
                    sum += christoffel_get(chris, k, i, j) * dx[i] * v_mid[j];
                }
            }
            k2[k] = -sum;
        }

        /* Update: v = v + k2 */
        for (uint32_t d = 0; d < dim; d++) {
            v[d] += k2[d];
        }
    }

    memcpy(v_transported, v, dim * sizeof(float));

    nimcp_free(v);
    nimcp_free(k1);
    nimcp_free(k2);
    nimcp_free(v_mid);
    return DIFFGEO_OK;
}

diffgeo_error_t parallel_transport_geodesic(
    const christoffel_symbols_t* chris,
    const riemannian_metric_t* metric,
    const float* p,
    const float* q,
    uint32_t dim,
    const float* v,
    float* v_transported,
    uint32_t num_steps)
{
    if (!chris || !metric || !p || !q || !v || !v_transported) return DIFFGEO_ERR_NULL_PTR;
    if (num_steps < 2) num_steps = DIFFGEO_TRANSPORT_STEPS;

    /* Generate geodesic curve via linear interpolation (flat approximation)
     * For curved spaces, should use geodesic_shoot, but this is a reasonable
     * first-order approximation for nearby points. */
    float* curve = nimcp_calloc((size_t)num_steps * dim, sizeof(float));
    if (!curve) return DIFFGEO_ERR_NO_MEMORY;

    for (uint32_t s = 0; s < num_steps; s++) {
        float t = (float)s / (float)(num_steps - 1);
        for (uint32_t d = 0; d < dim; d++) {
            curve[s * dim + d] = (1.0f - t) * p[d] + t * q[d];
        }
    }

    diffgeo_error_t rc = parallel_transport_along_curve(
        chris, curve, num_steps, dim, v, v_transported);

    nimcp_free(curve);
    return rc;
}

/*=============================================================================
 * Geodesics
 *===========================================================================*/

diffgeo_error_t geodesic_shoot(
    const christoffel_symbols_t* chris,
    uint32_t dim,
    const float* x0,
    const float* v0,
    float dt,
    uint32_t num_steps,
    float* trajectory)
{
    if (!chris || !x0 || !v0 || !trajectory) return DIFFGEO_ERR_NULL_PTR;
    if (dim == 0 || dim != chris->dim) return DIFFGEO_ERR_INVALID_DIM;

    /* State: position x[dim] and velocity v[dim] */
    float* x = nimcp_calloc(dim, sizeof(float));
    float* vel = nimcp_calloc(dim, sizeof(float));
    float* ax = nimcp_calloc(dim, sizeof(float)); /* acceleration */
    float* x_tmp = nimcp_calloc(dim, sizeof(float));
    float* v_tmp = nimcp_calloc(dim, sizeof(float));
    float* ax_tmp = nimcp_calloc(dim, sizeof(float));
    if (!x || !vel || !ax || !x_tmp || !v_tmp || !ax_tmp) {
        nimcp_free(x); nimcp_free(vel); nimcp_free(ax);
        nimcp_free(x_tmp); nimcp_free(v_tmp); nimcp_free(ax_tmp);
        return DIFFGEO_ERR_NO_MEMORY;
    }

    memcpy(x, x0, dim * sizeof(float));
    memcpy(vel, v0, dim * sizeof(float));

    /* Store initial point */
    memcpy(trajectory, x, dim * sizeof(float));

    /* RK4 integration of geodesic equation:
     * dx/dt = v
     * dv^k/dt = -Gamma^k_{ij} v^i v^j */
    for (uint32_t step = 0; step < num_steps; step++) {
        /* Compute acceleration: a^k = -Gamma^k_{ij} v^i v^j */
        for (uint32_t k = 0; k < dim; k++) {
            float sum = 0.0f;
            for (uint32_t i = 0; i < dim; i++) {
                for (uint32_t j = 0; j < dim; j++) {
                    sum += christoffel_get(chris, k, i, j) * vel[i] * vel[j];
                }
            }
            ax[k] = -sum;
        }

        /* Symplectic Euler (velocity Verlet would be better but this is simpler) */
        for (uint32_t d = 0; d < dim; d++) {
            vel[d] += ax[d] * dt;
            x[d] += vel[d] * dt;
        }

        /* Store trajectory point */
        memcpy(trajectory + (step + 1) * dim, x, dim * sizeof(float));
    }

    nimcp_free(x); nimcp_free(vel); nimcp_free(ax);
    nimcp_free(x_tmp); nimcp_free(v_tmp); nimcp_free(ax_tmp);
    return DIFFGEO_OK;
}

diffgeo_error_t geodesic_interpolate(
    const christoffel_symbols_t* chris,
    const riemannian_metric_t* metric,
    uint32_t dim,
    const float* p,
    const float* q,
    float t,
    float* result)
{
    if (!chris || !metric || !p || !q || !result) return DIFFGEO_ERR_NULL_PTR;
    if (dim == 0 || dim != chris->dim) return DIFFGEO_ERR_INVALID_DIM;

    t = clampf(t, 0.0f, 1.0f);

    /* Initial velocity: direction from p to q, scaled by t */
    float* v0 = nimcp_calloc(dim, sizeof(float));
    float* traj = nimcp_calloc((DIFFGEO_GEODESIC_STEPS + 1) * dim, sizeof(float));
    if (!v0 || !traj) {
        nimcp_free(v0); nimcp_free(traj);
        return DIFFGEO_ERR_NO_MEMORY;
    }

    for (uint32_t d = 0; d < dim; d++) {
        v0[d] = q[d] - p[d];
    }

    float dt_step = 1.0f / (float)DIFFGEO_GEODESIC_STEPS;
    diffgeo_error_t rc = geodesic_shoot(chris, dim, p, v0, dt_step,
                                         DIFFGEO_GEODESIC_STEPS, traj);
    if (rc != DIFFGEO_OK) {
        nimcp_free(v0); nimcp_free(traj);
        return rc;
    }

    /* Interpolate at t */
    uint32_t idx = (uint32_t)(t * DIFFGEO_GEODESIC_STEPS);
    if (idx >= DIFFGEO_GEODESIC_STEPS) idx = DIFFGEO_GEODESIC_STEPS;

    float frac = t * DIFFGEO_GEODESIC_STEPS - (float)idx;
    uint32_t idx_next = (idx + 1 <= DIFFGEO_GEODESIC_STEPS) ? idx + 1 : idx;

    for (uint32_t d = 0; d < dim; d++) {
        result[d] = (1.0f - frac) * traj[idx * dim + d] +
                    frac * traj[idx_next * dim + d];
    }

    nimcp_free(v0);
    nimcp_free(traj);
    return DIFFGEO_OK;
}

float geodesic_distance(
    const christoffel_symbols_t* chris,
    const riemannian_metric_t* metric,
    uint32_t dim,
    const float* p,
    const float* q)
{
    if (!chris || !metric || !p || !q || dim == 0) return 0.0f;

    /* Shoot geodesic and compute arc length */
    uint32_t steps = DIFFGEO_GEODESIC_STEPS;
    float* v0 = nimcp_calloc(dim, sizeof(float));
    float* traj = nimcp_calloc((steps + 1) * dim, sizeof(float));
    if (!v0 || !traj) {
        nimcp_free(v0); nimcp_free(traj);
        return 0.0f;
    }

    for (uint32_t d = 0; d < dim; d++) {
        v0[d] = q[d] - p[d];
    }

    float dt = 1.0f / (float)steps;
    if (geodesic_shoot(chris, dim, p, v0, dt, steps, traj) != DIFFGEO_OK) {
        nimcp_free(v0); nimcp_free(traj);
        return 0.0f;
    }

    /* Compute arc length: L = sum_i ||x_{i+1} - x_i||_g */
    float length = 0.0f;
    float* diff = nimcp_calloc(dim, sizeof(float));
    if (!diff) { nimcp_free(v0); nimcp_free(traj); return 0.0f; }

    for (uint32_t i = 0; i < steps; i++) {
        for (uint32_t d = 0; d < dim; d++) {
            diff[d] = traj[(i + 1) * dim + d] - traj[i * dim + d];
        }
        length += riemannian_norm(metric, diff);
    }

    nimcp_free(v0);
    nimcp_free(traj);
    nimcp_free(diff);
    return length;
}

/*=============================================================================
 * Exponential and Logarithmic Maps
 *===========================================================================*/

diffgeo_error_t exp_map(
    const christoffel_symbols_t* chris,
    uint32_t dim,
    const float* p,
    const float* v,
    float* result)
{
    if (!chris || !p || !v || !result) return DIFFGEO_ERR_NULL_PTR;
    if (dim == 0 || dim != chris->dim) return DIFFGEO_ERR_INVALID_DIM;

    /* exp_p(v) = geodesic(p, v, t=1)
     * Shoot geodesic from p with velocity v for unit time */
    uint32_t steps = DIFFGEO_GEODESIC_STEPS;
    float* traj = nimcp_calloc((steps + 1) * dim, sizeof(float));
    if (!traj) return DIFFGEO_ERR_NO_MEMORY;

    float dt = 1.0f / (float)steps;
    diffgeo_error_t rc = geodesic_shoot(chris, dim, p, v, dt, steps, traj);

    if (rc == DIFFGEO_OK) {
        memcpy(result, traj + steps * dim, dim * sizeof(float));
    }

    nimcp_free(traj);
    return rc;
}

diffgeo_error_t log_map(
    const christoffel_symbols_t* chris,
    const riemannian_metric_t* metric,
    uint32_t dim,
    const float* p,
    const float* q,
    float* result,
    uint32_t max_iterations)
{
    if (!chris || !metric || !p || !q || !result) return DIFFGEO_ERR_NULL_PTR;
    if (dim == 0 || dim != chris->dim) return DIFFGEO_ERR_INVALID_DIM;
    if (max_iterations == 0) max_iterations = 20;

    /* Initial guess: v = q - p (Euclidean) */
    float* v = nimcp_calloc(dim, sizeof(float));
    float* q_test = nimcp_calloc(dim, sizeof(float));
    if (!v || !q_test) {
        nimcp_free(v); nimcp_free(q_test);
        return DIFFGEO_ERR_NO_MEMORY;
    }

    for (uint32_t d = 0; d < dim; d++) {
        v[d] = q[d] - p[d];
    }

    /* Newton iteration: find v such that exp_p(v) = q
     * Update: v -= alpha * (exp_p(v) - q) */
    float alpha = 0.5f;
    for (uint32_t iter = 0; iter < max_iterations; iter++) {
        diffgeo_error_t rc = exp_map(chris, dim, p, v, q_test);
        if (rc != DIFFGEO_OK) break;

        float err_sq = 0.0f;
        for (uint32_t d = 0; d < dim; d++) {
            float diff = q_test[d] - q[d];
            err_sq += diff * diff;
            v[d] -= alpha * diff;
        }

        if (err_sq < DIFFGEO_EPSILON * DIFFGEO_EPSILON) break;
    }

    memcpy(result, v, dim * sizeof(float));
    nimcp_free(v);
    nimcp_free(q_test);
    return DIFFGEO_OK;
}
