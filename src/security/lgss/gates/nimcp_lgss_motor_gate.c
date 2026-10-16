/**
 * @file nimcp_lgss_motor_gate.c
 * @brief LGSS Motor Output Gate implementation
 *
 * WHAT: Implements gated motor output control with comprehensive safety constraints.
 * WHY:  Ensures all motor commands pass through safety validation before execution.
 * HOW:  Validates force/velocity/acceleration limits, collision detection, human safety.
 */

#include "security/lgss/gates/nimcp_lgss_motor_gate.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __linux__
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(lgss_motor_gate, MESH_ADAPTER_CATEGORY_SECURITY)


#endif

/* =============================================================================
 * Internal Structures
 * ============================================================================= */

/**
 * @brief Motor gate internal structure
 */
struct motor_gate {
    uint32_t magic;                     /**< Magic number for validation */
    motor_safety_constraints_t constraints[MOTOR_REGION_COUNT]; /**< Per-region constraints */
    bool region_locked[MOTOR_REGION_COUNT]; /**< Per-region lock state */
    bool emergency_stop_active;         /**< Emergency stop state */
    bool enabled;                       /**< Whether gate is enabled */
    motor_gate_config_t config;         /**< Gate configuration */
    motor_gate_stats_t stats;           /**< Operational statistics */
    uint32_t next_sequence_id;          /**< Next sequence ID for validation */
};

/* =============================================================================
 * Helper Functions
 * ============================================================================= */

/**
 * @brief Get current timestamp in nanoseconds
 */
static uint64_t get_timestamp_ns(void) {
#ifdef __linux__
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#else
    return 0;
#endif
}

/**
 * @brief Calculate vector magnitude
 */
