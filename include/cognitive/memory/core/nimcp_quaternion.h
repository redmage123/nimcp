//=============================================================================
// nimcp_quaternion.h - Quaternion Mathematics for Memory State Encoding
//=============================================================================
/**
 * @file nimcp_quaternion.h
 * @brief Quaternion operations for Prime Resonant memory state representation
 *
 * WHAT: Complete quaternion algebra for memory semantic state encoding
 * WHY:  Quaternions enable smooth interpolation and geometric operations on
 *       4D memory states (consolidation, emotion, salience, accessibility)
 * HOW:  Standard Hamilton quaternion algebra with memory-specific extensions
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Memory State as 4D Quaternion:
 *   +-----------------------------------------------------------------------+
 *   |  Each memory carries a quaternion state q = (w, x, y, z) where:      |
 *   |                                                                       |
 *   |  w = Consolidation strength [0, 1]                                    |
 *   |      How firmly encoded (0=fragile, 1=permanent)                     |
 *   |      Biological: synaptic tag strength, protein synthesis            |
 *   |                                                                       |
 *   |  x = Emotional valence [-1, +1]                                       |
 *   |      Affective tone (-1=negative, 0=neutral, +1=positive)            |
 *   |      Biological: amygdala activation pattern                         |
 *   |                                                                       |
 *   |  y = Salience/attention weight [0, 1]                                 |
 *   |      How attention-grabbing (0=background, 1=focal)                  |
 *   |      Biological: dopaminergic novelty signal                         |
 *   |                                                                       |
 *   |  z = Accessibility/retrieval ease [0, 1]                              |
 *   |      How easily retrieved (0=buried, 1=tip-of-tongue)                |
 *   |      Biological: hippocampal pattern completion strength             |
 *   +-----------------------------------------------------------------------+
 *
 *   Quaternion Operations for Memory:
 *   +-----------------------------------------------------------------------+
 *   |  Hamilton Product: q1 * q2 = Memory combination                       |
 *   |  - Non-commutative: order matters (context-dependent fusion)         |
 *   |  - Encodes how one memory modifies another                           |
 *   |                                                                       |
 *   |  SLERP: Spherical Linear Interpolation                                |
 *   |  - Smooth transition between memory states                           |
 *   |  - Used during consolidation and reconsolidation                     |
 *   |  - Constant angular velocity on unit hypersphere                     |
 *   |                                                                       |
 *   |  Geodesic Distance: Arc length on unit sphere                         |
 *   |  - Semantic similarity measure                                        |
 *   |  - d(q1, q2) = arccos(|q1 . q2|)                                      |
 *   |                                                                       |
 *   |  Quaternion Exponential/Logarithm:                                    |
 *   |  - Power operations: q^t for fractional consolidation               |
 *   |  - Blending: weighted average of multiple memories                   |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Basic operations: ~5-10ns (4x float arithmetic)
 * - SLERP: ~50ns (trig functions)
 * - Geodesic distance: ~20ns (dot product + arccos)
 * - Memory blend (N=10): ~200ns
 *
 * MEMORY:
 * - nimcp_quaternion_t: 16 bytes (4x float)
 * - No dynamic allocation in core operations
 *
 * INTEGRATION:
 * - Core: Prime Resonant memory nodes, Z-Ladder tiers
 * - Middleware: Resonance scoring, entanglement computation
 * - API: Memory state queries, emotional filtering
 *
 * @author NIMCP Development Team
 * @date 2026-01-08
 * @version 1.0.0
 */

#ifndef NIMCP_QUATERNION_H
#define NIMCP_QUATERNION_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "constants/nimcp_math_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Numerical epsilon for quaternion operations */
#define QUAT_EPSILON 1e-6f

/** Threshold for SLERP vs NLERP switch (cos(theta) close to 1) */
#define QUAT_SLERP_THRESHOLD 0.9995f

/** Pi constant */

/** Half Pi */
#ifndef M_PI_2
    #define M_PI_2 1.57079632679489661923f
