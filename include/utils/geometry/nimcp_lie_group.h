/**
 * @file nimcp_lie_group.h
 * @brief Lie Group Operations (SO(3), Matrix Exp/Log)
 *
 * WHAT: Matrix Lie group operations for rotation, orientation, and symmetry
 * WHY:  Brain processing involves 3D spatial reasoning, rotation-equivariant
 *       features, and smooth interpolation of orientations
 *
 * GROUPS:
 * - SO(3): 3D rotation group (3x3 orthogonal, det=1)
 * - so(3): Lie algebra (3x3 skew-symmetric matrices)
 * - Matrix exp/log: General matrix exponential/logarithm
 *
 * APPLICATIONS:
 * - Spatial reasoning in parietal cortex
 * - Head direction cells (orientation tracking)
 * - Rotation-equivariant neural features
 * - Smooth interpolation of rotations (SLERP via quaternions)
 *
 * @version 1.0.0
 * @date 2026-03-07
 */

#ifndef NIMCP_LIE_GROUP_H
#define NIMCP_LIE_GROUP_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

#define LIE_EPSILON     1e-7f
#define LIE_MAX_DIM     16

/*=============================================================================
 * SO(3) Rotation Group
 *===========================================================================*/

/**
 * @brief 3x3 rotation matrix (SO(3) element)
 *
 * Stored in row-major order. Constraint: R^T R = I, det(R) = 1.
 */
typedef struct {
    float m[9];     /**< 3x3 matrix, row-major */
} so3_rotation_t;

/**
 * @brief 3D vector in so(3) Lie algebra
 *
 * Represents skew-symmetric matrix:
 * [  0  -v3  v2 ]
 * [ v3   0  -v1 ]
 * [-v2  v1   0  ]
 */
typedef struct {
    float v[3];     /**< Axis-angle vector (axis * angle) */
} so3_algebra_t;

/**
 * @brief Create identity rotation
 */
NIMCP_EXPORT so3_rotation_t so3_identity(void);

/**
 * @brief Create rotation from axis-angle
 * @param axis Unit axis [3]
 * @param angle Rotation angle (radians)
 */
NIMCP_EXPORT so3_rotation_t so3_from_axis_angle(const float* axis, float angle);

/**
 * @brief Matrix exponential: so(3) -> SO(3) (Rodrigues' formula)
 *
 * exp(omega_hat) = I + sin(theta)/theta * omega_hat + (1-cos(theta))/theta^2 * omega_hat^2
 * where omega_hat is the skew-symmetric matrix of omega, theta = ||omega||
 */
NIMCP_EXPORT so3_rotation_t so3_exp(const so3_algebra_t* omega);

/**
 * @brief Matrix logarithm: SO(3) -> so(3)
 *
 * Returns axis-angle representation of rotation.
 * Uses: theta = acos((tr(R)-1)/2), omega = theta/(2*sin(theta)) * (R - R^T)
 */
NIMCP_EXPORT so3_algebra_t so3_log(const so3_rotation_t* R);

/**
 * @brief Multiply two rotations: R_result = R1 * R2
 */
NIMCP_EXPORT so3_rotation_t so3_multiply(const so3_rotation_t* R1, const so3_rotation_t* R2);

/**
 * @brief Transpose (inverse for rotation): R^T = R^{-1}
 */
NIMCP_EXPORT so3_rotation_t so3_transpose(const so3_rotation_t* R);

/**
 * @brief Apply rotation to 3D vector: y = R * x
 */
NIMCP_EXPORT void so3_rotate_vector(const so3_rotation_t* R, const float* x, float* y);

/**
 * @brief Geodesic distance between rotations: d(R1,R2) = ||log(R1^T R2)||
 */
NIMCP_EXPORT float so3_distance(const so3_rotation_t* R1, const so3_rotation_t* R2);

/**
 * @brief Geodesic interpolation (SLERP) between rotations
 * @param R1 Start rotation
 * @param R2 End rotation
 * @param t Interpolation parameter [0,1]
 */
NIMCP_EXPORT so3_rotation_t so3_slerp(
    const so3_rotation_t* R1, const so3_rotation_t* R2, float t);

/**
 * @brief Project matrix onto SO(3) (nearest rotation matrix)
 *
 * Uses polar decomposition: M = R * S where R is rotation, S is symmetric.
 * Computed via SVD: M = U * Sigma * V^T, R = U * diag(1,1,det(U*V^T)) * V^T
 */
NIMCP_EXPORT so3_rotation_t so3_project(const float* matrix_3x3);

/*=============================================================================
 * General Matrix Exponential (small matrices)
 *===========================================================================*/

/**
 * @brief Matrix exponential via Pade approximation
 *
 * exp(A) for small square matrices (up to LIE_MAX_DIM x LIE_MAX_DIM).
 * Uses scaling-and-squaring with [6/6] Pade approximant.
 *
 * @param A Input matrix [n*n], row-major
 * @param n Matrix dimension
 * @param result Output: exp(A) [n*n]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int matrix_exp(const float* A, uint32_t n, float* result);

/**
 * @brief Matrix logarithm via inverse scaling-and-squaring
 *
 * log(A) for matrices near identity.
 *
 * @param A Input matrix [n*n], row-major (must be invertible)
 * @param n Matrix dimension
 * @param result Output: log(A) [n*n]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int matrix_log(const float* A, uint32_t n, float* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LIE_GROUP_H */
