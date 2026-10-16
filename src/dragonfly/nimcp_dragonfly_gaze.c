/**
 * @file nimcp_dragonfly_gaze.c
 * @brief Predictive Gaze Stabilization System Implementation
 *
 * WHAT: Implements gaze stabilization with target lock
 * WHY:  Maintains visual tracking during body maneuvers
 * HOW:  Predictive head movement with vestibular-ocular reflex
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#include "dragonfly/nimcp_dragonfly_gaze.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_gaze)

//=============================================================================
// Local Helpers
//=============================================================================

static inline uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static inline float vec3_length(const float v[3]) {
    return sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static inline void vec3_normalize(const float v[3], float out[3]) {
    float len = vec3_length(v);
    if (len > 1e-6f) {
        out[0] = v[0] / len;
        out[1] = v[1] / len;
        out[2] = v[2] / len;
    } else {
        out[0] = 0.0f;
        out[1] = 0.0f;
        out[2] = 1.0f;
    }
}

//=============================================================================
// Internal Structure
//=============================================================================

struct dragonfly_gaze_s {
    /* Configuration */
    gaze_config_t config;

    /* Head state */
    head_state_t head;

    /* Current target */
    gaze_target_t target;
    bool has_target;

    /* Gaze state */
    gaze_mode_t mode;
    bool locked;
    float gaze_direction[3];

    /* Saccade state */
    saccade_state_t saccade;

    /* Self position */
    float self_position[3];

    /* Statistics */
    gaze_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Timing */
    uint64_t creation_time_us;
    uint64_t last_update_us;
};

//=============================================================================
// Name Functions
//=============================================================================

const char* dragonfly_gaze_mode_name(gaze_mode_t mode) {
    switch (mode) {
        case GAZE_MODE_FREE:       return "free";
        case GAZE_MODE_STABILIZED: return "stabilized";
        case GAZE_MODE_TARGET_LOCK: return "target_lock";
        case GAZE_MODE_PREDICTIVE: return "predictive";
        case GAZE_MODE_SACCADE:    return "saccade";
        default:                   return "unknown";
    }
}

//=============================================================================
// Configuration Functions
//=============================================================================

gaze_config_t gaze_default_config(void) {
    gaze_config_t config = {
        /* Head limits */
        .max_yaw_rad = 1.0f,          /* ~57 degrees */
        .max_pitch_up_rad = 0.7f,     /* ~40 degrees */
        .max_pitch_down_rad = 0.5f,   /* ~29 degrees */
        .max_roll_rad = 0.3f,         /* ~17 degrees */

        /* Movement limits */
        .max_yaw_rate = 10.0f,        /* rad/s */
        .max_pitch_rate = 8.0f,
        .max_roll_rate = 5.0f,

        /* Tracking gains */
        .pursuit_gain = 0.8f,
        .vor_gain = 0.95f,
        .prediction_horizon_ms = 50.0f,

        /* Saccade parameters */
        .saccade_threshold_rad = 0.3f,
        .saccade_latency_ms = 20.0f,
        .saccade_duration_ms = 30.0f,

        /* Stabilization */
        .stabilization_gain = 0.9f,
        .enable_predictive = true,
        .enable_vor = true
    };
    return config;
}

bool gaze_validate_config(const gaze_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gaze_validate_config: config is NULL");
        return false;
    }

    if (config->max_yaw_rad < 0.0f) {
        return false;
    }
    if (config->max_pitch_up_rad < 0.0f) {
        return false;
    }
    if (config->max_pitch_down_rad < 0.0f) {
        return false;
    }
    if (config->max_roll_rad < 0.0f) {
        return false;
    }

    if (config->max_yaw_rate <= 0.0f) {
        return false;
    }
    if (config->max_pitch_rate <= 0.0f) {
        return false;
    }
    if (config->max_roll_rate <= 0.0f) {
        return false;
    }

    if (config->pursuit_gain < 0.0f || config->pursuit_gain > 1.0f) {
        return false;
    }
    if (config->vor_gain < 0.0f || config->vor_gain > 1.0f) {
        return false;
    }
    if (config->stabilization_gain < 0.0f || config->stabilization_gain > 1.0f) {
        return false;
    }

    if (config->saccade_threshold_rad < 0.0f) {
        return false;
    }
    if (config->saccade_latency_ms < 0.0f) {
        return false;
    }
    if (config->saccade_duration_ms <= 0.0f) {
        return false;
    }

    return true;
}

