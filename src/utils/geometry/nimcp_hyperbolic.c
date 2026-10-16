#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_hyperbolic.c - Hyperbolic Geometry Implementation
//=============================================================================
/**
 * @file nimcp_hyperbolic.c
 * @brief Implementation of hyperbolic geometry operations for Poincaré ball
 *
 * IMPLEMENTATION NOTES:
 * - All operations use the Poincaré ball model B^n = {x : ||x|| < 1}
 * - Numerical stability is critical near the boundary ||x|| → 1
 * - We clip points to ||x|| < 0.9999 to prevent infinities
 * - Uses standard math.h functions (acosh, atanh, tanh, sqrt)
 *
 * COMPLEXITY ANALYSIS:
 * - Most operations are O(dim) due to vector operations
 * - Distance computation: O(dim)
 * - Exponential/logarithmic maps: O(dim)
 * - Möbius addition: O(dim)
 *
 * @author NIMCP Development Team
 * @date 2025-11-11
 */

#include "utils/geometry/nimcp_hyperbolic.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(hyperbolic)

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Safe acosh implementation
 *
 * WHAT: acosh(x) = log(x + sqrt(x²-1))
 * WHY: Standard acosh may not handle edge cases well
 */
static inline float safe_acosh(float x) {
    if (x < 1.0F + POINCARE_EPSILON) {
        return 0.0F;  // Distance near origin
    }
    if (x > 1e10F) {
        return logf(2.0F * x);  // Asymptotic form for large x
    }
    return acoshf(x);
}

/**
 * @brief Safe atanh implementation
 *
 * WHAT: atanh(x) = 0.5 * log((1+x)/(1-x))
 * WHY: atanh is undefined at x = ±1
 */
static inline float safe_atanh(float x) {
    if (fabsf(x) >= 1.0F - POINCARE_EPSILON) {
        x = copysignf(1.0F - POINCARE_EPSILON, x);  // Clip to valid range
    }
    return atanhf(x);
}

/**
 * @brief Dot product of two vectors
 */
static float dot_product(const float *a, const float *b, uint32_t dim) {
    float sum = 0.0F;
    for (uint32_t i = 0; i < dim; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

/**
 * @brief Squared norm of vector
 */
static float squared_norm(const float *v, uint32_t dim) {
    return dot_product(v, v, dim);
}

/**
 * @brief Vector subtraction: result = a - b
 */
static void vector_subtract(float *result, const float *a, const float *b, uint32_t dim) {
    for (uint32_t i = 0; i < dim; i++) {
        result[i] = a[i] - b[i];
    }
}

/**
 * @brief Vector scalar multiplication: result = scalar * v
 */
static void vector_scale(float *result, float scalar, const float *v, uint32_t dim) {
    for (uint32_t i = 0; i < dim; i++) {
        result[i] = scalar * v[i];
    }
}

/**
 * @brief Vector addition: result = a + b
 */
static void vector_add(float *result, const float *a, const float *b, uint32_t dim) {
    for (uint32_t i = 0; i < dim; i++) {
        result[i] = a[i] + b[i];
    }
}

//=============================================================================
// Memory Management
//=============================================================================

poincare_point_t* poincare_point_create(uint32_t dim, const float *coords, float curvature) {
    // Validate dimension
    if (dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "poincare_point_create: dim is zero");
        return NULL;
    }

    // Allocate point structure
    poincare_point_t *point = (poincare_point_t*)nimcp_malloc(sizeof(poincare_point_t));
    if (!point) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "point is NULL");

        return NULL;
    }

    // Allocate coordinate array
    point->coords = (float*)nimcp_malloc(dim * sizeof(float));
    if (!point->coords) {
        nimcp_free(point);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "poincare_point_create: point->coords is NULL");
        return NULL;
    }

    point->dim = dim;
    point->curvature = curvature;

    // Initialize coordinates
    if (coords) {
        memcpy(point->coords, coords, dim * sizeof(float));
    } else {
        // Initialize to origin
        memset(point->coords, 0, dim * sizeof(float));
    }

    // Clip to ensure point is within ball
    poincare_clip(point);

    return point;
}

void poincare_point_destroy(poincare_point_t *point) {
    if (!point) {
        return;
    }
    if (point->coords) {
        nimcp_free(point->coords);
    }
    nimcp_free(point);
}

poincare_point_t* poincare_point_copy(const poincare_point_t *src) {
    if (!src) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "src is NULL");

        return NULL;
    }
    return poincare_point_create(src->dim, src->coords, src->curvature);
}

//=============================================================================
// Basic Operations
//=============================================================================

