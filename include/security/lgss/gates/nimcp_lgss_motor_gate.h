/**
 * @file nimcp_lgss_motor_gate.h
 * @brief LGSS Motor Output Gate - Final safety barrier for motor commands
 *
 * WHAT: Provides gated motor output control with comprehensive safety constraints.
 *       All motor commands must pass through this gate before physical execution.
 *
 * WHY:  Motor commands directly affect physical systems and potentially humans.
 *       This gate ensures force, velocity, and acceleration limits are enforced,
 *       and that human contact is properly managed to prevent injury.
 *
 * HOW:  Implements per-region safety constraints, collision detection hooks,
 *       and real-time monitoring of motor command proposals. Commands that
 *       violate constraints are rejected with detailed violation information.
 *
 * BIOLOGICAL BASIS: Analogous to the motor cortex output pathway with
 *       cerebellar safety checking and spinal reflex integration.
 */

#ifndef NIMCP_LGSS_MOTOR_GATE_H
#define NIMCP_LGSS_MOTOR_GATE_H

#include "utils/validation/nimcp_common.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Constants
 * ============================================================================= */

/** Magic number for motor gate validation ('MTGT') */
#define NIMCP_MOTOR_GATE_MAGIC 0x4D544754

/** Maximum number of concurrent motor command proposals */
#define NIMCP_MOTOR_MAX_PROPOSALS 64

/** Default force limit in Newtons */
#define NIMCP_MOTOR_DEFAULT_FORCE_LIMIT 100.0f

/** Default velocity limit in m/s */
#define NIMCP_MOTOR_DEFAULT_VELOCITY_LIMIT 2.0f

/** Default acceleration limit in m/s^2 */
#define NIMCP_MOTOR_DEFAULT_ACCEL_LIMIT 5.0f

/** Position vector dimension */
#define NIMCP_MOTOR_POSITION_DIM 3

/* =============================================================================
 * Enumerations
 * ============================================================================= */

/**
 * @brief Motor body regions
 *
 * WHAT: Categorizes body regions for motor control
 * WHY:  Different regions have different safety constraints
 * HOW:  Used to index into region-specific constraint arrays
 */
typedef enum {
    MOTOR_REGION_HANDS = 0,    /**< Hand/finger motor control */
    MOTOR_REGION_ARMS,         /**< Arm motor control (shoulder to wrist) */
    MOTOR_REGION_LEGS,         /**< Leg motor control (hip to ankle) */
    MOTOR_REGION_HEAD,         /**< Head/neck motor control */
    MOTOR_REGION_EYES,         /**< Eye movement control (saccades, pursuit) */
    MOTOR_REGION_FACE,         /**< Facial muscle control */
    MOTOR_REGION_TORSO,        /**< Torso/trunk motor control */
    MOTOR_REGION_COUNT         /**< Total number of motor regions */
} motor_region_t;

/**
 * @brief Motor gate execution result
 */
typedef enum {
    MOTOR_EXEC_SUCCESS = 0,         /**< Command executed successfully */
    MOTOR_EXEC_FORCE_VIOLATION,     /**< Force limit exceeded */
    MOTOR_EXEC_VELOCITY_VIOLATION,  /**< Velocity limit exceeded */
    MOTOR_EXEC_ACCEL_VIOLATION,     /**< Acceleration limit exceeded */
    MOTOR_EXEC_CONTACT_FORBIDDEN,   /**< Contact attempted when not allowed */
    MOTOR_EXEC_HUMAN_CONTACT_RISK,  /**< Human contact risk detected */
    MOTOR_EXEC_COLLISION_DETECTED,  /**< Collision with obstacle detected */
    MOTOR_EXEC_REGION_LOCKED,       /**< Target region is currently locked */
    MOTOR_EXEC_GATE_DISABLED,       /**< Motor gate is disabled */
    MOTOR_EXEC_INVALID_PROPOSAL,    /**< Invalid command proposal */
    MOTOR_EXEC_ERROR                /**< General execution error */
} motor_exec_result_t;

/* =============================================================================
 * Structures
 * ============================================================================= */

/**
 * @brief Safety constraints for a motor region
 *
 * WHAT: Defines physical limits for motor commands in a specific body region
 * WHY:  Prevents damage to hardware, environment, and humans
 * HOW:  Commands exceeding these limits are rejected by the gate
 */
typedef struct {
    float force_limit;           /**< Maximum force in Newtons */
    float velocity_limit;        /**< Maximum velocity in m/s */
    float acceleration_limit;    /**< Maximum acceleration in m/s^2 */
    bool allow_contact;          /**< Whether contact with objects is allowed */
    bool allow_human_contact;    /**< Whether human contact is permitted */
    float contact_force_limit;   /**< Max force during allowed contact (N) */
    float jerk_limit;            /**< Maximum jerk (rate of accel change) */
    bool enabled;                /**< Whether this region is enabled */
} motor_safety_constraints_t;