#endif

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Quaternion representation for memory state
 *
 * Uses Hamilton convention: q = w + xi + yj + zk
 *
 * Memory semantic mapping:
 * - w: Consolidation strength [0, 1]
 * - x: Emotional valence [-1, +1]
 * - y: Salience/attention weight [0, 1]
 * - z: Accessibility/retrieval ease [0, 1]
 *
 * For unit quaternions: w^2 + x^2 + y^2 + z^2 = 1
 */
typedef struct {
    float w;  /**< Real part / Consolidation strength */
    float x;  /**< i component / Emotional valence */
    float y;  /**< j component / Salience weight */
    float z;  /**< k component / Accessibility */
} nimcp_quaternion_t;

/**
 * @brief 3D vector for axis-angle and rotation operations
 */
typedef struct {
    float x;
    float y;
    float z;
} nimcp_vec3_t;

/**
 * @brief Euler angles (in radians)
 */
typedef struct {
    float roll;   /**< Rotation around X axis */
    float pitch;  /**< Rotation around Y axis */
    float yaw;    /**< Rotation around Z axis */
} nimcp_euler_t;

/**
 * @brief Configuration for quaternion operations
 */
typedef struct {
    float normalize_threshold;  /**< Re-normalize if |q| deviates by this much */
    float slerp_threshold;      /**< Use NLERP instead of SLERP above this */
    bool auto_normalize;        /**< Auto-normalize after operations */
} nimcp_quat_config_t;

//=============================================================================
// Core Quaternion Creation
//=============================================================================

/**
 * @brief Create quaternion from components
 *
 * @param w Real part (consolidation strength for memory)
 * @param x i component (emotional valence for memory)
 * @param y j component (salience for memory)
 * @param z k component (accessibility for memory)
 * @return Quaternion with specified components
 *
 * Performance: ~3ns
 *
 * Example:
 *   // Create memory state: medium consolidation, negative emotion, high salience
 *   nimcp_quaternion_t q = quat_create(0.5f, -0.3f, 0.9f, 0.7f);
 */
NIMCP_EXPORT nimcp_quaternion_t quat_create(float w, float x, float y, float z);

/**
 * @brief Create quaternion from axis-angle rotation
 *
 * Creates a unit quaternion representing rotation of 'angle' radians
 * around the axis (axis.x, axis.y, axis.z).
 *
 * @param axis Rotation axis (will be normalized internally)
 * @param angle Rotation angle in radians
 * @return Unit quaternion representing the rotation
 *
 * Formula: q = (cos(angle/2), axis * sin(angle/2))
 *
 * Performance: ~25ns (trig + normalize)
 */
NIMCP_EXPORT nimcp_quaternion_t quat_from_axis_angle(nimcp_vec3_t axis, float angle);

/**
 * @brief Create quaternion from Euler angles (XYZ order)
 *
 * Converts Euler angles to quaternion using the rotation sequence
 * X (roll) -> Y (pitch) -> Z (yaw).
 *
 * @param roll Rotation around X axis in radians
 * @param pitch Rotation around Y axis in radians
 * @param yaw Rotation around Z axis in radians
 * @return Unit quaternion representing the rotation
 *
 * Performance: ~40ns (6 trig calls)
 */
NIMCP_EXPORT nimcp_quaternion_t quat_from_euler(float roll, float pitch, float yaw);

/**
 * @brief Create identity quaternion (no rotation)
 *
 * @return Identity quaternion (1, 0, 0, 0)
 *
 * Performance: ~2ns
 */
NIMCP_EXPORT nimcp_quaternion_t quat_identity(void);

//=============================================================================
// Core Quaternion Operations
//=============================================================================

/**
 * @brief Normalize quaternion to unit length
 *
 * Ensures |q| = 1, required for rotation operations.
 * Returns identity quaternion if input is zero or near-zero.
 *
 * @param q Input quaternion
 * @return Normalized unit quaternion
 *
 * Performance: ~15ns (sqrt)
 */
NIMCP_EXPORT nimcp_quaternion_t quat_normalize(nimcp_quaternion_t q);

