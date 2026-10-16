/**
 * @file nimcp_dragonfly_tracking.c
 * @brief CSTMD1-Inspired Target Tracking Implementation
 *
 * WHAT: Implements selective attention and target tracking
 * WHY:  Based on dragonfly CSTMD1 neuron for winner-take-all tracking
 * HOW:  State machine + Kalman filter + attention modulation
 *
 * @author NIMCP Team
 * @date 2024-12-27
 */

#include "dragonfly/nimcp_dragonfly_tracking.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/*=============================================================================
 * Health Agent Forward Declarations (Phase 8: Heartbeat for Long Operations)
 *===========================================================================*/
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_tracking)

#include <stddef.h>  /* for NULL */
//=============================================================================
// Constants
//=============================================================================

#define KALMAN_STATE_DIM 6    /* x, y, z, vx, vy, vz */
#define KALMAN_MEAS_DIM  3    /* x, y, z */

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

static inline float vec3_distance(const float a[3], const float b[3]) {
    float dx = a[0] - b[0];
    float dy = a[1] - b[1];
    float dz = a[2] - b[2];
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Position history entry
 */
typedef struct {
    float position[3];
    float velocity[3];
    uint64_t timestamp_us;
} history_entry_t;

/**
 * @brief Simple Kalman filter state (constant velocity model)
 */
typedef struct {
    float state[KALMAN_STATE_DIM];      /* x, y, z, vx, vy, vz */
    float P[KALMAN_STATE_DIM][KALMAN_STATE_DIM];  /* Covariance */
    float Q[KALMAN_STATE_DIM];          /* Process noise (diagonal) */
    float R[KALMAN_MEAS_DIM];           /* Measurement noise (diagonal) */
    bool initialized;
} kalman_filter_t;

/**
 * @brief Internal tracker structure
 */
struct dragonfly_tracker_s {
    /* Configuration */
    tracking_config_t config;

    /* Current state */
    track_state_t state;
    tracked_target_t target;

    /* Kalman filter */
    kalman_filter_t kalman;

    /* Position history */
    history_entry_t history[TRACKER_HISTORY_SIZE];
    uint32_t history_head;
    uint32_t history_count;

    /* State machine timing */
    uint64_t state_enter_us;
    uint64_t acquisition_start_us;

    /* Statistics */
    tracking_stats_t stats;
    uint64_t total_lock_time_us;
    uint32_t lock_count;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Timing */
    uint64_t last_update_us;
    uint64_t creation_time_us;
};

//=============================================================================
// Kalman Filter Functions
//=============================================================================

static void kalman_init(kalman_filter_t* kf, const tracking_config_t* config) {
    memset(kf, 0, sizeof(*kf));

    /* Initialize covariance with high uncertainty */
    for (int i = 0; i < KALMAN_STATE_DIM; i++) {
        kf->P[i][i] = 100.0f;
    }

    /* Process noise */
    float q_pos = config->process_noise;
    float q_vel = config->process_noise * 10.0f;
    kf->Q[0] = q_pos; kf->Q[1] = q_pos; kf->Q[2] = q_pos;
    kf->Q[3] = q_vel; kf->Q[4] = q_vel; kf->Q[5] = q_vel;

    /* Measurement noise */
    kf->R[0] = config->measurement_noise;
    kf->R[1] = config->measurement_noise;
    kf->R[2] = config->measurement_noise;
}

static void kalman_init_state(
    kalman_filter_t* kf,
    const float position[3],
    const float velocity[3]
) {
    kf->state[0] = position[0];
    kf->state[1] = position[1];
    kf->state[2] = position[2];
    if (velocity) {
        kf->state[3] = velocity[0];
        kf->state[4] = velocity[1];
        kf->state[5] = velocity[2];
    } else {
        kf->state[3] = 0.0f;
        kf->state[4] = 0.0f;
        kf->state[5] = 0.0f;
    }
    kf->initialized = true;
}

static void kalman_predict(kalman_filter_t* kf, float dt) {
    if (!kf->initialized) return;

    /* State prediction: x = F * x (constant velocity model) */
    kf->state[0] += kf->state[3] * dt;
    kf->state[1] += kf->state[4] * dt;
    kf->state[2] += kf->state[5] * dt;

    /* Covariance prediction: P = F * P * F' + Q */
    /* Simplified: just add process noise */
    for (int i = 0; i < KALMAN_STATE_DIM; i++) {
        kf->P[i][i] += kf->Q[i] * dt * dt;
    }
}

static void kalman_update(kalman_filter_t* kf, const float measurement[3]) {
    if (!kf->initialized) return;

    /* Innovation: y = z - H * x */
    float y[KALMAN_MEAS_DIM];
    y[0] = measurement[0] - kf->state[0];
    y[1] = measurement[1] - kf->state[1];
    y[2] = measurement[2] - kf->state[2];

    /* Innovation covariance: S = H * P * H' + R */
    float S[KALMAN_MEAS_DIM];
    S[0] = kf->P[0][0] + kf->R[0];
    S[1] = kf->P[1][1] + kf->R[1];
    S[2] = kf->P[2][2] + kf->R[2];

    /* Kalman gain: K = P * H' * inv(S) */
    float K[KALMAN_STATE_DIM][KALMAN_MEAS_DIM];
    for (int i = 0; i < KALMAN_STATE_DIM; i++) {
        K[i][0] = (S[0] > 1e-10f) ? kf->P[i][0] / S[0] : 0.0f;
        K[i][1] = (S[1] > 1e-10f) ? kf->P[i][1] / S[1] : 0.0f;
        K[i][2] = (S[2] > 1e-10f) ? kf->P[i][2] / S[2] : 0.0f;
    }

    /* State update: x = x + K * y */
    for (int i = 0; i < KALMAN_STATE_DIM; i++) {
        kf->state[i] += K[i][0] * y[0] + K[i][1] * y[1] + K[i][2] * y[2];
    }

    /* Covariance update: P = (I - K * H) * P */
    for (int i = 0; i < KALMAN_STATE_DIM; i++) {
        for (int j = 0; j < KALMAN_MEAS_DIM; j++) {
            kf->P[i][j] *= (1.0f - K[i][j]);
        }
    }
}

static void kalman_get_state(
    const kalman_filter_t* kf,
    float position[3],
    float velocity[3]
) {
    if (position) {
        position[0] = kf->state[0];
        position[1] = kf->state[1];
        position[2] = kf->state[2];
    }
    if (velocity) {
        velocity[0] = kf->state[3];
        velocity[1] = kf->state[4];
        velocity[2] = kf->state[5];
    }
}

//=============================================================================
// Configuration Functions
//=============================================================================

tracking_config_t tracking_default_config(void) {
    tracking_config_t config = {
        /* Lock thresholds */
        .acquisition_threshold = 0.3f,
        .lock_threshold = 0.7f,
        .break_threshold = 0.2f,

        /* Timing */
        .acquisition_time_ms = 50.0f,
        .prediction_horizon_ms = 100.0f,
        .max_occlusion_ms = 500,
        .lost_timeout_ms = 100.0f,

        /* Attention */
        .distractor_suppression = 0.8f,
        .attention_radius = 30.0f,
        .enable_motion_camouflage = true,

        /* Filter */
        .process_noise = 0.1f,
        .measurement_noise = 1.0f,
        .motion_model = MOTION_MODEL_CONSTANT_VELOCITY,

        /* Size selectivity - biological range ~1-10 degrees */
        .min_target_size = 0.01f,   /* ~0.5 degrees in radians */
        .max_target_size = 0.2f,    /* ~11 degrees in radians */
        .optimal_target_size = 0.05f /* ~3 degrees in radians */
    };
    return config;
}

bool tracking_validate_config(const tracking_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tracking_validate_config: config is NULL");
        return false;
    }

    if (config->acquisition_threshold < 0.0f ||
        config->acquisition_threshold > 1.0f) return false;
    if (config->lock_threshold < 0.0f ||
        config->lock_threshold > 1.0f) return false;
    if (config->break_threshold < 0.0f ||
        config->break_threshold > 1.0f) return false;
    if (config->lock_threshold < config->acquisition_threshold) {
        return false;
    }
    if (config->break_threshold > config->lock_threshold) {
        return false;
    }

    if (config->prediction_horizon_ms < 0.0f) {
        return false;
    }
    if (config->max_occlusion_ms == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tracking_validate_config: config->max_occlusion_ms is zero");
        return false;
    }

    if (config->distractor_suppression < 0.0f ||
        config->distractor_suppression > 1.0f) return false;
    if (config->attention_radius <= 0.0f) {
        return false;
    }

    if (config->process_noise < 0.0f) {
        return false;
    }
    if (config->measurement_noise < 0.0f) {
        return false;
    }

    if (config->min_target_size < 0.0f) {
        return false;
    }
    if (config->max_target_size < config->min_target_size) {
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

dragonfly_tracker_t* dragonfly_tracker_create(const tracking_config_t* config) {
    tracking_config_t cfg = config ? *config : tracking_default_config();

    if (!tracking_validate_config(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_tracker_create: invalid config");
        return NULL;
    }

    dragonfly_tracker_t* tracker = nimcp_calloc(1, sizeof(dragonfly_tracker_t));
    NIMCP_API_CHECK_ALLOC(tracker, "dragonfly_tracker_create: failed to allocate tracker");

    tracker->config = cfg;
    tracker->state = TRACK_STATE_SEARCHING;
    tracker->creation_time_us = get_time_us();
    tracker->state_enter_us = tracker->creation_time_us;

    kalman_init(&tracker->kalman, &cfg);

    tracker->mutex = nimcp_mutex_create(NULL);
    if (!tracker->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "dragonfly_tracker_create: failed to create mutex");
        nimcp_free(tracker);
        return NULL;
    }

    return tracker;
}

void dragonfly_tracker_destroy(dragonfly_tracker_t* tracker) {
    if (!tracker) return;

    if (tracker->mutex) {
        nimcp_mutex_destroy(tracker->mutex);
    }

    nimcp_free(tracker);
}

int dragonfly_tracker_reset(dragonfly_tracker_t* tracker) {
    if (!tracker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_tracker_reset: tracker is NULL");
        return -1;
    }

    nimcp_mutex_lock(tracker->mutex);

    tracker->state = TRACK_STATE_SEARCHING;
    tracker->state_enter_us = get_time_us();
    memset(&tracker->target, 0, sizeof(tracker->target));
    memset(&tracker->history, 0, sizeof(tracker->history));
    tracker->history_head = 0;
    tracker->history_count = 0;
    kalman_init(&tracker->kalman, &tracker->config);

    nimcp_mutex_unlock(tracker->mutex);

    return 0;
}

//=============================================================================
// Internal State Machine Functions
//=============================================================================

static void change_state(dragonfly_tracker_t* tracker, track_state_t new_state) {
    if (tracker->state == new_state) return;

    uint64_t now = get_time_us();

    /* Handle exit actions */
    if (tracker->state == TRACK_STATE_LOCKED) {
        uint64_t lock_duration = now - tracker->state_enter_us;
        tracker->total_lock_time_us += lock_duration;
        tracker->lock_count++;
        tracker->stats.lock_breaks++;
    }

    /* Handle entry actions */
    if (new_state == TRACK_STATE_LOCKED) {
        tracker->stats.successful_locks++;
        tracker->target.lock_time_us = 0;
    } else if (new_state == TRACK_STATE_PREDICTING) {
        tracker->stats.prediction_uses++;
    }

    tracker->state = new_state;
    tracker->state_enter_us = now;
    tracker->target.state = new_state;
}

static void add_to_history(
    dragonfly_tracker_t* tracker,
    const float position[3],
    const float velocity[3]
) {
    history_entry_t* entry = &tracker->history[tracker->history_head];
    memcpy(entry->position, position, sizeof(entry->position));
    if (velocity) {
        memcpy(entry->velocity, velocity, sizeof(entry->velocity));
    } else {
        memset(entry->velocity, 0, sizeof(entry->velocity));
    }
    entry->timestamp_us = get_time_us();

    tracker->history_head = (tracker->history_head + 1) % TRACKER_HISTORY_SIZE;
    if (tracker->history_count < TRACKER_HISTORY_SIZE) {
        tracker->history_count++;
    }
}

static const target_observation_t* select_best_target(
    dragonfly_tracker_t* tracker,
    const target_observation_t* observations,
    uint32_t num_observations
) {
    if (num_observations == 0) {
        return NULL;  /* No observations - normal condition */
    }

    const target_observation_t* best = NULL;
    float best_score = -1.0f;

    for (uint32_t i = 0; i < num_observations; i++) {
        const target_observation_t* obs = &observations[i];

        /* Skip low-confidence observations */
        if (obs->confidence < tracker->config.acquisition_threshold) continue;

        /* Size selectivity (STMD property) */
        float size_diff = fabsf(obs->size - tracker->config.optimal_target_size);
        float size_range = tracker->config.max_target_size -
                           tracker->config.min_target_size;
        float size_score = 1.0f - (size_diff / size_range);
        size_score = nimcp_clampf(size_score, 0.0f, 1.0f);

        /* If already tracking, prefer the tracked target */
        float id_bonus = 0.0f;
        if (tracker->state == TRACK_STATE_LOCKED ||
            tracker->state == TRACK_STATE_PREDICTING) {
            if (obs->target_id == tracker->target.target_id) {
                id_bonus = 0.5f;
            }
        }

        /* Combined score */
        float score = obs->confidence * size_score + id_bonus;

        if (score > best_score) {
            best_score = score;
            best = obs;
        }
    }

    return best;
}

static void update_acceleration_estimate(dragonfly_tracker_t* tracker) {
    if (tracker->history_count < 3) {
        memset(tracker->target.acceleration, 0,
               sizeof(tracker->target.acceleration));
        return;
    }

    /* Get three most recent velocity samples */
    uint32_t idx1 = (tracker->history_head + TRACKER_HISTORY_SIZE - 1) %
                    TRACKER_HISTORY_SIZE;
    uint32_t idx2 = (tracker->history_head + TRACKER_HISTORY_SIZE - 2) %
                    TRACKER_HISTORY_SIZE;

    const history_entry_t* h1 = &tracker->history[idx1];
    const history_entry_t* h2 = &tracker->history[idx2];

    float dt = (float)(h1->timestamp_us - h2->timestamp_us) / 1000000.0f;
    if (dt > 0.001f) {
        tracker->target.acceleration[0] = (h1->velocity[0] - h2->velocity[0]) / dt;
        tracker->target.acceleration[1] = (h1->velocity[1] - h2->velocity[1]) / dt;
        tracker->target.acceleration[2] = (h1->velocity[2] - h2->velocity[2]) / dt;
    }
}

static void check_motion_camouflage(
    dragonfly_tracker_t* tracker,
    const float self_position[3]
) {
    if (!tracker->config.enable_motion_camouflage) {
        tracker->target.is_approaching = false;
        tracker->target.time_to_contact = -1.0f;
        return;
    }

    /* Motion camouflage: target maintains constant bearing angle */
    /* For now, simple approach detection: negative range rate */
    float rel_pos[3] = {
        tracker->target.position[0] - (self_position ? self_position[0] : 0.0f),
        tracker->target.position[1] - (self_position ? self_position[1] : 0.0f),
        tracker->target.position[2] - (self_position ? self_position[2] : 0.0f)
    };

    float range = vec3_length(rel_pos);
    if (range < 0.001f) {
        tracker->target.is_approaching = true;
        tracker->target.time_to_contact = 0.0f;
        return;
    }

    /* Compute range rate (dot product of relative velocity and unit range) */
    float unit_range[3] = {
        rel_pos[0] / range,
        rel_pos[1] / range,
        rel_pos[2] / range
    };
    float range_rate = tracker->target.velocity[0] * unit_range[0] +
                       tracker->target.velocity[1] * unit_range[1] +
                       tracker->target.velocity[2] * unit_range[2];

    /* Negative range rate = approaching */
    tracker->target.is_approaching = (range_rate < 0.0f);

    if (tracker->target.is_approaching && range_rate < -0.001f) {
        tracker->target.time_to_contact = -range / range_rate;
    } else {
        tracker->target.time_to_contact = -1.0f;
    }
}

//=============================================================================
// Core Tracking Functions
//=============================================================================

int dragonfly_tracker_update(
    dragonfly_tracker_t* tracker,
    const target_observation_t* observations,
    uint32_t num_observations,
    float dt
) {
    /* Phase 8: Send heartbeat at start of tracking update */
    dragonfly_tracking_heartbeat("tracker_update", 0.0f);

    if (!tracker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_tracker_update: tracker is NULL");
        return -1;
    }
    if (num_observations > 0 && !observations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_tracker_update: observations is NULL");
        return -1;
    }
    if (dt <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_tracker_update: invalid dt");
        return -1;
    }

    nimcp_mutex_lock(tracker->mutex);

    uint64_t now = get_time_us();
    tracker->last_update_us = now;
    tracker->stats.total_observations += num_observations;

    /* Select best target from observations */
    const target_observation_t* best = select_best_target(
        tracker, observations, num_observations);

    /* Kalman prediction step */
    kalman_predict(&tracker->kalman, dt);

    /* State machine update */
    switch (tracker->state) {
        case TRACK_STATE_SEARCHING:
            if (best && best->confidence >= tracker->config.acquisition_threshold) {
                /* Start acquiring */
                tracker->target.target_id = best->target_id;
                memcpy(tracker->target.position, best->position,
                       sizeof(tracker->target.position));
                memcpy(tracker->target.velocity, best->velocity,
                       sizeof(tracker->target.velocity));
                tracker->target.confidence = best->confidence;
                tracker->target.size = best->size;
                tracker->target.observations = 1;

                kalman_init_state(&tracker->kalman, best->position, best->velocity);
                tracker->acquisition_start_us = now;

                if (best->confidence >= tracker->config.lock_threshold) {
                    change_state(tracker, TRACK_STATE_LOCKED);
                } else {
                    change_state(tracker, TRACK_STATE_ACQUIRING);
                }
            }
            break;

        case TRACK_STATE_ACQUIRING:
            if (best && best->target_id == tracker->target.target_id) {
                /* Update target */
                kalman_update(&tracker->kalman, best->position);
                kalman_get_state(&tracker->kalman,
                                 tracker->target.position,
                                 tracker->target.velocity);
                tracker->target.confidence = best->confidence;
                tracker->target.last_seen_us = now;
                tracker->target.observations++;

                /* Check lock threshold */
                float acq_time = (now - tracker->acquisition_start_us) / 1000.0f;
                if (best->confidence >= tracker->config.lock_threshold &&
                    acq_time >= tracker->config.acquisition_time_ms) {
                    change_state(tracker, TRACK_STATE_LOCKED);
                }
            } else {
                /* Lost during acquisition */
                change_state(tracker, TRACK_STATE_SEARCHING);
            }
            break;

        case TRACK_STATE_LOCKED:
            if (best && best->target_id == tracker->target.target_id) {
                /* Normal tracking update */
                kalman_update(&tracker->kalman, best->position);
                kalman_get_state(&tracker->kalman,
                                 tracker->target.position,
                                 tracker->target.velocity);
                tracker->target.confidence = best->confidence;
                tracker->target.last_seen_us = now;
                tracker->target.observations++;
                tracker->target.lock_time_us += (uint64_t)(dt * 1000000.0f);

                add_to_history(tracker, tracker->target.position,
                               tracker->target.velocity);
                update_acceleration_estimate(tracker);
                check_motion_camouflage(tracker, NULL);

                if (best->confidence < tracker->config.break_threshold) {
                    change_state(tracker, TRACK_STATE_LOST);
                }
            } else if (best == NULL) {
                /* Target occluded, switch to prediction */
                change_state(tracker, TRACK_STATE_PREDICTING);
            } else {
                /* Different target observed, keep locked on original */
                tracker->stats.distractors_suppressed++;

                /* But check if original confidence is dropping */
                tracker->target.confidence *= 0.95f;
                if (tracker->target.confidence < tracker->config.break_threshold) {
                    change_state(tracker, TRACK_STATE_LOST);
                }
            }
            break;

        case TRACK_STATE_PREDICTING:
            {
                /* Use predicted position from Kalman */
                kalman_get_state(&tracker->kalman,
                                 tracker->target.position,
                                 tracker->target.velocity);

                /* Decrease confidence during prediction */
                tracker->target.confidence *= 0.9f;

                uint64_t occlusion_time = now - tracker->state_enter_us;
                float occlusion_ms = occlusion_time / 1000.0f;

                if (best && best->target_id == tracker->target.target_id) {
                    /* Reacquired! */
                    kalman_update(&tracker->kalman, best->position);
                    tracker->target.confidence = best->confidence;
                    tracker->target.last_seen_us = now;
                    tracker->stats.reacquisitions++;
                    change_state(tracker, TRACK_STATE_LOCKED);
                } else if (occlusion_ms > tracker->config.max_occlusion_ms) {
                    /* Occlusion timeout */
                    change_state(tracker, TRACK_STATE_LOST);
                }
            }
            break;

        case TRACK_STATE_LOST:
            {
                float lost_time = (now - tracker->state_enter_us) / 1000.0f;
                if (lost_time >= tracker->config.lost_timeout_ms) {
                    change_state(tracker, TRACK_STATE_SEARCHING);
                    memset(&tracker->target, 0, sizeof(tracker->target));
                    tracker->kalman.initialized = false;
                }
            }
            break;
    }

    /* Update statistics */
    if (tracker->lock_count > 0) {
        tracker->stats.avg_lock_duration_ms =
            (float)tracker->total_lock_time_us / (float)tracker->lock_count / 1000.0f;
    }
    if (tracker->state == TRACK_STATE_LOCKED ||
        tracker->state == TRACK_STATE_PREDICTING) {
        float alpha = 0.1f;
        tracker->stats.avg_confidence =
            (1.0f - alpha) * tracker->stats.avg_confidence +
            alpha * tracker->target.confidence;
    }

    nimcp_mutex_unlock(tracker->mutex);

    return 0;
}

int dragonfly_tracker_predict(
    dragonfly_tracker_t* tracker,
    float lookahead_ms,
    float position[3],
    float velocity[3]
) {
    if (!tracker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_tracker_predict: tracker is NULL");
        return -1;
    }
    if (!position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_tracker_predict: position is NULL");
        return -1;
    }
    if (lookahead_ms < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_tracker_predict: invalid lookahead_ms");
        return -1;
    }

    nimcp_mutex_lock(tracker->mutex);

    if (tracker->state != TRACK_STATE_LOCKED &&
        tracker->state != TRACK_STATE_PREDICTING) {
        nimcp_mutex_unlock(tracker->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_tracker_predict: operation failed");
        return -1;
    }

    float dt = lookahead_ms / 1000.0f;

    /* Simple constant velocity prediction */
    position[0] = tracker->target.position[0] + tracker->target.velocity[0] * dt;
    position[1] = tracker->target.position[1] + tracker->target.velocity[1] * dt;
    position[2] = tracker->target.position[2] + tracker->target.velocity[2] * dt;

    /* Add acceleration term if available */
    if (tracker->history_count >= 3) {
        float half_dt_sq = 0.5f * dt * dt;
        position[0] += tracker->target.acceleration[0] * half_dt_sq;
        position[1] += tracker->target.acceleration[1] * half_dt_sq;
        position[2] += tracker->target.acceleration[2] * half_dt_sq;
    }

    if (velocity) {
        velocity[0] = tracker->target.velocity[0] +
                      tracker->target.acceleration[0] * dt;
        velocity[1] = tracker->target.velocity[1] +
                      tracker->target.acceleration[1] * dt;
        velocity[2] = tracker->target.velocity[2] +
                      tracker->target.acceleration[2] * dt;
    }

    nimcp_mutex_unlock(tracker->mutex);

    return 0;
}

int dragonfly_tracker_force_lock(
    dragonfly_tracker_t* tracker,
    uint32_t target_id
) {
    if (!tracker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_tracker_force_lock: tracker is NULL");
        return -1;
    }

    nimcp_mutex_lock(tracker->mutex);

    /* This would typically be used with a fresh observation */
    /* For now, just set the target ID and change state */
    tracker->target.target_id = target_id;
    change_state(tracker, TRACK_STATE_LOCKED);

    nimcp_mutex_unlock(tracker->mutex);

    return 0;
}

int dragonfly_tracker_break_lock(dragonfly_tracker_t* tracker) {
    if (!tracker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_tracker_break_lock: tracker is NULL");
        return -1;
    }

    nimcp_mutex_lock(tracker->mutex);

    change_state(tracker, TRACK_STATE_LOST);

    nimcp_mutex_unlock(tracker->mutex);

    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

const tracked_target_t* dragonfly_tracker_get_target(
    const dragonfly_tracker_t* tracker
) {
    if (!tracker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_tracker_get_target: tracker is NULL");
        return NULL;
    }

    if (tracker->state == TRACK_STATE_SEARCHING) {
        return NULL;  /* Not tracking - normal condition */
    }

    return &tracker->target;
}

track_state_t dragonfly_tracker_get_state(const dragonfly_tracker_t* tracker) {
    if (!tracker) return TRACK_STATE_SEARCHING;
    return tracker->state;
}

float dragonfly_tracker_get_gain(
    const dragonfly_tracker_t* tracker,
    uint32_t target_id
) {
    if (!tracker) return 0.0f;

    if (tracker->state != TRACK_STATE_LOCKED &&
        tracker->state != TRACK_STATE_PREDICTING) {
        return 1.0f;  /* No suppression when not tracking */
    }

    if (target_id == tracker->target.target_id) {
        return 1.0f;  /* Full attention on locked target */
    }

    /* Distractor suppression */
    return 1.0f - tracker->config.distractor_suppression;
}

int dragonfly_tracker_get_stats(
    const dragonfly_tracker_t* tracker,
    tracking_stats_t* stats
) {
    if (!tracker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_tracker_get_stats: tracker is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_tracker_get_stats: stats is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)tracker->mutex);
    *stats = tracker->stats;
    nimcp_mutex_unlock((nimcp_mutex_t*)tracker->mutex);

    return 0;
}

int dragonfly_tracker_reset_stats(dragonfly_tracker_t* tracker) {
    if (!tracker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_tracker_reset_stats: tracker is NULL");
        return -1;
    }

    nimcp_mutex_lock(tracker->mutex);
    memset(&tracker->stats, 0, sizeof(tracker->stats));
    tracker->total_lock_time_us = 0;
    tracker->lock_count = 0;
    nimcp_mutex_unlock(tracker->mutex);

    return 0;
}

bool dragonfly_tracker_is_locked(const dragonfly_tracker_t* tracker) {
    if (!tracker) {
        return false;
    }
    return tracker->state == TRACK_STATE_LOCKED ||
           tracker->state == TRACK_STATE_PREDICTING;
}

uint32_t dragonfly_tracker_get_locked_id(const dragonfly_tracker_t* tracker) {
    if (!tracker) return 0;
    if (!dragonfly_tracker_is_locked(tracker)) return 0;
    return tracker->target.target_id;
}

//=============================================================================
// Configuration Functions
//=============================================================================

int dragonfly_tracker_set_config(
    dragonfly_tracker_t* tracker,
    const tracking_config_t* config
) {
    if (!tracker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_tracker_set_config: tracker is NULL");
        return -1;
    }
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_tracker_set_config: config is NULL");
        return -1;
    }
    if (!tracking_validate_config(config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_tracker_set_config: invalid config");
        return -1;
    }

    nimcp_mutex_lock(tracker->mutex);
    tracker->config = *config;
    nimcp_mutex_unlock(tracker->mutex);

    return 0;
}

int dragonfly_tracker_get_config(
    const dragonfly_tracker_t* tracker,
    tracking_config_t* config
) {
    if (!tracker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_tracker_get_config: tracker is NULL");
        return -1;
    }
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_tracker_get_config: config is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)tracker->mutex);
    *config = tracker->config;
    nimcp_mutex_unlock((nimcp_mutex_t*)tracker->mutex);

    return 0;
}

//=============================================================================
// Advanced Functions
//=============================================================================

int dragonfly_tracker_get_history(
    const dragonfly_tracker_t* tracker,
    float* positions,
    uint32_t max_positions,
    uint32_t* count
) {
    if (!tracker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_tracker_get_history: tracker is NULL");
        return -1;
    }
    if (!positions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_tracker_get_history: positions is NULL");
        return -1;
    }
    if (!count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_tracker_get_history: count is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)tracker->mutex);

    uint32_t n = tracker->history_count;
    if (n > max_positions) n = max_positions;

    uint32_t start_idx = (tracker->history_head + TRACKER_HISTORY_SIZE - n) %
                         TRACKER_HISTORY_SIZE;

    for (uint32_t i = 0; i < n; i++) {
        uint32_t idx = (start_idx + i) % TRACKER_HISTORY_SIZE;
        positions[i * 3 + 0] = tracker->history[idx].position[0];
        positions[i * 3 + 1] = tracker->history[idx].position[1];
        positions[i * 3 + 2] = tracker->history[idx].position[2];
    }

    *count = n;

    nimcp_mutex_unlock((nimcp_mutex_t*)tracker->mutex);

    return 0;
}

