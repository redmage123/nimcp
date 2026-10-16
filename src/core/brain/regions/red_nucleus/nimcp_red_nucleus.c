/**
 * @file nimcp_red_nucleus.c
 * @brief Red Nucleus Implementation - Motor Coordination and Learning Center
 *
 * Implements motor coordination, rubrospinal tract output, cerebellar integration,
 * and error-based motor learning.
 */

#include "core/brain/regions/red_nucleus/nimcp_red_nucleus.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(red_nucleus, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * INTERNAL CONSTANTS
 *===========================================================================*/

#define RN_LOG_TAG "RED_NUCLEUS"
#define RN_DEFAULT_MAX_COMMANDS 64
#define RN_DEFAULT_MAX_TRAJECTORY 128
#define RN_MODULE_ID 0x6000

/* PID-like motor control constants */
#define RN_KP 0.5f    /* Proportional gain */
#define RN_KI 0.1f    /* Integral gain */
#define RN_KD 0.05f   /* Derivative gain */

static float vector3_magnitude(const rn_vector3_t* v) {
    return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
}

/* Vector utility functions - available for future trajectory computation */
#if 0
static void vector3_normalize(rn_vector3_t* v) {
    float mag = vector3_magnitude(v);
    if (mag > 0.0001f) {
        v->x /= mag;
        v->y /= mag;
        v->z /= mag;
    }
}

static void vector3_scale(rn_vector3_t* v, float scale) {
    v->x *= scale;
    v->y *= scale;
    v->z *= scale;
}

static rn_vector3_t vector3_subtract(const rn_vector3_t* a, const rn_vector3_t* b) {
    rn_vector3_t result = {
        .x = a->x - b->x,
        .y = a->y - b->y,
        .z = a->z - b->z
    };
    return result;
}
#endif

static float compute_pid_output(rn_learning_state_t* learn, float error, float dt) {
    /* Update integral with anti-windup */
    learn->error_integral += error * dt;
    learn->error_integral = nimcp_clampf(learn->error_integral, -10.0f, 10.0f);

    /* Compute derivative */
    float derivative = (fabsf(dt) > 1e-10f) ?
        (error - learn->error_derivative) / dt : 0.0f;
    learn->error_derivative = error;

    /* PID output */
    return RN_KP * error + RN_KI * learn->error_integral + RN_KD * derivative;
}

static void update_error_history(rn_learning_state_t* learn, float error) {
    learn->error_history[learn->error_index] = fabsf(error);
    learn->error_index = (learn->error_index + 1) % 32;
    if (learn->error_count < 32) {
        learn->error_count++;
    }

    /* Compute average error */
    float sum = 0.0f;
    for (uint32_t i = 0; i < learn->error_count; i++) {
        sum += learn->error_history[i];
    }
    learn->avg_error = sum / (float)learn->error_count;
}

static void apply_learning_update(rn_learning_state_t* learn, float error,
                                   float learning_rate) {
    update_error_history(learn, error);

    /* Update adaptation gain based on error */
    float error_mag = fabsf(error);
    float adaptation_delta = learning_rate * error_mag;

    /* Error reduction improves skill */
    if (error_mag < learn->avg_error) {
        learn->skill_level += adaptation_delta * 0.1f;
    } else {
        learn->skill_level -= adaptation_delta * 0.05f;
    }
    learn->skill_level = nimcp_clampf(learn->skill_level, 0.0f, 1.0f);

    /* Update adaptation gain (inverse of error) */
    learn->adaptation_gain = 1.0f + (1.0f - error_mag) * 0.5f;
    learn->adaptation_gain = nimcp_clampf(learn->adaptation_gain, 0.5f, 2.0f);

    learn->training_iterations++;
}

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

int rn_default_config(rn_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    memset(config, 0, sizeof(*config));

    /* Motor control */
    config->velocity_gain = 1.0f;
    config->force_gain = 1.0f;
    config->position_gain = 1.0f;
    config->damping_coefficient = 0.5f;

    /* Learning */
    config->base_learning_rate = 0.05f;
    config->error_threshold = 0.1f;
    config->adaptation_rate = 0.1f;
    config->skill_decay_rate = 0.001f;
    config->error_history_size = 32;

    /* Cerebellar integration */
    config->dentate_weight = 0.7f;
    config->olivary_gain = 0.5f;
    config->thalamic_threshold = 0.3f;

    /* Subdivision weights */
    config->magnocellular_weight = 0.6f;
    config->parvocellular_weight = 0.4f;

    /* Integration */
    config->enable_bio_async = true;
    config->enable_kg_wiring = true;
    config->enable_immune = true;
    config->enable_security = true;
    config->enable_logging = true;
    config->enable_quantum = false;
    config->enable_cerebellar = true;

    /* Resources */
    config->max_commands_queued = RN_DEFAULT_MAX_COMMANDS;
    config->max_trajectory_points = RN_DEFAULT_MAX_TRAJECTORY;
    config->update_interval_ms = 10;

    config->platform_tier = PLATFORM_TIER_FULL;

    return 0;
}

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