/**
 * @brief Compute quaternion conjugate
 *
 * Conjugate: q* = (w, -x, -y, -z)
 * For unit quaternions, conjugate equals inverse.
 *
 * @param q Input quaternion
 * @return Conjugate quaternion
 *
 * Performance: ~3ns
 */
NIMCP_EXPORT nimcp_quaternion_t quat_conjugate(nimcp_quaternion_t q);

/**
 * @brief Compute quaternion inverse
 *
 * Inverse: q^(-1) = q* / |q|^2
 * For unit quaternions: q^(-1) = q*
 *
 * @param q Input quaternion (must not be zero)
 * @return Inverse quaternion
 *
 * Performance: ~20ns (includes magnitude squared)
 */
NIMCP_EXPORT nimcp_quaternion_t quat_inverse(nimcp_quaternion_t q);

/**
 * @brief Hamilton product (quaternion multiplication)
 *
 * Non-commutative multiplication: q1 * q2 != q2 * q1
 *
 * Formula (a1,b1,c1,d1) x (a2,b2,c2,d2) =
 *   (a1*a2 - b1*b2 - c1*c2 - d1*d2,
 *    a1*b2 + b1*a2 + c1*d2 - d1*c2,
 *    a1*c2 - b1*d2 + c1*a2 + d1*b2,
 *    a1*d2 + b1*c2 - c1*b2 + d1*a2)
 *
 * @param q1 Left quaternion
 * @param q2 Right quaternion
 * @return Hamilton product q1 * q2
 *
 * Performance: ~10ns
 *
 * Memory application: Combine memory states where order represents
 * encoding vs retrieval context influence.
 */
NIMCP_EXPORT nimcp_quaternion_t quat_hamilton_product(nimcp_quaternion_t q1,
                                                       nimcp_quaternion_t q2);

/**
 * @brief Quaternion dot product
 *
 * Inner product: q1 . q2 = w1*w2 + x1*x2 + y1*y2 + z1*z2
 *
 * @param q1 First quaternion
 * @param q2 Second quaternion
 * @return Scalar dot product
 *
 * Performance: ~5ns
 *
 * Memory application: Raw similarity measure (before geodesic conversion)
 */
NIMCP_EXPORT float quat_dot(nimcp_quaternion_t q1, nimcp_quaternion_t q2);

/**
 * @brief Compute quaternion magnitude (norm)
 *
 * @param q Input quaternion
 * @return |q| = sqrt(w^2 + x^2 + y^2 + z^2)
 *
 * Performance: ~12ns (sqrt)
 */
NIMCP_EXPORT float quat_magnitude(nimcp_quaternion_t q);

/**
 * @brief Compute squared magnitude (faster than magnitude)
 *
 * @param q Input quaternion
 * @return |q|^2 = w^2 + x^2 + y^2 + z^2
 *
 * Performance: ~5ns
 */
NIMCP_EXPORT float quat_magnitude_squared(nimcp_quaternion_t q);

//=============================================================================
// Interpolation Operations
//=============================================================================

/**
 * @brief Spherical Linear Interpolation (SLERP)
 *
 * Interpolates along the shortest arc on the unit hypersphere.
 * Maintains constant angular velocity (unlike LERP/NLERP).
 *
 * Formula: slerp(q1, q2, t) = q1*sin((1-t)*theta)/sin(theta) +
 *                             q2*sin(t*theta)/sin(theta)
 *          where theta = arccos(q1 . q2)
 *
 * @param q1 Start quaternion (t=0)
 * @param q2 End quaternion (t=1)
 * @param t Interpolation parameter [0, 1]
 * @return Interpolated unit quaternion
 *
 * Performance: ~50ns
 *
 * Notes:
 * - Falls back to NLERP when q1 and q2 are very close
 * - Handles antipodal quaternions (q and -q represent same rotation)
 *
 * Memory application: Smooth consolidation transitions, memory blending
 */