float poincare_norm(const poincare_point_t *point) {
    if (!point || !point->coords) {
        return 0.0F;
    }
    float sum_squares = 0.0F;
    for (uint32_t i = 0; i < point->dim; i++) {
        sum_squares += point->coords[i] * point->coords[i];
    }
    return sqrtf(sum_squares);
}

void poincare_clip(poincare_point_t *point) {
    if (!point || !point->coords) {
        return;
    }

    float norm = poincare_norm(point);
    if (norm >= POINCARE_MAX_RADIUS) {
        // Project onto sphere of radius POINCARE_MAX_RADIUS
        float scale = POINCARE_MAX_RADIUS / norm;
        for (uint32_t i = 0; i < point->dim; i++) {
            point->coords[i] *= scale;
        }
    }
}

float poincare_euclidean_dist_squared(const poincare_point_t *x, const poincare_point_t *y) {
    if (!x || !y || !x->coords || !y->coords || x->dim != y->dim) {
        return 0.0F;
    }

    float sum = 0.0F;
    for (uint32_t i = 0; i < x->dim; i++) {
        float diff = x->coords[i] - y->coords[i];
        sum += diff * diff;
    }
    return sum;
}

float poincare_distance(const poincare_point_t *x, const poincare_point_t *y) {
    if (!x || !y || !x->coords || !y->coords || x->dim != y->dim) {
        return 0.0F;
    }

    // Compute ||x-y||²
    float numerator = poincare_euclidean_dist_squared(x, y);

    // Compute (1 - ||x||²)
    float x_norm_sq = squared_norm(x->coords, x->dim);
    float one_minus_x_sq = 1.0F - x_norm_sq;
    one_minus_x_sq = fmaxf(one_minus_x_sq, POINCARE_EPSILON);  // Prevent division by zero

    // Compute (1 - ||y||²)
    float y_norm_sq = squared_norm(y->coords, y->dim);
    float one_minus_y_sq = 1.0F - y_norm_sq;
    one_minus_y_sq = fmaxf(one_minus_y_sq, POINCARE_EPSILON);  // Prevent division by zero

    // Compute denominator: (1 - ||x||²)(1 - ||y||²)
    float denominator = one_minus_x_sq * one_minus_y_sq;

    // Compute Δ = ||x-y||² / ((1-||x||²)(1-||y||²))
    float delta = numerator / denominator;

    // Compute distance: d = acosh(1 + 2Δ)
    float arg = 1.0F + 2.0F * delta;
    return safe_acosh(arg);
}

//=============================================================================
// Exponential and Logarithmic Maps
//=============================================================================

poincare_point_t* poincare_exp_map(const poincare_point_t *base, const float *tangent_vec) {
    if (!base || !base->coords || !tangent_vec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "poincare_exp_map: required parameter is NULL (base, base->coords, tangent_vec)");
        return NULL;
    }

    // Compute conformal factor: λ_p = 2/(1-||p||²)
    float lambda_p = poincare_conformal_factor(base);

    // Compute ||v||
    float v_norm = sqrtf(squared_norm(tangent_vec, base->dim));

    // If tangent vector is zero, return copy of base
    if (v_norm < POINCARE_EPSILON) {
        return poincare_point_copy(base);
    }

    // Compute tanh(λ_p * ||v|| / 2)
    float tanh_arg = lambda_p * v_norm / 2.0F;
    float tanh_val = tanhf(tanh_arg);

    // Compute scaled direction: (tanh(...) / ||v||) * v
    float scale = tanh_val / v_norm;
    float *scaled_v = (float*)nimcp_malloc(base->dim * sizeof(float));
    if (!scaled_v) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "scaled_v is NULL");

        return NULL;
    }
    vector_scale(scaled_v, scale, tangent_vec, base->dim);

    // Create point from scaled vector
    poincare_point_t *scaled_point = poincare_point_create(base->dim, scaled_v, base->curvature);
    nimcp_free(scaled_v);
    if (!scaled_point) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "scaled_point is NULL");

        return NULL;
    }

    // Apply Möbius addition: base ⊕ scaled_point
    poincare_point_t *result = poincare_mobius_add(base, scaled_point);
    poincare_point_destroy(scaled_point);

    return result;
}

