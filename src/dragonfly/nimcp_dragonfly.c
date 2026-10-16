/**
 * @file nimcp_dragonfly.c
 * @brief Main Dragonfly Coordinator Implementation
 *
 * Integrates TSDN, tracking, prediction, and interception subsystems
 * into a unified dragonfly-inspired hunting pipeline.
 */

#include "dragonfly/nimcp_dragonfly.h"
#include "utils/thread/nimcp_thread.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdio.h>
#include "utils/exception/nimcp_exception_macros.h"

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Target tracking entry
 */
typedef struct {
    uint32_t id;
    bool active;
    dragonfly_target_info_t info;
    uint64_t first_seen_us;
    uint64_t last_seen_us;
} target_entry_t;

/**
 * @brief Dragonfly system state
 */
struct dragonfly_system_s {
    /* Configuration */
    dragonfly_config_t config;

    /* Subsystems */
    tsdn_population_t* tsdn;
    dragonfly_tracker_t* tracker;
    dragonfly_predictor_t* predictor;
    dragonfly_interceptor_t* interceptor;

    /* State */
    dragonfly_mode_t mode;
    dragonfly_hunt_result_t hunt_result;
    uint32_t primary_target_id;
    bool hunt_concluded;

    /* Self state */
    dragonfly_self_state_t self_state;
    bool self_state_valid;

    /* Targets */
    target_entry_t targets[DRAGONFLY_MAX_TARGETS];
    uint32_t num_active_targets;

    /* Current outputs */
    dragonfly_motor_cmd_t current_cmd;
    intercept_solution_t current_solution;
    trajectory_prediction_t current_prediction;
    tsdn_vector_t current_tsdn;

    /* Timing */
    uint64_t pursuit_start_us;
    uint64_t last_update_us;
    float total_pursuit_time_s;