NIMCP_EXPORT nimcp_quaternion_t quat_slerp(nimcp_quaternion_t q1,
                                            nimcp_quaternion_t q2,
                                            float t);

/**
 * @brief Normalized Linear Interpolation (NLERP)
 *
 * Faster than SLERP but doesn't maintain constant angular velocity.
 * Acceptable when interpolating small angles or when speed matters.
 *
 * Formula: nlerp(q1, q2, t) = normalize(lerp(q1, q2, t))
 *
 * @param q1 Start quaternion (t=0)
 * @param q2 End quaternion (t=1)
 * @param t Interpolation parameter [0, 1]
 * @return Interpolated unit quaternion
 *
 * Performance: ~20ns
 */
NIMCP_EXPORT nimcp_quaternion_t quat_nlerp(nimcp_quaternion_t q1,
                                            nimcp_quaternion_t q2,
                                            float t);

/**
 * @brief Compute geodesic distance between quaternions
 *
 * Angular distance on the unit hypersphere (great arc length).
 *
 * Formula: d(q1, q2) = arccos(|q1 . q2|)
 *
 * @param q1 First quaternion (will be normalized)
 * @param q2 Second quaternion (will be normalized)
 * @return Angular distance in radians [0, pi]
 *
 * Performance: ~25ns
 *
 * Memory application: Semantic similarity metric for resonance scoring
 */
NIMCP_EXPORT float quat_geodesic_distance(nimcp_quaternion_t q1,
                                           nimcp_quaternion_t q2);

//=============================================================================
// Exponential and Logarithm Operations
//=============================================================================

/**
 * @brief Quaternion exponential
 *
 * For q = (0, v) where v is a 3D vector:
 * exp(q) = (cos|v|, v/|v| * sin|v|)
 *
 * For general q = (w, v):
 * exp(q) = exp(w) * (cos|v|, v/|v| * sin|v|)
 *
 * @param q Input quaternion
 * @return Exponential exp(q)
 *
 * Performance: ~35ns
 *
 * Memory application: Transform log-space representations, power operations
 */
NIMCP_EXPORT nimcp_quaternion_t quat_exp(nimcp_quaternion_t q);

/**
 * @brief Quaternion logarithm
 *
 * For unit quaternion q = (cos(theta), v*sin(theta)):
 * log(q) = (0, v*theta) where theta = arccos(w)
 *
 * For general quaternion:
 * log(q) = (log|q|, v/|v| * arccos(w/|q|))
 *
 * @param q Input quaternion (must not be zero)
 * @return Logarithm log(q)
 *
 * Performance: ~40ns
 *
 * Memory application: Convert to linear space for weighted averaging
 */
NIMCP_EXPORT nimcp_quaternion_t quat_log(nimcp_quaternion_t q);

/**
 * @brief Quaternion power (fractional exponent)
 *
 * Computes q^t using exp/log: q^t = exp(t * log(q))
 *
 * @param q Input unit quaternion
 * @param t Exponent (can be fractional)
 * @return q raised to power t
 *
 * Performance: ~80ns (exp + log)
 *
 * Memory application: Partial consolidation, decay modeling
 *
 * Example:
 *   // Half-consolidation: sqrt of quaternion state
 *   nimcp_quaternion_t half = quat_power(q, 0.5f);
 */
NIMCP_EXPORT nimcp_quaternion_t quat_power(nimcp_quaternion_t q, float t);

//=============================================================================
// Rotation Operations
//=============================================================================

/**
 * @brief Convert quaternion to 3x3 rotation matrix
 *
 * @param q Input unit quaternion
 * @param out Output 3x3 matrix in row-major order (9 floats)
 *
 * Performance: ~20ns
 */
NIMCP_EXPORT void quat_to_rotation_matrix(nimcp_quaternion_t q, float out[9]);

/**
 * @brief Convert quaternion to Euler angles
 *
 * Returns Euler angles in XYZ order (roll, pitch, yaw).
 * Note: Euler angles have gimbal lock issues; prefer quaternions.
 *
 * @param q Input unit quaternion
 * @param roll Output rotation around X axis (radians)
 * @param pitch Output rotation around Y axis (radians)
 * @param yaw Output rotation around Z axis (radians)
 *
 * Performance: ~30ns
 */
