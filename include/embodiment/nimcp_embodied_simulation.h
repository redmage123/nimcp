/**
 * @file nimcp_embodied_simulation.h
 * @brief Motor Imagery and Action Simulation for NIMCP Embodied Cognition
 *
 * Biological Inspiration:
 * - Premotor cortex: Motor planning and action simulation
 * - Supplementary motor area: Internal action rehearsal
 * - Forward models: Predict sensory consequences of actions
 * - Inverse models: Compute motor commands for desired outcomes
 * - Mental imagery: Covert action simulation without execution
 *
 * This module enables:
 * - Internal action rehearsal without execution
 * - Predictive motor simulation
 * - Effort and cost estimation
 * - Action outcome prediction
 * - Motor imagery for planning
 *
 * Key Features:
 * - Action sequence simulation
 * - Forward model prediction
 * - Effort/energy cost estimation
 * - Collision and feasibility checking
 * - Temporal dynamics simulation
 * - Statistics tracking
 *
 * @version 1.0
 * @date 2025-01-13
 */

#ifndef NIMCP_EMBODIED_SIMULATION_H
#define NIMCP_EMBODIED_SIMULATION_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/**
 * @brief Maximum action steps in a simulation
 */
#define NIMCP_SIMULATION_MAX_STEPS 64

/**
 * @brief Maximum simultaneous simulations
 */
#define NIMCP_SIMULATION_MAX_CONCURRENT 8

/**
 * @brief Maximum effectors (limbs/body parts)
 */
#define NIMCP_SIMULATION_MAX_EFFECTORS 16

/**
 * @brief Maximum objects in simulation
 */
#define NIMCP_SIMULATION_MAX_OBJECTS 32

/**
 * @brief Trajectory history size
 */
#define NIMCP_SIMULATION_TRAJECTORY_SIZE 128

/**
 * @brief Maximum size of feasibility reason buffer
 */
#define NIMCP_SIMULATION_REASON_MAX 64

/* ============================================================================
 * Error Codes
 * ============================================================================ */

/**
 * @brief Simulation-specific error codes (9200-9299 range)
 */
typedef enum {
    NIMCP_SIM_OK = 0,                        /**< Operation successful */
    NIMCP_SIM_ERROR = 9200,                  /**< Generic simulation error */
    NIMCP_SIM_ERROR_NULL_PARAM = 9201,       /**< Null parameter provided */
    NIMCP_SIM_ERROR_INVALID_CONFIG = 9202,   /**< Invalid configuration */
    NIMCP_SIM_ERROR_NOT_INITIALIZED = 9203,  /**< System not initialized */
    NIMCP_SIM_ERROR_SIM_LIMIT = 9204,        /**< Simulation limit reached */
    NIMCP_SIM_ERROR_INVALID_SIM = 9205,      /**< Invalid simulation ID */
    NIMCP_SIM_ERROR_STEP_LIMIT = 9206,       /**< Step limit reached */
    NIMCP_SIM_ERROR_COLLISION = 9207,        /**< Collision detected */
    NIMCP_SIM_ERROR_INFEASIBLE = 9208,       /**< Action infeasible */
    NIMCP_SIM_ERROR_MEMORY = 9209,           /**< Memory allocation failed */
    NIMCP_SIM_ERROR_TIMEOUT = 9210           /**< Simulation timeout */
} nimcp_sim_error_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Simulation state
 */
typedef enum {
    NIMCP_SIM_STATE_IDLE = 0,        /**< Not running */
    NIMCP_SIM_STATE_RUNNING,         /**< Simulation in progress */
    NIMCP_SIM_STATE_PAUSED,          /**< Temporarily paused */
    NIMCP_SIM_STATE_COMPLETED,       /**< Successfully completed */
    NIMCP_SIM_STATE_FAILED,          /**< Failed (collision, etc.) */
    NIMCP_SIM_STATE_ABORTED,         /**< User aborted */
    NIMCP_SIM_STATE_COUNT
} nimcp_sim_state_t;

/**
 * @brief Action primitive types
 */