nimcp_red_nucleus_t* rn_create(const rn_config_t* config) {
    rn_config_t default_config;
    if (!config) {
        rn_default_config(&default_config);
        config = &default_config;
    }

    nimcp_red_nucleus_t* rn = (nimcp_red_nucleus_t*)nimcp_calloc(
        1, sizeof(nimcp_red_nucleus_t));
    if (!rn) {
        NIMCP_LOG_ERROR(RN_LOG_TAG, "Failed to allocate Red Nucleus");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn is NULL");

        return NULL;
    }

    /* Copy config */
    memcpy(&rn->config, config, sizeof(rn_config_t));

    /* Allocate command queue */
    rn->queue_capacity = config->max_commands_queued;
    rn->command_queue = (rn_motor_command_t*)nimcp_calloc(
        rn->queue_capacity, sizeof(rn_motor_command_t));
    if (!rn->command_queue) {
        NIMCP_LOG_ERROR(RN_LOG_TAG, "Failed to allocate command queue");
        nimcp_free(rn);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rn_create: rn->command_queue is NULL");
        return NULL;
    }

    /* Create mutex */
    rn->mutex = nimcp_mutex_create(NULL);
    if (!rn->mutex) {
        NIMCP_LOG_ERROR(RN_LOG_TAG, "Failed to create mutex");
        nimcp_free(rn->command_queue);
        nimcp_free(rn);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rn_create: rn->mutex is NULL");
        return NULL;
    }

    /* Initialize subdivisions */
    for (int i = 0; i < RN_SUBDIV_COUNT; i++) {
        rn->subdivisions.activity[i] = 0.0f;
        rn->subdivisions.modulation[i] = 1.0f;
        rn->subdivisions.active[i] = true;
    }

    /* Initialize learning state for each effector */
    for (int i = 0; i < RN_EFFECTOR_COUNT; i++) {
        rn->learning[i].adaptation_gain = 1.0f;
        rn->learning[i].learning_rate = config->base_learning_rate;
        rn->learning[i].skill_level = 0.0f;
    }

    /* Initialize outputs */
    for (int i = 0; i < RN_EFFECTOR_COUNT; i++) {
        rn->rubrospinal_output[i] = 0.0f;
    }

    /* Initialize cortical inputs */
    for (int i = 0; i < RN_CMD_COUNT; i++) {
        rn->cortical_input[i] = 0.0f;
    }

    rn->global_learning_modulation = 1.0f;

    NIMCP_LOG_INFO(RN_LOG_TAG, "Red Nucleus created with queue capacity %u",
                   rn->queue_capacity);

    return rn;
}

void rn_destroy(nimcp_red_nucleus_t* rn) {
    if (!rn) return;

    NIMCP_LOG_INFO(RN_LOG_TAG,
        "Destroying Red Nucleus (commands=%lu, errors=%lu, learning_updates=%lu)",
        rn->stats.commands_issued,
        rn->stats.errors_detected,
        rn->stats.learning_updates);

    /* Disconnect from systems */
    if (rn->connected) {
        rn_bio_async_disconnect(rn);
        rn_kg_unregister(rn);
    }

    /* Free trajectory if allocated */
    if (rn->current_trajectory && rn->current_trajectory->points) {
        nimcp_free(rn->current_trajectory->points);
        nimcp_free(rn->current_trajectory);
    }

    /* Clean up */
    if (rn->mutex) {
        nimcp_mutex_free(rn->mutex);
    }
    if (rn->command_queue) {
        nimcp_free(rn->command_queue);
    }

    nimcp_free(rn);
}

int rn_init(nimcp_red_nucleus_t* rn) {
    if (!rn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn is NULL");

        return -1;

    }

    nimcp_mutex_lock(rn->mutex);

    /* Reset queue state */
    rn->queue_head = 0;
    rn->queue_tail = 0;
    rn->queue_size = 0;
    rn->command_active = false;

    /* Reset outputs */
    for (int i = 0; i < RN_EFFECTOR_COUNT; i++) {
        rn->rubrospinal_output[i] = 0.0f;
    }
    rn->output_magnitude = 0.0f;

    /* Reset trajectory */
    rn->trajectory_index = 0;
    rn->trajectory_progress = 0.0f;

    /* Reset learning (preserve skill) */
    for (int i = 0; i < RN_EFFECTOR_COUNT; i++) {
        rn->learning[i].error_index = 0;
        rn->learning[i].error_count = 0;
        rn->learning[i].error_integral = 0.0f;
        rn->learning[i].error_derivative = 0.0f;
    }

    rn->cumulative_error = 0.0f;
    memset(&rn->last_error, 0, sizeof(rn_motor_error_t));

    /* Reset cerebellar signals */
    memset(&rn->dentate_input, 0, sizeof(rn_dentate_signal_t));
    memset(&rn->olivary_output, 0, sizeof(rn_olivary_output_t));
    memset(&rn->thalamic_output, 0, sizeof(rn_thalamic_output_t));

    /* Reset stats (preserve totals) */
    memset(&rn->stats, 0, sizeof(rn_stats_t));

    rn->initialized = true;
    rn->last_update_us = 0;

    nimcp_mutex_unlock(rn->mutex);

    NIMCP_LOG_INFO(RN_LOG_TAG, "Red Nucleus initialized");
    return 0;
}

int rn_reset(nimcp_red_nucleus_t* rn) {
    if (!rn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn is NULL");

        return -1;

    }
    return rn_init(rn);
}

/*=============================================================================
 * MOTOR COMMAND API
 *===========================================================================*/

int rn_issue_command(nimcp_red_nucleus_t* rn, const rn_motor_command_t* cmd) {
    if (!rn || !cmd || !rn->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_issue_command: required parameter is NULL (rn, cmd, rn->initialized)");
        return -1;
    }

    nimcp_mutex_lock(rn->mutex);

    /* Check queue capacity */
    if (rn->queue_size >= rn->queue_capacity) {
        nimcp_mutex_unlock(rn->mutex);
        NIMCP_LOG_WARN(RN_LOG_TAG, "Command queue full");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "rn_issue_command: capacity exceeded");
        return -1;
    }

    /* Add to queue */
    memcpy(&rn->command_queue[rn->queue_tail], cmd, sizeof(rn_motor_command_t));
    rn->queue_tail = (rn->queue_tail + 1) % rn->queue_capacity;
    rn->queue_size++;

    rn->stats.commands_issued++;

    /* Activate magnocellular for rubrospinal output */
    rn->subdivisions.activity[RN_SUBDIV_MAGNOCELLULAR] =
        fminf(1.0f, rn->subdivisions.activity[RN_SUBDIV_MAGNOCELLULAR] + 0.3f);

    /* Broadcast via bio-async */
    if (rn->bio_router && rn->config.enable_bio_async) {
        rn_bio_async_broadcast(rn, RN_BIO_MSG_MOTOR_CMD, cmd, sizeof(rn_motor_command_t));
    }

    nimcp_mutex_unlock(rn->mutex);

    NIMCP_LOG_DEBUG(RN_LOG_TAG, "Command issued: type=%s, effector=%s, mag=%.3f",
                    rn_cmd_type_string(cmd->type),
                    rn_effector_string(cmd->effector),
                    cmd->magnitude);

    return 0;
}