float dragonfly_tracker_get_prediction_confidence(
    const dragonfly_tracker_t* tracker
) {
    if (!tracker) return 0.0f;

    if (tracker->state == TRACK_STATE_LOCKED) {
        return tracker->target.confidence;
    } else if (tracker->state == TRACK_STATE_PREDICTING) {
        /* Confidence decays during prediction */
        uint64_t now = get_time_us();
        uint64_t pred_time = now - tracker->state_enter_us;
        float pred_ms = pred_time / 1000.0f;
        float decay = expf(-pred_ms / tracker->config.prediction_horizon_ms);
        return tracker->target.confidence * decay;
    }

    return 0.0f;
}

int dragonfly_tracker_set_external_velocity(
    dragonfly_tracker_t* tracker,
    const float velocity[3],
    float confidence
) {
    if (!tracker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_tracker_set_external_velocity: tracker is NULL");
        return -1;
    }
    if (!velocity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_tracker_set_external_velocity: velocity is NULL");
        return -1;
    }
    if (confidence < 0.0f || confidence > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_tracker_set_external_velocity: invalid confidence");
        return -1;
    }

    nimcp_mutex_lock(tracker->mutex);

    if (!tracker->kalman.initialized) {
        nimcp_mutex_unlock(tracker->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_tracker_set_external_velocity: tracker->kalman is NULL");
        return -1;
    }

    /* Blend external velocity with internal estimate */
    float alpha = confidence;
    tracker->kalman.state[3] = (1.0f - alpha) * tracker->kalman.state[3] +
                               alpha * velocity[0];
    tracker->kalman.state[4] = (1.0f - alpha) * tracker->kalman.state[4] +
                               alpha * velocity[1];
    tracker->kalman.state[5] = (1.0f - alpha) * tracker->kalman.state[5] +
                               alpha * velocity[2];

    /* Also update target state */
    tracker->target.velocity[0] = tracker->kalman.state[3];
    tracker->target.velocity[1] = tracker->kalman.state[4];
    tracker->target.velocity[2] = tracker->kalman.state[5];

    nimcp_mutex_unlock(tracker->mutex);

    return 0;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* dragonfly_tracker_state_name(track_state_t state) {
    switch (state) {
        case TRACK_STATE_SEARCHING:  return "SEARCHING";
        case TRACK_STATE_ACQUIRING:  return "ACQUIRING";
        case TRACK_STATE_LOCKED:     return "LOCKED";
        case TRACK_STATE_PREDICTING: return "PREDICTING";
        case TRACK_STATE_LOST:       return "LOST";
        default:                     return "UNKNOWN";
    }
}