typedef enum {
    NIMCP_ACTION_PRIM_NONE = 0,
    NIMCP_ACTION_PRIM_REACH,         /**< Reach to position */
    NIMCP_ACTION_PRIM_GRASP,         /**< Grasp object */
    NIMCP_ACTION_PRIM_RELEASE,       /**< Release object */
    NIMCP_ACTION_PRIM_TRANSPORT,     /**< Move object to location */
    NIMCP_ACTION_PRIM_ROTATE,        /**< Rotate effector/object */
    NIMCP_ACTION_PRIM_PUSH,          /**< Push object */
    NIMCP_ACTION_PRIM_PULL,          /**< Pull object */
    NIMCP_ACTION_PRIM_PRESS,         /**< Press (button, etc.) */
    NIMCP_ACTION_PRIM_LOCOMOTE,      /**< Whole body movement */
    NIMCP_ACTION_PRIM_BALANCE,       /**< Maintain balance */
    NIMCP_ACTION_PRIM_COUNT
} nimcp_action_primitive_t;

/**
 * @brief Effector types
 */
typedef enum {
    NIMCP_EFFECTOR_UNKNOWN = 0,
    NIMCP_EFFECTOR_LEFT_HAND,
    NIMCP_EFFECTOR_RIGHT_HAND,
    NIMCP_EFFECTOR_LEFT_FOOT,
    NIMCP_EFFECTOR_RIGHT_FOOT,
    NIMCP_EFFECTOR_HEAD,
    NIMCP_EFFECTOR_TORSO,
    NIMCP_EFFECTOR_BIMANUAL,         /**< Both hands coordinated */
    NIMCP_EFFECTOR_WHOLE_BODY,       /**< Whole body movement */
    NIMCP_EFFECTOR_COUNT
} nimcp_effector_type_t;

/**
 * @brief Forward model type
 */
typedef enum {
    NIMCP_FORWARD_MODEL_LINEAR = 0,  /**< Simple linear dynamics */
    NIMCP_FORWARD_MODEL_DYNAMICS,    /**< Full dynamics simulation */
    NIMCP_FORWARD_MODEL_LEARNED,     /**< Learned forward model */
    NIMCP_FORWARD_MODEL_COUNT
} nimcp_forward_model_type_t;

/**
 * @brief Simulation fidelity level
 */
typedef enum {
    NIMCP_FIDELITY_LOW = 0,          /**< Fast, approximate */
    NIMCP_FIDELITY_MEDIUM,           /**< Balanced */
    NIMCP_FIDELITY_HIGH,             /**< Accurate, slower */
    NIMCP_FIDELITY_COUNT
} nimcp_sim_fidelity_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief 3D position
 */
typedef struct {
    double x;
    double y;
    double z;
} nimcp_sim_position_t;

/**
 * @brief Quaternion orientation
 */
typedef struct {
    double w;
    double x;
    double y;
    double z;
} nimcp_sim_quaternion_t;

/**
 * @brief Effector state
 */
typedef struct {
    uint32_t effector_id;            /**< Effector identifier */
    nimcp_effector_type_t type;      /**< Effector type */

    nimcp_sim_position_t position;   /**< Current position */
    nimcp_sim_quaternion_t orientation; /**< Current orientation */
    double velocity[3];              /**< Linear velocity */
    double angular_velocity[3];      /**< Angular velocity */

    double grip_force;               /**< Current grip force (hands) */
    bool is_grasping;                /**< Currently holding object */
    uint32_t grasped_object_id;      /**< ID of held object */

    double fatigue;                  /**< Fatigue level [0-1] */
    double max_force;                /**< Maximum force capacity */
} nimcp_effector_state_t;

/**
 * @brief Object in simulation
 */
typedef struct {
    uint32_t object_id;              /**< Object identifier */

    nimcp_sim_position_t position;   /**< Position */
    nimcp_sim_quaternion_t orientation; /**< Orientation */
    double velocity[3];              /**< Linear velocity */
    double angular_velocity[3];      /**< Angular velocity */

    double mass;                     /**< Mass (kg) */
    double friction;                 /**< Surface friction */
    double dimensions[3];            /**< Bounding box */

    bool is_static;                  /**< Cannot be moved */
    bool is_grasped;                 /**< Currently grasped */
    uint32_t grasped_by;             /**< Effector holding this */
} nimcp_sim_object_t;