int rn_command_velocity(nimcp_red_nucleus_t* rn, rn_effector_t effector,
                        const rn_vector3_t* velocity, float duration_ms) {
    if (!rn || !velocity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_issue_command: required parameter is NULL (rn, velocity)");
        return -1;
    }

    rn_motor_command_t cmd = {
        .type = RN_CMD_VELOCITY,
        .effector = effector,
        .value = *velocity,
        .magnitude = vector3_magnitude(velocity),
        .urgency = 0.5f,
        .duration_ms = duration_ms,
        .timestamp_us = 0,  /* Will be set by system */
        .sequence_id = 0
    };

    /* Apply velocity gain */
    cmd.magnitude *= rn->config.velocity_gain;

    return rn_issue_command(rn, &cmd);
}

int rn_command_force(nimcp_red_nucleus_t* rn, rn_effector_t effector,
                     const rn_vector3_t* force, float duration_ms) {
    if (!rn || !force) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_issue_command: required parameter is NULL (rn, force)");
        return -1;
    }

    rn_motor_command_t cmd = {
        .type = RN_CMD_FORCE,
        .effector = effector,
        .value = *force,
        .magnitude = vector3_magnitude(force),
        .urgency = 0.6f,
        .duration_ms = duration_ms,
        .timestamp_us = 0,
        .sequence_id = 0
    };

    cmd.magnitude *= rn->config.force_gain;

    return rn_issue_command(rn, &cmd);
}

int rn_command_position(nimcp_red_nucleus_t* rn, rn_effector_t effector,
                        const rn_vector3_t* position, float duration_ms) {
    if (!rn || !position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_issue_command: required parameter is NULL (rn, position)");
        return -1;
    }

    rn_motor_command_t cmd = {
        .type = RN_CMD_POSITION,
        .effector = effector,
        .value = *position,
        .magnitude = vector3_magnitude(position),
        .urgency = 0.5f,
        .duration_ms = duration_ms,
        .timestamp_us = 0,
        .sequence_id = 0
    };

    cmd.magnitude *= rn->config.position_gain;

    return rn_issue_command(rn, &cmd);
}

int rn_command_trajectory(nimcp_red_nucleus_t* rn, const rn_trajectory_t* trajectory) {
    if (!rn || !trajectory || trajectory->num_points == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_command_trajectory: required parameter is NULL (rn, trajectory)");
        return -1;
    }

    nimcp_mutex_lock(rn->mutex);

    /* Free existing trajectory */
    if (rn->current_trajectory) {
        if (rn->current_trajectory->points) {
            nimcp_free(rn->current_trajectory->points);
        }
        nimcp_free(rn->current_trajectory);
    }

    /* Allocate new trajectory */
    rn->current_trajectory = (rn_trajectory_t*)nimcp_malloc(sizeof(rn_trajectory_t));
    if (!rn->current_trajectory) {
        nimcp_mutex_unlock(rn->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rn_command_trajectory: rn->current_trajectory is NULL");
        return -1;
    }

    rn->current_trajectory->points = (rn_trajectory_point_t*)nimcp_malloc(
        trajectory->num_points * sizeof(rn_trajectory_point_t));
    if (!rn->current_trajectory->points) {
        nimcp_free(rn->current_trajectory);
        rn->current_trajectory = NULL;
        nimcp_mutex_unlock(rn->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_command_trajectory: rn->current_trajectory->points is NULL");
        return -1;
    }

    /* Copy trajectory data */
    memcpy(rn->current_trajectory->points, trajectory->points,
           trajectory->num_points * sizeof(rn_trajectory_point_t));
    rn->current_trajectory->num_points = trajectory->num_points;
    rn->current_trajectory->effector = trajectory->effector;
    rn->current_trajectory->total_duration_ms = trajectory->total_duration_ms;
    rn->current_trajectory->smooth_interpolation = trajectory->smooth_interpolation;

    rn->trajectory_index = 0;
    rn->trajectory_progress = 0.0f;

    nimcp_mutex_unlock(rn->mutex);

    NIMCP_LOG_INFO(RN_LOG_TAG, "Trajectory set: %u points, %.1f ms duration",
                   trajectory->num_points, trajectory->total_duration_ms);

    return 0;
}

int rn_command_posture(nimcp_red_nucleus_t* rn, const rn_vector3_t* adjustment,
                       float urgency) {
    if (!rn || !adjustment) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_command_trajectory: required parameter is NULL (rn, adjustment)");
        return -1;
    }

    rn_motor_command_t cmd = {
        .type = RN_CMD_POSTURE,
        .effector = RN_EFFECTOR_AXIAL,
        .value = *adjustment,
        .magnitude = vector3_magnitude(adjustment),
        .urgency = urgency,
        .duration_ms = 100.0f,  /* Posture adjustments are typically quick */
        .timestamp_us = 0,
        .sequence_id = 0
    };

    /* Broadcast posture adjustment */
    if (rn->bio_router && rn->config.enable_bio_async) {
        rn_bio_async_broadcast(rn, RN_BIO_MSG_POSTURE_ADJUST,
                               adjustment, sizeof(rn_vector3_t));
    }

    return rn_issue_command(rn, &cmd);
}

float rn_get_output(const nimcp_red_nucleus_t* rn, rn_effector_t effector) {
    if (!rn || effector >= RN_EFFECTOR_COUNT) return 0.0f;
    return rn->rubrospinal_output[effector];
}

int rn_get_all_outputs(const nimcp_red_nucleus_t* rn, float* outputs) {
    if (!rn || !outputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_get_all_outputs: required parameter is NULL (rn, outputs)");
        return -1;
    }

    nimcp_mutex_lock(((nimcp_red_nucleus_t*)rn)->mutex);
    memcpy(outputs, rn->rubrospinal_output, RN_EFFECTOR_COUNT * sizeof(float));
    nimcp_mutex_unlock(((nimcp_red_nucleus_t*)rn)->mutex);

    return 0;
}

/*=============================================================================
 * MOTOR LEARNING API
 *===========================================================================*/

int rn_process_error(nimcp_red_nucleus_t* rn, const rn_motor_error_t* error) {
    if (!rn || !error) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_process_error: required parameter is NULL (rn, error)");
        return -1;
    }

    nimcp_mutex_lock(rn->mutex);

    /* Store last error */
    memcpy(&rn->last_error, error, sizeof(rn_motor_error_t));

    /* Update statistics */
    rn->stats.errors_detected++;
    rn->cumulative_error += fabsf(error->error_magnitude);

    /* Update learning for affected effector */
    if (error->effector < RN_EFFECTOR_COUNT) {
        rn_learning_state_t* learn = &rn->learning[error->effector];
        float effective_rate = learn->learning_rate * rn->global_learning_modulation;

        apply_learning_update(learn, error->error_magnitude, effective_rate);

        /* Check if error threshold exceeded for adaptation */
        if (fabsf(error->error_magnitude) > rn->config.error_threshold) {
            /* Compute PID correction */
            float correction = compute_pid_output(learn, error->error_magnitude, 0.01f);

            /* Apply to rubrospinal output */
            rn->rubrospinal_output[error->effector] += correction * 0.1f;
            rn->rubrospinal_output[error->effector] =
                nimcp_clampf(rn->rubrospinal_output[error->effector], -1.0f, 1.0f);

            rn->stats.errors_corrected++;
        }
    }

    /* Activate parvocellular for error processing/learning */
    rn->subdivisions.activity[RN_SUBDIV_PARVOCELLULAR] =
        fminf(1.0f, rn->subdivisions.activity[RN_SUBDIV_PARVOCELLULAR] +
              fabsf(error->error_magnitude) * 0.5f);

    /* Generate olivary output for cerebellar learning */
    rn->olivary_output.error_signal = error->error_magnitude;
    rn->olivary_output.error_type = error->type;
    rn->olivary_output.effector = error->effector;
    rn->olivary_output.learning_request =
        rn->learning[error->effector].learning_rate;
    rn->olivary_output.timestamp_us = error->timestamp_us;

    rn->stats.olivary_outputs_sent++;
    rn->stats.learning_updates++;

    /* Broadcast error and learning update */
    if (rn->bio_router && rn->config.enable_bio_async) {
        rn_bio_async_broadcast(rn, RN_BIO_MSG_ERROR_SIGNAL,
                               error, sizeof(rn_motor_error_t));
        rn_bio_async_broadcast(rn, RN_BIO_MSG_LEARNING_UPDATE,
                               &rn->learning[error->effector],
                               sizeof(rn_learning_state_t));
        rn_bio_async_broadcast(rn, RN_BIO_MSG_OLIVARY_OUTPUT,
                               &rn->olivary_output, sizeof(rn_olivary_output_t));
    }

    /* Update KG */
    if (rn->kg && rn->config.enable_kg_wiring) {
        rn_kg_update_state(rn);
    }

    nimcp_mutex_unlock(rn->mutex);

    NIMCP_LOG_DEBUG(RN_LOG_TAG, "Error processed: type=%s, effector=%s, mag=%.3f",
                    rn_error_type_string(error->type),
                    rn_effector_string(error->effector),
                    error->error_magnitude);

    return 0;
}

