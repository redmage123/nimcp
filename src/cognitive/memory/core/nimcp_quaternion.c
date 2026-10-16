//=============================================================================
// nimcp_quaternion.c - Quaternion Mathematics Implementation
//=============================================================================
/**
 * @file nimcp_quaternion.c
 * @brief Implementation of quaternion operations for Prime Resonant memory
 *
 * WHAT: Complete quaternion algebra with memory-specific extensions
 * WHY:  Enable smooth interpolation and geometric operations on 4D memory states
 * HOW:  Standard Hamilton algebra with numerical stability guards
 *
 * IMPLEMENTATION NOTES:
 * - All operations include epsilon guards for numerical stability
 * - SLERP falls back to NLERP for nearly-parallel quaternions
 * - Antipodal quaternion handling ensures shortest path interpolation
 * - Log/exp operations handle edge cases (zero quaternion, identity)
 *
 * @author NIMCP Development Team
 * @date 2026-01-08
 * @version 1.0.0
 */

#include "cognitive/memory/core/nimcp_quaternion.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <float.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"
#include <math.h>

BRIDGE_BOILERPLATE(quaternion, MESH_ADAPTER_CATEGORY_MEMORY)


//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Safe arccos with clamping to avoid NaN from numerical errors
 */
static inline float safe_acos(float x) {
    return acosf(nimcp_clampf(x, -1.0f, 1.0f));
}

/**
 * @brief Fast inverse square root approximation (optional, use sqrtf for now)
 */
static inline float fast_inv_sqrt(float x) {
    if (x < QUAT_EPSILON) return 0.0f;
    return 1.0f / sqrtf(x);
}

//=============================================================================
// Core Quaternion Creation
//=============================================================================

nimcp_quaternion_t quat_create(float w, float x, float y, float z) {
    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_create", 0.0f);


    nimcp_quaternion_t q;
    q.w = w;
    q.x = x;
    q.y = y;
    q.z = z;
    return q;
}

nimcp_quaternion_t quat_from_axis_angle(nimcp_vec3_t axis, float angle) {
    // Normalize axis first
    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_from_axis_angle", 0.0f);


    float mag = quat_vec3_magnitude(axis);
    if (mag < QUAT_EPSILON) {
        // Zero axis: return identity
        return quat_identity();
    }

    float inv_mag = 1.0f / mag;
    float ax = axis.x * inv_mag;
    float ay = axis.y * inv_mag;
    float az = axis.z * inv_mag;

    float half_angle = angle * 0.5f;
    float s = sinf(half_angle);
    float c = cosf(half_angle);

    nimcp_quaternion_t q;
    q.w = c;
    q.x = ax * s;
    q.y = ay * s;
    q.z = az * s;

    return q;
}

nimcp_quaternion_t quat_from_euler(float roll, float pitch, float yaw) {
    // Compute half angles
    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_from_euler", 0.0f);


    float hr = roll * 0.5f;
    float hp = pitch * 0.5f;
    float hy = yaw * 0.5f;

    // Compute trig functions
    float cr = cosf(hr);
    float sr = sinf(hr);
    float cp = cosf(hp);
    float sp = sinf(hp);
    float cy = cosf(hy);
    float sy = sinf(hy);

    // Combine using XYZ rotation order
    nimcp_quaternion_t q;
    q.w = cr * cp * cy + sr * sp * sy;
    q.x = sr * cp * cy - cr * sp * sy;
    q.y = cr * sp * cy + sr * cp * sy;
    q.z = cr * cp * sy - sr * sp * cy;

    return q;
}

nimcp_quaternion_t quat_identity(void) {
    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_identity", 0.0f);


    nimcp_quaternion_t q;
    q.w = 1.0f;
    q.x = 0.0f;
    q.y = 0.0f;
    q.z = 0.0f;
    return q;
}

//=============================================================================
// Core Quaternion Operations
//=============================================================================

nimcp_quaternion_t quat_normalize(nimcp_quaternion_t q) {
    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_normalize", 0.0f);


    float mag_sq = q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z;

    if (mag_sq < QUAT_EPSILON * QUAT_EPSILON) {
        // Near-zero quaternion: return identity
        return quat_identity();
    }

    float inv_mag = fast_inv_sqrt(mag_sq);

    nimcp_quaternion_t result;
    result.w = q.w * inv_mag;
    result.x = q.x * inv_mag;
    result.y = q.y * inv_mag;
    result.z = q.z * inv_mag;

    return result;
}

