//=============================================================================
// nimcp_hyperbolic.h - Hyperbolic Geometry Operations (Poincaré Ball Model)
//=============================================================================
/**
 * @file nimcp_hyperbolic.h
 * @brief Hyperbolic geometry operations for hierarchical knowledge embeddings
 *
 * MATHEMATICAL BACKGROUND:
 * Hyperbolic space H^n has constant negative curvature K = -1. Unlike Euclidean
 * space where circumference grows linearly (C = 2πr), hyperbolic circumference
 * grows exponentially (C ∝ e^r). This exponential growth perfectly matches
 * tree hierarchies where number of nodes grows as 2^depth.
 *
 * POINCARÉ BALL MODEL:
 * We use the Poincaré ball B^n = {x ∈ R^n : ||x|| < 1} with metric:
 *   ds² = 4/(1 - ||x||²)² * ||dx||²
 *
 * KEY OPERATIONS:
 * - Distance: d(x,y) = acosh(1 + 2||x-y||²/((1-||x||²)(1-||y||²)))
 * - Exponential map: Converts tangent vectors to points on manifold
 * - Logarithmic map: Converts manifold points to tangent vectors
 * - Möbius addition: Parallel transport on the manifold
 *
 * BENEFITS FOR NIMCP:
 * - 200x memory reduction: Embed 1M concepts in 5D instead of 1000D
 * - Natural hierarchies: WordNet, moral principles, abstraction levels
 * - Zero-shot learning: Inherit properties from hyperbolic neighbors
 *
 * PART B1.1: Hyperbolic Knowledge Graph Embeddings
 *
 * REFERENCES:
 * - Nickel & Kiela (2017) "Poincaré Embeddings"
 * - Ganea et al. (2018) "Hyperbolic Neural Networks"
 * - Chami et al. (2019) "Hyperbolic Graph Convolutional Networks"
 *
 * @author NIMCP Development Team
 * @date 2025-11-11
 * @version 1.0.0
 */

#ifndef NIMCP_HYPERBOLIC_H
#define NIMCP_HYPERBOLIC_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Default curvature for hyperbolic space (K = -1) */
#define HYPERBOLIC_CURVATURE_DEFAULT (-1.0f)

/** Maximum radius in Poincaré ball (||x|| < 1, but clip to 0.9999 for stability) */
#define POINCARE_MAX_RADIUS (0.9999f)

/** Numerical epsilon for stability (prevents division by zero near boundary) */
#define POINCARE_EPSILON (1e-7f)

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Point in Poincaré ball model of hyperbolic space
 *
 * WHAT: Represents point in B^n = {x ∈ R^n : ||x|| < 1}
 * WHY: Enables hierarchical embeddings with exponential capacity
 * HOW: Stores n-dimensional coordinates with constraint ||coords|| < 1
 *
 * INVARIANTS:
 * - coords must be non-NULL if dim > 0
 * - ||coords|| < POINCARE_MAX_RADIUS (enforced by clipping)
 * - curvature should be negative (typically -1)
 */
typedef struct {
    float *coords;       /**< Coordinates [dim], ||coords|| < 1 */
    uint32_t dim;        /**< Dimension (typically 2-10 for knowledge) */
    float curvature;     /**< Curvature K (default -1) */
} poincare_point_t;

/**
 * @brief Configuration for hyperbolic embeddings
 *
 * WHAT: Parameters controlling hyperbolic space properties
 * WHY: Allows tuning of embedding quality vs efficiency
 */
typedef struct {
    uint32_t dim;              /**< Embedding dimension (2-10) */
    float curvature;           /**< Curvature K (default -1) */
    float learning_rate;       /**< For Riemannian SGD */
    float clip_norm;           /**< Gradient clipping (default 1.0) */
    bool use_exponential_map;  /**< Expensive but more accurate */
} hyperbolic_config_t;

//=============================================================================
// Memory Management
//=============================================================================

/**
 * @brief Create a point in Poincaré ball
 *
 * WHAT: Allocates and initializes a hyperbolic point
 * WHY: Factory function for safe point creation
 * HOW: Allocates coords array, initializes to origin or given values
 *
 * @param dim Dimension (must be > 0)
 * @param coords Initial coordinates (can be NULL for origin)
 * @param curvature Curvature K (use -1.0 for standard hyperbolic space)
 * @return Allocated point or NULL on failure
 *
 * COMPLEXITY: O(dim)
 * MEMORY: sizeof(poincare_point_t) + dim * sizeof(float)
 */
poincare_point_t* poincare_point_create(uint32_t dim, const float *coords, float curvature);

/**
 * @brief Destroy a Poincaré point
 *
 * WHAT: Frees memory associated with point
 * WHY: Proper cleanup prevents memory leaks
 *
 * @param point Point to destroy (NULL-safe)
 *
 * COMPLEXITY: O(1)
 */
