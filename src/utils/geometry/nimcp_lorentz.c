/**
 * @file nimcp_lorentz.c
 * @brief Lorentz (Hyperboloid) Model Implementation
 *
 * Implements hyperbolic geometry operations on the upper sheet of the
 * two-sheeted hyperboloid: H^n = {x in R^{n+1} : <x,x>_L = -1/c, x_0 > 0}
 */

#include "utils/geometry/nimcp_lorentz.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <float.h>

#define LOG_MOD "LORENTZ"

/*=============================================================================
 * Internal Helpers
 *===========================================================================*/

static inline float safe_acosh_l(float x) {
    if (x < 1.0f + LORENTZ_EPSILON) return 0.0f;
    if (x > 1e10f) return logf(2.0f * x);
    return acoshf(x);
}

static inline float clampf_l(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/*=============================================================================
 * Point Management
 *===========================================================================*/

lorentz_point_t* lorentz_point_create(
    uint32_t dim, const float* coords, float curvature)
{
    if (dim == 0 || dim > LORENTZ_MAX_DIM) return NULL;
    if (curvature <= 0.0f) curvature = 1.0f;

    lorentz_point_t* p = nimcp_calloc(1, sizeof(lorentz_point_t));
    if (!p) return NULL;

    p->dim = dim;
    p->curvature = curvature;
    p->coords = nimcp_calloc(dim + 1, sizeof(float));
    if (!p->coords) { nimcp_free(p); return NULL; }

    if (coords) {
        /* Copy spatial coordinates and compute time component */
        for (uint32_t i = 0; i < dim; i++) {
            p->coords[i + 1] = coords[i];
        }
        /* Enforce hyperboloid constraint: x_0 = sqrt(1/c + sum(x_i^2)) */
        float spatial_sq = 0.0f;
        for (uint32_t i = 1; i <= dim; i++) {
            spatial_sq += p->coords[i] * p->coords[i];
        }
        p->coords[0] = sqrtf(1.0f / curvature + spatial_sq);
    } else {
        /* Origin of hyperboloid: (1/sqrt(c), 0, 0, ..., 0) */
        p->coords[0] = 1.0f / sqrtf(curvature);
    }

    return p;
}

void lorentz_point_destroy(lorentz_point_t* point) {
    if (!point) return;
    nimcp_free(point->coords);
    nimcp_free(point);
}

lorentz_point_t* lorentz_point_copy(const lorentz_point_t* src) {
    if (!src) return NULL;

    lorentz_point_t* p = nimcp_calloc(1, sizeof(lorentz_point_t));
    if (!p) return NULL;

    p->dim = src->dim;
    p->curvature = src->curvature;
    p->coords = nimcp_calloc(src->dim + 1, sizeof(float));
    if (!p->coords) { nimcp_free(p); return NULL; }

    memcpy(p->coords, src->coords, (src->dim + 1) * sizeof(float));
    return p;
}

void lorentz_project(lorentz_point_t* point) {
    if (!point || !point->coords) return;

    /* Project onto hyperboloid: x_0 = sqrt(1/c + sum(x_i^2)) */
    float spatial_sq = 0.0f;
    for (uint32_t i = 1; i <= point->dim; i++) {
        spatial_sq += point->coords[i] * point->coords[i];
    }
    point->coords[0] = sqrtf(1.0f / point->curvature + spatial_sq);
}

/*=============================================================================
 * Core Operations
 *===========================================================================*/

float lorentz_inner_product(
    const lorentz_point_t* x, const lorentz_point_t* y)
{
    if (!x || !y || x->dim != y->dim) return 0.0f;

    /* <x,y>_L = -x_0*y_0 + sum(x_i*y_i) */
    float result = -x->coords[0] * y->coords[0];
    for (uint32_t i = 1; i <= x->dim; i++) {
        result += x->coords[i] * y->coords[i];
    }
    return result;
}

float lorentz_distance(
    const lorentz_point_t* x, const lorentz_point_t* y)
{
    if (!x || !y || x->dim != y->dim) return 0.0f;

    float c = x->curvature;
    float ip = lorentz_inner_product(x, y);
    /* d(x,y) = (1/sqrt(c)) * acosh(-c * <x,y>_L) */
    float arg = -c * ip;
    return safe_acosh_l(arg) / sqrtf(c);
}

float lorentz_tangent_norm(const float* v, uint32_t dim) {
    if (!v || dim == 0) return 0.0f;

    /* ||v||_L = sqrt(<v,v>_L) where <v,v>_L = -v_0^2 + sum(v_i^2) */
    float ip = -v[0] * v[0];
    for (uint32_t i = 1; i <= dim; i++) {
        ip += v[i] * v[i];
    }
    /* Tangent vectors have positive Lorentzian norm */
    return (ip > 0.0f) ? sqrtf(ip) : 0.0f;
}

/*=============================================================================
 * Exponential and Logarithmic Maps
 *===========================================================================*/

lorentz_point_t* lorentz_exp_map(
    const lorentz_point_t* base, const float* tangent_vec)
{
    if (!base || !tangent_vec) return NULL;

    uint32_t n = base->dim;
    float c = base->curvature;
    float sqrt_c = sqrtf(c);

    /* ||v||_L */
    float v_norm = lorentz_tangent_norm(tangent_vec, n);

    lorentz_point_t* result = nimcp_calloc(1, sizeof(lorentz_point_t));
    if (!result) return NULL;
    result->dim = n;
    result->curvature = c;
    result->coords = nimcp_calloc(n + 1, sizeof(float));
    if (!result->coords) { nimcp_free(result); return NULL; }

    if (v_norm < LORENTZ_EPSILON) {
        /* Zero tangent vector -> stay at base */
        memcpy(result->coords, base->coords, (n + 1) * sizeof(float));
        return result;
    }

    /* exp_x(v) = cosh(sqrt(c)*||v||_L) * x + sinh(sqrt(c)*||v||_L) / (sqrt(c)*||v||_L) * v */
    float scaled_norm = sqrt_c * v_norm;
    float cosh_val = coshf(scaled_norm);
    float sinh_over = sinhf(scaled_norm) / (sqrt_c * v_norm);

    for (uint32_t i = 0; i <= n; i++) {
        result->coords[i] = cosh_val * base->coords[i] + sinh_over * tangent_vec[i];
    }

    /* Project to ensure numerical precision */
    lorentz_project(result);

    return result;
}

float* lorentz_log_map(
    const lorentz_point_t* base, const lorentz_point_t* point)
{
    if (!base || !point || base->dim != point->dim) return NULL;

    uint32_t n = base->dim;
    float c = base->curvature;

    float dist = lorentz_distance(base, point);
    if (dist < LORENTZ_EPSILON) {
        /* Same point -> zero tangent vector */
        return nimcp_calloc(n + 1, sizeof(float));
    }

    float ip = lorentz_inner_product(base, point);

    /* u = y + <x,y>_L * c * x (project y onto tangent space at x) */
    float* u = nimcp_calloc(n + 1, sizeof(float));
    if (!u) return NULL;

    for (uint32_t i = 0; i <= n; i++) {
        u[i] = point->coords[i] + ip * c * base->coords[i];
    }

    /* Normalize: log_x(y) = d(x,y) * u / ||u||_L */
    float u_norm = lorentz_tangent_norm(u, n);
    if (u_norm < LORENTZ_EPSILON) {
        nimcp_free(u);
        return nimcp_calloc(n + 1, sizeof(float));
    }

    float scale = dist / u_norm;
    for (uint32_t i = 0; i <= n; i++) {
        u[i] *= scale;
    }

    return u;
}

/*=============================================================================
 * Parallel Transport
 *===========================================================================*/

int lorentz_parallel_transport(
    const lorentz_point_t* x,
    const lorentz_point_t* y,
    const float* v,
    float* result)
{
    if (!x || !y || !v || !result) return -1;
    if (x->dim != y->dim) return -1;

    uint32_t n = x->dim;
    float c = x->curvature;

    float ip_xy = lorentz_inner_product(x, y);
    float ip_yv = 0.0f;

    /* <y,v>_L */
    ip_yv = -y->coords[0] * v[0];
    for (uint32_t i = 1; i <= n; i++) {
        ip_yv += y->coords[i] * v[i];
    }

    float denom = 1.0f / c - ip_xy;
    if (fabsf(denom) < LORENTZ_EPSILON) {
        /* Points are the same or numerically indistinguishable */
        memcpy(result, v, (n + 1) * sizeof(float));
        return 0;
    }

    /* PT_{x->y}(v) = v + <y,v>_L / (1/c - <x,y>_L) * (x + y) */
    float factor = ip_yv / denom;
    for (uint32_t i = 0; i <= n; i++) {
        result[i] = v[i] + factor * (x->coords[i] + y->coords[i]);
    }

    return 0;
}

/*=============================================================================
 * Model Conversion
 *===========================================================================*/

int lorentz_to_poincare(const lorentz_point_t* lorentz, float* poincare) {
    if (!lorentz || !poincare) return -1;

    /* p_i = x_{i+1} / (1 + x_0) for i = 0..dim-1 */
    float denom = 1.0f + lorentz->coords[0];
    if (fabsf(denom) < LORENTZ_EPSILON) return -1;

    for (uint32_t i = 0; i < lorentz->dim; i++) {
        poincare[i] = lorentz->coords[i + 1] / denom;
    }

    return 0;
}

lorentz_point_t* lorentz_from_poincare(
    const float* poincare, uint32_t dim, float curvature)
{
    if (!poincare || dim == 0) return NULL;
    if (curvature <= 0.0f) curvature = 1.0f;

    /* ||p||^2 */
    float p_sq = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        p_sq += poincare[i] * poincare[i];
    }

    if (p_sq >= 1.0f - LORENTZ_EPSILON) {
        /* Point at or beyond Poincare boundary */
        return NULL;
    }

    lorentz_point_t* pt = nimcp_calloc(1, sizeof(lorentz_point_t));
    if (!pt) return NULL;
    pt->dim = dim;
    pt->curvature = curvature;
    pt->coords = nimcp_calloc(dim + 1, sizeof(float));
    if (!pt->coords) { nimcp_free(pt); return NULL; }

    float denom = 1.0f - p_sq;
    /* x_0 = (1 + ||p||^2) / (1 - ||p||^2) */
    pt->coords[0] = (1.0f + p_sq) / denom;
    /* x_i = 2*p_i / (1 - ||p||^2) */
    for (uint32_t i = 0; i < dim; i++) {
        pt->coords[i + 1] = 2.0f * poincare[i] / denom;
    }

    return pt;
}

lorentz_point_t* lorentz_midpoint(
    const lorentz_point_t* x, const lorentz_point_t* y)
{
    return lorentz_slerp(x, y, 0.5f);
}

lorentz_point_t* lorentz_slerp(
    const lorentz_point_t* x, const lorentz_point_t* y, float t)
{
    if (!x || !y || x->dim != y->dim) return NULL;

    t = clampf_l(t, 0.0f, 1.0f);

    /* SLERP on hyperboloid:
     * 1. Compute log_x(y) to get tangent vector
     * 2. Scale by t
     * 3. Exp map back */
    float* v = lorentz_log_map(x, y);
    if (!v) return lorentz_point_copy(x);

    /* Scale tangent vector by t */
    for (uint32_t i = 0; i <= x->dim; i++) {
        v[i] *= t;
    }

    lorentz_point_t* result = lorentz_exp_map(x, v);
    nimcp_free(v);

    return result;
}
