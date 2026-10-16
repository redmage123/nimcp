/**
 * @file nimcp_dragonfly_lnn.h
 * @brief Sidecar LNN reservoir for dragonfly temporal smoothing (Phase 4i).
 *
 * Dragonfly's existing trackers (TSDN, tracking, prediction, intercept) are
 * retained intact. This module adds an LNN *alongside* them — a small NCP
 * network that consumes dragonfly state each tick and produces a smoothed
 * continuous-time representation. Reservoir-first: weights are untrained
 * random projectors, so the LNN acts as a learnable-τ liquid state machine
 * denoiser. Training can be added later without API changes.
 *
 * Typical consumer: the octopus bridges sampler can read the LNN-smoothed
 * state instead of raw TSDN for stable arm input. But the LNN is a first-
 * class brain subsystem, not tied to octopus.
 *
 * SOLID:
 *   SRP: owns just the LNN + input/output tensors; does not touch dragonfly
 *        internals beyond public getters.
 *   OCP: additional channels or a training attachment can be added without
 *        changing this header.
 *   DIP: callers depend on these accessors, not lnn_network_t directly.
 */
#ifndef NIMCP_DRAGONFLY_LNN_H
#define NIMCP_DRAGONFLY_LNN_H

#include <stdbool.h>
#include <stdint.h>

#include "common/nimcp_export.h"
#include "dragonfly/nimcp_dragonfly.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Input/output dimensionality of the reservoir. */
#define DRAGONFLY_LNN_DIM 16u

/**
 * @brief Opaque handle.
 */
typedef struct dragonfly_lnn_s dragonfly_lnn_t;

/**
 * @brief Create the sidecar LNN bound to the given dragonfly system.
 *
 * The dragonfly pointer must outlive the returned handle; this module
 * keeps a non-owning reference to it.
 *
 * @param df Dragonfly system (must not be NULL).
 * @return Handle on success, NULL on any failure (LNN alloc, tensor alloc).
 */
NIMCP_EXPORT dragonfly_lnn_t* dragonfly_lnn_create(dragonfly_system_t* df);

/**
 * @brief Destroy the reservoir. NULL-safe.
 */
NIMCP_EXPORT void dragonfly_lnn_destroy(dragonfly_lnn_t* dl);

/**
 * @brief Pull current dragonfly state, run one LNN forward step, cache
 *        the output for later `_get_output` calls.
 *
 * Safe to call at any cadence; LNN internal state evolves continuously.
 *
 * @param dl    Handle (NULL returns -1).
 * @param dt_ms Integration step in milliseconds (non-positive uses default).
 * @return 0 on success, -1 on failure (stale dragonfly, tensor error, etc.).
 */
NIMCP_EXPORT int dragonfly_lnn_step(dragonfly_lnn_t* dl, float dt_ms);

/**
 * @brief Copy the last step's output into caller buffer.
 *
 * Channel semantics (stable across runs):
 *   [ 0 ..  2]  smoothed target position xyz (tanh-squashed)
 *   [ 3 ..  5]  smoothed target velocity xyz (tanh-squashed)
 *   [ 6 ..  8]  smoothed target predicted position xyz (tanh-squashed)
 *   [ 9]        confidence
 *   [10]        threat level
 *   [11]        TSDN direction sin
 *   [12]        TSDN direction cos
 *   [13]        TSDN magnitude
 *   [14]        TSDN angular velocity (tanh-squashed)
 *   [15]        mode / 4
 *
 * Channels are zero-padded if less than max_dim slots are populated.
 *
 * @param dl      Handle.
 * @param out     Caller buffer.
 * @param max_dim Buffer capacity in floats (clamped to DRAGONFLY_LNN_DIM).
 * @return Number of floats written; 0 on error.
 */
NIMCP_EXPORT uint32_t dragonfly_lnn_get_output(const dragonfly_lnn_t* dl,
                                                float* out,
                                                uint32_t max_dim);

/**
 * @brief Total number of successful forward steps since creation.
 */
NIMCP_EXPORT uint64_t dragonfly_lnn_get_step_count(const dragonfly_lnn_t* dl);

/**
 * @brief Phase 4k: retrieve the SO(3)-accumulated target orientation.
 *
 * The reservoir integrates an SO(3) rotation each step from TSDN angular
 * velocity (direction × angular_velocity × dt) via the Lie-algebra
 * exponential map + group composition. This keeps orientation on the
 * rotation manifold across arbitrarily long trajectories — no gimbal
 * lock, no drift from Euler-angle accumulation.
 *
 * Output is axis-angle: a unit rotation axis plus an angle magnitude
 * (radians). On a fresh reservoir, axis = (0,0,1) and angle = 0.
 *
 * @param dl            Handle.
 * @param axis_out      3-float output buffer (rotation axis, unit).
 * @param angle_out     Output: rotation magnitude in radians.
 * @return 0 on success, -1 on NULL args.
 */
NIMCP_EXPORT int dragonfly_lnn_get_so3_axis_angle(const dragonfly_lnn_t* dl,
                                                    float axis_out[3],
                                                    float* angle_out);

/**
 * @brief Phase 4k: geodesic distance (radians) between current and a
 *        reference orientation. Useful as a "how much has target
 *        direction rotated since t0" scalar.
 *
 * @return Distance in radians [0, π]; 0 on error.
 */
NIMCP_EXPORT float dragonfly_lnn_get_so3_distance_from_identity(
    const dragonfly_lnn_t* dl);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_LNN_H */
