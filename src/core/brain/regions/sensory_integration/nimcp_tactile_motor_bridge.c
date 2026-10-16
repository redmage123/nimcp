/**
 * @file nimcp_tactile_motor_bridge.c
 * @brief Tactile-Motor Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-12
 *
 * @author NIMCP Development Team
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/sensory_integration/nimcp_tactile_motor_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"

BRIDGE_BOILERPLATE_MESH_ONLY(tactile_motor_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "TACTILE_MOTOR_BRIDGE"


/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct tactile_motor_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    tactile_motor_config_t config;

    nimcp_somatosensory_t* soma;
    void* motor_cortex;

    bool is_connected;
    tactile_motor_status_t status;
    tactile_motor_mode_t mode;

    /* Grip state */
    tactile_motor_grip_t current_grip;
    uint32_t active_effector;

    tactile_motor_stats_t stats;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static float randf(void) {
    return (float)nimcp_tl_rand() / (float)RAND_MAX;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int tactile_motor_default_config(tactile_motor_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tactile_motor_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(tactile_motor_config_t));

    config->max_effectors = TACTILE_MOTOR_MAX_EFFECTORS;
    config->grip_gain = TACTILE_MOTOR_GRIP_GAIN;
    config->slip_threshold = TACTILE_MOTOR_SLIP_THRESHOLD;
    config->exploration_speed = 0.1f;
    config->force_limit = 10.0f;
    config->enable_slip_detection = true;
    config->enable_force_control = true;
    config->enable_exploration = true;
    config->enable_logging = false;

    return 0;
}

tactile_motor_bridge_t* tactile_motor_bridge_create(const tactile_motor_config_t* config) {
    tactile_motor_bridge_t* bridge = (tactile_motor_bridge_t*)nimcp_calloc(1, sizeof(tactile_motor_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "tactile_motor_bridge_create: allocation failed");
        return NULL;
    }

    bridge_base_init(&bridge->base, 0, "tactile_motor");

    if (config) {
        memcpy(&bridge->config, config, sizeof(tactile_motor_config_t));
    } else {
        tactile_motor_default_config(&bridge->config);
    }

    bridge->is_connected = false;
    bridge->status = TACTILE_MOTOR_STATUS_IDLE;
    bridge->mode = TACTILE_MOTOR_MODE_IDLE;

    NIMCP_LOGGING_INFO("Created %s bridge", "tactile_motor");
    return bridge;
}

void tactile_motor_bridge_destroy(tactile_motor_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "tactile_motor");
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection Implementation
 * ============================================================================ */

int tactile_motor_connect(tactile_motor_bridge_t* bridge, nimcp_somatosensory_t* soma, void* motor_cortex) {
    if (!bridge || !soma) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tactile_motor_connect: required parameter is NULL (bridge, soma)");
        return -1;
    }

    bridge->soma = soma;
    bridge->motor_cortex = motor_cortex;
    bridge->is_connected = true;
    bridge->status = TACTILE_MOTOR_STATUS_IDLE;

    return 0;
}

int tactile_motor_disconnect(tactile_motor_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tactile_motor_disconnect: bridge is NULL");
        return -1;
    }

    bridge->soma = NULL;
    bridge->motor_cortex = NULL;
    bridge->is_connected = false;

    return 0;
}

bool tactile_motor_is_connected(const tactile_motor_bridge_t* bridge) {
    return bridge && bridge->is_connected;
}

/* ============================================================================
 * Grip Control Implementation
 * ============================================================================ */

int tactile_motor_init_grasp(tactile_motor_bridge_t* bridge, uint32_t effector) {
    if (!bridge || !bridge->is_connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tactile_motor_init_grasp: required parameter is NULL (bridge, bridge->is_connected)");
        return -1;
    }

    bridge->active_effector = effector;
    bridge->mode = TACTILE_MOTOR_MODE_GRASPING;
    bridge->status = TACTILE_MOTOR_STATUS_ACTIVE;

    /* Initialize grip state */
    bridge->current_grip.grip_force = 0.0f;
    bridge->current_grip.slip_margin = 0.2f;
    bridge->current_grip.object_weight_estimate = 0.0f;
    bridge->current_grip.friction_estimate = 0.5f;
    bridge->current_grip.is_slipping = false;
    bridge->current_grip.stable_grasp = false;

    return 0;
}