nimcp_quaternion_t quat_conjugate(nimcp_quaternion_t q) {
    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_conjugate", 0.0f);


    nimcp_quaternion_t result;
    result.w = q.w;
    result.x = -q.x;
    result.y = -q.y;
    result.z = -q.z;
    return result;
}

nimcp_quaternion_t quat_inverse(nimcp_quaternion_t q) {
    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_inverse", 0.0f);


    float mag_sq = q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z;

    if (mag_sq < QUAT_EPSILON * QUAT_EPSILON) {
        // Cannot invert zero quaternion: return identity
        return quat_identity();
    }

    float inv_mag_sq = 1.0f / mag_sq;

    nimcp_quaternion_t result;
    result.w = q.w * inv_mag_sq;
    result.x = -q.x * inv_mag_sq;
    result.y = -q.y * inv_mag_sq;
    result.z = -q.z * inv_mag_sq;

    return result;
}

nimcp_quaternion_t quat_hamilton_product(nimcp_quaternion_t q1, nimcp_quaternion_t q2) {
    // Hamilton product formula:
    // (a1, b1, c1, d1) x (a2, b2, c2, d2) =
    //   (a1*a2 - b1*b2 - c1*c2 - d1*d2,
    //    a1*b2 + b1*a2 + c1*d2 - d1*c2,
    //    a1*c2 - b1*d2 + c1*a2 + d1*b2,
    //    a1*d2 + b1*c2 - c1*b2 + d1*a2)

    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_hamilton_produc", 0.0f);


    nimcp_quaternion_t result;

    result.w = q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z;
    result.x = q1.w * q2.x + q1.x * q2.w + q1.y * q2.z - q1.z * q2.y;
    result.y = q1.w * q2.y - q1.x * q2.z + q1.y * q2.w + q1.z * q2.x;
    result.z = q1.w * q2.z + q1.x * q2.y - q1.y * q2.x + q1.z * q2.w;

    return result;
}

float quat_dot(nimcp_quaternion_t q1, nimcp_quaternion_t q2) {
    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_dot", 0.0f);


    return q1.w * q2.w + q1.x * q2.x + q1.y * q2.y + q1.z * q2.z;
}