    /* Statistics */
    dragonfly_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

//=============================================================================
// Helper Functions
//=============================================================================

static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static float vec3_magnitude(const float v[3]) {
    return sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static float vec3_distance(const float a[3], const float b[3]) {
    float d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
    return vec3_magnitude(d);
}

static target_entry_t* find_target(dragonfly_system_t* system, uint32_t id) {
    for (uint32_t i = 0; i < DRAGONFLY_MAX_TARGETS; i++) {
        if (system->targets[i].active && system->targets[i].id == id) {
            return &system->targets[i];
        }
    }
    return NULL;  /* Not found - normal search miss */
}

static target_entry_t* find_free_slot(dragonfly_system_t* system) {
    for (uint32_t i = 0; i < DRAGONFLY_MAX_TARGETS; i++) {
        if (!system->targets[i].active) {
            return &system->targets[i];
        }
    }
    return NULL;  /* No free slot available */
}

static target_entry_t* get_primary_target_entry(dragonfly_system_t* system) {
    return find_target(system, system->primary_target_id);
}

//=============================================================================
// Configuration Functions
//=============================================================================

dragonfly_config_t dragonfly_default_config(void) {
    dragonfly_config_t config;
    memset(&config, 0, sizeof(config));

    /* Subsystem configs */
    tsdn_config_default(&config.tsdn_config);
    config.tracker_config = tracking_default_config();
    config.prediction_config = prediction_default_config();
    config.intercept_config = intercept_default_config();

    /* System parameters */
    config.min_target_size = 0.01f;       /* ~0.5 degrees */
    config.max_target_distance = 100.0f;  /* meters */
    config.abort_distance = 200.0f;       /* meters */
    config.intercept_threshold = 0.5f;    /* meters for success */
    config.pursuit_timeout_s = 30.0f;     /* 30 second max pursuit */

    /* Mode transitions - lock threshold must be >= pursue threshold */
    config.lock_threshold = 0.85f;
    config.pursue_threshold = 0.7f;
    config.abort_threshold = 0.2f;

    /* Energy management */
    config.energy_aware = true;
    config.min_energy_reserve = 0.1f;

    return config;
}

bool dragonfly_validate_config(const dragonfly_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_validate_config: config is NULL");
        return false;
    }

    /* Validate subsystem configs */
    if (tsdn_config_validate(&config->tsdn_config) != 0) {
        return false;
    }
    if (!tracking_validate_config(&config->tracker_config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_validate_config: tracking_validate_config is NULL");
        return false;
    }
    if (!prediction_validate_config(&config->prediction_config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_validate_config: prediction_validate_config is NULL");
        return false;
    }
    if (!intercept_validate_config(&config->intercept_config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_validate_config: intercept_validate_config is NULL");
        return false;
    }

    /* Validate system parameters */
    if (config->min_target_size <= 0.0f) {
        return false;
    }
    if (config->max_target_distance <= 0.0f) {
        return false;
    }
    if (config->abort_distance <= config->max_target_distance) {
        return false;
    }
    if (config->intercept_threshold <= 0.0f) {
        return false;
    }
    if (config->pursuit_timeout_s <= 0.0f) {
        return false;
    }

    /* Validate thresholds */
    if (config->lock_threshold < 0.0f || config->lock_threshold > 1.0f) {
        return false;
    }
    if (config->pursue_threshold < 0.0f || config->pursue_threshold > 1.0f) {
        return false;
    }
    if (config->abort_threshold < 0.0f || config->abort_threshold > 1.0f) {
        return false;
    }

    /* Validate energy */
    if (config->min_energy_reserve < 0.0f || config->min_energy_reserve > 1.0f) {
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

dragonfly_system_t* dragonfly_system_create(const dragonfly_config_t* config) {
    dragonfly_config_t default_config;
    if (!config) {
        default_config = dragonfly_default_config();
        config = &default_config;
    }

    if (!dragonfly_validate_config(config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_system_create: invalid config");
        return NULL;
    }

    dragonfly_system_t* system = (dragonfly_system_t*)nimcp_calloc(1, sizeof(dragonfly_system_t));
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dragonfly_system_create: failed to allocate system");
        return NULL;
    }

    system->config = *config;

    /* Create mutex */
    system->mutex = nimcp_mutex_create(NULL);
    if (!system->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "dragonfly_system_create: failed to create mutex");
        nimcp_free(system);
        return NULL;
    }

    /* Create subsystems */
    system->tsdn = tsdn_create(&config->tsdn_config);
    if (!system->tsdn) goto error;

    system->tracker = dragonfly_tracker_create(&config->tracker_config);
    if (!system->tracker) goto error;

    system->predictor = dragonfly_predictor_create(&config->prediction_config);
    if (!system->predictor) goto error;

    system->interceptor = dragonfly_interceptor_create(&config->intercept_config);
    if (!system->interceptor) goto error;

    /* Initialize state */
    system->mode = DRAGONFLY_MODE_IDLE;
    system->hunt_result = DRAGONFLY_HUNT_IN_PROGRESS;
    system->hunt_concluded = false;
    system->self_state_valid = false;
    system->num_active_targets = 0;

    return system;

error:
    dragonfly_system_destroy(system);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "dragonfly_system_create: subsystem creation failed");
    return NULL;
}

void dragonfly_system_destroy(dragonfly_system_t* system) {
    if (!system) return;

    if (system->interceptor) dragonfly_interceptor_destroy(system->interceptor);
    if (system->predictor) dragonfly_predictor_destroy(system->predictor);
    if (system->tracker) dragonfly_tracker_destroy(system->tracker);
    if (system->tsdn) tsdn_destroy(system->tsdn);
    if (system->mutex) nimcp_mutex_destroy(system->mutex);

    nimcp_free(system);
}

int dragonfly_system_reset(dragonfly_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_system_reset: system is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    /* Reset subsystems */
    tsdn_reset(system->tsdn);
    dragonfly_tracker_reset(system->tracker);
    dragonfly_predictor_reset(system->predictor);
    dragonfly_interceptor_reset(system->interceptor);

    /* Reset state */
    system->mode = DRAGONFLY_MODE_IDLE;
    system->hunt_result = DRAGONFLY_HUNT_IN_PROGRESS;
    system->hunt_concluded = false;
    system->self_state_valid = false;
    system->num_active_targets = 0;
    system->primary_target_id = 0;

    /* Clear targets */
    memset(system->targets, 0, sizeof(system->targets));

    /* Clear outputs */
    memset(&system->current_cmd, 0, sizeof(system->current_cmd));
    memset(&system->current_solution, 0, sizeof(system->current_solution));
    memset(&system->current_prediction, 0, sizeof(system->current_prediction));
    memset(&system->current_tsdn, 0, sizeof(system->current_tsdn));

    /* Reset statistics */
    memset(&system->stats, 0, sizeof(system->stats));

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

//=============================================================================
// Core Pipeline Functions
//=============================================================================

int dragonfly_process_detection(
    dragonfly_system_t* system,
    const dragonfly_detection_t* detection
) {
    if (!system || !detection) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_process_detection: NULL argument");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    system->stats.detections_processed++;

    /* Filter by size and distance */
    float distance = vec3_magnitude(detection->position);
    if (detection->size < system->config.min_target_size ||
        distance > system->config.max_target_distance) {
        nimcp_mutex_unlock(system->mutex);
        return 0;  /* Filtered, not an error */
    }

    /* Update TSDN with direction */
    float direction = atan2f(detection->position[1], detection->position[0]);
    system->current_tsdn = tsdn_encode_direction(system->tsdn, direction);

    /* Create observation for tracker */
    target_observation_t obs;
    memset(&obs, 0, sizeof(obs));
    obs.target_id = detection->id;
    memcpy(obs.position, detection->position, sizeof(obs.position));

    /* Estimate velocity from motion */
    obs.velocity[0] = detection->motion_speed * cosf(detection->motion_direction_rad);
    obs.velocity[1] = detection->motion_speed * sinf(detection->motion_direction_rad);
    obs.velocity[2] = 0.0f;

    obs.size = detection->size;
    obs.contrast = detection->contrast;
    obs.confidence = detection->contrast;  /* Use contrast as confidence */
    obs.timestamp_us = detection->timestamp_us;

    /* Update tracker - using 0.01s as default dt */
    dragonfly_tracker_update(system->tracker, &obs, 1, 0.01f);

    /* Get tracking state */
    track_state_t track_state = dragonfly_tracker_get_state(system->tracker);

    /* Find or create target entry */
    target_entry_t* target = find_target(system, detection->id);
    if (!target) {
        target = find_free_slot(system);
        if (target) {
            target->active = true;
            target->id = detection->id;
            target->first_seen_us = detection->timestamp_us;
            system->num_active_targets++;
        }
    }

    if (target) {
        target->last_seen_us = detection->timestamp_us;
        target->info.id = detection->id;
        target->info.state = track_state;
        memcpy(target->info.position, detection->position, sizeof(target->info.position));
        memcpy(target->info.velocity, obs.velocity, sizeof(target->info.velocity));
        target->info.confidence = detection->contrast;
    }

    /* Mode transitions based on tracking state */
    if (system->mode == DRAGONFLY_MODE_SCANNING ||
        system->mode == DRAGONFLY_MODE_IDLE) {
        if (track_state == TRACK_STATE_LOCKED) {
            system->mode = DRAGONFLY_MODE_TRACKING;
            system->primary_target_id = detection->id;
            system->stats.targets_tracked++;
        }
    }

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int dragonfly_update_self_state(
    dragonfly_system_t* system,
    const dragonfly_self_state_t* self_state
) {
    if (!system || !self_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_update_self_state: NULL argument");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);
    system->self_state = *self_state;
    system->self_state_valid = true;
    nimcp_mutex_unlock(system->mutex);

    return 0;
}

int dragonfly_update(dragonfly_system_t* system, float dt_s) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_update: system is NULL");
        return -1;
    }
    if (dt_s <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_update: invalid dt_s");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    uint64_t start_time = get_time_us();

    /* Check for hunt conclusion */
    if (system->hunt_concluded) {
        nimcp_mutex_unlock(system->mutex);
        return 0;
    }

    /* Mode-specific updates */
    target_entry_t* primary = get_primary_target_entry(system);

    switch (system->mode) {
        case DRAGONFLY_MODE_IDLE:
            /* Nothing to do */
            break;

        case DRAGONFLY_MODE_SCANNING:
            /* Just wait for detections */
            break;

        case DRAGONFLY_MODE_TRACKING:
            if (primary && system->self_state_valid) {
                /* Update prediction */
                dragonfly_predictor_update(system->predictor,
                                          primary->info.position,
                                          primary->info.velocity,
                                          dt_s);
                dragonfly_predictor_predict(system->predictor, 1000.0f, &system->current_prediction);

                primary->info.evasion_type = system->current_prediction.evasion.current_type;

                /* Auto-transition to pursuing if confidence high enough */
                if (primary->info.confidence >= system->config.pursue_threshold) {
                    system->mode = DRAGONFLY_MODE_PURSUING;
                    system->pursuit_start_us = get_time_us();
                    system->stats.pursuits_initiated++;
                }
            }
            break;

        case DRAGONFLY_MODE_PURSUING:
            if (primary && system->self_state_valid) {
                /* Update prediction */
                dragonfly_predictor_update(system->predictor,
                                          primary->info.position,
                                          primary->info.velocity,
                                          dt_s);
                dragonfly_predictor_predict(system->predictor, 1000.0f, &system->current_prediction);

                /* Check for interception opportunity */
                interceptor_state_t self;
                memcpy(self.position, system->self_state.position, sizeof(self.position));
                memcpy(self.velocity, system->self_state.velocity, sizeof(self.velocity));
                self.max_speed = system->self_state.max_speed;
                self.max_accel = system->self_state.max_accel;
                self.max_turn_rate = system->self_state.max_turn_rate;

                target_state_t target;
                memcpy(target.position, primary->info.position, sizeof(target.position));
                memcpy(target.velocity, primary->info.velocity, sizeof(target.velocity));
                memcpy(target.acceleration, primary->info.acceleration, sizeof(target.acceleration));
                target.confidence = primary->info.confidence;

                dragonfly_intercept_compute(system->interceptor, &self, &target,
                                           &system->current_solution);

                primary->info.feasibility = system->current_solution.feasibility;
                memcpy(primary->info.intercept_point, system->current_solution.intercept_point,
                       sizeof(primary->info.intercept_point));
                primary->info.lead_angle_rad = system->current_solution.lead_angle_rad;
                primary->info.time_to_intercept_s = system->current_solution.intercept_time_s;

                /* Generate motor command */
                system->current_cmd.heading_rad = system->current_solution.heading_rad;
                memcpy(system->current_cmd.acceleration, system->current_solution.required_accel,
                       sizeof(system->current_cmd.acceleration));
                system->current_cmd.urgency = primary->info.confidence;
                system->current_cmd.timestamp_us = get_time_us();

                /* Check if close enough to intercept */
                float dist = vec3_distance(system->self_state.position, primary->info.position);
                if (dist < system->config.intercept_threshold) {
                    system->mode = DRAGONFLY_MODE_INTERCEPTING;
                    system->stats.intercepts_attempted++;
                }

                /* Check pursuit timeout */
                float pursuit_time = (float)(get_time_us() - system->pursuit_start_us) / 1000000.0f;
                if (pursuit_time > system->config.pursuit_timeout_s) {
                    system->hunt_result = DRAGONFLY_HUNT_TIMEOUT;
                    system->hunt_concluded = true;
                    system->mode = DRAGONFLY_MODE_IDLE;
                }

                /* Check abort conditions */
                if (primary->info.confidence < system->config.abort_threshold) {
                    system->hunt_result = DRAGONFLY_HUNT_ESCAPED;
                    system->hunt_concluded = true;
                    system->stats.escaped_targets++;
                    system->mode = DRAGONFLY_MODE_IDLE;
                }

                if (dist > system->config.abort_distance) {
                    system->hunt_result = DRAGONFLY_HUNT_ESCAPED;
                    system->hunt_concluded = true;
                    system->stats.escaped_targets++;
                    system->mode = DRAGONFLY_MODE_IDLE;
                }

                /* Check energy */
                if (system->config.energy_aware &&
                    system->self_state.energy_level < system->config.min_energy_reserve) {
                    system->hunt_result = DRAGONFLY_HUNT_ABORTED;
                    system->hunt_concluded = true;
                    system->stats.aborted_pursuits++;
                    system->mode = DRAGONFLY_MODE_IDLE;
                }

                system->total_pursuit_time_s = pursuit_time;
            }
            break;

        case DRAGONFLY_MODE_INTERCEPTING:
            if (primary && system->self_state_valid) {
                float dist = vec3_distance(system->self_state.position, primary->info.position);
                if (dist < system->config.intercept_threshold) {
                    system->hunt_result = DRAGONFLY_HUNT_SUCCESS;
                    system->hunt_concluded = true;
                    system->stats.successful_intercepts++;
                    system->mode = DRAGONFLY_MODE_IDLE;
                }
            }
            break;
    }

    /* Update timing stats */
    uint64_t elapsed = get_time_us() - start_time;
    system->stats.total_updates++;
    {
        float new_avg =
            (system->stats.avg_update_time_us * (system->stats.total_updates - 1) + (float)elapsed)
            / (float)system->stats.total_updates;
        if (isfinite(new_avg)) {
            system->stats.avg_update_time_us = new_avg;
        }
    }

    system->last_update_us = get_time_us();

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int dragonfly_get_motor_command(
    const dragonfly_system_t* system,
    dragonfly_motor_cmd_t* cmd
) {
    if (!system || !cmd) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_get_motor_command: NULL argument");
        return -1;
    }

    nimcp_mutex_lock(((dragonfly_system_t*)system)->mutex);
    *cmd = system->current_cmd;
    nimcp_mutex_unlock(((dragonfly_system_t*)system)->mutex);

    return 0;
}

//=============================================================================
// State Query Functions
//=============================================================================

dragonfly_mode_t dragonfly_get_mode(const dragonfly_system_t* system) {
    if (!system) return DRAGONFLY_MODE_IDLE;

    nimcp_mutex_lock(((dragonfly_system_t*)system)->mutex);
    dragonfly_mode_t mode = system->mode;
    nimcp_mutex_unlock(((dragonfly_system_t*)system)->mutex);

    return mode;
}

dragonfly_hunt_result_t dragonfly_get_hunt_result(const dragonfly_system_t* system) {
    if (!system) return DRAGONFLY_HUNT_ABORTED;

    nimcp_mutex_lock(((dragonfly_system_t*)system)->mutex);
    dragonfly_hunt_result_t result = system->hunt_result;
    nimcp_mutex_unlock(((dragonfly_system_t*)system)->mutex);

    return result;
}

int dragonfly_get_primary_target(
    const dragonfly_system_t* system,
    dragonfly_target_info_t* target
) {
    if (!system || !target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_get_primary_target: NULL argument");
        return -1;
    }

    nimcp_mutex_lock(((dragonfly_system_t*)system)->mutex);

    target_entry_t* entry = find_target((dragonfly_system_t*)system, system->primary_target_id);
    if (!entry) {
        nimcp_mutex_unlock(((dragonfly_system_t*)system)->mutex);
        return -1;  /* No primary target is a normal state */
    }

    *target = entry->info;
    nimcp_mutex_unlock(((dragonfly_system_t*)system)->mutex);

    return 0;
}

int dragonfly_get_state(
    const dragonfly_system_t* system,
    dragonfly_state_t* state
) {
    if (!system || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_get_state: NULL argument");
        return -1;
    }

    nimcp_mutex_lock(((dragonfly_system_t*)system)->mutex);

    /* Get primary target info */
    target_entry_t* entry = find_target((dragonfly_system_t*)system, system->primary_target_id);

    if (entry && entry->active) {
        state->target_id = entry->info.id;
        state->confidence = entry->info.confidence;
        state->time_to_intercept_ms = entry->info.time_to_intercept_s * 1000.0f;
        state->is_tracking = true;
        state->evasion_detected = (entry->info.evasion_type != EVASION_NONE);
    } else {
        state->target_id = 0;
        state->confidence = 0.0f;
        state->time_to_intercept_ms = 0.0f;
        state->is_tracking = false;
        state->evasion_detected = false;
    }

    nimcp_mutex_unlock(((dragonfly_system_t*)system)->mutex);

    return 0;
}

int dragonfly_get_all_targets(
    const dragonfly_system_t* system,
    dragonfly_target_info_t* targets,
    uint32_t max_targets,
    uint32_t* num_targets
) {
    if (!system || !targets || !num_targets) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_get_all_targets: NULL argument");
        return -1;
    }

    nimcp_mutex_lock(((dragonfly_system_t*)system)->mutex);

    uint32_t count = 0;
    for (uint32_t i = 0; i < DRAGONFLY_MAX_TARGETS && count < max_targets; i++) {
        if (system->targets[i].active) {
            targets[count++] = system->targets[i].info;
        }
    }

    *num_targets = count;
    nimcp_mutex_unlock(((dragonfly_system_t*)system)->mutex);

    return 0;
}

int dragonfly_get_tsdn_vector(
    const dragonfly_system_t* system,
    tsdn_vector_t* vector
) {
    if (!system || !vector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_get_tsdn_vector: NULL argument");
        return -1;
    }

    nimcp_mutex_lock(((dragonfly_system_t*)system)->mutex);
    *vector = system->current_tsdn;
    nimcp_mutex_unlock(((dragonfly_system_t*)system)->mutex);

    return 0;
}

int dragonfly_get_prediction(
    const dragonfly_system_t* system,
    trajectory_prediction_t* prediction
) {
    if (!system || !prediction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_get_prediction: NULL argument");
        return -1;
    }

    nimcp_mutex_lock(((dragonfly_system_t*)system)->mutex);
    *prediction = system->current_prediction;
    nimcp_mutex_unlock(((dragonfly_system_t*)system)->mutex);

    return 0;
}

int dragonfly_get_intercept_solution(
    const dragonfly_system_t* system,
    intercept_solution_t* solution
) {
    if (!system || !solution) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_get_intercept_solution: NULL argument");
        return -1;
    }

    nimcp_mutex_lock(((dragonfly_system_t*)system)->mutex);
    *solution = system->current_solution;
    nimcp_mutex_unlock(((dragonfly_system_t*)system)->mutex);

    return 0;
}

//=============================================================================
// Control Functions
//=============================================================================

int dragonfly_start_scan(dragonfly_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_start_scan: system is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    if (system->mode != DRAGONFLY_MODE_IDLE) {
        nimcp_mutex_unlock(system->mutex);
        return -1;  /* System not idle - normal operational state */
    }

    system->mode = DRAGONFLY_MODE_SCANNING;
    system->hunt_result = DRAGONFLY_HUNT_IN_PROGRESS;
    system->hunt_concluded = false;

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int dragonfly_lock_target(dragonfly_system_t* system, uint32_t target_id) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_lock_target: system is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    target_entry_t* target = find_target(system, target_id);
    if (!target) {
        nimcp_mutex_unlock(system->mutex);
        return -1;  /* Target not found */
    }

    system->primary_target_id = target_id;
    system->mode = DRAGONFLY_MODE_TRACKING;
    system->stats.targets_tracked++;

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int dragonfly_start_pursuit(dragonfly_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_start_pursuit: system is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    if (system->mode != DRAGONFLY_MODE_TRACKING) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "dragonfly_start_pursuit: not tracking");
        nimcp_mutex_unlock(system->mutex);
        return -1;
    }

    if (!get_primary_target_entry(system)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "dragonfly_start_pursuit: no primary target");
        nimcp_mutex_unlock(system->mutex);
        return -1;
    }

    system->mode = DRAGONFLY_MODE_PURSUING;
    system->pursuit_start_us = get_time_us();
    system->stats.pursuits_initiated++;

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int dragonfly_abort_pursuit(dragonfly_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_abort_pursuit: system is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    if (system->mode == DRAGONFLY_MODE_PURSUING ||
        system->mode == DRAGONFLY_MODE_INTERCEPTING) {
        system->hunt_result = DRAGONFLY_HUNT_ABORTED;
        system->hunt_concluded = true;
        system->stats.aborted_pursuits++;
        system->mode = DRAGONFLY_MODE_IDLE;
    }

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int dragonfly_go_idle(dragonfly_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_go_idle: system is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);
    system->mode = DRAGONFLY_MODE_IDLE;
    nimcp_mutex_unlock(system->mutex);

    return 0;
}

//=============================================================================
// Statistics and Configuration
//=============================================================================

int dragonfly_get_stats(
    const dragonfly_system_t* system,
    dragonfly_stats_t* stats
) {
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_get_stats: NULL argument");
        return -1;
    }

    nimcp_mutex_lock(((dragonfly_system_t*)system)->mutex);

    *stats = system->stats;

    /* Get subsystem stats */
    tsdn_get_stats(system->tsdn, &stats->tsdn_stats);
    dragonfly_tracker_get_stats(system->tracker, &stats->tracker_stats);
    dragonfly_predictor_get_stats(system->predictor, &stats->prediction_stats);
    dragonfly_interceptor_get_stats(system->interceptor, &stats->intercept_stats);

    /* Calculate success rate */
    uint64_t total_hunts = stats->successful_intercepts + stats->escaped_targets +
                           stats->aborted_pursuits;
    if (total_hunts > 0) {
        stats->success_rate = (float)stats->successful_intercepts / (float)total_hunts;
    } else {
        stats->success_rate = 0.0f;
    }

    nimcp_mutex_unlock(((dragonfly_system_t*)system)->mutex);
    return 0;
}

int dragonfly_reset_stats(dragonfly_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_reset_stats: system is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    memset(&system->stats, 0, sizeof(system->stats));
    tsdn_reset_stats(system->tsdn);
    dragonfly_tracker_reset_stats(system->tracker);
    dragonfly_predictor_reset_stats(system->predictor);
    dragonfly_interceptor_reset_stats(system->interceptor);

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int dragonfly_set_config(
    dragonfly_system_t* system,
    const dragonfly_config_t* config
) {
    if (!system || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_set_config: NULL argument");
        return -1;
    }
    if (!dragonfly_validate_config(config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_set_config: invalid config");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    system->config = *config;

    /* Update subsystem configs */
    /* Note: TSDN config update requires recreating the population */
    dragonfly_tracker_set_config(system->tracker, &config->tracker_config);
    dragonfly_predictor_set_config(system->predictor, &config->prediction_config);
    dragonfly_interceptor_set_config(system->interceptor, &config->intercept_config);

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int dragonfly_get_config(
    const dragonfly_system_t* system,
    dragonfly_config_t* config
) {
    if (!system || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_get_config: NULL argument");
        return -1;
    }

    nimcp_mutex_lock(((dragonfly_system_t*)system)->mutex);
    *config = system->config;
    nimcp_mutex_unlock(((dragonfly_system_t*)system)->mutex);

    return 0;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* dragonfly_mode_name(dragonfly_mode_t mode) {
    switch (mode) {
        case DRAGONFLY_MODE_IDLE:         return "Idle";
        case DRAGONFLY_MODE_SCANNING:     return "Scanning";
        case DRAGONFLY_MODE_TRACKING:     return "Tracking";
        case DRAGONFLY_MODE_PURSUING:     return "Pursuing";
        case DRAGONFLY_MODE_INTERCEPTING: return "Intercepting";
        default:                          return "Unknown";
    }
}

const char* dragonfly_hunt_result_name(dragonfly_hunt_result_t result) {
    switch (result) {
        case DRAGONFLY_HUNT_IN_PROGRESS: return "In Progress";
        case DRAGONFLY_HUNT_SUCCESS:     return "Success";
        case DRAGONFLY_HUNT_ESCAPED:     return "Escaped";
        case DRAGONFLY_HUNT_ABORTED:     return "Aborted";
        case DRAGONFLY_HUNT_TIMEOUT:     return "Timeout";
        default:                          return "Unknown";
    }
}

const char* dragonfly_version(void) {
    static __thread char version[32];
    snprintf(version, sizeof(version), "%d.%d.%d",
             DRAGONFLY_VERSION_MAJOR,
             DRAGONFLY_VERSION_MINOR,
             DRAGONFLY_VERSION_PATCH);
    return version;
}