/**
 * @brief Action step in simulation
 */
typedef struct {
    uint32_t step_id;                /**< Step identifier */
    nimcp_action_primitive_t primitive; /**< Action primitive */
    uint32_t effector_id;            /**< Effector to use */

    /* Target specification */
    nimcp_sim_position_t target_position; /**< Target position */
    nimcp_sim_quaternion_t target_orientation; /**< Target orientation */
    uint32_t target_object_id;       /**< Target object (if applicable) */

    /* Timing */
    double start_time;               /**< Planned start time */
    double duration;                 /**< Planned duration */

    /* Constraints */
    double max_velocity;             /**< Maximum velocity */
    double max_force;                /**< Maximum force */
    double precision;                /**< Required precision */

    /* Execution state */
    bool is_completed;               /**< Step completed */
    bool is_successful;              /**< Step succeeded */
    double actual_duration;          /**< Actual duration */
} nimcp_action_step_t;

/**
 * @brief Trajectory waypoint
 */
typedef struct {
    double time;                     /**< Time in simulation */
    nimcp_sim_position_t position;   /**< Position at time */
    nimcp_sim_quaternion_t orientation; /**< Orientation at time */
    double velocity[3];              /**< Velocity at time */
    double effort;                   /**< Instantaneous effort */
} nimcp_trajectory_point_t;

/**
 * @brief Forward model prediction
 */
typedef struct {
    /* Predicted end state */
    nimcp_effector_state_t predicted_effector; /**< Predicted effector state */
    nimcp_sim_object_t predicted_objects[NIMCP_SIMULATION_MAX_OBJECTS];
    uint32_t num_predicted_objects;

    /* Sensory predictions */
    double predicted_visual[3];      /**< Where we expect to see effector */
    double predicted_proprio[3];     /**< Predicted proprioceptive state */
    double predicted_tactile;        /**< Predicted tactile feedback */

    /* Uncertainty */
    double position_uncertainty;     /**< Position prediction uncertainty */
    double outcome_confidence;       /**< Overall prediction confidence */

    double prediction_time;          /**< Time to generate prediction */
} nimcp_forward_prediction_t;

/**
 * @brief Effort/cost estimation
 */
typedef struct {
    double metabolic_cost;           /**< Estimated energy expenditure (J) */
    double time_cost;                /**< Time to complete (seconds) */
    double cognitive_load;           /**< Mental effort required [0-1] */
    double fatigue_cost;             /**< Fatigue induced [0-1] */
    double risk;                     /**< Risk of failure [0-1] */
    double total_cost;               /**< Weighted total cost */
} nimcp_effort_estimate_t;

/**
 * @brief Collision event
 */
typedef struct {
    uint32_t effector_id;            /**< Effector involved */
    uint32_t object_id;              /**< Object collided with */
    nimcp_sim_position_t contact_point; /**< Point of contact */
    double impact_force;             /**< Estimated impact force */
    double time;                     /**< Time of collision */
    bool is_self_collision;          /**< Collision with own body */
} nimcp_collision_event_t;

/**
 * @brief Simulation result
 */
typedef struct {
    uint32_t sim_id;                 /**< Simulation identifier */
    nimcp_sim_state_t final_state;   /**< Final state */

    /* Outcome */
    bool goal_achieved;              /**< Primary goal achieved */
    double success_probability;      /**< Estimated success rate */
    nimcp_effort_estimate_t effort;  /**< Effort estimation */

    /* Trajectory */
    nimcp_trajectory_point_t trajectory[NIMCP_SIMULATION_TRAJECTORY_SIZE];
    uint32_t trajectory_length;

    /* Events */
    nimcp_collision_event_t collisions[8]; /**< Collision events */
    uint32_t num_collisions;

    /* Timing */
    double sim_duration;             /**< Simulated time elapsed */
    double real_duration;            /**< Real time elapsed */
    uint32_t steps_completed;        /**< Number of steps completed */
} nimcp_sim_result_t;

/**
 * @brief Simulation statistics
 */