/**
 * @brief Motor command proposal
 *
 * WHAT: A proposed motor command awaiting gate approval
 * WHY:  All motor commands must be validated before execution
 * HOW:  Contains target kinematics and expected interaction state
 */
typedef struct {
    motor_region_t region;           /**< Target body region */
    float target_position[NIMCP_MOTOR_POSITION_DIM];  /**< Target position (x,y,z) */
    float target_velocity[NIMCP_MOTOR_POSITION_DIM];  /**< Target velocity vector */
    float target_force[NIMCP_MOTOR_POSITION_DIM];     /**< Target force vector */
    float duration;                  /**< Command duration in seconds */
    bool is_contact_expected;        /**< Whether contact is expected */
    uint64_t timestamp;              /**< Proposal timestamp (ns since epoch) */
    uint32_t sequence_id;            /**< Unique sequence identifier */
    void* metadata;                  /**< Optional command metadata */
    size_t metadata_size;            /**< Size of metadata in bytes */
} motor_command_proposal_t;

/**
 * @brief Motor gate violation details
 *
 * WHAT: Detailed information about why a command was rejected
 * WHY:  Enables debugging and adaptive constraint tuning
 * HOW:  Populated when motor_gate_execute() returns non-success
 */
typedef struct {
    motor_exec_result_t result;      /**< Type of violation */
    motor_region_t region;           /**< Affected region */
    float actual_value;              /**< Actual value that caused violation */
    float limit_value;               /**< The limit that was exceeded */
    char description[256];           /**< Human-readable violation description */
    uint64_t timestamp;              /**< When violation occurred */
} motor_gate_violation_t;

/**
 * @brief Motor gate statistics
 *
 * WHAT: Operational statistics for the motor gate
 * WHY:  Enables monitoring and tuning of safety constraints
 * HOW:  Updated atomically during gate operations
 */
typedef struct {
    uint64_t commands_submitted;     /**< Total commands submitted */
    uint64_t commands_approved;      /**< Commands that passed the gate */
    uint64_t commands_rejected;      /**< Commands rejected for violations */
    uint64_t force_violations;       /**< Count of force limit violations */
    uint64_t velocity_violations;    /**< Count of velocity limit violations */
    uint64_t accel_violations;       /**< Count of acceleration violations */
    uint64_t contact_violations;     /**< Count of contact violations */
    uint64_t human_contact_events;   /**< Human contact risk events */
    float avg_approval_latency_us;   /**< Average approval latency (us) */
    float max_approval_latency_us;   /**< Maximum approval latency (us) */
} motor_gate_stats_t;

/**
 * @brief Collision detection callback type
 *
 * WHAT: Function pointer for external collision detection
 * WHY:  Allows integration with external collision detection systems
 * HOW:  Called before command approval to check for collisions
 *
 * @param proposal The motor command proposal to check
 * @param user_data User-provided context data
 * @return true if collision detected, false if clear
 */
typedef bool (*motor_collision_callback_t)(
    const motor_command_proposal_t* proposal,
    void* user_data
);

/**
 * @brief Human proximity callback type
 *
 * WHAT: Function pointer for human proximity detection
 * WHY:  Enables safe human-robot interaction
 * HOW:  Returns distance to nearest detected human
 *
 * @param region The motor region being queried
 * @param user_data User-provided context data
 * @return Distance to nearest human in meters, or -1.0 if unknown
 */
typedef float (*motor_human_proximity_callback_t)(
    motor_region_t region,
    void* user_data
);

/** Forward declaration */
typedef struct motor_gate motor_gate_t;

/**
 * @brief Motor gate configuration
 */
typedef struct {
    bool emergency_stop_enabled;     /**< Enable emergency stop capability */
    float global_force_scale;        /**< Global force scaling factor (0.0-1.0) */
    float global_velocity_scale;     /**< Global velocity scaling factor */
    motor_collision_callback_t collision_callback;     /**< Collision detector */
    motor_human_proximity_callback_t proximity_callback; /**< Human proximity */
    void* callback_user_data;        /**< User data for callbacks */
    bool log_all_commands;           /**< Log all commands (debug mode) */
    bool strict_mode;                /**< Reject any borderline cases */
} motor_gate_config_t;

/* =============================================================================
 * Function Declarations
 * ============================================================================= */

/**
 * @brief Create a motor gate instance
 *
 * WHAT: Allocates and initializes a new motor gate
 * WHY:  Motor gate is required for safe motor command execution
 * HOW:  Allocates memory, initializes default constraints
 *
 * @param config Optional configuration (NULL for defaults)
 * @return New motor gate instance, or NULL on failure
 */
motor_gate_t* motor_gate_create(const motor_gate_config_t* config);