int rn_report_error(nimcp_red_nucleus_t* rn, rn_effector_t effector,
                    rn_error_type_t error_type, float error_magnitude) {
    if (!rn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn is NULL");

        return -1;

    }

    rn_motor_error_t error = {
        .type = error_type,
        .effector = effector,
        .error_magnitude = error_magnitude,
        .error_vector = {0.0f, 0.0f, 0.0f},
        .timestamp_us = 0,
        .command_id = 0
    };

    return rn_process_error(rn, &error);
}

int rn_get_learning_state(const nimcp_red_nucleus_t* rn, rn_effector_t effector,
                          rn_learning_state_t* state) {
    if (!rn || !state || effector >= RN_EFFECTOR_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_process_error: required parameter is NULL (rn, state)");
        return -1;
    }

    nimcp_mutex_lock(((nimcp_red_nucleus_t*)rn)->mutex);
    memcpy(state, &rn->learning[effector], sizeof(rn_learning_state_t));
    nimcp_mutex_unlock(((nimcp_red_nucleus_t*)rn)->mutex);

    return 0;
}

float rn_get_skill_level(const nimcp_red_nucleus_t* rn, rn_effector_t effector) {
    if (!rn || effector >= RN_EFFECTOR_COUNT) return 0.0f;
    return rn->learning[effector].skill_level;
}

int rn_set_learning_modulation(nimcp_red_nucleus_t* rn, float modulation) {
    if (!rn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn is NULL");

        return -1;

    }

    nimcp_mutex_lock(rn->mutex);
    rn->global_learning_modulation = nimcp_clampf(modulation, 0.0f, 2.0f);
    nimcp_mutex_unlock(rn->mutex);

    NIMCP_LOG_DEBUG(RN_LOG_TAG, "Learning modulation set to %.3f", modulation);

    return 0;
}

int rn_reset_learning(nimcp_red_nucleus_t* rn, rn_effector_t effector) {
    if (!rn || effector >= RN_EFFECTOR_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "rn_reset_learning: rn is NULL");
        return -1;
    }

    nimcp_mutex_lock(rn->mutex);

    rn_learning_state_t* learn = &rn->learning[effector];
    memset(learn->error_history, 0, sizeof(learn->error_history));
    learn->error_index = 0;
    learn->error_count = 0;
    learn->adaptation_gain = 1.0f;
    learn->learning_rate = rn->config.base_learning_rate;
    learn->error_integral = 0.0f;
    learn->error_derivative = 0.0f;
    learn->avg_error = 0.0f;
    learn->error_reduction = 0.0f;
    learn->skill_level = 0.0f;
    learn->training_iterations = 0;

    nimcp_mutex_unlock(rn->mutex);

    NIMCP_LOG_INFO(RN_LOG_TAG, "Learning reset for effector %s",
                   rn_effector_string(effector));

    return 0;
}

/*=============================================================================
 * CEREBELLAR INTEGRATION
 *===========================================================================*/

