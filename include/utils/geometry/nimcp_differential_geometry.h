/**
 * @file nimcp_differential_geometry.h
 * @brief Differential Geometry Operations for Neural Manifolds
 *
 * WHAT: Core differential geometry primitives for manifold-aware learning
 * WHY:  Neural activity lives on curved manifolds; Euclidean operations
 *       introduce systematic errors. Proper geometry enables:
 *       - Natural gradient descent (2-10x faster convergence)
 *       - Parallel transport of gradients between tangent spaces
 *       - Geodesic interpolation for smooth state transitions
 *       - Curvature-aware pruning and plasticity
 *
 * MODULES:
 * 1. Riemannian Metric: General metric tensor operations
 * 2. Christoffel Symbols: Connection coefficients for covariant derivative
 * 3. Curvature: Riemann tensor, Ricci tensor, scalar curvature
 * 4. Parallel Transport: Move vectors along curves on manifold
 * 5. Geodesics: Shortest paths and interpolation on manifold
 *
 * COMPLEMENTS:
 * - nimcp_hyperbolic.h: Poincare ball model (specific manifold)
 * - nimcp_information_geometry.h: Fisher metric on parameter space
 * - nimcp_lorentz.h: Hyperboloid model (alternative hyperbolic)
 * - nimcp_lie_group.h: Matrix Lie groups (SO(3), etc.)
 *
 * @version 1.0.0
 * @date 2026-03-07
 */

#ifndef NIMCP_DIFFERENTIAL_GEOMETRY_H
#define NIMCP_DIFFERENTIAL_GEOMETRY_H

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

#define DIFFGEO_MAX_DIM         64
#define DIFFGEO_EPSILON         1e-7f
#define DIFFGEO_GEODESIC_STEPS  100
#define DIFFGEO_TRANSPORT_STEPS 50

/*=============================================================================
 * Error Codes
 *===========================================================================*/

typedef enum {
    DIFFGEO_OK              =  0,
    DIFFGEO_ERR_NULL_PTR    = -1,
    DIFFGEO_ERR_INVALID_DIM = -2,
    DIFFGEO_ERR_SINGULAR    = -3,
    DIFFGEO_ERR_DIVERGED    = -4,
    DIFFGEO_ERR_NO_MEMORY   = -5,
    DIFFGEO_ERR_NOT_INIT    = -6
} diffgeo_error_t;

/*=============================================================================
 * Riemannian Metric Tensor
 *===========================================================================*/

/**
 * @brief Riemannian metric tensor g_{ij} at a point
 *
 * Stores the symmetric positive-definite metric tensor as a flat array
 * in row-major order. For dim=n, the metric has n*(n+1)/2 independent
 * components but we store the full n*n for simplicity.
 */
typedef struct {
    float* g;           /**< Metric tensor g_{ij} [dim*dim], row-major */
    float* g_inv;       /**< Inverse metric g^{ij} [dim*dim] */
    float det;          /**< Determinant of metric */
    float sqrt_det;     /**< sqrt(|det(g)|) for volume element */
    uint32_t dim;       /**< Dimension */
    bool inv_valid;     /**< Whether g_inv is up to date */
} riemannian_metric_t;

/**
 * @brief Create a Riemannian metric tensor
 * @param dim Dimension of the manifold
 * @return Allocated metric (identity by default) or NULL
 */
NIMCP_EXPORT riemannian_metric_t* riemannian_metric_create(uint32_t dim);

/**
 * @brief Destroy a Riemannian metric
 */
NIMCP_EXPORT void riemannian_metric_destroy(riemannian_metric_t* metric);

/**
 * @brief Set metric from flat array
 * @param metric Metric to update
 * @param g_data Symmetric matrix data [dim*dim], row-major
 * @return DIFFGEO_OK or error
 */
NIMCP_EXPORT diffgeo_error_t riemannian_metric_set(
    riemannian_metric_t* metric,
    const float* g_data);

/**
 * @brief Compute inverse metric g^{ij}
 *
 * Uses Cholesky decomposition for symmetric positive definite matrices.
 * Falls back to LU with pivoting if Cholesky fails.
 *
 * @param metric Metric to invert
 * @return DIFFGEO_OK or DIFFGEO_ERR_SINGULAR
 */
NIMCP_EXPORT diffgeo_error_t riemannian_metric_invert(riemannian_metric_t* metric);

/**
 * @brief Compute inner product <u, v>_g = g_{ij} u^i v^j
 * @param metric Metric tensor
 * @param u First vector [dim]
 * @param v Second vector [dim]
 * @return Inner product value
 */
NIMCP_EXPORT float riemannian_inner_product(
    const riemannian_metric_t* metric,
    const float* u,
    const float* v);

/**
 * @brief Compute norm ||v||_g = sqrt(<v,v>_g)
 */
NIMCP_EXPORT float riemannian_norm(
    const riemannian_metric_t* metric,
    const float* v);

/**
 * @brief Raise index: v^i = g^{ij} v_j
 * @param metric Metric (must have valid inverse)
 * @param v_lower Covariant vector v_j [dim]
 * @param v_upper Output: contravariant vector v^i [dim]
 */