typedef struct {
    uint64_t total_simulations;      /**< Total simulations run */
    uint64_t successful_simulations; /**< Successful simulations */
    uint64_t failed_simulations;     /**< Failed simulations */
    uint64_t total_steps;            /**< Total steps simulated */
    uint64_t collision_count;        /**< Total collisions */

    double avg_sim_duration;         /**< Average simulation duration */
    double avg_prediction_error;     /**< Average prediction error */
    double avg_effort_estimate;      /**< Average effort estimate */

    uint64_t creation_time;          /**< System creation timestamp */
} nimcp_sim_stats_t;

/**
 * @brief Configuration parameters
 */
typedef struct {
    /* Simulation parameters */
    nimcp_sim_fidelity_t fidelity;   /**< Simulation fidelity */
    nimcp_forward_model_type_t forward_model; /**< Forward model type */
    double time_step;                /**< Simulation time step (seconds) */
    double max_sim_time;             /**< Maximum simulation time */

    /* Physics parameters */
    double gravity[3];               /**< Gravity vector */
    double air_resistance;           /**< Air resistance coefficient */

    /* Effort calculation weights */
    double metabolic_weight;         /**< Weight for metabolic cost */
    double time_weight;              /**< Weight for time cost */
    double fatigue_weight;           /**< Weight for fatigue */
    double risk_weight;              /**< Weight for risk */

    /* Collision detection */
    bool enable_collision_detection; /**< Enable collision checking */
    double collision_margin;         /**< Safety margin (meters) */

    /* Limits */
    uint32_t max_steps;              /**< Maximum steps per simulation */
    uint32_t max_concurrent;         /**< Maximum concurrent simulations */

    /* Uncertainty */
    double position_noise;           /**< Position noise std dev */
    double velocity_noise;           /**< Velocity noise std dev */
} nimcp_sim_config_t;

/**
 * @brief Simulation context (opaque)
 */
typedef struct nimcp_sim_context nimcp_sim_context_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Configuration to fill
 */
void nimcp_sim_default_config(nimcp_sim_config_t* config);

/**
 * @brief Create simulation context
 *
 * @param config Configuration parameters
 * @return Context pointer or NULL on failure
 */
nimcp_sim_context_t* nimcp_sim_create(const nimcp_sim_config_t* config);

/**
 * @brief Initialize existing context
 *
 * @param ctx Context to initialize
 * @param config Configuration parameters
 * @return NIMCP_SIM_OK on success
 */
nimcp_sim_error_t nimcp_sim_init(
    nimcp_sim_context_t* ctx,
    const nimcp_sim_config_t* config
);

/**
 * @brief Reset simulation context
 *
 * @param ctx Context to reset
 * @return NIMCP_SIM_OK on success
 */
nimcp_sim_error_t nimcp_sim_reset(nimcp_sim_context_t* ctx);

/**
 * @brief Destroy simulation context
 *
 * @param ctx Context to destroy
 */
void nimcp_sim_destroy(nimcp_sim_context_t* ctx);

/* ============================================================================
 * Effector and Object Setup API
 * ============================================================================ */

/**
 * @brief Set effector state
 *
 * @param ctx Simulation context
 * @param effector Effector state
 * @return NIMCP_SIM_OK on success
 */
nimcp_sim_error_t nimcp_sim_set_effector(
    nimcp_sim_context_t* ctx,
    const nimcp_effector_state_t* effector
);

/**
 * @brief Get effector state
 *
 * @param ctx Simulation context
 * @param effector_id Effector to query
 * @param effector Output state
 * @return NIMCP_SIM_OK on success
 */
nimcp_sim_error_t nimcp_sim_get_effector(
    const nimcp_sim_context_t* ctx,
    uint32_t effector_id,
    nimcp_effector_state_t* effector
);

/**
 * @brief Add object to simulation
 *
 * @param ctx Simulation context
 * @param object Object to add
 * @return NIMCP_SIM_OK on success
 */
nimcp_sim_error_t nimcp_sim_add_object(
    nimcp_sim_context_t* ctx,
    const nimcp_sim_object_t* object
);

/**
 * @brief Update object state
 *
 * @param ctx Simulation context
 * @param object Updated object
 * @return NIMCP_SIM_OK on success
 */