NIMCP_EXPORT void quat_to_euler(nimcp_quaternion_t q,
                                 float* roll, float* pitch, float* yaw);

/**
 * @brief Rotate a 3D vector by quaternion
 *
 * Rotation: v' = q * v * q^(-1)
 * where v is treated as pure quaternion (0, vx, vy, vz).
 *
 * @param q Rotation quaternion (should be unit quaternion)
 * @param v Input vector
 * @return Rotated vector
 *
 * Performance: ~25ns
 */
NIMCP_EXPORT nimcp_vec3_t quat_rotate_vector(nimcp_quaternion_t q, nimcp_vec3_t v);

//=============================================================================
// Memory-Specific Operations
//=============================================================================

/**
 * @brief Blend multiple memory quaternions with weights
 *
 * Computes weighted average of quaternions in log space:
 * result = exp(sum(weights[i] * log(quats[i])) / sum(weights))
 *
 * @param quats Array of quaternions to blend
 * @param weights Array of weights (need not sum to 1)
 * @param count Number of quaternions
 * @return Blended unit quaternion
 *
 * Performance: ~100ns for count=5
 *
 * Memory application: Combine retrieved memories, schema extraction
 *
 * Example:
 *   // Blend 3 memories with different weights
 *   nimcp_quaternion_t quats[3] = {q1, q2, q3};
 *   float weights[3] = {0.5f, 0.3f, 0.2f};
 *   nimcp_quaternion_t blended = quat_blend_memories(quats, weights, 3);
 */
NIMCP_EXPORT nimcp_quaternion_t quat_blend_memories(const nimcp_quaternion_t* quats,
                                                     const float* weights,
                                                     size_t count);

/**
 * @brief Generate smooth interpolation path between two quaternions
 *
 * Creates 'steps' interpolated quaternions from q_start to q_end.
 *
 * @param q_start Starting quaternion
 * @param q_end Ending quaternion
 * @param steps Number of interpolation steps (minimum 2)
 * @param out Output array of size 'steps' (caller-allocated)
 * @return true on success, false on error
 *
 * Performance: ~50ns * steps
 *
 * Memory application: Smooth consolidation trajectory, reconsolidation path
 */
NIMCP_EXPORT bool quat_interpolate_path(nimcp_quaternion_t q_start,
                                         nimcp_quaternion_t q_end,
                                         size_t steps,
                                         nimcp_quaternion_t* out);

/**
 * @brief Semantic distance with component importance weighting
 *
 * Computes weighted distance accounting for different importance
 * of consolidation, emotion, salience, and accessibility.
 *
 * @param q1 First memory quaternion
 * @param q2 Second memory quaternion
 * @param w_weight Weight for consolidation component
 * @param x_weight Weight for emotion component
 * @param y_weight Weight for salience component
 * @param z_weight Weight for accessibility component
 * @return Weighted distance metric [0, inf)
 *
 * Performance: ~15ns
 *
 * Memory application: Custom similarity for different retrieval contexts
 * (e.g., emotional memories weight x_component more heavily)
 */
NIMCP_EXPORT float quat_semantic_distance(nimcp_quaternion_t q1,
                                           nimcp_quaternion_t q2,
                                           float w_weight,
                                           float x_weight,
                                           float y_weight,
                                           float z_weight);

/**
 * @brief Semantic distance with default equal weights
 *
 * Convenience function using equal weights for all components.
 *
 * @param q1 First memory quaternion
 * @param q2 Second memory quaternion
 * @return Distance metric [0, inf)
 *
 * Performance: ~15ns
 */
NIMCP_EXPORT float quat_semantic_distance_default(nimcp_quaternion_t q1,
                                                   nimcp_quaternion_t q2);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Check if quaternion is unit (normalized)
 *
 * @param q Input quaternion
 * @param epsilon Tolerance for deviation from 1.0
 * @return true if |q| is within epsilon of 1.0
 *
 * Performance: ~8ns
 */
