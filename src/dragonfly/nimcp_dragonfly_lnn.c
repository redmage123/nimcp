/**
 * @file nimcp_dragonfly_lnn.c
 * @brief Sidecar LNN reservoir for dragonfly temporal smoothing (Phase 4i).
 *
 * See include/dragonfly/nimcp_dragonfly_lnn.h for API + rationale.
 */
#include "dragonfly/nimcp_dragonfly_lnn.h"

#include "lnn/nimcp_lnn_network.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/error/nimcp_error_codes.h"
/* Phase 4k: SO(3) Lie group for target orientation tracking. */
#include "utils/geometry/nimcp_lie_group.h"

#include <math.h>
#include <string.h>

struct dragonfly_lnn_s {
    dragonfly_system_t* df;       /* non-owning */
    lnn_network_t*      net;
    nimcp_tensor_t*     input;
    nimcp_tensor_t*     output;
    float               last_output[DRAGONFLY_LNN_DIM];
    uint64_t            step_count;
    float               default_dt_ms;
    /* Phase 4k: SO(3) accumulator for smooth target-orientation tracking.
     * Updated each step by multiplying in the delta rotation derived from
     * TSDN direction + angular velocity. Starts at identity. */
    so3_rotation_t      orientation;
    /* Last direction (radians) observed, used to compute the delta
     * rotation around the vertical axis across steps. */
    float               last_direction_rad;
    bool                has_last_direction;
};

dragonfly_lnn_t* dragonfly_lnn_create(dragonfly_system_t* df) {
    if (!df) return NULL;

    dragonfly_lnn_t* dl = (dragonfly_lnn_t*)nimcp_calloc(
        1, sizeof(dragonfly_lnn_t));
    if (!dl) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                              "dragonfly_lnn_create: handle alloc failed");
        return NULL;
    }
    dl->df                 = df;
    dl->default_dt_ms      = 1.0f;
    dl->orientation        = so3_identity();
    dl->has_last_direction = false;

    /* Small NCP reservoir: 16 → 8 → 8 → 16. Enough to exhibit LTC dynamics
     * without over-parameterizing for what is a 16-dim smoothing task. */
    dl->net = lnn_network_create_ncp(DRAGONFLY_LNN_DIM, 8, 8,
                                      DRAGONFLY_LNN_DIM);
    if (!dl->net) {
        NIMCP_LOGGING_WARN("dragonfly_lnn_create: NCP alloc failed; "
                           "dragonfly will run without LNN reservoir");
        nimcp_free(dl);
        return NULL;
    }
    /* Fixed seed for reproducibility; the reservoir is untrained so seed
     * choice is cosmetic — just avoid the default time-based seed so two
     * brains built from the same config produce comparable smoothing. */
    if (lnn_network_init_weights(dl->net, 0xD14F04u) != 0) {
        lnn_network_destroy(dl->net);
        nimcp_free(dl);
        return NULL;
    }

    uint32_t dims[1] = { DRAGONFLY_LNN_DIM };
    dl->input  = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    dl->output = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    if (!dl->input || !dl->output) {
        if (dl->input)  nimcp_tensor_destroy(dl->input);
        if (dl->output) nimcp_tensor_destroy(dl->output);
        lnn_network_destroy(dl->net);
        nimcp_free(dl);
        return NULL;
    }

    NIMCP_LOGGING_INFO("dragonfly_lnn_create: 16→8→8→16 reservoir live");
    return dl;
}

void dragonfly_lnn_destroy(dragonfly_lnn_t* dl) {
    if (!dl) return;
    if (dl->input)  nimcp_tensor_destroy(dl->input);
    if (dl->output) nimcp_tensor_destroy(dl->output);
    if (dl->net)    lnn_network_destroy(dl->net);
    nimcp_free(dl);
}

/* Pack current dragonfly state into the LNN's 16-dim input tensor. Returns
 * true on success. Unpopulated slots are zeroed — e.g., if no primary
 * target exists, position/velocity channels stay at 0 and the LNN state
 * naturally relaxes back toward baseline.
 *
 * tv_out (optional): when non-NULL, receives the TSDN vector read inside
 * this function so the caller can reuse it without a second dragonfly
 * accessor call this tick. Set to an all-zero vector with valid=false
 * when no TSDN is available. */
static bool _pack_input(dragonfly_lnn_t* dl, float* in, tsdn_vector_t* tv_out) {
    memset(in, 0, sizeof(float) * DRAGONFLY_LNN_DIM);

    /* Primary target position/velocity/predicted. */
    dragonfly_target_info_t t = {0};
    if (dragonfly_get_primary_target(dl->df, &t) == 0) {
        /* Positions/velocities can be unbounded — tanh-squash so the
         * LNN sees stable [-1,1] inputs regardless of world scale. */
        in[0]  = tanhf(t.position[0]);
        in[1]  = tanhf(t.position[1]);
        in[2]  = tanhf(t.position[2]);
        in[3]  = tanhf(t.velocity[0]);
        in[4]  = tanhf(t.velocity[1]);
        in[5]  = tanhf(t.velocity[2]);
        in[6]  = tanhf(t.predicted_position[0]);
        in[7]  = tanhf(t.predicted_position[1]);
        in[8]  = tanhf(t.predicted_position[2]);
        in[9]  = t.confidence  < 0.0f ? 0.0f :
                 t.confidence  > 1.0f ? 1.0f : t.confidence;
        in[10] = t.threat_level < 0.0f ? 0.0f :
                 t.threat_level > 1.0f ? 1.0f : t.threat_level;
    }

    /* TSDN vector — read once, share with caller. */
    tsdn_vector_t tv = {0};
    bool tv_ok = (dragonfly_get_tsdn_vector(dl->df, &tv) == 0 && tv.valid);
    if (tv_ok) {
        in[11] = sinf(tv.direction);
        in[12] = cosf(tv.direction);
        in[13] = tv.magnitude < 0.0f ? 0.0f :
                 tv.magnitude > 1.0f ? 1.0f : tv.magnitude;
        in[14] = tanhf(tv.angular_velocity);
    }
    if (tv_out) *tv_out = tv;

    /* Current mode, normalized to [0,1]. DRAGONFLY_MODE_INTERCEPTING = 4. */
    int mi = (int)dragonfly_get_mode(dl->df);
    if (mi < 0) mi = 0;
    if (mi > 4) mi = 4;
    in[15] = (float)mi / 4.0f;

    return true;
}