//=============================================================================
// Internal Helpers
//=============================================================================

static void compute_target_angles(
    const float self_pos[3],
    const float target_pos[3],
    float* azimuth,
    float* elevation
) {
    float dx = target_pos[0] - self_pos[0];
    float dy = target_pos[1] - self_pos[1];
    float dz = target_pos[2] - self_pos[2];

    float horizontal = sqrtf(dx*dx + dy*dy);

    *azimuth = atan2f(dy, dx);
    *elevation = atan2f(dz, horizontal);
}

static void apply_vor_compensation(
    dragonfly_gaze_t gaze,
    const body_state_t* body,
    float dt_s
) {
    if (!gaze->config.enable_vor || !body) return;

    float gain = gaze->config.vor_gain;

    /* Counter-rotate head to compensate for body rotation */
    gaze->head.yaw_rad -= body->yaw_rate * dt_s * gain;
    gaze->head.pitch_rad -= body->pitch_rate * dt_s * gain;
    gaze->head.roll_rad -= body->roll_rate * dt_s * gain;
}

static void apply_head_limits(dragonfly_gaze_t gaze) {
    gaze->head.yaw_rad = nimcp_clampf(gaze->head.yaw_rad,
                                  -gaze->config.max_yaw_rad,
                                  gaze->config.max_yaw_rad);

    gaze->head.pitch_rad = nimcp_clampf(gaze->head.pitch_rad,
                                    -gaze->config.max_pitch_down_rad,
                                    gaze->config.max_pitch_up_rad);

    gaze->head.roll_rad = nimcp_clampf(gaze->head.roll_rad,
                                   -gaze->config.max_roll_rad,
                                   gaze->config.max_roll_rad);

    /* Check limits */
    gaze->head.yaw_limit = (fabsf(gaze->head.yaw_rad) >= gaze->config.max_yaw_rad * 0.95f);
    gaze->head.pitch_limit = (gaze->head.pitch_rad >= gaze->config.max_pitch_up_rad * 0.95f ||
                              gaze->head.pitch_rad <= -gaze->config.max_pitch_down_rad * 0.95f);
}

static void update_saccade(dragonfly_gaze_t gaze, float dt_s) {
    if (!gaze->saccade.in_progress) return;

    float dt_ms = dt_s * 1000.0f;
    float progress_inc = dt_ms / gaze->saccade.duration_ms;
    gaze->saccade.progress += progress_inc;

    if (gaze->saccade.progress >= 1.0f) {
        /* Saccade complete */
        gaze->saccade.in_progress = false;
        gaze->saccade.progress = 1.0f;
        gaze->mode = gaze->has_target ? GAZE_MODE_TARGET_LOCK : GAZE_MODE_STABILIZED;
        gaze->stats.saccades++;
    } else {
        /* Smooth saccade trajectory (sigmoid-like) */
        float t = gaze->saccade.progress;
        float smooth = t * t * (3.0f - 2.0f * t);  /* Hermite interpolation */

        /* Interpolate to target angles */
        /* (In a full implementation, would track start angles) */
        gaze->head.yaw_rad += (gaze->saccade.target_az_rad - gaze->head.yaw_rad) * smooth * 0.1f;
        gaze->head.pitch_rad += (gaze->saccade.target_el_rad - gaze->head.pitch_rad) * smooth * 0.1f;
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

dragonfly_gaze_t dragonfly_gaze_create(const gaze_config_t* config) {
    gaze_config_t cfg = config ? *config : gaze_default_config();

    if (!gaze_validate_config(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "dragonfly_gaze_create: invalid configuration");
        return NULL;
    }

    dragonfly_gaze_t gaze = nimcp_calloc(1, sizeof(struct dragonfly_gaze_s));
    if (!gaze) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "dragonfly_gaze_create: failed to allocate gaze");
        return NULL;
    }

    gaze->config = cfg;
    gaze->creation_time_us = get_time_us();
    gaze->mode = GAZE_MODE_STABILIZED;

    /* Initialize gaze direction (forward) */
    gaze->gaze_direction[0] = 1.0f;
    gaze->gaze_direction[1] = 0.0f;
    gaze->gaze_direction[2] = 0.0f;

    gaze->mutex = nimcp_mutex_create(NULL);
    if (!gaze->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "dragonfly_gaze_create: failed to create mutex");
        nimcp_free(gaze);
        return NULL;
    }

    return gaze;
}