static float vector_magnitude(const float v[NIMCP_MOTOR_POSITION_DIM]) {
    return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

/**
 * @brief Initialize default constraints for a region
 */
static void init_default_constraints(motor_safety_constraints_t* constraints) {
    constraints->force_limit = NIMCP_MOTOR_DEFAULT_FORCE_LIMIT;
    constraints->velocity_limit = NIMCP_MOTOR_DEFAULT_VELOCITY_LIMIT;
    constraints->acceleration_limit = NIMCP_MOTOR_DEFAULT_ACCEL_LIMIT;
    constraints->allow_contact = false;
    constraints->allow_human_contact = false;
    constraints->contact_force_limit = 10.0f;  /* 10N for contact operations */
    constraints->jerk_limit = 50.0f;           /* 50 m/s^3 */
    constraints->enabled = true;
}

/**
 * @brief Validate gate structure
 */
static bool validate_gate(const motor_gate_t* gate) {
    return gate != NULL && gate->magic == NIMCP_MOTOR_GATE_MAGIC;
}

/**
 * @brief Validate motor region enum
 */
static bool validate_region(motor_region_t region) {
    return region >= 0 && region < MOTOR_REGION_COUNT;
}

/**
 * @brief Fill violation details
 */
static void fill_violation(
    motor_gate_violation_t* violation,
    motor_exec_result_t result,
    motor_region_t region,
    float actual,
    float limit,
    const char* description
) {
    if (violation == NULL) {
        return;
    }
    violation->result = result;
    violation->region = region;
    violation->actual_value = actual;
    violation->limit_value = limit;
    violation->timestamp = get_timestamp_ns();
    if (description != NULL) {
        snprintf(violation->description, sizeof(violation->description), "%s", description);
    } else {
        violation->description[0] = '\0';
    }
}

/* =============================================================================
 * Public Functions
 * ============================================================================= */

motor_gate_t* motor_gate_create(const motor_gate_config_t* config) {
    motor_gate_t* gate = (motor_gate_t*)nimcp_calloc(1, sizeof(motor_gate_t));
    NIMCP_API_CHECK_ALLOC(gate, "Failed to allocate motor gate");

    gate->magic = NIMCP_MOTOR_GATE_MAGIC;
    gate->enabled = true;
    gate->emergency_stop_active = false;
    gate->next_sequence_id = 1;

    /* Initialize default constraints for all regions */
    for (int i = 0; i < MOTOR_REGION_COUNT; i++) {
        init_default_constraints(&gate->constraints[i]);
        gate->region_locked[i] = false;
    }

    /* Apply region-specific default adjustments */
    /* Eyes have much lower force limits */
    gate->constraints[MOTOR_REGION_EYES].force_limit = 0.5f;
    gate->constraints[MOTOR_REGION_EYES].velocity_limit = 0.5f;
    gate->constraints[MOTOR_REGION_EYES].allow_contact = false;

    /* Face has lower force limits */
    gate->constraints[MOTOR_REGION_FACE].force_limit = 5.0f;

    /* Head/neck needs careful handling */
    gate->constraints[MOTOR_REGION_HEAD].force_limit = 20.0f;
    gate->constraints[MOTOR_REGION_HEAD].velocity_limit = 1.0f;

    /* Apply configuration if provided */
    if (config != NULL) {
        memcpy(&gate->config, config, sizeof(motor_gate_config_t));
    } else {
        /* Default configuration */
        gate->config.emergency_stop_enabled = true;
        gate->config.global_force_scale = 1.0f;
        gate->config.global_velocity_scale = 1.0f;
        gate->config.collision_callback = NULL;
        gate->config.proximity_callback = NULL;
        gate->config.callback_user_data = NULL;
        gate->config.log_all_commands = false;
        gate->config.strict_mode = false;
    }

    /* Initialize statistics */
    memset(&gate->stats, 0, sizeof(motor_gate_stats_t));

    return gate;
}

void motor_gate_destroy(motor_gate_t* gate) {
    if (gate == NULL) {
        return;
    }
    if (gate->magic != NIMCP_MOTOR_GATE_MAGIC) {
        return;  /* Invalid gate */
    }

    gate->magic = 0;  /* Invalidate before free */
    nimcp_free(gate);
}

nimcp_result_t motor_gate_set_constraints(
    motor_gate_t* gate,
    motor_region_t region,
    const motor_safety_constraints_t* constraints
) {
    NIMCP_API_CHECK_NULL(gate, NIMCP_ERROR_INVALID_PARAM, "NULL gate in set_constraints");
    NIMCP_API_CHECK(validate_gate(gate), NIMCP_ERROR_INVALID_PARAM, "Invalid gate magic in set_constraints");
    NIMCP_API_CHECK(validate_region(region), NIMCP_ERROR_INVALID_PARAM, "Invalid region in set_constraints");
    NIMCP_API_CHECK_NULL(constraints, NIMCP_ERROR_NULL_POINTER, "NULL constraints in set_constraints");

    /* Validate constraint values */
    if (constraints->force_limit < 0.0f ||
        constraints->velocity_limit < 0.0f ||
        constraints->acceleration_limit < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "lgss_motor_gate: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memcpy(&gate->constraints[region], constraints, sizeof(motor_safety_constraints_t));
    return NIMCP_SUCCESS;
}

nimcp_result_t motor_gate_get_constraints(
    const motor_gate_t* gate,
    motor_region_t region,
    motor_safety_constraints_t* constraints
) {
    if (!validate_gate(gate)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "lgss_motor_gate: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!validate_region(region)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "lgss_motor_gate: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (constraints == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_motor_gate: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    memcpy(constraints, &gate->constraints[region], sizeof(motor_safety_constraints_t));
    return NIMCP_SUCCESS;
}

motor_exec_result_t motor_gate_execute(
    motor_gate_t* gate,
    const motor_command_proposal_t* proposal,
    motor_gate_violation_t* violation
) {
    uint64_t start_time = get_timestamp_ns();

    /* Validate inputs */
    if (!validate_gate(gate)) {
        fill_violation(violation, MOTOR_EXEC_ERROR, 0, 0, 0, "Invalid gate");
        return MOTOR_EXEC_ERROR;
    }

    if (proposal == NULL) {
        fill_violation(violation, MOTOR_EXEC_INVALID_PROPOSAL, 0, 0, 0, "NULL proposal");
        gate->stats.commands_submitted++;
        gate->stats.commands_rejected++;
        return MOTOR_EXEC_INVALID_PROPOSAL;
    }

    gate->stats.commands_submitted++;

    /* Check if gate is enabled */
    if (!gate->enabled) {
        fill_violation(violation, MOTOR_EXEC_GATE_DISABLED, proposal->region, 0, 0,
                      "Motor gate is disabled");
        gate->stats.commands_rejected++;
        return MOTOR_EXEC_GATE_DISABLED;
    }

    /* Check emergency stop */
    if (gate->emergency_stop_active) {
        fill_violation(violation, MOTOR_EXEC_REGION_LOCKED, proposal->region, 0, 0,
                      "Emergency stop is active");
        gate->stats.commands_rejected++;
        return MOTOR_EXEC_REGION_LOCKED;
    }

    /* Validate region */
    if (!validate_region(proposal->region)) {
        fill_violation(violation, MOTOR_EXEC_INVALID_PROPOSAL, 0, 0, 0,
                      "Invalid motor region");
        gate->stats.commands_rejected++;
        return MOTOR_EXEC_INVALID_PROPOSAL;
    }

    /* Check region lock */
    if (gate->region_locked[proposal->region]) {
        fill_violation(violation, MOTOR_EXEC_REGION_LOCKED, proposal->region, 0, 0,
                      "Target region is locked");
        gate->stats.commands_rejected++;
        return MOTOR_EXEC_REGION_LOCKED;
    }

    /* Check region enabled */
    const motor_safety_constraints_t* constraints = &gate->constraints[proposal->region];
    if (!constraints->enabled) {
        fill_violation(violation, MOTOR_EXEC_REGION_LOCKED, proposal->region, 0, 0,
                      "Target region is disabled");
        gate->stats.commands_rejected++;
        return MOTOR_EXEC_REGION_LOCKED;
    }

    /* Calculate magnitudes */
    float velocity_mag = vector_magnitude(proposal->target_velocity);
    float force_mag = vector_magnitude(proposal->target_force);

    /* Apply global scaling */
    float scaled_force_limit = constraints->force_limit * gate->config.global_force_scale;
    float scaled_velocity_limit = constraints->velocity_limit * gate->config.global_velocity_scale;

    /* Check velocity limit */
    if (velocity_mag > scaled_velocity_limit) {
        fill_violation(violation, MOTOR_EXEC_VELOCITY_VIOLATION, proposal->region,
                      velocity_mag, scaled_velocity_limit,
                      "Velocity exceeds limit");
        gate->stats.commands_rejected++;
        gate->stats.velocity_violations++;
        return MOTOR_EXEC_VELOCITY_VIOLATION;
    }

    /* Check force limit */
    if (force_mag > scaled_force_limit) {
        fill_violation(violation, MOTOR_EXEC_FORCE_VIOLATION, proposal->region,
                      force_mag, scaled_force_limit,
                      "Force exceeds limit");
        gate->stats.commands_rejected++;
        gate->stats.force_violations++;
        return MOTOR_EXEC_FORCE_VIOLATION;
    }

    /* Check contact constraints */
    if (proposal->is_contact_expected) {
        if (!constraints->allow_contact) {
            fill_violation(violation, MOTOR_EXEC_CONTACT_FORBIDDEN, proposal->region,
                          0, 0, "Contact not allowed for this region");
            gate->stats.commands_rejected++;
            gate->stats.contact_violations++;
            return MOTOR_EXEC_CONTACT_FORBIDDEN;
        }

        /* Check contact force limit */
        if (force_mag > constraints->contact_force_limit) {
            fill_violation(violation, MOTOR_EXEC_FORCE_VIOLATION, proposal->region,
                          force_mag, constraints->contact_force_limit,
                          "Contact force exceeds limit");
            gate->stats.commands_rejected++;
            gate->stats.force_violations++;
            return MOTOR_EXEC_FORCE_VIOLATION;
        }
    }

    /* Check human proximity if callback is configured */
    if (gate->config.proximity_callback != NULL) {
        float distance = gate->config.proximity_callback(
            proposal->region,
            gate->config.callback_user_data
        );

        /* If human detected nearby and contact expected */
        if (distance >= 0.0f && distance < 1.0f) {  /* Within 1 meter */
            if (!constraints->allow_human_contact) {
                fill_violation(violation, MOTOR_EXEC_HUMAN_CONTACT_RISK, proposal->region,
                              distance, 1.0f, "Human detected in proximity");
                gate->stats.commands_rejected++;
                gate->stats.human_contact_events++;
                return MOTOR_EXEC_HUMAN_CONTACT_RISK;
            }

            /* Apply extra safety margin for human interaction */
            if (force_mag > constraints->contact_force_limit * 0.5f) {
                fill_violation(violation, MOTOR_EXEC_HUMAN_CONTACT_RISK, proposal->region,
                              force_mag, constraints->contact_force_limit * 0.5f,
                              "Force too high for human proximity");
                gate->stats.commands_rejected++;
                gate->stats.human_contact_events++;
                return MOTOR_EXEC_HUMAN_CONTACT_RISK;
            }
        }
    }

    /* Check collision if callback is configured */
    if (gate->config.collision_callback != NULL) {
        bool collision = gate->config.collision_callback(
            proposal,
            gate->config.callback_user_data
        );

        if (collision) {
            fill_violation(violation, MOTOR_EXEC_COLLISION_DETECTED, proposal->region,
                          0, 0, "Collision detected");
            gate->stats.commands_rejected++;
            return MOTOR_EXEC_COLLISION_DETECTED;
        }
    }

    /* All checks passed - approve the command */
    gate->stats.commands_approved++;

    /* Update latency statistics */
    uint64_t elapsed_us = (get_timestamp_ns() - start_time) / 1000;
    float elapsed_f = (float)elapsed_us;

    /* Update average latency (exponential moving average) */
    if (gate->stats.commands_approved == 1) {
        gate->stats.avg_approval_latency_us = elapsed_f;
    } else {
        gate->stats.avg_approval_latency_us =
            gate->stats.avg_approval_latency_us * 0.95f + elapsed_f * 0.05f;
    }

    if (elapsed_f > gate->stats.max_approval_latency_us) {
        gate->stats.max_approval_latency_us = elapsed_f;
    }

    /* Clear violation output */
    if (violation != NULL) {
        memset(violation, 0, sizeof(motor_gate_violation_t));
        violation->result = MOTOR_EXEC_SUCCESS;
        violation->region = proposal->region;
        violation->timestamp = get_timestamp_ns();
    }

    return MOTOR_EXEC_SUCCESS;
}

bool motor_gate_would_violate(
    const motor_gate_t* gate,
    const motor_command_proposal_t* proposal,
    motor_gate_violation_t* violation
) {
    /* Create a mutable copy of gate for execute call */
    /* Note: This is a const-safe check - we cast away const only to call
     * execute which won't actually modify anything when checking */
    motor_gate_t* mutable_gate = (motor_gate_t*)gate;

    /* Save current stats */
    motor_gate_stats_t saved_stats;
    if (validate_gate(gate)) {
        memcpy(&saved_stats, &gate->stats, sizeof(motor_gate_stats_t));
    }

    /* Execute to check for violations */
    motor_exec_result_t result = motor_gate_execute(mutable_gate, proposal, violation);

    /* Restore stats (we don't want would_violate to affect stats) */
    if (validate_gate(gate)) {
        memcpy(&mutable_gate->stats, &saved_stats, sizeof(motor_gate_stats_t));
    }

    return result != MOTOR_EXEC_SUCCESS;
}

nimcp_result_t motor_gate_emergency_stop(motor_gate_t* gate) {
    if (!validate_gate(gate)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "motor_gate_emergency_stop: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!gate->config.emergency_stop_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_SUPPORTED, "motor_gate_emergency_stop: error condition");
        return NIMCP_ERROR_NOT_SUPPORTED;
    }

    gate->emergency_stop_active = true;

    /* Lock all regions */
    for (int i = 0; i < MOTOR_REGION_COUNT; i++) {
        gate->region_locked[i] = true;
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t motor_gate_release_emergency_stop(motor_gate_t* gate) {
    if (!validate_gate(gate)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "motor_gate_release_emergency_stop: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    gate->emergency_stop_active = false;

    /* Unlock all regions */
    for (int i = 0; i < MOTOR_REGION_COUNT; i++) {
        gate->region_locked[i] = false;
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t motor_gate_lock_region(motor_gate_t* gate, motor_region_t region) {
    if (!validate_gate(gate)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "motor_gate_lock_region: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!validate_region(region)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "motor_gate_lock_region: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    gate->region_locked[region] = true;
    return NIMCP_SUCCESS;
}

nimcp_result_t motor_gate_unlock_region(motor_gate_t* gate, motor_region_t region) {
    if (!validate_gate(gate)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "motor_gate_unlock_region: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!validate_region(region)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "motor_gate_unlock_region: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    gate->region_locked[region] = false;
    return NIMCP_SUCCESS;
}

nimcp_result_t motor_gate_get_stats(
    const motor_gate_t* gate,
    motor_gate_stats_t* stats
) {
    if (!validate_gate(gate)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "lgss_motor_gate: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (stats == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_motor_gate: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    memcpy(stats, &gate->stats, sizeof(motor_gate_stats_t));
    return NIMCP_SUCCESS;
}

nimcp_result_t motor_gate_reset_stats(motor_gate_t* gate) {
    if (!validate_gate(gate)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "motor_gate_reset_stats: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memset(&gate->stats, 0, sizeof(motor_gate_stats_t));
    return NIMCP_SUCCESS;
}

const char* motor_region_name(motor_region_t region) {
    switch (region) {
        case MOTOR_REGION_HANDS: return "HANDS";
        case MOTOR_REGION_ARMS:  return "ARMS";
        case MOTOR_REGION_LEGS:  return "LEGS";
        case MOTOR_REGION_HEAD:  return "HEAD";
        case MOTOR_REGION_EYES:  return "EYES";
        case MOTOR_REGION_FACE:  return "FACE";
        case MOTOR_REGION_TORSO: return "TORSO";
        default:                 return "UNKNOWN";
    }
}

const char* motor_exec_result_name(motor_exec_result_t result) {
    switch (result) {
        case MOTOR_EXEC_SUCCESS:           return "SUCCESS";
        case MOTOR_EXEC_FORCE_VIOLATION:   return "FORCE_VIOLATION";
        case MOTOR_EXEC_VELOCITY_VIOLATION: return "VELOCITY_VIOLATION";
        case MOTOR_EXEC_ACCEL_VIOLATION:   return "ACCEL_VIOLATION";
        case MOTOR_EXEC_CONTACT_FORBIDDEN: return "CONTACT_FORBIDDEN";
        case MOTOR_EXEC_HUMAN_CONTACT_RISK: return "HUMAN_CONTACT_RISK";
        case MOTOR_EXEC_COLLISION_DETECTED: return "COLLISION_DETECTED";
        case MOTOR_EXEC_REGION_LOCKED:     return "REGION_LOCKED";
        case MOTOR_EXEC_GATE_DISABLED:     return "GATE_DISABLED";
        case MOTOR_EXEC_INVALID_PROPOSAL:  return "INVALID_PROPOSAL";
        case MOTOR_EXEC_ERROR:             return "ERROR";
        default:                           return "UNKNOWN";
    }
}