int rn_process_dentate_input(nimcp_red_nucleus_t* rn,
                             const rn_dentate_signal_t* signal) {
    if (!rn || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_reset_learning: required parameter is NULL (rn, signal)");
        return -1;
    }

    nimcp_mutex_lock(rn->mutex);

    /* Store dentate input */
    memcpy(&rn->dentate_input, signal, sizeof(rn_dentate_signal_t));

    rn->stats.dentate_signals_received++;

    /* Apply dentate corrections to rubrospinal output */
    float dentate_gain = rn->config.dentate_weight * signal->activity;

    for (uint32_t i = 0; i < signal->num_corrections && i < RN_EFFECTOR_COUNT; i++) {
        float correction = signal->motor_correction[i] * dentate_gain;
        rn->rubrospinal_output[i] += correction;
        rn->rubrospinal_output[i] = nimcp_clampf(rn->rubrospinal_output[i], -1.0f, 1.0f);
    }

    /* Apply timing adjustment to magnocellular output */
    rn->subdivisions.modulation[RN_SUBDIV_MAGNOCELLULAR] *=
        (1.0f + signal->timing_adjustment * 0.2f);
    rn->subdivisions.modulation[RN_SUBDIV_MAGNOCELLULAR] =
        nimcp_clampf(rn->subdivisions.modulation[RN_SUBDIV_MAGNOCELLULAR], 0.5f, 2.0f);

    /* Update thalamic output (dentato-rubro-thalamic pathway) */
    if (signal->activity > rn->config.thalamic_threshold) {
        rn->thalamic_output.activity = signal->activity * 0.8f;
        rn->thalamic_output.motor_readiness = signal->activity;
        rn->thalamic_output.movement_intention = signal->motor_correction[0];
        rn->thalamic_output.timestamp_us = signal->timestamp_us;

        rn->stats.thalamic_outputs_sent++;

        if (rn->bio_router && rn->config.enable_bio_async) {
            rn_bio_async_broadcast(rn, RN_BIO_MSG_THALAMIC_OUTPUT,
                                   &rn->thalamic_output, sizeof(rn_thalamic_output_t));
        }
    }

    /* Broadcast cerebellar input received */
    if (rn->bio_router && rn->config.enable_bio_async) {
        rn_bio_async_broadcast(rn, RN_BIO_MSG_CEREBELLAR_INPUT,
                               signal, sizeof(rn_dentate_signal_t));
    }

    nimcp_mutex_unlock(rn->mutex);

    NIMCP_LOG_DEBUG(RN_LOG_TAG, "Dentate input: activity=%.3f, timing_adj=%.3f",
                    signal->activity, signal->timing_adjustment);

    return 0;
}

int rn_get_olivary_output(const nimcp_red_nucleus_t* rn,
                          rn_olivary_output_t* output) {
    if (!rn || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_reset_learning: required parameter is NULL (rn, output)");
        return -1;
    }

    nimcp_mutex_lock(((nimcp_red_nucleus_t*)rn)->mutex);
    memcpy(output, &rn->olivary_output, sizeof(rn_olivary_output_t));
    nimcp_mutex_unlock(((nimcp_red_nucleus_t*)rn)->mutex);

    return 0;
}

int rn_get_thalamic_output(const nimcp_red_nucleus_t* rn,
                           rn_thalamic_output_t* output) {
    if (!rn || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_reset_learning: required parameter is NULL (rn, output)");
        return -1;
    }

    nimcp_mutex_lock(((nimcp_red_nucleus_t*)rn)->mutex);
    memcpy(output, &rn->thalamic_output, sizeof(rn_thalamic_output_t));
    nimcp_mutex_unlock(((nimcp_red_nucleus_t*)rn)->mutex);

    return 0;
}

int rn_cerebellum_connect(nimcp_red_nucleus_t* rn,
                          struct cerebellum_adapter* cerebellum) {
    if (!rn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn is NULL");

        return -1;

    }

    rn->cerebellum = cerebellum;
    NIMCP_LOG_INFO(RN_LOG_TAG, "Connected to cerebellum");

    return 0;
}

int rn_process_cerebellar_error(nimcp_red_nucleus_t* rn,
                                float error_magnitude,
                                rn_error_type_t error_type) {
    if (!rn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn is NULL");

        return -1;

    }

    /* Generate olivary output based on cerebellar error */
    nimcp_mutex_lock(rn->mutex);

    rn->olivary_output.error_signal = error_magnitude * rn->config.olivary_gain;
    rn->olivary_output.error_type = error_type;
    rn->olivary_output.learning_request = rn->config.base_learning_rate;
    rn->olivary_output.timestamp_us = 0;

    /* Activate parvocellular for olivary output */
    rn->subdivisions.activity[RN_SUBDIV_PARVOCELLULAR] =
        fminf(1.0f, rn->subdivisions.activity[RN_SUBDIV_PARVOCELLULAR] +
              fabsf(error_magnitude) * 0.4f);

    rn->stats.olivary_outputs_sent++;

    nimcp_mutex_unlock(rn->mutex);

    NIMCP_LOG_DEBUG(RN_LOG_TAG, "Cerebellar error processed: mag=%.3f, type=%s",
                    error_magnitude, rn_error_type_string(error_type));

    return 0;
}

/*=============================================================================
 * CORTICAL INPUT API
 *===========================================================================*/

int rn_set_cortical_input(nimcp_red_nucleus_t* rn,
                          rn_motor_cmd_type_t cmd_type,
                          float input) {
    if (!rn || cmd_type >= RN_CMD_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "rn_reset_learning: rn is NULL");
        return -1;
    }

    nimcp_mutex_lock(rn->mutex);
    rn->cortical_input[cmd_type] = nimcp_clampf(input, 0.0f, 1.0f);
    nimcp_mutex_unlock(rn->mutex);

    return 0;
}

float rn_get_cortical_input(const nimcp_red_nucleus_t* rn,
                            rn_motor_cmd_type_t cmd_type) {
    if (!rn || cmd_type >= RN_CMD_COUNT) return 0.0f;
    return rn->cortical_input[cmd_type];
}

/*=============================================================================
 * KG WIRING INTEGRATION
 *===========================================================================*/