void dragonfly_gaze_destroy(dragonfly_gaze_t gaze) {
    if (!gaze) return;

    if (gaze->mutex) {
        nimcp_mutex_destroy(gaze->mutex);
    }

    nimcp_free(gaze);
}

int dragonfly_gaze_reset(dragonfly_gaze_t gaze) {
    if (!gaze) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_gaze_reset: gaze is NULL");
        return -1;
    }

    nimcp_mutex_lock(gaze->mutex);

    memset(&gaze->head, 0, sizeof(gaze->head));
    gaze->has_target = false;
    gaze->mode = GAZE_MODE_STABILIZED;
    gaze->locked = false;
    gaze->saccade.in_progress = false;

    gaze->gaze_direction[0] = 1.0f;
    gaze->gaze_direction[1] = 0.0f;
    gaze->gaze_direction[2] = 0.0f;

    nimcp_mutex_unlock(gaze->mutex);

    return 0;
}

//=============================================================================
// Target Functions
//=============================================================================

int dragonfly_gaze_set_target(
    dragonfly_gaze_t gaze,
    const gaze_target_t* target
) {
    if (!gaze || !target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_gaze_set_target: required parameter is NULL (gaze, target)");
        return -1;
    }

    nimcp_mutex_lock(gaze->mutex);

    gaze->target = *target;
    gaze->target.last_update_us = get_time_us();
    gaze->has_target = true;

    /* Check if saccade needed */
    float target_az, target_el;
    compute_target_angles(gaze->self_position, target->position, &target_az, &target_el);

    float error = fabsf(target_az - gaze->head.yaw_rad) +
                  fabsf(target_el - gaze->head.pitch_rad);

    if (error > gaze->config.saccade_threshold_rad && !gaze->saccade.in_progress) {
        gaze->saccade.target_az_rad = target_az;
        gaze->saccade.target_el_rad = target_el;
        gaze->saccade.duration_ms = gaze->config.saccade_duration_ms;
        gaze->saccade.in_progress = true;
        gaze->saccade.progress = 0.0f;
        gaze->mode = GAZE_MODE_SACCADE;
    }

    nimcp_mutex_unlock(gaze->mutex);

    return 0;
}

int dragonfly_gaze_update_target(
    dragonfly_gaze_t gaze,
    const float position[3],
    const float velocity[3]
) {
    if (!gaze || !position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_gaze_update_target: required parameter is NULL (gaze, position)");
        return -1;
    }

    nimcp_mutex_lock(gaze->mutex);

    if (gaze->has_target) {
        memcpy(gaze->target.position, position, sizeof(gaze->target.position));
        if (velocity) {
            memcpy(gaze->target.velocity, velocity, sizeof(gaze->target.velocity));
            gaze->target.is_moving = (vec3_length(velocity) > 0.1f);
        }
        gaze->target.last_update_us = get_time_us();
    }

    nimcp_mutex_unlock(gaze->mutex);

    return 0;
}

int dragonfly_gaze_clear_target(dragonfly_gaze_t gaze) {
    if (!gaze) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_gaze_clear_target: gaze is NULL");
        return -1;
    }

    nimcp_mutex_lock(gaze->mutex);

    gaze->has_target = false;
    gaze->locked = false;
    gaze->mode = GAZE_MODE_STABILIZED;

    nimcp_mutex_unlock(gaze->mutex);

    return 0;
}