int dragonfly_lnn_step(dragonfly_lnn_t* dl, float dt_ms) {
    if (!dl || !dl->net || !dl->input || !dl->output) return -1;

    float* in_data = (float*)nimcp_tensor_data(dl->input);
    if (!in_data) return -1;
    tsdn_vector_t tv = {0};
    if (!_pack_input(dl, in_data, &tv)) return -1;

    float use_dt = dt_ms > 0.0f ? dt_ms : dl->default_dt_ms;
    if (lnn_network_forward_step(dl->net, dl->input,
                                  dl->output, use_dt) != 0) {
        return -1;
    }

    const float* out_data =
        (const float*)nimcp_tensor_data_const(dl->output);
    if (!out_data) return -1;

    /* Snapshot the output into last_output[] so callers can read at any
     * cadence without holding the tensor pointer. tanh-squash for bounded
     * downstream use. */
    for (uint32_t i = 0; i < DRAGONFLY_LNN_DIM; i++) {
        dl->last_output[i] = tanhf(out_data[i]);
    }
    dl->step_count++;

    /* Phase 4k: update the SO(3) orientation accumulator. Treat the TSDN
     * vector's direction as azimuth (yaw) and elevation as pitch, and use
     * angular_velocity as a rate signal. The delta rotation is constructed
     * in the Lie algebra so(3) (axis-angle form), mapped to SO(3) via
     * so3_exp, and composed with the current orientation via so3_multiply.
     * This keeps orientation on the rotation manifold regardless of how
     * many steps pass — no Euler-angle drift, no gimbal singularities.
     *
     * tv was already read in _pack_input and passed back via out-param,
     * so we don't call dragonfly_get_tsdn_vector again here. */
    if (tv.valid) {
        /* Yaw delta: change in direction since last observation, wrapped
         * to [-π, π] so big jumps across the azimuth seam don't send the
         * rotation the long way around. */
        float yaw_delta = 0.0f;
        if (dl->has_last_direction) {
            yaw_delta = tv.direction - dl->last_direction_rad;
            const float PI = 3.14159265358979323846f;
            while (yaw_delta >  PI) yaw_delta -= 2.0f * PI;
            while (yaw_delta < -PI) yaw_delta += 2.0f * PI;
        }
        dl->last_direction_rad = tv.direction;
        dl->has_last_direction = true;

        /* Also accumulate angular velocity × dt as an independent source
         * — use a small weighted blend so noisy direction samples don't
         * dominate the slower velocity estimate. */
        float dt_s        = use_dt / 1000.0f;
        float vel_delta   = tv.angular_velocity * dt_s;
        float yaw         = 0.7f * yaw_delta + 0.3f * vel_delta;
        float pitch_delta = tv.elevation * dt_s * 0.1f;

        so3_algebra_t omega = {0};
        omega.v[0] = pitch_delta; /* x-axis rotation = pitch */
        omega.v[1] = 0.0f;
        omega.v[2] = yaw;         /* z-axis rotation = yaw */
        so3_rotation_t delta = so3_exp(&omega);
        dl->orientation = so3_multiply(&dl->orientation, &delta);
    }

    return 0;
}

uint32_t dragonfly_lnn_get_output(const dragonfly_lnn_t* dl,
                                   float* out,
                                   uint32_t max_dim) {
    if (!dl || !out || max_dim == 0) return 0;
    uint32_t n = max_dim < DRAGONFLY_LNN_DIM ? max_dim : DRAGONFLY_LNN_DIM;
    memcpy(out, dl->last_output, sizeof(float) * n);
    return n;
}

uint64_t dragonfly_lnn_get_step_count(const dragonfly_lnn_t* dl) {
    return dl ? dl->step_count : 0;
}

int dragonfly_lnn_get_so3_axis_angle(const dragonfly_lnn_t* dl,
                                       float axis_out[3],
                                       float* angle_out) {
    if (!dl || !axis_out || !angle_out) return -1;
    so3_algebra_t logR = so3_log(&dl->orientation);
    /* so3_log returns axis × angle. Decompose: angle = |v|, axis = v / angle.
     * Identity rotation → |v| ≈ 0; report axis = (0,0,1), angle = 0. */
    float mag = sqrtf(logR.v[0] * logR.v[0] +
                      logR.v[1] * logR.v[1] +
                      logR.v[2] * logR.v[2]);
    if (mag < 1e-8f) {
        axis_out[0] = 0.0f;
        axis_out[1] = 0.0f;
        axis_out[2] = 1.0f;
        *angle_out  = 0.0f;
    } else {
        axis_out[0] = logR.v[0] / mag;
        axis_out[1] = logR.v[1] / mag;
        axis_out[2] = logR.v[2] / mag;
        *angle_out  = mag;
    }
    return 0;
}

float dragonfly_lnn_get_so3_distance_from_identity(const dragonfly_lnn_t* dl) {
    if (!dl) return 0.0f;
    so3_rotation_t I = so3_identity();
    return so3_distance(&I, &dl->orientation);
}