/**
 * @brief Destroy a motor gate instance
 *
 * WHAT: Deallocates a motor gate and all associated resources
 * WHY:  Proper cleanup prevents memory leaks
 * HOW:  Frees all internal structures and the gate itself
 *
 * @param gate Motor gate to destroy
 */
void motor_gate_destroy(motor_gate_t* gate);

/**
 * @brief Set safety constraints for a specific region
 *
 * WHAT: Configures safety limits for a body region
 * WHY:  Different regions have different physical capabilities and risks
 * HOW:  Stores constraints for validation during command execution
 *
 * @param gate Motor gate instance
 * @param region Target body region
 * @param constraints Safety constraints to apply
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t motor_gate_set_constraints(
    motor_gate_t* gate,
    motor_region_t region,
    const motor_safety_constraints_t* constraints
);

/**
 * @brief Get current safety constraints for a region
 *
 * WHAT: Retrieves current safety constraints for a body region
 * WHY:  Allows inspection of current limits for debugging/UI
 * HOW:  Copies current constraints to output structure
 *
 * @param gate Motor gate instance
 * @param region Target body region
 * @param constraints Output buffer for constraints
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t motor_gate_get_constraints(
    const motor_gate_t* gate,
    motor_region_t region,
    motor_safety_constraints_t* constraints
);

/**
 * @brief Execute a motor command through the gate
 *
 * WHAT: Validates and executes a motor command proposal
 * WHY:  This is the ONLY path through which motor commands should execute
 * HOW:  Validates against constraints, checks collision/proximity, executes
 *
 * @param gate Motor gate instance
 * @param proposal Motor command proposal
 * @param violation Output: violation details if command rejected (can be NULL)
 * @return Execution result code
 */
motor_exec_result_t motor_gate_execute(
    motor_gate_t* gate,
    const motor_command_proposal_t* proposal,
    motor_gate_violation_t* violation
);

/**
 * @brief Check if a command would violate constraints without executing
 *
 * WHAT: Dry-run validation of a motor command
 * WHY:  Allows pre-checking commands before submission
 * HOW:  Performs all validations but skips actual execution
 *
 * @param gate Motor gate instance
 * @param proposal Motor command proposal to check
 * @param violation Output: violation details if would fail (can be NULL)
 * @return true if command would violate constraints, false if safe
 */
bool motor_gate_would_violate(
    const motor_gate_t* gate,
    const motor_command_proposal_t* proposal,
    motor_gate_violation_t* violation
);

/**
 * @brief Trigger emergency stop on all motor regions
 *
 * WHAT: Immediately halts all motor activity
 * WHY:  Emergency safety mechanism for dangerous situations
 * HOW:  Sets all regions to locked state, issues stop commands
 *
 * @param gate Motor gate instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t motor_gate_emergency_stop(motor_gate_t* gate);

/**
 * @brief Release emergency stop and resume normal operation
 *
 * WHAT: Clears emergency stop state
 * WHY:  Allows resumption after emergency has been cleared
 * HOW:  Unlocks regions and resets emergency state flags
 *
 * @param gate Motor gate instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t motor_gate_release_emergency_stop(motor_gate_t* gate);

/**
 * @brief Lock a specific motor region
 *
 * WHAT: Prevents any commands to a specific region
 * WHY:  Selective disabling for maintenance or safety
 * HOW:  Sets region lock flag, rejects all proposals to region
 *
 * @param gate Motor gate instance
 * @param region Region to lock
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t motor_gate_lock_region(motor_gate_t* gate, motor_region_t region);

/**
 * @brief Unlock a specific motor region
 *
 * WHAT: Re-enables commands to a specific region
 * WHY:  Restores operation after lock condition cleared
 * HOW:  Clears region lock flag
 *
 * @param gate Motor gate instance
 * @param region Region to unlock
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t motor_gate_unlock_region(motor_gate_t* gate, motor_region_t region);

/**
 * @brief Get motor gate statistics
 *
 * WHAT: Retrieves operational statistics
 * WHY:  Enables monitoring and performance analysis
 * HOW:  Copies current statistics to output structure
 *
 * @param gate Motor gate instance
 * @param stats Output buffer for statistics
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t motor_gate_get_stats(
    const motor_gate_t* gate,
    motor_gate_stats_t* stats
);

/**
 * @brief Reset motor gate statistics
 *
 * WHAT: Clears all accumulated statistics
 * WHY:  Allows fresh statistics collection
 * HOW:  Zeros all statistic counters atomically
 *
 * @param gate Motor gate instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t motor_gate_reset_stats(motor_gate_t* gate);

/**
 * @brief Get human-readable name for a motor region
 *
 * @param region Motor region enum value
 * @return Static string name for the region
 */
const char* motor_region_name(motor_region_t region);

/**
 * @brief Get human-readable name for an execution result
 *
 * @param result Execution result enum value
 * @return Static string name for the result
 */
const char* motor_exec_result_name(motor_exec_result_t result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LGSS_MOTOR_GATE_H */