void poincare_point_destroy(poincare_point_t *point);

/**
 * @brief Copy a Poincaré point
 *
 * WHAT: Creates deep copy of point
 * WHY: Needed for operations that return new points
 *
 * @param src Source point
 * @return Copy of point or NULL on failure
 *
 * COMPLEXITY: O(dim)
 */
poincare_point_t* poincare_point_copy(const poincare_point_t *src);

//=============================================================================
// Basic Operations
//=============================================================================

/**
 * @brief Compute Euclidean norm of point coordinates
 *
 * WHAT: ||x|| = sqrt(x₁² + x₂² + ... + xₙ²)
 * WHY: Used in distance calculations and clipping
 *
 * @param point Point to compute norm of
 * @return Euclidean norm ||point.coords||
 *
 * COMPLEXITY: O(dim)
 */
float poincare_norm(const poincare_point_t *point);

/**
 * @brief Clip point to stay within Poincaré ball
 *
 * WHAT: If ||x|| >= 1, project to ||x|| = POINCARE_MAX_RADIUS
 * WHY: Prevents numerical instability near boundary
 * HOW: x' = x * (POINCARE_MAX_RADIUS / ||x||)
 *
 * @param point Point to clip (modified in-place)
 *
 * COMPLEXITY: O(dim)
 */
void poincare_clip(poincare_point_t *point);

/**
 * @brief Compute hyperbolic distance in Poincaré ball
 *
 * WHAT: d_H(x, y) = acosh(1 + 2||x-y||² / ((1-||x||²)(1-||y||²)))
 * WHY: Fundamental distance metric in hyperbolic space
 * HOW: Uses conformal model formula
 *
 * MATHEMATICAL DERIVATION:
 * The Poincaré distance comes from integrating the metric tensor:
 *   ds = 2/(1-||x||²) * ||dx||
 * For geodesic connecting x and y, we get:
 *   d(x,y) = acosh(1 + 2Δ) where Δ = ||x-y||² / ((1-||x||²)(1-||y||²))
 *
 * PROPERTIES:
 * - d(x,y) = 0 iff x = y
 * - d(x,y) = d(y,x) (symmetric)
 * - d(x,z) ≤ d(x,y) + d(y,z) (triangle inequality)
 * - d(x,y) → ∞ as x or y → boundary
 *
 * @param x First point
 * @param y Second point
 * @return Hyperbolic distance (≥ 0)
 *
 * COMPLEXITY: O(dim)
 * NUMERICAL STABILITY: Uses acosh(x) = log(x + sqrt(x²-1)) for large x
 */
float poincare_distance(const poincare_point_t *x, const poincare_point_t *y);

/**
 * @brief Compute squared Euclidean distance (helper)
 *
 * WHAT: ||x - y||² = Σᵢ(xᵢ - yᵢ)²
 * WHY: Frequently needed in hyperbolic formulas
 *
 * @param x First point
 * @param y Second point
 * @return Squared Euclidean distance
 *
 * COMPLEXITY: O(dim)
 */
float poincare_euclidean_dist_squared(const poincare_point_t *x, const poincare_point_t *y);

//=============================================================================
// Exponential and Logarithmic Maps
//=============================================================================

/**
 * @brief Exponential map: tangent space → manifold
 *
 * WHAT: Maps tangent vector v at point p to a point on the manifold
 * WHY: Required for optimization (apply gradients to manifold points)
 * HOW: exp_p(v) = p ⊕ (tanh(λ_p||v||/2) * v/||v||)
 *
 * MATHEMATICAL BACKGROUND:
 * The exponential map follows the geodesic starting at p with initial velocity v
 * for unit time. In Poincaré ball:
 *   λ_p = 2/(1-||p||²)  (conformal factor)
 *   exp_p(v) = p ⊕ (tanh(λ_p||v||/2) * v/||v||)
 * where ⊕ is Möbius addition
 *
 * USAGE: Riemannian gradient descent
 *   x_{t+1} = exp_{x_t}(-η * grad_R f(x_t))
 *
 * @param base Base point p ∈ B^n
 * @param tangent_vec Tangent vector v ∈ T_p B^n [dim]
 * @return Point on manifold exp_p(v)
 *
 * COMPLEXITY: O(dim)
 */
poincare_point_t* poincare_exp_map(const poincare_point_t *base, const float *tangent_vec);