int tactile_motor_update_grip(tactile_motor_bridge_t* bridge, const touch_event_t* feedback,
                              tactile_motor_grip_t* grip) {
    if (!bridge || !feedback || !grip) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tactile_motor_init_grasp: required parameter is NULL (bridge, feedback, grip)");
        return -1;
    }
    if (!bridge->config.enable_force_control) {
        return -1;  /* Feature disabled, not an error worth reporting to immune */
    }

    /* Update grip based on tactile feedback */
    float pressure = feedback->pressure;
    float slip_velocity = feedback->slip_velocity;

    /* Detect slip */
    if (bridge->config.enable_slip_detection && slip_velocity > bridge->config.slip_threshold) {
        bridge->current_grip.is_slipping = true;
        bridge->stats.slip_events++;

        /* Increase grip force */
        bridge->current_grip.grip_force += bridge->config.grip_gain * slip_velocity;
        bridge->stats.grip_adjustments++;
    } else {
        bridge->current_grip.is_slipping = false;
    }

    /* Update weight estimate from pressure */
    bridge->current_grip.object_weight_estimate = pressure * 0.5f;

    /* Update friction estimate */
    if (slip_velocity > 0.01f && bridge->current_grip.grip_force > 0.0f) {
        bridge->current_grip.friction_estimate =
            bridge->current_grip.grip_force / (slip_velocity + 0.01f);
    }

    /* Clamp grip force */
    if (bridge->current_grip.grip_force > bridge->config.force_limit) {
        bridge->current_grip.grip_force = bridge->config.force_limit;
    }

    /* Check stability */
    bridge->current_grip.stable_grasp = !bridge->current_grip.is_slipping &&
                                        bridge->current_grip.grip_force > 0.1f;

    /* Copy to output */
    memcpy(grip, &bridge->current_grip, sizeof(tactile_motor_grip_t));

    bridge->stats.feedback_received++;
    {
        float new_force = bridge->stats.avg_grip_force * 0.95f +
                          bridge->current_grip.grip_force * 0.05f;
        if (isfinite(new_force)) bridge->stats.avg_grip_force = new_force;
    }

    return 0;
}

int tactile_motor_adjust_grip_force(tactile_motor_bridge_t* bridge, float target_force) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tactile_motor_adjust_grip_force: bridge is NULL");
        return -1;
    }

    bridge->current_grip.grip_force = target_force;
    if (bridge->current_grip.grip_force > bridge->config.force_limit) {
        bridge->current_grip.grip_force = bridge->config.force_limit;
    }
    if (bridge->current_grip.grip_force < 0.0f) {
        bridge->current_grip.grip_force = 0.0f;
    }

    bridge->stats.grip_adjustments++;

    return 0;
}

int tactile_motor_detect_slip(tactile_motor_bridge_t* bridge, const touch_event_t* feedback,
                              bool* slipping) {
    if (!bridge || !feedback || !slipping) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tactile_motor_adjust_grip_force: required parameter is NULL (bridge, feedback, slipping)");
        return -1;
    }

    *slipping = feedback->slip_velocity > bridge->config.slip_threshold;

    return 0;
}

int tactile_motor_release_grasp(tactile_motor_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tactile_motor_release_grasp: bridge is NULL");
        return -1;
    }

    bridge->current_grip.grip_force = 0.0f;
    bridge->current_grip.stable_grasp = false;
    bridge->mode = TACTILE_MOTOR_MODE_RELEASING;
    bridge->status = TACTILE_MOTOR_STATUS_ACTIVE;

    return 0;
}

/* ============================================================================
 * Motor Command Implementation
 * ============================================================================ */

int tactile_motor_send_command(tactile_motor_bridge_t* bridge, const tactile_motor_command_t* cmd) {
    if (!bridge || !cmd) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tactile_motor_send_command: required parameter is NULL (bridge, cmd)");
        return -1;
    }
    (void)cmd;

    bridge->stats.commands_sent++;

    return 0;
}

int tactile_motor_receive_feedback(tactile_motor_bridge_t* bridge, uint32_t effector,
                                   float* feedback) {
    if (!bridge || !feedback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tactile_motor_send_command: required parameter is NULL (bridge, feedback)");
        return -1;
    }
    (void)effector;

    /* Simulated feedback */
    *feedback = bridge->current_grip.grip_force + randf() * 0.1f;

    bridge->stats.feedback_received++;

    return 0;
}

int tactile_motor_compute_prediction_error(tactile_motor_bridge_t* bridge,
                                           tactile_motor_command_t* cmd) {
    if (!bridge || !cmd) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tactile_motor_send_command: required parameter is NULL (bridge, cmd)");
        return -1;
    }

    if (cmd->predicted_feedback && cmd->actual_feedback && cmd->command_dim > 0) {
        float error = 0.0f;
        for (uint32_t i = 0; i < cmd->command_dim; i++) {
            float diff = cmd->predicted_feedback[i] - cmd->actual_feedback[i];
            error += diff * diff;
        }
        cmd->prediction_error = sqrtf(error / cmd->command_dim);

        {
            float new_err = bridge->stats.avg_prediction_error * 0.9f +
                            cmd->prediction_error * 0.1f;
            if (isfinite(new_err)) bridge->stats.avg_prediction_error = new_err;
        }
    }

    return 0;
}

/* ============================================================================
 * Exploration Implementation
 * ============================================================================ */