float quat_magnitude(nimcp_quaternion_t q) {
    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_magnitude", 0.0f);


    return sqrtf(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
}

float quat_magnitude_squared(nimcp_quaternion_t q) {
    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_magnitude_squar", 0.0f);


    return q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z;
}

//=============================================================================
// Interpolation Operations
//=============================================================================

nimcp_quaternion_t quat_slerp(nimcp_quaternion_t q1, nimcp_quaternion_t q2, float t) {
    // Clamp t to [0, 1]
    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_slerp", 0.0f);


    t = nimcp_clampf(t, 0.0f, 1.0f);

    // Compute dot product (cosine of angle)
    float dot = quat_dot(q1, q2);

    // Handle antipodal quaternions: ensure shortest path
    nimcp_quaternion_t q2_adj = q2;
    if (dot < 0.0f) {
        q2_adj = quat_negate(q2);
        dot = -dot;
    }

    // If quaternions are very close, use NLERP to avoid division by zero
    if (dot > QUAT_SLERP_THRESHOLD) {
        return quat_nlerp(q1, q2_adj, t);
    }

    // Standard SLERP
    float theta = safe_acos(dot);
    float sin_theta = sinf(theta);

    // Guard against numerical issues
    if (sin_theta < QUAT_EPSILON) {
        return quat_nlerp(q1, q2_adj, t);
    }

    float inv_sin_theta = 1.0f / sin_theta;
    float s1 = sinf((1.0f - t) * theta) * inv_sin_theta;
    float s2 = sinf(t * theta) * inv_sin_theta;

    nimcp_quaternion_t result;
    result.w = s1 * q1.w + s2 * q2_adj.w;
    result.x = s1 * q1.x + s2 * q2_adj.x;
    result.y = s1 * q1.y + s2 * q2_adj.y;
    result.z = s1 * q1.z + s2 * q2_adj.z;

    return result;
}

nimcp_quaternion_t quat_nlerp(nimcp_quaternion_t q1, nimcp_quaternion_t q2, float t) {
    // Clamp t to [0, 1]
    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_nlerp", 0.0f);


    t = nimcp_clampf(t, 0.0f, 1.0f);

    // Handle antipodal quaternions
    float dot = quat_dot(q1, q2);
    nimcp_quaternion_t q2_adj = q2;
    if (dot < 0.0f) {
        q2_adj = quat_negate(q2);
    }

    // Linear interpolation
    float t1 = 1.0f - t;
    nimcp_quaternion_t result;
    result.w = t1 * q1.w + t * q2_adj.w;
    result.x = t1 * q1.x + t * q2_adj.x;
    result.y = t1 * q1.y + t * q2_adj.y;
    result.z = t1 * q1.z + t * q2_adj.z;

    // Normalize to unit quaternion
    return quat_normalize(result);
}

float quat_geodesic_distance(nimcp_quaternion_t q1, nimcp_quaternion_t q2) {
    // Normalize inputs
    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_geodesic_distan", 0.0f);


    nimcp_quaternion_t q1n = quat_normalize(q1);
    nimcp_quaternion_t q2n = quat_normalize(q2);

    // Geodesic distance = arccos(|q1 . q2|)
    float dot = quat_dot(q1n, q2n);

    // Handle antipodal: q and -q represent same rotation
    float abs_dot = fabsf(dot);

    // Clamp to [-1, 1] for numerical stability
    abs_dot = nimcp_clampf(abs_dot, 0.0f, 1.0f);

    return safe_acos(abs_dot);
}

//=============================================================================
// Exponential and Logarithm Operations
//=============================================================================

nimcp_quaternion_t quat_exp(nimcp_quaternion_t q) {
    // For q = (w, v) where v = (x, y, z):
    // exp(q) = exp(w) * (cos|v|, v/|v| * sin|v|)

    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_exp", 0.0f);


    float v_mag_sq = q.x * q.x + q.y * q.y + q.z * q.z;
    float exp_w = expf(q.w);

    if (v_mag_sq < QUAT_EPSILON * QUAT_EPSILON) {
        // Pure real quaternion: exp(w, 0) = (exp(w), 0)
        nimcp_quaternion_t result;
        result.w = exp_w;
        result.x = 0.0f;
        result.y = 0.0f;
        result.z = 0.0f;
        return result;
    }

    float v_mag = sqrtf(v_mag_sq);
    float sin_v = sinf(v_mag);
    float cos_v = cosf(v_mag);
    float coeff = exp_w * sin_v / v_mag;

    nimcp_quaternion_t result;
    result.w = exp_w * cos_v;
    result.x = q.x * coeff;
    result.y = q.y * coeff;
    result.z = q.z * coeff;

    return result;
}

nimcp_quaternion_t quat_log(nimcp_quaternion_t q) {
    // For general q = (w, v):
    // log(q) = (log|q|, v/|v| * arccos(w/|q|))

    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_log", 0.0f);


    float q_mag = quat_magnitude(q);

    if (q_mag < QUAT_EPSILON) {
        // Cannot take log of zero: return zero quaternion
        return quat_create(0.0f, 0.0f, 0.0f, 0.0f);
    }

    float v_mag_sq = q.x * q.x + q.y * q.y + q.z * q.z;

    if (v_mag_sq < QUAT_EPSILON * QUAT_EPSILON) {
        // Pure real quaternion: log(w, 0) = (log|w|, 0) for positive w
        // For negative w, we'd have log(w, 0) = (log|w|, pi*n) but we return (log|w|, 0)
        nimcp_quaternion_t result;
        result.w = logf(q_mag);
        result.x = 0.0f;
        result.y = 0.0f;
        result.z = 0.0f;
        return result;
    }

    float v_mag = sqrtf(v_mag_sq);
    float theta = safe_acos(nimcp_clampf(q.w / q_mag, -1.0f, 1.0f));
    float coeff = theta / v_mag;

    nimcp_quaternion_t result;
    result.w = logf(q_mag);
    result.x = q.x * coeff;
    result.y = q.y * coeff;
    result.z = q.z * coeff;

    return result;
}

nimcp_quaternion_t quat_power(nimcp_quaternion_t q, float t) {
    // q^t = exp(t * log(q))

    // Handle special cases
    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_power", 0.0f);


    if (fabsf(t) < QUAT_EPSILON) {
        // q^0 = identity
        return quat_identity();
    }

    float q_mag = quat_magnitude(q);
    if (q_mag < QUAT_EPSILON) {
        // 0^t = 0 for t > 0
        return quat_create(0.0f, 0.0f, 0.0f, 0.0f);
    }

    // For unit quaternions, simplified formula:
    // q = (cos(theta), v*sin(theta))
    // q^t = (cos(t*theta), v*sin(t*theta))

    if (fabsf(q_mag - 1.0f) < QUAT_EPSILON) {
        // Unit quaternion: use simplified formula
        float v_mag_sq = q.x * q.x + q.y * q.y + q.z * q.z;

        if (v_mag_sq < QUAT_EPSILON * QUAT_EPSILON) {
            // Near identity: q ≈ (1, 0, 0, 0) or (-1, 0, 0, 0)
            if (q.w > 0) {
                return quat_identity();
            } else {
                // q = -1 (rotation by 2*pi)
                // q^t = rotation by 2*pi*t
                float theta = (float)M_PI * t;
                return quat_create(cosf(theta), sinf(theta), 0.0f, 0.0f);
            }
        }

        float v_mag = sqrtf(v_mag_sq);
        float theta = atan2f(v_mag, q.w);
        float new_theta = theta * t;

        float sin_new = sinf(new_theta);
        float cos_new = cosf(new_theta);
        float coeff = sin_new / v_mag;

        nimcp_quaternion_t result;
        result.w = cos_new;
        result.x = q.x * coeff;
        result.y = q.y * coeff;
        result.z = q.z * coeff;

        return result;
    }

    // General case: exp(t * log(q))
    nimcp_quaternion_t log_q = quat_log(q);
    nimcp_quaternion_t scaled;
    scaled.w = log_q.w * t;
    scaled.x = log_q.x * t;
    scaled.y = log_q.y * t;
    scaled.z = log_q.z * t;

    return quat_exp(scaled);
}

//=============================================================================
// Rotation Operations
//=============================================================================

void quat_to_rotation_matrix(nimcp_quaternion_t q, float out[9]) {
    // Normalize first
    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_to_rotation_mat", 0.0f);


    nimcp_quaternion_t qn = quat_normalize(q);

    float w = qn.w, x = qn.x, y = qn.y, z = qn.z;

    // Precompute products
    float xx = x * x, yy = y * y, zz = z * z;
    float xy = x * y, xz = x * z, yz = y * z;
    float wx = w * x, wy = w * y, wz = w * z;

    // Row-major 3x3 matrix
    out[0] = 1.0f - 2.0f * (yy + zz);
    out[1] = 2.0f * (xy - wz);
    out[2] = 2.0f * (xz + wy);

    out[3] = 2.0f * (xy + wz);
    out[4] = 1.0f - 2.0f * (xx + zz);
    out[5] = 2.0f * (yz - wx);

    out[6] = 2.0f * (xz - wy);
    out[7] = 2.0f * (yz + wx);
    out[8] = 1.0f - 2.0f * (xx + yy);
}

void quat_to_euler(nimcp_quaternion_t q, float* roll, float* pitch, float* yaw) {
    if (!roll || !pitch || !yaw) return;

    // Normalize first
    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_to_euler", 0.0f);


    nimcp_quaternion_t qn = quat_normalize(q);

    float w = qn.w, x = qn.x, y = qn.y, z = qn.z;

    // Roll (X-axis rotation)
    float sinr_cosp = 2.0f * (w * x + y * z);
    float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
    *roll = atan2f(sinr_cosp, cosr_cosp);

    // Pitch (Y-axis rotation)
    float sinp = 2.0f * (w * y - z * x);
    if (fabsf(sinp) >= 1.0f) {
        // Gimbal lock: use 90 degrees
        *pitch = copysignf((float)M_PI_2, sinp);
    } else {
        *pitch = asinf(sinp);
    }

    // Yaw (Z-axis rotation)
    float siny_cosp = 2.0f * (w * z + x * y);
    float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
    *yaw = atan2f(siny_cosp, cosy_cosp);
}

nimcp_vec3_t quat_rotate_vector(nimcp_quaternion_t q, nimcp_vec3_t v) {
    // Rotation: v' = q * v * q^(-1)
    // where v is treated as pure quaternion (0, vx, vy, vz)

    // Optimized formula (avoiding full Hamilton product):
    // v' = v + 2*w*(axis x v) + 2*(axis x (axis x v))
    // where q = (w, axis)

    nimcp_quaternion_t qn = quat_normalize(q);

    // t = 2 * cross(axis, v)
    float tx = 2.0f * (qn.y * v.z - qn.z * v.y);
    float ty = 2.0f * (qn.z * v.x - qn.x * v.z);
    float tz = 2.0f * (qn.x * v.y - qn.y * v.x);

    // v' = v + w*t + cross(axis, t)
    nimcp_vec3_t result;
    result.x = v.x + qn.w * tx + (qn.y * tz - qn.z * ty);
    result.y = v.y + qn.w * ty + (qn.z * tx - qn.x * tz);
    result.z = v.z + qn.w * tz + (qn.x * ty - qn.y * tx);

    return result;
}

//=============================================================================
// Memory-Specific Operations
//=============================================================================

nimcp_quaternion_t quat_blend_memories(const nimcp_quaternion_t* quats,
                                        const float* weights,
                                        size_t count) {
    if (!quats || !weights || count == 0) {
        return quat_identity();
    }

    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_blend_memories", 0.0f);


    if (count == 1) {
        return quat_normalize(quats[0]);
    }

    // Compute total weight
    float total_weight = 0.0f;
    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            quaternion_heartbeat("quaternion_loop",
                             (float)(i + 1) / (float)count);
        }

        total_weight += weights[i];
    }

    if (total_weight < QUAT_EPSILON) {
        return quat_identity();
    }

    float inv_total = 1.0f / (fabsf(total_weight) > 1e-7f ? total_weight : 1e-7f);

    // For small counts, use iterative SLERP
    // For larger counts, use log-space averaging

    if (count <= 3) {
        // Iterative SLERP approach
        nimcp_quaternion_t result = quats[0];
        float accum_weight = weights[0];

        for (size_t i = 1; i < count; i++) {
            float w_i = weights[i];
            float t = w_i / (accum_weight + w_i);
            result = quat_slerp(result, quats[i], t);
            accum_weight += w_i;
        }

        return quat_normalize(result);
    }

    // Log-space averaging for larger counts
    // result = exp(sum(w_i * log(q_i)) / sum(w_i))

    // Use first quaternion as reference to handle sign ambiguity
    nimcp_quaternion_t ref = quat_normalize(quats[0]);

    nimcp_quaternion_t sum_log = quat_create(0.0f, 0.0f, 0.0f, 0.0f);

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            quaternion_heartbeat("quaternion_loop",
                             (float)(i + 1) / (float)count);
        }

        nimcp_quaternion_t qi = quat_normalize(quats[i]);

        // Handle antipodal: ensure same hemisphere as reference
        if (quat_dot(ref, qi) < 0.0f) {
            qi = quat_negate(qi);
        }

        nimcp_quaternion_t log_qi = quat_log(qi);
        float w_i = weights[i] * inv_total;

        sum_log.w += log_qi.w * w_i;
        sum_log.x += log_qi.x * w_i;
        sum_log.y += log_qi.y * w_i;
        sum_log.z += log_qi.z * w_i;
    }

    return quat_normalize(quat_exp(sum_log));
}