int rn_kg_register(nimcp_red_nucleus_t* rn, struct nimcp_brain_kg* kg,
                   uint64_t admin_token) {
    if (!rn || !kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_reset_learning: required parameter is NULL (rn, kg)");
        return -1;
    }

    nimcp_mutex_lock(rn->mutex);

    rn->kg = kg;
    rn->kg_state.admin_token = admin_token;

    /* Register main Red Nucleus node */
    rn->kg_state.region_node_id = RN_MODULE_ID;

    /* Register subdivision nodes */
    for (int i = 0; i < RN_SUBDIV_COUNT; i++) {
        rn->kg_state.subdiv_node_ids[i] = RN_MODULE_ID + 1 + i;
    }

    /* Register command type nodes */
    for (int i = 0; i < RN_CMD_COUNT; i++) {
        rn->kg_state.cmd_node_ids[i] = RN_MODULE_ID + 100 + i;
    }

    /* Register effector nodes */
    for (int i = 0; i < RN_EFFECTOR_COUNT; i++) {
        rn->kg_state.effector_node_ids[i] = RN_MODULE_ID + 200 + i;
    }

    rn->kg_state.registered = true;
    rn->stats.kg_updates++;

    nimcp_mutex_unlock(rn->mutex);

    NIMCP_LOG_INFO(RN_LOG_TAG, "Registered with KG, node_id=%lu",
                   rn->kg_state.region_node_id);

    return 0;
}

int rn_kg_unregister(nimcp_red_nucleus_t* rn) {
    if (!rn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn is NULL");

        return -1;

    }

    nimcp_mutex_lock(rn->mutex);

    if (rn->kg_state.registered) {
        rn->kg_state.registered = false;
        rn->kg = NULL;
    }

    nimcp_mutex_unlock(rn->mutex);
    return 0;
}

/* rn_kg_update_state is implemented in nimcp_red_nucleus_kg_wiring.c */

int rn_kg_query(nimcp_red_nucleus_t* rn, const char* query,
                void* result, size_t result_size) {
    if (!rn || !query || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_kg_unregister: required parameter is NULL (rn, query, result)");
        return -1;
    }
    if (!rn->kg_state.registered) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_kg_unregister: rn->kg_state is NULL");
        return -1;
    }

    (void)result_size;

    return 0;
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

int rn_bio_async_connect(nimcp_red_nucleus_t* rn,
                         struct nimcp_bio_router* router) {
    if (!rn || !router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_kg_unregister: required parameter is NULL (rn, router)");
        return -1;
    }

    nimcp_mutex_lock(rn->mutex);

    rn->bio_router = router;
    rn->connected = true;

    nimcp_mutex_unlock(rn->mutex);

    NIMCP_LOG_INFO(RN_LOG_TAG, "Connected to bio-async router");
    return 0;
}

int rn_bio_async_disconnect(nimcp_red_nucleus_t* rn) {
    if (!rn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn is NULL");

        return -1;

    }

    nimcp_mutex_lock(rn->mutex);

    if (rn->bio_router) {
        rn->bio_router = NULL;
        rn->connected = false;
    }

    nimcp_mutex_unlock(rn->mutex);
    return 0;
}

int rn_bio_async_broadcast(nimcp_red_nucleus_t* rn, rn_bio_msg_type_t msg_type,
                           const void* payload, size_t payload_size) {
    if (!rn || !rn->bio_router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_bio_async_disconnect: required parameter is NULL (rn, rn->bio_router)");
        return -1;
    }

    rn->stats.bio_msgs_sent++;

    /* Actual bio-async API calls would go here */
    (void)msg_type;
    (void)payload;
    (void)payload_size;

    return 0;
}

int rn_bio_async_subscribe(nimcp_red_nucleus_t* rn, uint32_t subscription_mask) {
    if (!rn || !rn->bio_router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_bio_async_subscribe: required parameter is NULL (rn, rn->bio_router)");
        return -1;
    }

    (void)subscription_mask;

    return 0;
}

/*=============================================================================
 * OTHER SYSTEM INTEGRATIONS
 *===========================================================================*/

int rn_immune_connect(nimcp_red_nucleus_t* rn,
                      struct nimcp_immune_system* immune) {
    if (!rn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn is NULL");

        return -1;

    }
    rn->immune = immune;
    NIMCP_LOG_DEBUG(RN_LOG_TAG, "Connected to immune system");
    return 0;
}

int rn_security_connect(nimcp_red_nucleus_t* rn,
                        struct nimcp_security_context* security) {
    if (!rn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn is NULL");

        return -1;

    }
    rn->security = security;
    NIMCP_LOG_DEBUG(RN_LOG_TAG, "Connected to security context");
    return 0;
}

int rn_snn_connect(nimcp_red_nucleus_t* rn, struct nimcp_snn_network* snn,
                   struct nimcp_plasticity_engine* plasticity) {
    if (!rn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn is NULL");

        return -1;

    }
    rn->snn = snn;
    rn->plasticity = plasticity;
    NIMCP_LOG_DEBUG(RN_LOG_TAG, "Connected to SNN/plasticity");
    return 0;
}

int rn_hypothalamus_connect(nimcp_red_nucleus_t* rn,
                            struct nimcp_hypothalamus* hypo) {
    if (!rn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn is NULL");

        return -1;

    }
    rn->hypothalamus = hypo;
    NIMCP_LOG_DEBUG(RN_LOG_TAG, "Connected to hypothalamus");
    return 0;
}

int rn_thalamus_connect(nimcp_red_nucleus_t* rn, struct nimcp_thalamus* thalamus) {
    if (!rn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn is NULL");

        return -1;

    }
    rn->thalamus = thalamus;
    NIMCP_LOG_DEBUG(RN_LOG_TAG, "Connected to thalamus");
    return 0;
}

int rn_cognitive_connect(nimcp_red_nucleus_t* rn,
                         struct nimcp_cognitive_hub* hub) {
    if (!rn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn is NULL");

        return -1;

    }
    rn->cognitive_hub = hub;
    NIMCP_LOG_DEBUG(RN_LOG_TAG, "Connected to cognitive hub");
    return 0;
}

int rn_training_connect(nimcp_red_nucleus_t* rn,
                        struct nimcp_training_context* training) {
    if (!rn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn is NULL");

        return -1;

    }
    rn->training = training;
    NIMCP_LOG_DEBUG(RN_LOG_TAG, "Connected to training system");
    return 0;
}