float* poincare_log_map(const poincare_point_t *base, const poincare_point_t *point) {
    if (!base || !point || !base->coords || !point->coords || base->dim != point->dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "poincare_log_map: required parameter is NULL (base, point, base->coords, point->coords)");
        return NULL;
    }

    // Compute -base (negation of base point)
    float *neg_base = (float*)nimcp_malloc(base->dim * sizeof(float));
    if (!neg_base) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neg_base is NULL");

        return NULL;
    }
    vector_scale(neg_base, -1.0F, base->coords, base->dim);

    // Create point for -base
    poincare_point_t *neg_base_point = poincare_point_create(base->dim, neg_base, base->curvature);
    nimcp_free(neg_base);
    if (!neg_base_point) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neg_base_point is NULL");

        return NULL;
    }

    // Compute (-base) ⊕ point
    poincare_point_t *diff = poincare_mobius_add(neg_base_point, point);
    poincare_point_destroy(neg_base_point);
    if (!diff) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "diff is NULL");

        return NULL;
    }

    // Compute ||(-base) ⊕ point||
    float diff_norm = poincare_norm(diff);

    // Allocate result tangent vector
    float *tangent = (float*)nimcp_malloc(base->dim * sizeof(float));
    if (!tangent) {
        poincare_point_destroy(diff);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "poincare_log_map: tangent is NULL");
        return NULL;
    }

    // If diff is too small, return zero vector
    if (diff_norm < POINCARE_EPSILON) {
        memset(tangent, 0, base->dim * sizeof(float));
        poincare_point_destroy(diff);
        return tangent;
    }

    // Compute conformal factor: λ_p = 2/(1-||p||²)
    float lambda_p = poincare_conformal_factor(base);

    // Compute artanh(||diff||)
    float artanh_norm = safe_atanh(diff_norm);

    // Compute scale: (2/λ_p) * artanh(||diff||) / ||diff||
    float scale = (2.0F / lambda_p) * (artanh_norm / diff_norm);

    // Compute result: scale * diff
    vector_scale(tangent, scale, diff->coords, base->dim);

    poincare_point_destroy(diff);
    return tangent;
}

//=============================================================================
// Möbius Addition and Gyrovector Operations
//=============================================================================

poincare_point_t* poincare_mobius_add(const poincare_point_t *x, const poincare_point_t *y) {
    if (!x || !y || !x->coords || !y->coords || x->dim != y->dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "poincare_mobius_add: required parameter is NULL (x, y, x->coords, y->coords)");
        return NULL;
    }

    // Formula: x ⊕ y = ((1+2⟨x,y⟩+||y||²)x + (1-||x||²)y) / (1+2⟨x,y⟩+||x||²||y||²)

    // Compute dot product: ⟨x,y⟩
    float dot_xy = dot_product(x->coords, y->coords, x->dim);

    // Compute ||x||² and ||y||²
    float x_norm_sq = squared_norm(x->coords, x->dim);
    float y_norm_sq = squared_norm(y->coords, y->dim);

    // Compute numerator coefficients
    float coeff_x = 1.0F + 2.0F * dot_xy + y_norm_sq;
    float coeff_y = 1.0F - x_norm_sq;

    // Compute denominator: 1 + 2⟨x,y⟩ + ||x||²||y||²
    float denominator = 1.0F + 2.0F * dot_xy + x_norm_sq * y_norm_sq;
    denominator = fmaxf(denominator, POINCARE_EPSILON);  // Prevent division by zero

    // Allocate result coordinates
    float *result_coords = (float*)nimcp_malloc(x->dim * sizeof(float));
    if (!result_coords) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result_coords is NULL");

        return NULL;
    }

    // Compute numerator: coeff_x * x + coeff_y * y
    for (uint32_t i = 0; i < x->dim; i++) {
        result_coords[i] = (coeff_x * x->coords[i] + coeff_y * y->coords[i]) / denominator;
    }

    // Create result point
    poincare_point_t *result = poincare_point_create(x->dim, result_coords, x->curvature);
    nimcp_free(result_coords);

    return result;
}

poincare_point_t* poincare_mobius_scalar_mult(float r, const poincare_point_t *x) {
    if (!x || !x->coords) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "poincare_mobius_scalar_mult: required parameter is NULL (x, x->coords)");
        return NULL;
    }

    // Compute ||x||
    float x_norm = poincare_norm(x);

    // If x is at origin, return origin
    if (x_norm < POINCARE_EPSILON) {
        return poincare_point_copy(x);
    }

    // Compute artanh(||x||)
    float artanh_norm = safe_atanh(x_norm);

    // Compute tanh(r * artanh(||x||))
    float tanh_val = tanhf(r * artanh_norm);

    // Compute scale: tanh(...) / ||x||
    float scale = tanh_val / x_norm;

    // Allocate result coordinates
    float *result_coords = (float*)nimcp_malloc(x->dim * sizeof(float));
    if (!result_coords) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result_coords is NULL");

        return NULL;
    }

    // Compute result: scale * x
    vector_scale(result_coords, scale, x->coords, x->dim);

    // Create result point
    poincare_point_t *result = poincare_point_create(x->dim, result_coords, x->curvature);
    nimcp_free(result_coords);

    return result;
}