nimcp_sim_error_t nimcp_sim_update_object(
    nimcp_sim_context_t* ctx,
    const nimcp_sim_object_t* object
);

/**
 * @brief Remove object from simulation
 *
 * @param ctx Simulation context
 * @param object_id Object to remove
 * @return NIMCP_SIM_OK on success
 */
nimcp_sim_error_t nimcp_sim_remove_object(
    nimcp_sim_context_t* ctx,
    uint32_t object_id
);

/* ============================================================================
 * Action Simulation API
 * ============================================================================ */

/**
 * @brief Start new simulation
 *
 * WHAT: Creates a new action simulation
 * WHY:  Entry point for motor imagery
 *
 * @param ctx Simulation context
 * @param sim_id Output simulation ID
 * @return NIMCP_SIM_OK on success
 */
nimcp_sim_error_t nimcp_sim_start(
    nimcp_sim_context_t* ctx,
    uint32_t* sim_id
);

/**
 * @brief Add action step to simulation
 *
 * @param ctx Simulation context
 * @param sim_id Simulation to add to
 * @param step Action step
 * @return NIMCP_SIM_OK on success
 */
nimcp_sim_error_t nimcp_sim_add_step(
    nimcp_sim_context_t* ctx,
    uint32_t sim_id,
    const nimcp_action_step_t* step
);

/**
 * @brief Run simulation
 *
 * WHAT: Executes the simulation
 * WHY:  Generates predictions and estimates
 *
 * @param ctx Simulation context
 * @param sim_id Simulation to run
 * @param result Output result
 * @return NIMCP_SIM_OK on success
 */
nimcp_sim_error_t nimcp_sim_run(
    nimcp_sim_context_t* ctx,
    uint32_t sim_id,
    nimcp_sim_result_t* result
);

/**
 * @brief Run single step
 *
 * @param ctx Simulation context
 * @param sim_id Simulation ID
 * @return NIMCP_SIM_OK on success
 */
nimcp_sim_error_t nimcp_sim_step(
    nimcp_sim_context_t* ctx,
    uint32_t sim_id
);

/**
 * @brief Pause simulation
 *
 * @param ctx Simulation context
 * @param sim_id Simulation to pause
 * @return NIMCP_SIM_OK on success
 */
nimcp_sim_error_t nimcp_sim_pause(
    nimcp_sim_context_t* ctx,
    uint32_t sim_id
);

/**
 * @brief Resume simulation
 *
 * @param ctx Simulation context
 * @param sim_id Simulation to resume
 * @return NIMCP_SIM_OK on success
 */
nimcp_sim_error_t nimcp_sim_resume(
    nimcp_sim_context_t* ctx,
    uint32_t sim_id
);

/**
 * @brief Abort simulation
 *
 * @param ctx Simulation context
 * @param sim_id Simulation to abort
 * @return NIMCP_SIM_OK on success
 */
nimcp_sim_error_t nimcp_sim_abort(
    nimcp_sim_context_t* ctx,
    uint32_t sim_id
);

/**
 * @brief Get simulation state
 *
 * @param ctx Simulation context
 * @param sim_id Simulation to query
 * @param state Output state
 * @return NIMCP_SIM_OK on success
 */
nimcp_sim_error_t nimcp_sim_get_state(
    const nimcp_sim_context_t* ctx,
    uint32_t sim_id,
    nimcp_sim_state_t* state
);

/* ============================================================================
 * Forward Model API
 * ============================================================================ */

/**
 * @brief Run forward model prediction
 *
 * WHAT: Predicts outcome of motor command
 * WHY:  Core predictive simulation
 *
 * @param ctx Simulation context
 * @param effector_id Effector to predict for
 * @param motor_command Motor command (velocity target)
 * @param duration Prediction horizon (seconds)
 * @param prediction Output prediction
 * @return NIMCP_SIM_OK on success
 */
nimcp_sim_error_t nimcp_sim_forward_predict(
    nimcp_sim_context_t* ctx,
    uint32_t effector_id,
    const double* motor_command,
    double duration,
    nimcp_forward_prediction_t* prediction
);

/**
 * @brief Predict action sequence outcome
 *
 * @param ctx Simulation context
 * @param steps Action steps
 * @param num_steps Number of steps
 * @param prediction Output prediction
 * @return NIMCP_SIM_OK on success
 */