int rn_perception_connect(nimcp_red_nucleus_t* rn,
                          struct nimcp_perception_system* perception) {
    if (!rn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn is NULL");

        return -1;

    }
    rn->perception = perception;
    NIMCP_LOG_DEBUG(RN_LOG_TAG, "Connected to perception system");
    return 0;
}

int rn_symbolic_connect(nimcp_red_nucleus_t* rn,
                        struct nimcp_symbolic_engine* symbolic) {
    if (!rn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn is NULL");

        return -1;

    }
    rn->symbolic = symbolic;
    NIMCP_LOG_DEBUG(RN_LOG_TAG, "Connected to symbolic engine");
    return 0;
}

int rn_swarm_connect(nimcp_red_nucleus_t* rn,
                     struct nimcp_swarm_context* swarm) {
    if (!rn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn is NULL");

        return -1;

    }
    rn->swarm = swarm;
    NIMCP_LOG_DEBUG(RN_LOG_TAG, "Connected to swarm system");
    return 0;
}

int rn_dragonfly_connect(nimcp_red_nucleus_t* rn,
                         struct nimcp_dragonfly_context* dragonfly) {
    if (!rn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn is NULL");

        return -1;

    }
    rn->dragonfly = dragonfly;
    NIMCP_LOG_DEBUG(RN_LOG_TAG, "Connected to dragonfly system");
    return 0;
}

int rn_portia_connect(nimcp_red_nucleus_t* rn,
                      struct nimcp_portia_context* portia) {
    if (!rn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn is NULL");

        return -1;

    }
    rn->portia = portia;
    NIMCP_LOG_DEBUG(RN_LOG_TAG, "Connected to portia system");
    return 0;
}

int rn_qmc_connect(nimcp_red_nucleus_t* rn, struct nimcp_qmc_context* qmc) {
    if (!rn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn is NULL");

        return -1;

    }
    rn->qmc = qmc;
    NIMCP_LOG_DEBUG(RN_LOG_TAG, "Connected to QMC system");
    return 0;
}

int rn_omni_connect(nimcp_red_nucleus_t* rn, struct nimcp_omni_predictor* omni) {
    if (!rn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn is NULL");

        return -1;

    }
    rn->omni = omni;
    NIMCP_LOG_DEBUG(RN_LOG_TAG, "Connected to omnidirectional predictor");
    return 0;
}

int rn_substrate_connect(nimcp_red_nucleus_t* rn,
                         struct nimcp_neural_substrate* substrate) {
    if (!rn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn is NULL");

        return -1;

    }
    rn->substrate = substrate;
    NIMCP_LOG_DEBUG(RN_LOG_TAG, "Connected to neural substrate");
    return 0;
}

/*=============================================================================
 * UPDATE AND STATE
 *===========================================================================*/

int rn_update(nimcp_red_nucleus_t* rn, float dt) {
    if (!rn || !rn->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_update: required parameter is NULL (rn, rn->initialized)");
        return -1;
    }

    nimcp_mutex_lock(rn->mutex);

    /* Process queued commands */
    if (!rn->command_active && rn->queue_size > 0) {
        /* Get next command from queue */
        memcpy(&rn->current_command, &rn->command_queue[rn->queue_head],
               sizeof(rn_motor_command_t));
        rn->queue_head = (rn->queue_head + 1) % rn->queue_capacity;
        rn->queue_size--;
        rn->command_active = true;
    }

    /* Process active command */
    if (rn->command_active) {
        rn_effector_t eff = rn->current_command.effector;
        float cmd_output = rn->current_command.magnitude;

        /* Apply cortical modulation */
        cmd_output *= (0.5f + rn->cortical_input[rn->current_command.type] * 0.5f);

        /* Apply learning adaptation */
        if (eff < RN_EFFECTOR_COUNT) {
            cmd_output *= rn->learning[eff].adaptation_gain;
        }

        /* Apply subdivision modulation */
        cmd_output *= rn->config.magnocellular_weight *
                      rn->subdivisions.modulation[RN_SUBDIV_MAGNOCELLULAR];

        /* Apply damping */
        float damped = cmd_output - rn->config.damping_coefficient *
                       rn->rubrospinal_output[eff];

        /* Update rubrospinal output */
        rn->rubrospinal_output[eff] += damped * dt * 10.0f;
        rn->rubrospinal_output[eff] =
            nimcp_clampf(rn->rubrospinal_output[eff], -1.0f, 1.0f);

        /* Update duration */
        rn->current_command.duration_ms -= dt * 1000.0f;
        if (rn->current_command.duration_ms <= 0) {
            rn->command_active = false;
            rn->stats.commands_completed++;
        }
    }

    /* Process trajectory if active */
    if (rn->current_trajectory && rn->trajectory_index < rn->current_trajectory->num_points) {
        rn_trajectory_point_t* target =
            &rn->current_trajectory->points[rn->trajectory_index];

        rn->trajectory_progress += (fabsf(rn->current_trajectory->total_duration_ms) > 1e-10f) ?
            dt * 1000.0f / rn->current_trajectory->total_duration_ms : 0.0f;

        if (rn->trajectory_progress >= ((rn->current_trajectory->num_points > 0) ?
            1.0f / rn->current_trajectory->num_points : 1.0f)) {
            rn->trajectory_index++;
            rn->trajectory_progress = 0.0f;
        }

        /* Generate position command from trajectory */
        if (rn->trajectory_index < rn->current_trajectory->num_points) {
            rn_effector_t eff = rn->current_trajectory->effector;
            float pos_mag = vector3_magnitude(&target->position);
            rn->rubrospinal_output[eff] += pos_mag * 0.1f * dt;
            rn->rubrospinal_output[eff] =
                nimcp_clampf(rn->rubrospinal_output[eff], -1.0f, 1.0f);
        }
    }

    /* Compute total output magnitude */
    rn->output_magnitude = 0.0f;
    for (int i = 0; i < RN_EFFECTOR_COUNT; i++) {
        rn->output_magnitude += fabsf(rn->rubrospinal_output[i]);
    }
    rn->output_magnitude /= RN_EFFECTOR_COUNT;

    /* Decay subdivision activities */
    for (int i = 0; i < RN_SUBDIV_COUNT; i++) {
        rn->subdivisions.activity[i] *= (1.0f - 0.1f * dt);
    }

    /* Decay skill levels slightly (use it or lose it) */
    for (int i = 0; i < RN_EFFECTOR_COUNT; i++) {
        if (rn->rubrospinal_output[i] < 0.1f) {
            rn->learning[i].skill_level -= rn->config.skill_decay_rate * dt;
            rn->learning[i].skill_level =
                fmaxf(0.0f, rn->learning[i].skill_level);
        }
    }

    /* Update statistics */
    rn->stats.avg_error_magnitude =
        (rn->stats.avg_error_magnitude * rn->stats.errors_detected +
         rn->cumulative_error) / (rn->stats.errors_detected + 1);

    /* Update timestamp */
    rn->last_update_us += (uint64_t)(dt * 1000000.0f);

    nimcp_mutex_unlock(rn->mutex);
    return 0;
}