NIMCP_EXPORT diffgeo_error_t riemannian_raise_index(
    const riemannian_metric_t* metric,
    const float* v_lower,
    float* v_upper);

/**
 * @brief Lower index: v_i = g_{ij} v^j
 */
NIMCP_EXPORT diffgeo_error_t riemannian_lower_index(
    const riemannian_metric_t* metric,
    const float* v_upper,
    float* v_lower);

/*=============================================================================
 * Christoffel Symbols (Levi-Civita Connection)
 *===========================================================================*/

/**
 * @brief Christoffel symbols of the second kind: Gamma^k_{ij}
 *
 * Stores the connection coefficients for the Levi-Civita connection:
 * Gamma^k_{ij} = (1/2) g^{kl} (dg_{li}/dx^j + dg_{lj}/dx^i - dg_{ij}/dx^l)
 */
typedef struct {
    float* gamma;       /**< Gamma^k_{ij} [dim*dim*dim], index order [k][i][j] */
    uint32_t dim;       /**< Dimension */
} christoffel_symbols_t;

/**
 * @brief Create Christoffel symbol storage
 * @param dim Manifold dimension
 * @return Allocated symbols (zeroed) or NULL
 */
NIMCP_EXPORT christoffel_symbols_t* christoffel_create(uint32_t dim);

/**
 * @brief Destroy Christoffel symbols
 */
NIMCP_EXPORT void christoffel_destroy(christoffel_symbols_t* chris);

/**
 * @brief Compute Christoffel symbols from metric and its derivatives
 *
 * @param chris Output Christoffel symbols
 * @param metric Current metric g_{ij}
 * @param dg_dx Metric derivatives dg_{ij}/dx^k [dim*dim*dim], index [k][i][j]
 * @return DIFFGEO_OK or error
 */
NIMCP_EXPORT diffgeo_error_t christoffel_compute(
    christoffel_symbols_t* chris,
    const riemannian_metric_t* metric,
    const float* dg_dx);

/**
 * @brief Get Gamma^k_{ij}
 */
NIMCP_EXPORT float christoffel_get(
    const christoffel_symbols_t* chris,
    uint32_t k, uint32_t i, uint32_t j);

/*=============================================================================
 * Curvature
 *===========================================================================*/

/**
 * @brief Curvature data at a point on the manifold
 */
typedef struct {
    float* riemann;         /**< Riemann tensor R^l_{ijk} [dim^4] */
    float* ricci;           /**< Ricci tensor R_{ij} [dim*dim] */
    float scalar_curvature; /**< Scalar curvature R = g^{ij} R_{ij} */
    float* sectional;       /**< Sectional curvatures K(e_i, e_j) [dim*dim] */
    uint32_t dim;           /**< Dimension */
} curvature_data_t;

/**
 * @brief Create curvature data storage
 */
NIMCP_EXPORT curvature_data_t* curvature_create(uint32_t dim);

/**
 * @brief Destroy curvature data
 */
NIMCP_EXPORT void curvature_destroy(curvature_data_t* curv);

/**
 * @brief Compute Ricci tensor from Christoffel symbols
 *
 * R_{ij} = d_k Gamma^k_{ij} - d_j Gamma^k_{ik}
 *        + Gamma^k_{kl} Gamma^l_{ij} - Gamma^k_{jl} Gamma^l_{ik}
 *
 * @param curv Output curvature data
 * @param chris Christoffel symbols
 * @param dgamma_dx Christoffel derivatives [dim^4]
 * @return DIFFGEO_OK or error
 */
NIMCP_EXPORT diffgeo_error_t curvature_compute_ricci(
    curvature_data_t* curv,
    const christoffel_symbols_t* chris,
    const float* dgamma_dx);

/**
 * @brief Compute scalar curvature R = g^{ij} R_{ij}
 */
NIMCP_EXPORT diffgeo_error_t curvature_compute_scalar(
    curvature_data_t* curv,
    const riemannian_metric_t* metric);

/**
 * @brief Compute sectional curvature K(u, v)
 *
 * K(u,v) = R(u,v,v,u) / (g(u,u)g(v,v) - g(u,v)^2)
 *
 * @param curv Curvature data (needs Riemann tensor)
 * @param metric Riemannian metric
 * @param u First tangent vector [dim]
 * @param v Second tangent vector [dim]
 * @return Sectional curvature value
 */
NIMCP_EXPORT float curvature_sectional(
    const curvature_data_t* curv,
    const riemannian_metric_t* metric,
    const float* u,
    const float* v);

/*=============================================================================
 * Parallel Transport
 *===========================================================================*/

/**
 * @brief Parallel transport a vector along a curve on the manifold
 *
 * Solves the parallel transport equation:
 *   dv^k/dt + Gamma^k_{ij} (dx^i/dt) v^j = 0
 *
 * Uses RK4 integration along the specified curve.
 *
 * @param chris Christoffel symbols (assumed constant or interpolated)
 * @param curve Curve points [num_points * dim]
 * @param num_points Number of points along curve
 * @param dim Manifold dimension
 * @param v_initial Initial vector at curve[0] [dim]
 * @param v_transported Output: transported vector at curve[num_points-1] [dim]
 * @return DIFFGEO_OK or error
 */