bool quat_interpolate_path(nimcp_quaternion_t q_start,
                            nimcp_quaternion_t q_end,
                            size_t steps,
                            nimcp_quaternion_t* out) {
    if (!out || steps < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "quat_interpolate_path: invalid parameters");

            return false;
    }

    // Normalize inputs
    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_interpolate_pat", 0.0f);


    nimcp_quaternion_t qs = quat_normalize(q_start);
    nimcp_quaternion_t qe = quat_normalize(q_end);

    // Generate interpolated path
    for (size_t i = 0; i < steps; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && steps > 256) {
            quaternion_heartbeat("quaternion_loop",
                             (float)(i + 1) / (float)steps);
        }

        float t = (float)i / (float)(steps - 1);
        out[i] = quat_slerp(qs, qe, t);
    }

    return true;
}

float quat_semantic_distance(nimcp_quaternion_t q1,
                              nimcp_quaternion_t q2,
                              float w_weight,
                              float x_weight,
                              float y_weight,
                              float z_weight) {
    // Weighted Euclidean distance in quaternion space
    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_semantic_distan", 0.0f);


    float dw = q1.w - q2.w;
    float dx = q1.x - q2.x;
    float dy = q1.y - q2.y;
    float dz = q1.z - q2.z;

    return sqrtf(w_weight * dw * dw +
                 x_weight * dx * dx +
                 y_weight * dy * dy +
                 z_weight * dz * dz);
}