nimcp_sim_error_t nimcp_sim_predict_sequence(
    nimcp_sim_context_t* ctx,
    const nimcp_action_step_t* steps,
    uint32_t num_steps,
    nimcp_forward_prediction_t* prediction
);

/* ============================================================================
 * Effort Estimation API
 * ============================================================================ */

/**
 * @brief Estimate effort for action
 *
 * WHAT: Estimates metabolic and time costs
 * WHY:  Cost-benefit decision making
 *
 * @param ctx Simulation context
 * @param step Action step
 * @param estimate Output effort estimate
 * @return NIMCP_SIM_OK on success
 */
nimcp_sim_error_t nimcp_sim_estimate_effort(
    nimcp_sim_context_t* ctx,
    const nimcp_action_step_t* step,
    nimcp_effort_estimate_t* estimate
);

/**
 * @brief Estimate sequence effort
 *
 * @param ctx Simulation context
 * @param steps Action steps
 * @param num_steps Number of steps
 * @param estimate Output total effort
 * @return NIMCP_SIM_OK on success
 */
nimcp_sim_error_t nimcp_sim_estimate_sequence_effort(
    nimcp_sim_context_t* ctx,
    const nimcp_action_step_t* steps,
    uint32_t num_steps,
    nimcp_effort_estimate_t* estimate
);

/**
 * @brief Compare action alternatives
 *
 * @param ctx Simulation context
 * @param alternatives Array of action steps
 * @param num_alternatives Number of alternatives
 * @param best_index Output: index of best alternative
 * @param estimates Output: effort estimates for each
 * @return NIMCP_SIM_OK on success
 */
nimcp_sim_error_t nimcp_sim_compare_alternatives(
    nimcp_sim_context_t* ctx,
    const nimcp_action_step_t* alternatives,
    uint32_t num_alternatives,
    uint32_t* best_index,
    nimcp_effort_estimate_t* estimates
);

/* ============================================================================
 * Feasibility and Collision API
 * ============================================================================ */

/**
 * @brief Check action feasibility
 *
 * WHAT: Determines if action is physically possible
 * WHY:  Filter impossible actions before simulation
 *
 * @param ctx Simulation context
 * @param step Action to check
 * @param is_feasible Output: true if feasible
 * @param reason Output: reason if not feasible (optional)
 * @return NIMCP_SIM_OK on success
 */
nimcp_sim_error_t nimcp_sim_check_feasibility(
    nimcp_sim_context_t* ctx,
    const nimcp_action_step_t* step,
    bool* is_feasible,
    char* reason
);

/**
 * @brief Check for collision along path
 *
 * @param ctx Simulation context
 * @param effector_id Effector to check
 * @param start Start position
 * @param end End position
 * @param has_collision Output: true if collision
 * @param collision Output collision event (if any)
 * @return NIMCP_SIM_OK on success
 */
nimcp_sim_error_t nimcp_sim_check_collision(
    nimcp_sim_context_t* ctx,
    uint32_t effector_id,
    const nimcp_sim_position_t* start,
    const nimcp_sim_position_t* end,
    bool* has_collision,
    nimcp_collision_event_t* collision
);

/* ============================================================================
 * Statistics and Utility API
 * ============================================================================ */

/**
 * @brief Get statistics
 *
 * @param ctx Simulation context
 * @param stats Output statistics
 * @return NIMCP_SIM_OK on success
 */
nimcp_sim_error_t nimcp_sim_get_stats(
    const nimcp_sim_context_t* ctx,
    nimcp_sim_stats_t* stats
);

/**
 * @brief Get simulation state name
 *
 * @param state Simulation state
 * @return String name
 */
const char* nimcp_sim_state_name(nimcp_sim_state_t state);

/**
 * @brief Get action primitive name
 *
 * @param primitive Action primitive
 * @return String name
 */
const char* nimcp_sim_primitive_name(nimcp_action_primitive_t primitive);

/**
 * @brief Get effector type name
 *
 * @param type Effector type
 * @return String name
 */
const char* nimcp_sim_effector_name(nimcp_effector_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMBODIED_SIMULATION_H */