int dragonfly_gaze_lock(dragonfly_gaze_t gaze) {
    if (!gaze || !gaze->has_target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_gaze_lock: required parameter is NULL (gaze, gaze->has_target)");
        return -1;
    }

    nimcp_mutex_lock(gaze->mutex);

    gaze->locked = true;
    gaze->mode = GAZE_MODE_TARGET_LOCK;
    gaze->stats.target_locks++;

    nimcp_mutex_unlock(gaze->mutex);

    return 0;
}

int dragonfly_gaze_unlock(dragonfly_gaze_t gaze) {
    if (!gaze) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_gaze_unlock: gaze is NULL");
        return -1;
    }

    nimcp_mutex_lock(gaze->mutex);

    gaze->locked = false;
    gaze->mode = GAZE_MODE_STABILIZED;

    nimcp_mutex_unlock(gaze->mutex);

    return 0;
}

//=============================================================================
// Update Functions
//=============================================================================

int dragonfly_gaze_update(
    dragonfly_gaze_t gaze,
    const body_state_t* body_state,
    const float self_position[3],
    float dt_s,
    gaze_command_t* command
) {
    if (!gaze || dt_s <= 0.0f || !command) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_gaze_update: required parameter is NULL (gaze, command)");
        return -1;
    }

    nimcp_mutex_lock(gaze->mutex);

    if (self_position) {
        memcpy(gaze->self_position, self_position, sizeof(gaze->self_position));
    }

    /* Apply VOR compensation */
    apply_vor_compensation(gaze, body_state, dt_s);

    /* Update saccade if in progress */
    if (gaze->saccade.in_progress) {
        update_saccade(gaze, dt_s);
    }

    /* Smooth pursuit tracking */
    if (gaze->has_target && gaze->mode == GAZE_MODE_TARGET_LOCK) {
        float target_pos[3];
        memcpy(target_pos, gaze->target.position, sizeof(target_pos));

        /* Predictive tracking */
        if (gaze->config.enable_predictive && gaze->target.is_moving) {
            float pred_time = gaze->config.prediction_horizon_ms / 1000.0f;
            for (int i = 0; i < 3; i++) {
                target_pos[i] += gaze->target.velocity[i] * pred_time;
            }
        }

        float target_az, target_el;
        compute_target_angles(gaze->self_position, target_pos, &target_az, &target_el);

        /* Smooth pursuit */
        float yaw_error = target_az - gaze->head.yaw_rad;
        float pitch_error = target_el - gaze->head.pitch_rad;

        float yaw_rate = yaw_error * gaze->config.pursuit_gain / dt_s;
        float pitch_rate = pitch_error * gaze->config.pursuit_gain / dt_s;

        /* Rate limiting */
        yaw_rate = nimcp_clampf(yaw_rate, -gaze->config.max_yaw_rate, gaze->config.max_yaw_rate);
        pitch_rate = nimcp_clampf(pitch_rate, -gaze->config.max_pitch_rate, gaze->config.max_pitch_rate);

        gaze->head.yaw_rad += yaw_rate * dt_s;
        gaze->head.pitch_rad += pitch_rate * dt_s;
        gaze->head.yaw_rate = yaw_rate;
        gaze->head.pitch_rate = pitch_rate;

        /* Update statistics */
        float tracking_error = sqrtf(yaw_error * yaw_error + pitch_error * pitch_error);
        gaze->stats.avg_tracking_error_rad =
            (gaze->stats.avg_tracking_error_rad * gaze->stats.updates + tracking_error) /
            (gaze->stats.updates + 1);

        if (tracking_error < 0.1f) {
            gaze->stats.time_on_target_s += dt_s;
            gaze->locked = true;
        } else if (tracking_error > 0.3f) {
            gaze->stats.target_losses++;
            gaze->locked = false;
        }
    }

    /* Apply limits */
    apply_head_limits(gaze);

    /* Compute gaze direction in world frame */
    float cy = cosf(gaze->head.yaw_rad);
    float sy = sinf(gaze->head.yaw_rad);
    float cp = cosf(gaze->head.pitch_rad);
    float sp = sinf(gaze->head.pitch_rad);

    gaze->gaze_direction[0] = cp * cy;
    gaze->gaze_direction[1] = cp * sy;
    gaze->gaze_direction[2] = sp;

    /* Fill command output */
    command->yaw_cmd_rad = gaze->head.yaw_rad;
    command->pitch_cmd_rad = gaze->head.pitch_rad;
    command->roll_cmd_rad = gaze->head.roll_rad;
    command->yaw_rate_cmd = gaze->head.yaw_rate;
    command->pitch_rate_cmd = gaze->head.pitch_rate;
    command->mode = gaze->mode;
    command->tracking_error_rad = gaze->stats.avg_tracking_error_rad;
    command->lock_quality = gaze->locked ? 1.0f : 0.0f;

    if (gaze->has_target && gaze->config.enable_predictive) {
        float pred_time = gaze->config.prediction_horizon_ms / 1000.0f;
        float pred_pos[3];
        for (int i = 0; i < 3; i++) {
            pred_pos[i] = gaze->target.position[i] + gaze->target.velocity[i] * pred_time;
        }
        compute_target_angles(gaze->self_position, pred_pos,
                              &command->predicted_target_az, &command->predicted_target_el);
    }

    gaze->stats.updates++;
    gaze->last_update_us = get_time_us();

    nimcp_mutex_unlock(gaze->mutex);

    return 0;
}