NIMCP_EXPORT diffgeo_error_t parallel_transport_along_curve(
    const christoffel_symbols_t* chris,
    const float* curve,
    uint32_t num_points,
    uint32_t dim,
    const float* v_initial,
    float* v_transported);

/**
 * @brief Parallel transport between two points via geodesic
 *
 * Computes geodesic from p to q, then transports v along it.
 * Convenience wrapper for parallel_transport_along_curve.
 *
 * @param chris Christoffel symbols
 * @param metric Riemannian metric
 * @param p Start point [dim]
 * @param q End point [dim]
 * @param dim Manifold dimension
 * @param v Vector at p [dim]
 * @param v_transported Output: vector at q [dim]
 * @param num_steps Number of integration steps
 * @return DIFFGEO_OK or error
 */
NIMCP_EXPORT diffgeo_error_t parallel_transport_geodesic(
    const christoffel_symbols_t* chris,
    const riemannian_metric_t* metric,
    const float* p,
    const float* q,
    uint32_t dim,
    const float* v,
    float* v_transported,
    uint32_t num_steps);

/*=============================================================================
 * Geodesics
 *===========================================================================*/

/**
 * @brief Compute geodesic from initial point with initial velocity
 *
 * Solves the geodesic equation:
 *   d²x^k/dt² + Gamma^k_{ij} (dx^i/dt)(dx^j/dt) = 0
 *
 * Using RK4 on the equivalent first-order system.
 *
 * @param chris Christoffel symbols
 * @param dim Manifold dimension
 * @param x0 Initial position [dim]
 * @param v0 Initial velocity [dim]
 * @param dt Time step
 * @param num_steps Number of steps
 * @param trajectory Output: trajectory points [(num_steps+1) * dim]
 * @return DIFFGEO_OK or error
 */
NIMCP_EXPORT diffgeo_error_t geodesic_shoot(
    const christoffel_symbols_t* chris,
    uint32_t dim,
    const float* x0,
    const float* v0,
    float dt,
    uint32_t num_steps,
    float* trajectory);

/**
 * @brief Geodesic interpolation (SLERP-like on general manifold)
 *
 * Computes point at fraction t along geodesic from p to q.
 * t=0 gives p, t=1 gives q.
 *
 * @param chris Christoffel symbols
 * @param metric Riemannian metric
 * @param dim Manifold dimension
 * @param p Start point [dim]
 * @param q End point [dim]
 * @param t Interpolation parameter [0, 1]
 * @param result Output: interpolated point [dim]
 * @return DIFFGEO_OK or error
 */
NIMCP_EXPORT diffgeo_error_t geodesic_interpolate(
    const christoffel_symbols_t* chris,
    const riemannian_metric_t* metric,
    uint32_t dim,
    const float* p,
    const float* q,
    float t,
    float* result);

/**
 * @brief Compute geodesic distance between two points
 *
 * Approximates by shooting geodesic and measuring arc length.
 *
 * @param chris Christoffel symbols
 * @param metric Riemannian metric
 * @param dim Manifold dimension
 * @param p First point [dim]
 * @param q Second point [dim]
 * @return Geodesic distance (>= 0)
 */
NIMCP_EXPORT float geodesic_distance(
    const christoffel_symbols_t* chris,
    const riemannian_metric_t* metric,
    uint32_t dim,
    const float* p,
    const float* q);

/*=============================================================================
 * Exponential and Logarithmic Maps (General Manifold)
 *===========================================================================*/

/**
 * @brief General exponential map: T_p M -> M
 *
 * Maps tangent vector v at point p to manifold point by following
 * the geodesic with initial velocity v for unit time.
 *
 * @param chris Christoffel symbols
 * @param dim Manifold dimension
 * @param p Base point [dim]
 * @param v Tangent vector [dim]
 * @param result Output: exp_p(v) [dim]
 * @return DIFFGEO_OK or error
 */
NIMCP_EXPORT diffgeo_error_t exp_map(
    const christoffel_symbols_t* chris,
    uint32_t dim,
    const float* p,
    const float* v,
    float* result);

/**
 * @brief General logarithmic map: M -> T_p M
 *
 * Inverse of exponential map. Finds tangent vector v at p such that
 * exp_p(v) = q. Uses shooting method with Newton iteration.
 *
 * @param chris Christoffel symbols
 * @param metric Riemannian metric
 * @param dim Manifold dimension
 * @param p Base point [dim]
 * @param q Target point [dim]
 * @param result Output: log_p(q) [dim]
 * @param max_iterations Maximum Newton iterations
 * @return DIFFGEO_OK or error
 */
NIMCP_EXPORT diffgeo_error_t log_map(
    const christoffel_symbols_t* chris,
    const riemannian_metric_t* metric,
    uint32_t dim,
    const float* p,
    const float* q,
    float* result,
    uint32_t max_iterations);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DIFFERENTIAL_GEOMETRY_H */