float quat_semantic_distance_default(nimcp_quaternion_t q1, nimcp_quaternion_t q2) {
    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_semantic_distan", 0.0f);


    return quat_semantic_distance(q1, q2, 1.0f, 1.0f, 1.0f, 1.0f);
}

//=============================================================================
// Utility Functions
//=============================================================================

bool quat_is_unit(nimcp_quaternion_t q, float epsilon) {
    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_is_unit", 0.0f);


    float mag_sq = q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z;
    return fabsf(mag_sq - 1.0f) < epsilon;
}

bool quat_is_zero(nimcp_quaternion_t q, float epsilon) {
    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_is_zero", 0.0f);


    float mag_sq = q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z;
    return mag_sq < epsilon * epsilon;
}

bool quat_approx_equal(nimcp_quaternion_t q1, nimcp_quaternion_t q2, float epsilon) {
    // Check both q1 ≈ q2 and q1 ≈ -q2 (same rotation)
    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_approx_equal", 0.0f);


    bool direct = (fabsf(q1.w - q2.w) < epsilon &&
                   fabsf(q1.x - q2.x) < epsilon &&
                   fabsf(q1.y - q2.y) < epsilon &&
                   fabsf(q1.z - q2.z) < epsilon);

    if (direct) return true;

    bool negated = (fabsf(q1.w + q2.w) < epsilon &&
                    fabsf(q1.x + q2.x) < epsilon &&
                    fabsf(q1.y + q2.y) < epsilon &&
                    fabsf(q1.z + q2.z) < epsilon);

    return negated;
}