int dragonfly_gaze_saccade_to(
    dragonfly_gaze_t gaze,
    float target_az_rad,
    float target_el_rad
) {
    if (!gaze) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_gaze_saccade_to: gaze is NULL");
        return -1;
    }

    nimcp_mutex_lock(gaze->mutex);

    gaze->saccade.target_az_rad = target_az_rad;
    gaze->saccade.target_el_rad = target_el_rad;
    gaze->saccade.duration_ms = gaze->config.saccade_duration_ms;
    gaze->saccade.in_progress = true;
    gaze->saccade.progress = 0.0f;
    gaze->mode = GAZE_MODE_SACCADE;

    nimcp_mutex_unlock(gaze->mutex);

    return 0;
}

int dragonfly_gaze_set_mode(dragonfly_gaze_t gaze, gaze_mode_t mode) {
    if (!gaze) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_gaze_set_mode: gaze is NULL");
        return -1;
    }

    nimcp_mutex_lock(gaze->mutex);
    gaze->mode = mode;
    nimcp_mutex_unlock(gaze->mutex);

    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

int dragonfly_gaze_get_head_state(
    const dragonfly_gaze_t gaze,
    head_state_t* state
) {
    if (!gaze || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_gaze_get_head_state: required parameter is NULL (gaze, state)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)gaze->mutex);
    *state = gaze->head;
    nimcp_mutex_unlock((nimcp_mutex_t*)gaze->mutex);

    return 0;
}

int dragonfly_gaze_get_direction(
    const dragonfly_gaze_t gaze,
    float direction[3]
) {
    if (!gaze || !direction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_gaze_get_direction: required parameter is NULL (gaze, direction)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)gaze->mutex);
    memcpy(direction, gaze->gaze_direction, sizeof(gaze->gaze_direction));
    nimcp_mutex_unlock((nimcp_mutex_t*)gaze->mutex);

    return 0;
}

gaze_mode_t dragonfly_gaze_get_mode(const dragonfly_gaze_t gaze) {
    if (!gaze) return GAZE_MODE_FREE;
    return gaze->mode;
}

bool dragonfly_gaze_is_locked(const dragonfly_gaze_t gaze) {
    if (!gaze) {
        return false;
    }
    return gaze->locked;
}

float dragonfly_gaze_get_error(const dragonfly_gaze_t gaze) {
    if (!gaze) return 0.0f;
    return gaze->stats.avg_tracking_error_rad;
}

int dragonfly_gaze_get_stats(
    const dragonfly_gaze_t gaze,
    gaze_stats_t* stats
) {
    if (!gaze || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_gaze_get_stats: required parameter is NULL (gaze, stats)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)gaze->mutex);
    *stats = gaze->stats;
    nimcp_mutex_unlock((nimcp_mutex_t*)gaze->mutex);

    return 0;
}
