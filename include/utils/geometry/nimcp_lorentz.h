/**
 * @file nimcp_lorentz.h
 * @brief Lorentz (Hyperboloid) Model of Hyperbolic Space
 *
 * WHAT: Alternative hyperbolic model using the upper sheet of a hyperboloid
 * WHY:  More numerically stable than Poincare ball for:
 *       - Large distances (no boundary crowding)
 *       - High-dimensional embeddings
 *       - Gradient computation (simpler formulas)
 *
 * MODEL:
 * The Lorentz model uses H^n = {x in R^{n+1} : <x,x>_L = -1, x_0 > 0}
 * where <x,y>_L = -x_0*y_0 + x_1*y_1 + ... + x_n*y_n (Minkowski inner product)
 *
 * KEY OPERATIONS:
 * - Distance: d(x,y) = acosh(-<x,y>_L)
 * - Exp map: exp_x(v) = cosh(||v||_L)*x + sinh(||v||_L)*v/||v||_L
 * - Log map: log_x(y) = d(x,y) * (y + <x,y>_L * x) / ||y + <x,y>_L * x||_L
 * - Parallel transport: PT_{x->y}(v) = v - <y,v>_L/(1-<x,y>_L) * (x+y)
 *
 * CONVERSION:
 * - Lorentz -> Poincare: p_i = x_i / (1 + x_0) for i=1..n
 * - Poincare -> Lorentz: x_0 = (1+||p||^2)/(1-||p||^2), x_i = 2p_i/(1-||p||^2)
 *
 * @version 1.0.0
 * @date 2026-03-07
 */

#ifndef NIMCP_LORENTZ_H
#define NIMCP_LORENTZ_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

/*=============================================================================
 * Constants
 *===========================================================================*/

#define LORENTZ_EPSILON     1e-7f
#define LORENTZ_MAX_DIM     64

/*=============================================================================
 * Types
 *===========================================================================*/

/**
 * @brief Point on the hyperboloid H^n in R^{n+1}
 *
 * coords[0] is the "time" component, coords[1..dim] are spatial.
 * Constraint: -coords[0]^2 + sum(coords[1..dim]^2) = -1/c (c = curvature)
 */
typedef struct {
    float* coords;      /**< Coordinates [dim+1] (Minkowski space) */
    uint32_t dim;       /**< Hyperbolic dimension n (ambient = n+1) */
    float curvature;    /**< Curvature c > 0 (K = -c, default c = 1) */
} lorentz_point_t;

/*=============================================================================
 * Point Management
 *===========================================================================*/

/**
 * @brief Create a point on the hyperboloid
 * @param dim Hyperbolic dimension n
 * @param coords Initial spatial coords [dim] (or NULL for origin)
 * @param curvature Curvature c (use 1.0 for standard)
 * @return Point on hyperboloid or NULL
 */
NIMCP_EXPORT lorentz_point_t* lorentz_point_create(
    uint32_t dim, const float* coords, float curvature);

NIMCP_EXPORT void lorentz_point_destroy(lorentz_point_t* point);
NIMCP_EXPORT lorentz_point_t* lorentz_point_copy(const lorentz_point_t* src);

/**
 * @brief Project point onto hyperboloid (enforce constraint)
 */
NIMCP_EXPORT void lorentz_project(lorentz_point_t* point);

/*=============================================================================
 * Core Operations
 *===========================================================================*/

/**
 * @brief Minkowski inner product: <x,y>_L = -x_0*y_0 + sum(x_i*y_i)
 */
NIMCP_EXPORT float lorentz_inner_product(
    const lorentz_point_t* x, const lorentz_point_t* y);

/**
 * @brief Hyperbolic distance: d(x,y) = (1/sqrt(c)) * acosh(-c * <x,y>_L)
 */
NIMCP_EXPORT float lorentz_distance(
    const lorentz_point_t* x, const lorentz_point_t* y);

/**
 * @brief Lorentz norm of tangent vector at x: ||v||_L = sqrt(<v,v>_L)
 * @param v Tangent vector [dim+1]
 * @param dim Hyperbolic dimension
 */
NIMCP_EXPORT float lorentz_tangent_norm(const float* v, uint32_t dim);

/*=============================================================================
 * Exponential and Logarithmic Maps
 *===========================================================================*/

/**
 * @brief Exponential map on hyperboloid
 *
 * exp_x(v) = cosh(||v||_L/sqrt(c)) * x + sqrt(c) * sinh(||v||_L/sqrt(c)) * v / ||v||_L
 */
NIMCP_EXPORT lorentz_point_t* lorentz_exp_map(
    const lorentz_point_t* base, const float* tangent_vec);

/**
 * @brief Logarithmic map on hyperboloid
 *
 * log_x(y) = d(x,y) * (y + <x,y>_L * c * x) / ||y + <x,y>_L * c * x||_L
 *
 * @return Tangent vector [dim+1], caller must free
 */
NIMCP_EXPORT float* lorentz_log_map(
    const lorentz_point_t* base, const lorentz_point_t* point);

/*=============================================================================
 * Parallel Transport
 *===========================================================================*/

/**
 * @brief Parallel transport from x to y on hyperboloid
 *
 * PT_{x->y}(v) = v + <y,v>_L / (1/c - <x,y>_L) * (x + y)
 *
 * Closed-form (no ODE integration needed).
 *
 * @param x Start point
 * @param y End point
 * @param v Tangent vector at x [dim+1]
 * @param result Output: transported vector at y [dim+1]
 */
NIMCP_EXPORT int lorentz_parallel_transport(
    const lorentz_point_t* x,
    const lorentz_point_t* y,
    const float* v,
    float* result);

/*=============================================================================
 * Model Conversion
 *===========================================================================*/

/**
 * @brief Convert Lorentz point to Poincare ball coordinates
 * @param lorentz Lorentz point
 * @param poincare Output: Poincare coordinates [dim], caller must free
 */
NIMCP_EXPORT int lorentz_to_poincare(
    const lorentz_point_t* lorentz, float* poincare);

/**
 * @brief Convert Poincare ball to Lorentz point
 * @param poincare Poincare coordinates [dim]
 * @param dim Dimension
 * @param curvature Curvature
 * @return Lorentz point or NULL
 */
NIMCP_EXPORT lorentz_point_t* lorentz_from_poincare(
    const float* poincare, uint32_t dim, float curvature);

/**
 * @brief Geodesic midpoint on hyperboloid
 */
NIMCP_EXPORT lorentz_point_t* lorentz_midpoint(
    const lorentz_point_t* x, const lorentz_point_t* y);

/**
 * @brief Geodesic interpolation (SLERP on hyperboloid)
 * @param x Start point
 * @param y End point
 * @param t Parameter [0,1]
 * @return Interpolated point or NULL
 */
NIMCP_EXPORT lorentz_point_t* lorentz_slerp(
    const lorentz_point_t* x, const lorentz_point_t* y, float t);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LORENTZ_H */