nimcp_quaternion_t quat_negate(nimcp_quaternion_t q) {
    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_negate", 0.0f);


    nimcp_quaternion_t result;
    result.w = -q.w;
    result.x = -q.x;
    result.y = -q.y;
    result.z = -q.z;
    return result;
}

nimcp_quaternion_t quat_add(nimcp_quaternion_t q1, nimcp_quaternion_t q2) {
    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_add", 0.0f);


    nimcp_quaternion_t result;
    result.w = q1.w + q2.w;
    result.x = q1.x + q2.x;
    result.y = q1.y + q2.y;
    result.z = q1.z + q2.z;
    return result;
}

nimcp_quaternion_t quat_sub(nimcp_quaternion_t q1, nimcp_quaternion_t q2) {
    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_sub", 0.0f);


    nimcp_quaternion_t result;
    result.w = q1.w - q2.w;
    result.x = q1.x - q2.x;
    result.y = q1.y - q2.y;
    result.z = q1.z - q2.z;
    return result;
}

nimcp_quaternion_t quat_scale(nimcp_quaternion_t q, float s) {
    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_scale", 0.0f);


    nimcp_quaternion_t result;
    result.w = q.w * s;
    result.x = q.x * s;
    result.y = q.y * s;
    result.z = q.z * s;
    return result;
}

nimcp_quat_config_t quat_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_default_config", 0.0f);


    nimcp_quat_config_t config;
    config.normalize_threshold = 0.001f;
    config.slerp_threshold = QUAT_SLERP_THRESHOLD;
    config.auto_normalize = true;
    return config;
}

//=============================================================================
// Vector Helper Functions
//=============================================================================