//=============================================================================
// Riemannian Optimization
//=============================================================================

float* poincare_riemannian_gradient(const poincare_point_t *point, const float *euclidean_grad) {
    if (!point || !point->coords || !euclidean_grad) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "poincare_riemannian_gradient: required parameter is NULL (point, point->coords, euclidean_grad)");
        return NULL;
    }

    // Compute conformal factor: (1-||x||²)²/4
    float x_norm_sq = squared_norm(point->coords, point->dim);
    float one_minus_x_sq = 1.0F - x_norm_sq;
    float conformal = (one_minus_x_sq * one_minus_x_sq) / 4.0F;

    // Allocate result
    float *riem_grad = (float*)nimcp_malloc(point->dim * sizeof(float));
    if (!riem_grad) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "riem_grad is NULL");

        return NULL;
    }

    // Scale Euclidean gradient: grad_R = conformal * grad_E
    vector_scale(riem_grad, conformal, euclidean_grad, point->dim);

    return riem_grad;
}

bool poincare_sgd_step(poincare_point_t *point, const float *euclidean_grad, float learning_rate) {
    if (!point || !euclidean_grad) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "poincare_sgd_step: required parameter is NULL (point, euclidean_grad)");
        return false;
    }

    // Compute Riemannian gradient
    float *riem_grad = poincare_riemannian_gradient(point, euclidean_grad);
    if (!riem_grad) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "poincare_sgd_step: riem_grad is NULL");
        return false;
    }

    // Compute negative gradient scaled by learning rate: -η * grad_R
    float *step = (float*)nimcp_malloc(point->dim * sizeof(float));
    if (!step) {
        nimcp_free(riem_grad);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "poincare_sgd_step: step is NULL");
        return false;
    }
    vector_scale(step, -learning_rate, riem_grad, point->dim);
    nimcp_free(riem_grad);

    // Apply exponential map
    poincare_point_t *new_point = poincare_exp_map(point, step);
    nimcp_free(step);
    if (!new_point) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "poincare_sgd_step: new_point is NULL");
        return false;
    }

    // Update point in-place
    memcpy(point->coords, new_point->coords, point->dim * sizeof(float));
    poincare_point_destroy(new_point);

    // Clip to ensure point stays in ball
    poincare_clip(point);

    return true;
}

//=============================================================================
// Utility Functions
//=============================================================================

float poincare_conformal_factor(const poincare_point_t *point) {
    if (!point || !point->coords) {
        return 2.0F;  // Factor at origin
    }

    float norm_sq = squared_norm(point->coords, point->dim);
    float one_minus_norm_sq = 1.0F - norm_sq;
    one_minus_norm_sq = fmaxf(one_minus_norm_sq, POINCARE_EPSILON);  // Prevent division by zero

    return 2.0F / one_minus_norm_sq;
}

hyperbolic_config_t poincare_default_config(void) {
    hyperbolic_config_t config = {
        .dim = 5,                      // 5D sufficient for most hierarchies
        .curvature = -1.0F,            // Standard hyperbolic space
        .learning_rate = 0.01F,        // Conservative default
        .clip_norm = 1.0F,             // Gradient clipping
        .use_exponential_map = true    // Use accurate exponential map
    };
    return config;
}

//=============================================================================
// Debugging and Visualization
//=============================================================================

void poincare_point_print(const poincare_point_t *point, const char *label) {
    if (!point || !point->coords) {
        printf("%s: NULL\n", label ? label : "Point");
        return;
    }

    printf("%s: [", label ? label : "Point");
    for (uint32_t i = 0; i < point->dim; i++) {
        printf("%.4f", point->coords[i]);
        if (i < point->dim - 1) {
            printf(", ");
        }
    }
    float norm = poincare_norm(point);
    printf("] (||x|| = %.4f)\n", norm);
}

bool poincare_point_is_valid(const poincare_point_t *point) {
    if (!point || !point->coords) {
        return false;
    }

    // Check dimension
    if (point->dim == 0) {
        return false;
    }

    // Check all coordinates are finite
    for (uint32_t i = 0; i < point->dim; i++) {
        if (!isfinite(point->coords[i])) {
            return false;
        }
    }

    // Check norm is within ball
    float norm = poincare_norm(point);
    if (norm >= 1.0F) {
        return false;
    }

    return true;
}