int rn_get_stats(const nimcp_red_nucleus_t* rn, rn_stats_t* stats) {
    if (!rn || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_get_stats: required parameter is NULL (rn, stats)");
        return -1;
    }

    nimcp_mutex_lock(((nimcp_red_nucleus_t*)rn)->mutex);
    memcpy(stats, &rn->stats, sizeof(rn_stats_t));
    nimcp_mutex_unlock(((nimcp_red_nucleus_t*)rn)->mutex);

    return 0;
}

float rn_get_subdivision_activity(const nimcp_red_nucleus_t* rn,
                                  rn_subdivision_t subdiv) {
    if (!rn || subdiv >= RN_SUBDIV_COUNT) return 0.0f;
    return rn->subdivisions.activity[subdiv];
}

int rn_clear_commands(nimcp_red_nucleus_t* rn) {
    if (!rn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn is NULL");

        return -1;

    }

    nimcp_mutex_lock(rn->mutex);

    rn->queue_head = 0;
    rn->queue_tail = 0;
    rn->queue_size = 0;
    rn->command_active = false;

    nimcp_mutex_unlock(rn->mutex);
    return 0;
}

int rn_abort_command(nimcp_red_nucleus_t* rn) {
    if (!rn) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn is NULL");

        return -1;

    }

    nimcp_mutex_lock(rn->mutex);

    if (rn->command_active) {
        rn->command_active = false;
        rn->stats.commands_aborted++;
        NIMCP_LOG_INFO(RN_LOG_TAG, "Command aborted");
    }

    nimcp_mutex_unlock(rn->mutex);
    return 0;
}

/*=============================================================================
 * QUANTUM OPTIMIZATION
 *===========================================================================*/

int rn_qmc_optimize_commands(nimcp_red_nucleus_t* rn) {
    if (!rn || !rn->qmc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_qmc_optimize_commands: required parameter is NULL (rn, rn->qmc)");
        return -1;
    }

    /* Use QMC for motor command optimization */
    NIMCP_LOG_DEBUG(RN_LOG_TAG, "QMC command optimization requested");
    return 0;
}

int rn_qmcts_trajectory_search(nimcp_red_nucleus_t* rn,
                               const rn_vector3_t* start,
                               const rn_vector3_t* goal,
                               uint32_t num_iterations,
                               rn_trajectory_t* trajectory) {
    if (!rn || !start || !goal || !trajectory || !rn->qmc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rn_qmc_optimize_commands: required parameter is NULL (rn, start, goal, trajectory, rn->qmc)");
        return -1;
    }

    NIMCP_LOG_DEBUG(RN_LOG_TAG, "QMCTS trajectory search: %u iterations",
                    num_iterations);

    /* Fallback to simple linear trajectory */
    trajectory->num_points = 2;
    trajectory->points = (rn_trajectory_point_t*)nimcp_malloc(
        2 * sizeof(rn_trajectory_point_t));
    if (!trajectory->points) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rn_qmc_optimize_commands: trajectory->points is NULL");
        return -1;
    }

    /* Start point */
    trajectory->points[0].position = *start;
    trajectory->points[0].velocity = (rn_vector3_t){0.0f, 0.0f, 0.0f};
    trajectory->points[0].time_ms = 0.0f;

    /* Goal point */
    trajectory->points[1].position = *goal;
    trajectory->points[1].velocity = (rn_vector3_t){0.0f, 0.0f, 0.0f};
    trajectory->points[1].time_ms = 1000.0f;

    trajectory->effector = RN_EFFECTOR_FORELIMB_DISTAL;
    trajectory->total_duration_ms = 1000.0f;
    trajectory->smooth_interpolation = true;

    return 0;
}

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

const char* rn_subdivision_string(rn_subdivision_t subdiv) {
    static const char* names[] = {
        "Magnocellular (RNm)",
        "Parvocellular (RNp)"
    };
    if (subdiv >= RN_SUBDIV_COUNT) return "Unknown";
    return names[subdiv];
}

const char* rn_cmd_type_string(rn_motor_cmd_type_t cmd_type) {
    static const char* names[] = {
        "Velocity",
        "Force",
        "Position",
        "Trajectory",
        "Posture",
        "Balance",
        "Skilled"
    };
    if (cmd_type >= RN_CMD_COUNT) return "Unknown";
    return names[cmd_type];
}

const char* rn_effector_string(rn_effector_t effector) {
    static const char* names[] = {
        "Forelimb Proximal",
        "Forelimb Distal",
        "Hindlimb Proximal",
        "Hindlimb Distal",
        "Axial"
    };
    if (effector >= RN_EFFECTOR_COUNT) return "Unknown";
    return names[effector];
}

const char* rn_error_type_string(rn_error_type_t error_type) {
    static const char* names[] = {
        "Position",
        "Velocity",
        "Force",
        "Timing",
        "Trajectory"
    };
    if (error_type >= RN_ERROR_COUNT) return "Unknown";
    return names[error_type];
}

const char* rn_bio_msg_string(rn_bio_msg_type_t msg_type) {
    static const char* names[] = {
        "Motor Command",
        "Error Signal",
        "Learning Update",
        "Cerebellar Input",
        "Olivary Output",
        "Thalamic Output",
        "Posture Adjust",
        "State Request"
    };
    if (msg_type >= RN_BIO_MSG_COUNT) return "Unknown";
    return names[msg_type];
}