nimcp_vec3_t quat_vec3_create(float x, float y, float z) {
    nimcp_vec3_t v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

nimcp_vec3_t quat_vec3_normalize(nimcp_vec3_t v) {
    float mag_sq = v.x * v.x + v.y * v.y + v.z * v.z;

    if (mag_sq < QUAT_EPSILON * QUAT_EPSILON) {
        nimcp_vec3_t zero = {0.0f, 0.0f, 0.0f};
        return zero;
    }

    float inv_mag = 1.0f / sqrtf(mag_sq);

    nimcp_vec3_t result;
    result.x = v.x * inv_mag;
    result.y = v.y * inv_mag;
    result.z = v.z * inv_mag;

    return result;
}

float quat_vec3_magnitude(nimcp_vec3_t v) {
    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_vec3_magnitude", 0.0f);


    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

float quat_vec3_dot(nimcp_vec3_t v1, nimcp_vec3_t v2) {
    /* Phase 8: Heartbeat at operation start */
    quaternion_heartbeat("quaternion_quat_vec3_dot", 0.0f);


    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void quaternion_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_quaternion_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration
 *
 * NOTE: The quaternion module is a pure math library without mutable state.
 * Training functions operate on a quaternion passed as instance, treating it
 * as a 4D learnable state vector with gradient-like updates.
 * ============================================================================ */

int quaternion_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "quaternion_training_begin: NULL argument");
        return -1;
    }
    quaternion_heartbeat_instance(NULL, "quaternion_training_begin", 0.0f);

    /* Cast to quaternion and normalize to unit sphere for training start */
    nimcp_quaternion_t* q = (nimcp_quaternion_t*)instance;

    /* Ensure quaternion is normalized before training begins */
    float mag_sq = q->w * q->w + q->x * q->x + q->y * q->y + q->z * q->z;
    if (mag_sq > QUAT_EPSILON * QUAT_EPSILON) {
        float inv_mag = 1.0f / sqrtf(mag_sq);
        q->w *= inv_mag;
        q->x *= inv_mag;
        q->y *= inv_mag;
        q->z *= inv_mag;
    } else {
        /* Degenerate quaternion: reset to identity */
        q->w = 1.0f;
        q->x = 0.0f;
        q->y = 0.0f;
        q->z = 0.0f;
    }

    quaternion_heartbeat_instance(NULL, "quaternion_training_begin", 1.0f);
    return 0;
}

int quaternion_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "quaternion_training_end: NULL argument");
        return -1;
    }
    quaternion_heartbeat_instance(NULL, "quaternion_training_end", 0.0f);

    /* Final normalization to ensure valid quaternion state post-training */
    nimcp_quaternion_t* q = (nimcp_quaternion_t*)instance;

    float mag_sq = q->w * q->w + q->x * q->x + q->y * q->y + q->z * q->z;
    if (mag_sq > QUAT_EPSILON * QUAT_EPSILON) {
        float inv_mag = 1.0f / sqrtf(mag_sq);
        q->w *= inv_mag;
        q->x *= inv_mag;
        q->y *= inv_mag;
        q->z *= inv_mag;
    }

    quaternion_heartbeat_instance(NULL, "quaternion_training_end", 1.0f);
    return 0;
}

int quaternion_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "quaternion_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    quaternion_heartbeat_instance(NULL, "quaternion_training_step", progress);

    nimcp_quaternion_t* q = (nimcp_quaternion_t*)instance;

    /*
     * Apply exponential map perturbation for training adaptation:
     * Use a decreasing learning rate as progress increases (annealing).
     * The perturbation is applied in the tangent space via exp map.
     */
    float lr = 0.01f * (1.0f - 0.9f * progress);  /* Anneal from 0.01 to 0.001 */

    /* Small gradient-like update toward consolidation (w component) */
    q->w += lr * (1.0f - q->w);

    /* Re-normalize to maintain unit quaternion constraint */
    float mag_sq = q->w * q->w + q->x * q->x + q->y * q->y + q->z * q->z;
    if (mag_sq > QUAT_EPSILON * QUAT_EPSILON) {
        float inv_mag = 1.0f / sqrtf(mag_sq);
        q->w *= inv_mag;
        q->x *= inv_mag;
        q->y *= inv_mag;
        q->z *= inv_mag;
    }

    return 0;
}