int tactile_motor_start_exploration(tactile_motor_bridge_t* bridge, const float* start_pos,
                                    const float* bounds, tactile_motor_exploration_t* exploration) {
    if (!bridge || !exploration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tactile_motor_send_command: required parameter is NULL (bridge, exploration)");
        return -1;
    }
    if (!bridge->config.enable_exploration) {
        return -1;  /* Feature disabled, not an error worth reporting to immune */
    }
    (void)start_pos;
    (void)bounds;

    bridge->mode = TACTILE_MOTOR_MODE_EXPLORING;
    bridge->status = TACTILE_MOTOR_STATUS_ACTIVE;

    /* Initialize exploration */
    exploration->num_waypoints = 10;
    exploration->waypoints = (float*)nimcp_calloc(exploration->num_waypoints * 3, sizeof(float));
    if (!exploration->waypoints) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "tactile_motor_send_command: exploration->waypoints is NULL");
        return -1;
    }

    /* Generate waypoints */
    for (uint32_t i = 0; i < exploration->num_waypoints; i++) {
        exploration->waypoints[i * 3] = randf() * 10.0f;
        exploration->waypoints[i * 3 + 1] = randf() * 10.0f;
        exploration->waypoints[i * 3 + 2] = 0.0f;
    }

    exploration->current_waypoint = 0;
    exploration->collected_samples = NULL;
    exploration->num_samples = 0;
    exploration->exploration_complete = false;

    return 0;
}

int tactile_motor_step_exploration(tactile_motor_bridge_t* bridge,
                                   tactile_motor_exploration_t* exploration,
                                   touch_event_t* sample) {
    if (!bridge || !exploration || !sample) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tactile_motor_send_command: required parameter is NULL (bridge, exploration, sample)");
        return -1;
    }

    if (exploration->current_waypoint >= exploration->num_waypoints) {
        exploration->exploration_complete = true;
        return 0;
    }

    /* Simulate collecting a sample */
    sample->pressure = randf();
    sample->slip_velocity = randf() * 0.05f;

    exploration->current_waypoint++;
    exploration->num_samples++;

    return 0;
}

int tactile_motor_finish_exploration(tactile_motor_bridge_t* bridge,
                                     tactile_motor_exploration_t* exploration) {
    if (!bridge || !exploration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tactile_motor_send_command: required parameter is NULL (bridge, exploration)");
        return -1;
    }

    exploration->exploration_complete = true;
    bridge->mode = TACTILE_MOTOR_MODE_IDLE;
    bridge->status = TACTILE_MOTOR_STATUS_IDLE;

    bridge->stats.explorations_completed++;

    return 0;
}

/* ============================================================================
 * Mode Control Implementation
 * ============================================================================ */

int tactile_motor_set_mode(tactile_motor_bridge_t* bridge, tactile_motor_mode_t mode) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tactile_motor_set_mode: bridge is NULL");
        return -1;
    }
    bridge->mode = mode;
    return 0;
}

tactile_motor_mode_t tactile_motor_get_mode(const tactile_motor_bridge_t* bridge) {
    if (!bridge) return TACTILE_MOTOR_MODE_IDLE;
    return bridge->mode;
}

tactile_motor_status_t tactile_motor_get_status(const tactile_motor_bridge_t* bridge) {
    if (!bridge) return TACTILE_MOTOR_STATUS_ERROR;
    return bridge->status;
}

/* ============================================================================
 * Statistics Implementation
 * ============================================================================ */

int tactile_motor_get_stats(const tactile_motor_bridge_t* bridge, tactile_motor_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tactile_motor_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    memcpy(stats, &bridge->stats, sizeof(tactile_motor_stats_t));
    return 0;
}

int tactile_motor_reset_stats(tactile_motor_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tactile_motor_reset_stats: bridge is NULL");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(tactile_motor_stats_t));
    return 0;
}

void tactile_motor_print_summary(const tactile_motor_bridge_t* bridge) {
    if (!bridge) return;

    printf("=== Tactile-Motor Bridge Summary ===\n");
    printf("Connected: %s\n", bridge->is_connected ? "Yes" : "No");
    printf("Mode: %d\n", bridge->mode);
    printf("Commands Sent: %lu\n", (unsigned long)bridge->stats.commands_sent);
    printf("Feedback Received: %lu\n", (unsigned long)bridge->stats.feedback_received);
    printf("Slip Events: %lu\n", (unsigned long)bridge->stats.slip_events);
    printf("Grip Adjustments: %lu\n", (unsigned long)bridge->stats.grip_adjustments);
    printf("Explorations Completed: %lu\n", (unsigned long)bridge->stats.explorations_completed);
    printf("Avg Prediction Error: %.4f\n", bridge->stats.avg_prediction_error);
    printf("Avg Grip Force: %.2f\n", bridge->stats.avg_grip_force);
    printf("====================================\n");
}

/* ============================================================================
 * Cleanup
 * ============================================================================ */

void tactile_motor_exploration_free(tactile_motor_exploration_t* exploration) {
    if (!exploration) return;
    nimcp_free(exploration->waypoints);
    nimcp_free(exploration->collected_samples);
    exploration->waypoints = NULL;
    exploration->collected_samples = NULL;
}