NIMCP_EXPORT bool quat_is_unit(nimcp_quaternion_t q, float epsilon);

/**
 * @brief Check if quaternion is zero or near-zero
 *
 * @param q Input quaternion
 * @param epsilon Tolerance for zero check
 * @return true if |q| < epsilon
 *
 * Performance: ~8ns
 */
NIMCP_EXPORT bool quat_is_zero(nimcp_quaternion_t q, float epsilon);

/**
 * @brief Check if two quaternions are approximately equal
 *
 * Compares component-wise with tolerance.
 *
 * @param q1 First quaternion
 * @param q2 Second quaternion
 * @param epsilon Tolerance per component
 * @return true if all components match within epsilon
 *
 * Performance: ~5ns
 */
NIMCP_EXPORT bool quat_approx_equal(nimcp_quaternion_t q1,
                                     nimcp_quaternion_t q2,
                                     float epsilon);

/**
 * @brief Negate quaternion (same rotation, opposite representation)
 *
 * @param q Input quaternion
 * @return Negated quaternion (-w, -x, -y, -z)
 *
 * Performance: ~3ns
 */
NIMCP_EXPORT nimcp_quaternion_t quat_negate(nimcp_quaternion_t q);

/**
 * @brief Add two quaternions (component-wise)
 *
 * @param q1 First quaternion
 * @param q2 Second quaternion
 * @return Component-wise sum
 *
 * Performance: ~3ns
 */
NIMCP_EXPORT nimcp_quaternion_t quat_add(nimcp_quaternion_t q1,
                                          nimcp_quaternion_t q2);

/**
 * @brief Subtract two quaternions (component-wise)
 *
 * @param q1 First quaternion
 * @param q2 Second quaternion
 * @return Component-wise difference (q1 - q2)
 *
 * Performance: ~3ns
 */
NIMCP_EXPORT nimcp_quaternion_t quat_sub(nimcp_quaternion_t q1,
                                          nimcp_quaternion_t q2);

/**
 * @brief Scale quaternion by scalar
 *
 * @param q Input quaternion
 * @param s Scalar multiplier
 * @return Scaled quaternion (s*w, s*x, s*y, s*z)
 *
 * Performance: ~3ns
 */
NIMCP_EXPORT nimcp_quaternion_t quat_scale(nimcp_quaternion_t q, float s);

/**
 * @brief Get default quaternion configuration
 *
 * @return Default configuration values
 */
NIMCP_EXPORT nimcp_quat_config_t quat_default_config(void);

/**
 * @brief Create a 3D vector (quaternion module version)
 *
 * @param x X component
 * @param y Y component
 * @param z Z component
 * @return Vector with specified components
 *
 * Performance: ~2ns
 * @note Use this version when working with quaternion rotations
 */
NIMCP_EXPORT nimcp_vec3_t quat_vec3_create(float x, float y, float z);

/**
 * @brief Normalize a 3D vector (quaternion module version)
 *
 * @param v Input vector
 * @return Unit vector in same direction (or zero vector if input is zero)
 *
 * Performance: ~15ns
 * @note Use this version when working with quaternion rotations
 */
NIMCP_EXPORT nimcp_vec3_t quat_vec3_normalize(nimcp_vec3_t v);

/**
 * @brief Compute 3D vector magnitude (quaternion module version)
 *
 * @param v Input vector
 * @return |v| = sqrt(x^2 + y^2 + z^2)
 *
 * Performance: ~12ns
 */
NIMCP_EXPORT float quat_vec3_magnitude(nimcp_vec3_t v);

/**
 * @brief Compute 3D dot product (quaternion module version)
 *
 * @param v1 First vector
 * @param v2 Second vector
 * @return v1 . v2
 *
 * Performance: ~5ns
 */
NIMCP_EXPORT float quat_vec3_dot(nimcp_vec3_t v1, nimcp_vec3_t v2);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_QUATERNION_H