/**
 * @brief Logarithmic map: manifold → tangent space
 *
 * WHAT: Maps point q on manifold to tangent vector at p
 * WHY: Required for computing Riemannian gradients
 * HOW: log_p(q) = (2/λ_p) * artanh(||(-p) ⊕ q||) * ((-p) ⊕ q)/||(−p) ⊕ q||
 *
 * MATHEMATICAL BACKGROUND:
 * Inverse of exponential map. Returns tangent vector v such that exp_p(v) = q.
 *
 * USAGE: Convert distances to tangent space for learning
 *
 * @param base Base point p ∈ B^n
 * @param point Target point q ∈ B^n
 * @return Tangent vector log_p(q) [dim], caller must free
 *
 * COMPLEXITY: O(dim)
 * MEMORY: Allocates dim * sizeof(float)
 */
float* poincare_log_map(const poincare_point_t *base, const poincare_point_t *point);

//=============================================================================
// Möbius Addition and Gyrovector Operations
//=============================================================================

/**
 * @brief Möbius addition (gyrovector addition)
 *
 * WHAT: x ⊕ y = ((1+2⟨x,y⟩+||y||²)x + (1-||x||²)y) / (1+2⟨x,y⟩+||x||²||y||²)
 * WHY: Analog of vector addition in hyperbolic space
 * HOW: Uses conformal model formula
 *
 * PROPERTIES:
 * - Not commutative: x ⊕ y ≠ y ⊕ x (gyrovector space!)
 * - Associative up to gyration: (x ⊕ y) ⊕ z = x ⊕ (y ⊕ gyr[x,y]z)
 * - Origin is identity: 0 ⊕ x = x ⊕ 0 = x
 * - Inverse: x ⊕ (-x) = 0
 *
 * USAGE: Parallel transport, neural network operations
 *
 * @param x First point
 * @param y Second point
 * @return x ⊕ y (new point)
 *
 * COMPLEXITY: O(dim)
 */
poincare_point_t* poincare_mobius_add(const poincare_point_t *x, const poincare_point_t *y);

/**
 * @brief Möbius scalar multiplication
 *
 * WHAT: r ⊗ x = tanh(r * artanh(||x||)) * x/||x||
 * WHY: Scaling in hyperbolic space
 *
 * @param r Scalar multiplier
 * @param x Point to scale
 * @return r ⊗ x (new point)
 *
 * COMPLEXITY: O(dim)
 */
poincare_point_t* poincare_mobius_scalar_mult(float r, const poincare_point_t *x);

//=============================================================================
// Riemannian Optimization
//=============================================================================

/**
 * @brief Compute Riemannian gradient
 *
 * WHAT: grad_R f = (1-||x||²)²/4 * grad_E f
 * WHY: Euclidean gradient doesn't respect manifold structure
 * HOW: Scale by conformal factor
 *
 * USAGE:
 *   grad_E = euclidean_gradient(loss, x)
 *   grad_R = poincare_riemannian_gradient(x, grad_E)
 *   x_new = poincare_exp_map(x, -learning_rate * grad_R)
 *
 * @param point Current point x
 * @param euclidean_grad Euclidean gradient ∂f/∂x [dim]
 * @return Riemannian gradient [dim], caller must free
 *
 * COMPLEXITY: O(dim)
 */
float* poincare_riemannian_gradient(const poincare_point_t *point, const float *euclidean_grad);

/**
 * @brief Perform Riemannian SGD step
 *
 * WHAT: x_{t+1} = exp_x(-η * grad_R f(x))
 * WHY: Optimization that respects manifold geometry
 * HOW: Computes Riemannian gradient, applies via exponential map
 *
 * @param point Current point (modified in-place)
 * @param euclidean_grad Euclidean gradient [dim]
 * @param learning_rate Step size η
 * @return true on success, false on error
 *
 * COMPLEXITY: O(dim)
 */
bool poincare_sgd_step(poincare_point_t *point, const float *euclidean_grad, float learning_rate);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Compute conformal factor at point
 *
 * WHAT: λ_p = 2/(1-||p||²)
 * WHY: Appears in many hyperbolic formulas
 *
 * @param point Point p
 * @return Conformal factor
 *
 * COMPLEXITY: O(dim) (needs to compute norm)
 */
float poincare_conformal_factor(const poincare_point_t *point);

/**
 * @brief Get default hyperbolic configuration
 *
 * @return Default config with reasonable parameters
 */
hyperbolic_config_t poincare_default_config(void);

//=============================================================================
// Debugging and Visualization
//=============================================================================

/**
 * @brief Print point coordinates
 *
 * @param point Point to print
 * @param label Optional label (can be NULL)
 */
void poincare_point_print(const poincare_point_t *point, const char *label);

/**
 * @brief Check if point is valid (within ball, finite coords)
 *
 * @param point Point to validate
 * @return true if valid, false otherwise
 */
bool poincare_point_is_valid(const poincare_point_t *point);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_HYPERBOLIC_H
