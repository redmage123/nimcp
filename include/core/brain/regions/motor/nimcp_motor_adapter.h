/**
 * @file nimcp_motor_adapter.h
 * @brief Brain adapter for Motor Cortex integration
 *
 * WHAT: Unified adapter connecting Motor Cortex sub-modules to the brain system
 * WHY:  Enable seamless integration with cognitive layers, training, and event system
 * HOW:  Orchestrates primary motor cortex (M1), premotor, and supplementary motor areas
 *
 * ARCHITECTURE:
 * - Wraps all motor sub-modules (M1, premotor, SMA)
 * - Provides high-level API for motor planning and execution
 * - Integrates with basal ganglia for action selection
 * - Connects to cerebellum for motor coordination
 * - Routes through thalamus (VA/VL nuclei)
 *
 * BIOLOGICAL BASIS:
 * - Models Brodmann area 4 (primary motor cortex, M1)
 * - Brodmann area 6 (premotor and supplementary motor areas)
 * - Homuncular somatotopic organization
 * - Corticospinal tract for movement execution
 *
 * @version Phase M1: Motor Cortex Brain Integration
 * @date 2025-12-30
 */

#ifndef NIMCP_MOTOR_ADAPTER_H
#define NIMCP_MOTOR_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Bio-async communication system */
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

/* Logging system */
#include "utils/logging/nimcp_logging.h"

/* Unified memory system */
#include "utils/memory/nimcp_unified_memory.h"

/* Forward declaration for opaque adapter type */
typedef struct motor_adapter motor_adapter_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Default configuration values
 */
#define MOTOR_DEFAULT_MAX_PROGRAMS          32
#define MOTOR_DEFAULT_MAX_EFFECTORS         64
#define MOTOR_DEFAULT_MAX_TRAJECTORIES      16
#define MOTOR_DEFAULT_PLANNING_HORIZON_MS   500.0f
#define MOTOR_DEFAULT_EXECUTION_RATE_HZ     100.0f

/**
 * @brief Body region identifiers (somatotopic mapping)
 */
typedef enum {
    MOTOR_REGION_FACE = 0,       /**< Face and mouth muscles */
    MOTOR_REGION_HAND_LEFT,      /**< Left hand and fingers */
    MOTOR_REGION_HAND_RIGHT,     /**< Right hand and fingers */
    MOTOR_REGION_ARM_LEFT,       /**< Left arm */
    MOTOR_REGION_ARM_RIGHT,      /**< Right arm */
    MOTOR_REGION_TRUNK,          /**< Trunk/core muscles */
    MOTOR_REGION_LEG_LEFT,       /**< Left leg */
    MOTOR_REGION_LEG_RIGHT,      /**< Right leg */
    MOTOR_REGION_FOOT_LEFT,      /**< Left foot */
    MOTOR_REGION_FOOT_RIGHT,     /**< Right foot */
    MOTOR_REGION_EYE,            /**< Eye movements (superior colliculus integration) */
    MOTOR_REGION_COUNT           /**< Total number of regions */
} motor_region_t;

/**
 * @brief Movement type classification
 */
typedef enum {
    MOVEMENT_TYPE_DISCRETE = 0,  /**< Single discrete movement (e.g., button press) */
    MOVEMENT_TYPE_SERIAL,        /**< Sequence of movements (e.g., typing) */
    MOVEMENT_TYPE_CONTINUOUS,    /**< Continuous movement (e.g., drawing) */
    MOVEMENT_TYPE_BALLISTIC,     /**< Fast, preprogrammed (e.g., throwing) */
    MOVEMENT_TYPE_CORRECTIVE     /**< Error-correcting (e.g., reaching adjustment) */
} movement_type_t;

/**
 * @brief Motor cortex adapter configuration
 */
typedef struct {
    /* Capacity limits */
    uint32_t max_motor_programs;     /**< Maximum stored motor programs */
    uint32_t max_effectors;          /**< Maximum controllable effectors */
    uint32_t max_trajectories;       /**< Maximum parallel trajectory plans */

    /* Timing parameters */
    float planning_horizon_ms;       /**< Forward planning window */
    float execution_rate_hz;         /**< Motor command update rate */
    float reaction_time_ms;          /**< Minimum reaction time */

    /* Processing options */
    bool enable_premotor;            /**< Enable premotor cortex processing */
    bool enable_sma;                 /**< Enable supplementary motor area */
    bool enable_trajectory_opt;      /**< Enable trajectory optimization */
    bool enable_feedforward;         /**< Enable feedforward control */
    bool enable_feedback;            /**< Enable feedback control */

    /* Integration options */
    bool enable_basal_ganglia;       /**< Enable BG integration */
    bool enable_cerebellum;          /**< Enable cerebellar integration */
    bool enable_thalamus;            /**< Enable thalamic routing */

    /* Event system */
    bool enable_events;              /**< Enable event bus integration */

    /* Training */
    bool enable_training;            /**< Enable motor learning */
    float learning_rate;             /**< Base learning rate */

    /* Bio-async communication */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    nimcp_bio_channel_type_t default_channel; /**< Default neuromodulator channel */
} motor_config_t;

/*=============================================================================
 * STATUS AND STATE
 *===========================================================================*/

/**
 * @brief Processing status of the adapter
 */
typedef enum {
    MOTOR_STATUS_IDLE = 0,           /**< Ready for commands */
    MOTOR_STATUS_PLANNING,           /**< Planning motor program */
    MOTOR_STATUS_PREPARING,          /**< Preparing for execution */
    MOTOR_STATUS_EXECUTING,          /**< Currently executing movement */
    MOTOR_STATUS_CORRECTING,         /**< Applying error corrections */
    MOTOR_STATUS_COMPLETE,           /**< Movement complete */
    MOTOR_STATUS_ERROR               /**< Error state */
} motor_status_t;

/**
 * @brief Error codes for motor operations
 */
typedef enum {
    MOTOR_ERROR_NONE = 0,
    MOTOR_ERROR_INVALID_INPUT,
    MOTOR_ERROR_PLANNING_FAILURE,
    MOTOR_ERROR_EXECUTION_FAILURE,
    MOTOR_ERROR_TRAJECTORY_INFEASIBLE,
    MOTOR_ERROR_EFFECTOR_CONFLICT,
    MOTOR_ERROR_TIMING_VIOLATION,
    MOTOR_ERROR_BUFFER_OVERFLOW,
    MOTOR_ERROR_INTERNAL
} motor_error_t;

/*=============================================================================
 * INPUT/OUTPUT STRUCTURES
 *===========================================================================*/

/**
 * @brief 3D position/velocity vector
 */
typedef struct {
    float x;                         /**< X coordinate/component */
    float y;                         /**< Y coordinate/component */
    float z;                         /**< Z coordinate/component */
} motor_vec3_t;

/**
 * @brief Motor effector state
 */
typedef struct {
    uint32_t effector_id;            /**< Unique effector identifier */
    motor_region_t region;           /**< Body region */
    motor_vec3_t position;           /**< Current position */
    motor_vec3_t velocity;           /**< Current velocity */
    float force;                     /**< Applied force [0, 1] */
    float stiffness;                 /**< Joint stiffness [0, 1] */
    bool is_active;                  /**< Currently being controlled */
} motor_effector_state_t;

/**
 * @brief Motor command for a single effector
 */
typedef struct {
    uint32_t effector_id;            /**< Target effector */
    motor_vec3_t target_position;    /**< Target position */
    motor_vec3_t target_velocity;    /**< Target velocity */
    float target_force;              /**< Target force [0, 1] */
    float duration_ms;               /**< Movement duration */
    double timestamp_ms;             /**< Execution time */
} motor_command_t;

/**
 * @brief Trajectory waypoint
 */
typedef struct {
    motor_vec3_t position;           /**< Waypoint position */
    motor_vec3_t velocity;           /**< Velocity at waypoint */
    float time_ms;                   /**< Time from start */
} trajectory_waypoint_t;

/**
 * @brief Motor program (stored movement sequence)
 */
typedef struct {
    uint32_t program_id;             /**< Unique program identifier */
    char name[64];                   /**< Program name */
    movement_type_t type;            /**< Movement type */
    motor_region_t primary_region;   /**< Primary body region */
    uint32_t num_commands;           /**< Number of commands in program */
    float total_duration_ms;         /**< Total program duration */
    float complexity;                /**< Movement complexity [0, 1] */
    bool is_learned;                 /**< Learned vs preprogrammed */
} motor_program_info_t;

/**
 * @brief Movement goal specification
 */
typedef struct {
    motor_region_t region;           /**< Target body region */
    motor_vec3_t target_position;    /**< Desired end position */
    motor_vec3_t target_velocity;    /**< Desired end velocity (0 = stop) */
    float max_duration_ms;           /**< Maximum allowed duration */
    float precision_required;        /**< Required accuracy [0, 1] */
    movement_type_t type;            /**< Movement type hint */
    float urgency;                   /**< Movement urgency [0, 1] */
} motor_goal_t;

/**
 * @brief Movement result
 */
typedef struct {
    bool success;                    /**< Movement completed successfully */
    motor_vec3_t final_position;     /**< Actual final position */
    motor_vec3_t position_error;     /**< Error from target */
    float actual_duration_ms;        /**< Actual movement duration */
    float accuracy;                  /**< Movement accuracy [0, 1] */
    uint32_t corrections_applied;    /**< Number of corrections */
    float energy_cost;               /**< Estimated metabolic cost */
} motor_result_t;

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Adapter statistics
 */
typedef struct {
    /* Processing counts */
    uint64_t movements_planned;      /**< Total movements planned */
    uint64_t movements_executed;     /**< Total movements executed */
    uint64_t commands_generated;     /**< Total motor commands */
    uint64_t corrections_applied;    /**< Total corrections */

    /* Success/failure */
    uint64_t successful_movements;   /**< Successful completions */
    uint64_t failed_movements;       /**< Failed movements */
    uint64_t planning_errors;        /**< Planning failures */
    uint64_t execution_errors;       /**< Execution failures */

    /* Performance */
    float avg_accuracy;              /**< Average accuracy [0, 1] */
    float avg_latency_ms;            /**< Average planning latency */
    float avg_duration_ms;           /**< Average movement duration */
    float total_energy_cost;         /**< Cumulative energy expenditure */

    /* Training */
    uint64_t training_iterations;    /**< Training updates */
    float training_loss;             /**< Current training loss */
} motor_stats_t;

/*=============================================================================
 * CALLBACK TYPES
 *===========================================================================*/

/**
 * @brief Callback for motor command output
 */
typedef void (*motor_command_callback_t)(
    const motor_command_t* command,
    void* user_data
);

/**
 * @brief Callback for movement completion notification
 */
typedef void (*motor_complete_callback_t)(
    const motor_result_t* result,
    void* user_data
);

/**
 * @brief Callback for sensory feedback (proprioception)
 */
typedef void (*motor_feedback_callback_t)(
    uint32_t effector_id,
    const motor_effector_state_t* state,
    void* user_data
);

/**
 * @brief Callback for event notification
 */
typedef void (*motor_event_callback_t)(
    uint32_t event_type,
    const void* event_data,
    void* user_data
);

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * WHAT: Returns default configuration for motor cortex adapter
 * WHY:  Provide sensible defaults for common use cases
 * HOW:  Initialize all fields with biologically-motivated values
 *
 * @return Default configuration structure
 */
motor_config_t motor_default_config(void);

/**
 * @brief Create motor cortex adapter
 *
 * WHAT: Allocate and initialize the adapter with all sub-modules
 * WHY:  Central point for motor control initialization
 * HOW:  Create M1, premotor, SMA processors; initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New adapter instance, or NULL on failure
 */
motor_adapter_t* motor_create(const motor_config_t* config);

/**
 * @brief Destroy motor cortex adapter
 *
 * WHAT: Free all resources associated with the adapter
 * WHY:  Prevent memory leaks
 * HOW:  Destroy sub-modules, free buffers and programs
 *
 * @param adapter Adapter to destroy
 */
void motor_destroy(motor_adapter_t* adapter);

/**
 * @brief Reset adapter state
 *
 * WHAT: Clear buffers and reset to idle state
 * WHY:  Prepare for new movement without full reinitialization
 * HOW:  Reset all sub-modules, clear command buffers
 *
 * @param adapter Adapter instance
 * @return true on success, false on failure
 */
bool motor_reset(motor_adapter_t* adapter);

/*=============================================================================
 * MOTOR PROGRAM MANAGEMENT
 *===========================================================================*/

/**
 * @brief Store a motor program
 *
 * WHAT: Save a learned motor program for future execution
 * WHY:  Enable skill learning and motor memory
 * HOW:  Store command sequence with metadata
 *
 * @param adapter Adapter instance
 * @param name Program name
 * @param commands Command sequence
 * @param num_commands Number of commands
 * @param type Movement type
 * @return Program ID, or 0 on failure
 */
uint32_t motor_store_program(motor_adapter_t* adapter,
                              const char* name,
                              const motor_command_t* commands,
                              uint32_t num_commands,
                              movement_type_t type);

/**
 * @brief Retrieve a motor program
 *
 * WHAT: Get stored motor program by ID
 * WHY:  Access learned motor skills
 * HOW:  Look up in program storage
 *
 * @param adapter Adapter instance
 * @param program_id Program identifier
 * @param info Output program info
 * @return true if found, false otherwise
 */
bool motor_get_program(const motor_adapter_t* adapter,
                        uint32_t program_id,
                        motor_program_info_t* info);

/**
 * @brief Delete a motor program
 *
 * WHAT: Remove stored motor program
 * WHY:  Free storage, remove obsolete skills
 * HOW:  Remove from program storage
 *
 * @param adapter Adapter instance
 * @param program_id Program to delete
 * @return true on success
 */
bool motor_delete_program(motor_adapter_t* adapter, uint32_t program_id);

/*=============================================================================
 * MOTOR PLANNING
 *===========================================================================*/

/**
 * @brief Plan movement to goal
 *
 * WHAT: Generate motor plan to achieve goal
 * WHY:  Convert high-level intention to motor commands
 * HOW:  Use premotor/SMA for trajectory planning
 *
 * @param adapter Adapter instance
 * @param goal Movement goal specification
 * @return true if plan generated, false on failure
 */
bool motor_plan_movement(motor_adapter_t* adapter, const motor_goal_t* goal);

/**
 * @brief Plan trajectory with waypoints
 *
 * WHAT: Generate trajectory through specified waypoints
 * WHY:  Enable complex multi-point movements
 * HOW:  Optimize trajectory through waypoints
 *
 * @param adapter Adapter instance
 * @param region Target body region
 * @param waypoints Trajectory waypoints
 * @param num_waypoints Number of waypoints
 * @return true if trajectory planned, false on failure
 */
bool motor_plan_trajectory(motor_adapter_t* adapter,
                            motor_region_t region,
                            const trajectory_waypoint_t* waypoints,
                            uint32_t num_waypoints);

/**
 * @brief Plan execution of stored program
 *
 * WHAT: Prepare to execute stored motor program
 * WHY:  Execute learned motor skills
 * HOW:  Load program, adapt to current state
 *
 * @param adapter Adapter instance
 * @param program_id Program to execute
 * @param speed_factor Speed scaling factor (1.0 = normal)
 * @return true if ready to execute, false on failure
 */
bool motor_plan_program_execution(motor_adapter_t* adapter,
                                   uint32_t program_id,
                                   float speed_factor);

/*=============================================================================
 * MOTOR EXECUTION
 *===========================================================================*/

/**
 * @brief Begin movement execution
 *
 * WHAT: Start executing planned movement
 * WHY:  Initiate motor output
 * HOW:  Begin generating motor commands
 *
 * @param adapter Adapter instance
 * @return true on success
 */
bool motor_begin_execution(motor_adapter_t* adapter);

/**
 * @brief Update execution state (call at execution rate)
 *
 * WHAT: Advance movement execution by one timestep
 * WHY:  Generate continuous motor output
 * HOW:  Compute next commands, apply corrections
 *
 * @param adapter Adapter instance
 * @param dt_ms Time since last update
 * @return true if still executing, false if complete or error
 */
bool motor_update_execution(motor_adapter_t* adapter, float dt_ms);

/**
 * @brief Stop current execution
 *
 * WHAT: Immediately stop current movement
 * WHY:  Emergency stop, interruption
 * HOW:  Generate stop commands, clear buffers
 *
 * @param adapter Adapter instance
 * @return true on success
 */
bool motor_stop_execution(motor_adapter_t* adapter);

/**
 * @brief Get next motor command
 *
 * WHAT: Retrieve next command from output queue
 * WHY:  Feed motor system incrementally
 * HOW:  Pop from command queue
 *
 * @param adapter Adapter instance
 * @param command Output command (filled on success)
 * @return true if command available, false if queue empty
 */
bool motor_get_next_command(motor_adapter_t* adapter, motor_command_t* command);

/**
 * @brief Get movement result
 *
 * WHAT: Get result of completed movement
 * WHY:  Evaluate movement outcome
 * HOW:  Return cached result after completion
 *
 * @param adapter Adapter instance
 * @param result Output result structure
 * @return true if result available
 */
bool motor_get_result(const motor_adapter_t* adapter, motor_result_t* result);

/*=============================================================================
 * SENSORY FEEDBACK
 *===========================================================================*/

/**
 * @brief Update effector state from sensory feedback
 *
 * WHAT: Receive proprioceptive feedback
 * WHY:  Enable closed-loop control
 * HOW:  Update internal state, compute corrections
 *
 * @param adapter Adapter instance
 * @param effector_id Effector receiving feedback
 * @param state Current effector state
 * @return true on success
 */
bool motor_update_feedback(motor_adapter_t* adapter,
                            uint32_t effector_id,
                            const motor_effector_state_t* state);

/**
 * @brief Process visual feedback for reaching
 *
 * WHAT: Incorporate visual information for corrections
 * WHY:  Visuomotor integration for accurate reaching
 * HOW:  Compute visually-guided corrections
 *
 * @param adapter Adapter instance
 * @param effector_id Effector being tracked
 * @param visual_position Visually observed position
 * @param confidence Visual estimate confidence [0, 1]
 * @return true on success
 */
bool motor_update_visual_feedback(motor_adapter_t* adapter,
                                   uint32_t effector_id,
                                   const motor_vec3_t* visual_position,
                                   float confidence);

/*=============================================================================
 * INTEGRATION INTERFACES
 *===========================================================================*/

/**
 * @brief Receive action selection from basal ganglia
 *
 * WHAT: Accept selected action from BG
 * WHY:  BG determines which movement to execute
 * HOW:  Use BG output to select motor program
 *
 * @param adapter Adapter instance
 * @param action_id Selected action identifier
 * @param vigor Movement vigor/amplitude [0, 1]
 * @return true on success
 */
bool motor_receive_bg_selection(motor_adapter_t* adapter,
                                 uint32_t action_id,
                                 float vigor);

/**
 * @brief Receive cerebellar correction
 *
 * WHAT: Apply cerebellar timing/coordination correction
 * WHY:  Cerebellum provides fine motor control
 * HOW:  Modify motor commands based on cerebellar output
 *
 * @param adapter Adapter instance
 * @param effector_id Effector to correct
 * @param timing_correction Timing adjustment (ms)
 * @param amplitude_correction Amplitude adjustment factor
 * @return true on success
 */
bool motor_receive_cerebellar_correction(motor_adapter_t* adapter,
                                          uint32_t effector_id,
                                          float timing_correction,
                                          float amplitude_correction);

/*=============================================================================
 * TRAINING INTERFACE
 *===========================================================================*/

/**
 * @brief Train on movement outcome
 *
 * WHAT: Learn from movement success/failure
 * WHY:  Improve motor control through practice
 * HOW:  Backpropagate error to motor program
 *
 * @param adapter Adapter instance
 * @param target_position Desired final position
 * @param learning_rate Learning rate (0 = use config default)
 * @return true on success
 */
bool motor_train_movement(motor_adapter_t* adapter,
                           const motor_vec3_t* target_position,
                           float learning_rate);

/**
 * @brief Train from demonstration
 *
 * WHAT: Learn motor program from observed trajectory
 * WHY:  Imitation learning of motor skills
 * HOW:  Encode demonstrated trajectory as motor program
 *
 * @param adapter Adapter instance
 * @param waypoints Demonstrated trajectory
 * @param num_waypoints Number of waypoints
 * @param program_name Name for learned program
 * @return Program ID if learned, 0 on failure
 */
uint32_t motor_train_from_demonstration(motor_adapter_t* adapter,
                                         const trajectory_waypoint_t* waypoints,
                                         uint32_t num_waypoints,
                                         const char* program_name);

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

/**
 * @brief Get current processing status
 *
 * @param adapter Adapter instance
 * @return Current status
 */
motor_status_t motor_get_status(const motor_adapter_t* adapter);

/**
 * @brief Get last error code
 *
 * @param adapter Adapter instance
 * @return Last error, or MOTOR_ERROR_NONE
 */
motor_error_t motor_get_last_error(const motor_adapter_t* adapter);

/**
 * @brief Get error description string
 *
 * @param error Error code
 * @return Human-readable error description
 */
const char* motor_error_string(motor_error_t error);

/**
 * @brief Get status description string
 *
 * @param status Status code
 * @return Human-readable status description
 */
const char* motor_status_string(motor_status_t status);

/**
 * @brief Get adapter statistics
 *
 * @param adapter Adapter instance
 * @param stats Output statistics structure
 * @return true on success
 */
bool motor_get_stats(const motor_adapter_t* adapter, motor_stats_t* stats);

/**
 * @brief Get adapter configuration
 *
 * @param adapter Adapter instance
 * @param config Output configuration structure
 * @return true on success
 */
bool motor_get_config(const motor_adapter_t* adapter, motor_config_t* config);

/**
 * @brief Get effector state
 *
 * @param adapter Adapter instance
 * @param effector_id Effector to query
 * @param state Output state structure
 * @return true if effector exists
 */
bool motor_get_effector_state(const motor_adapter_t* adapter,
                               uint32_t effector_id,
                               motor_effector_state_t* state);

/*=============================================================================
 * CALLBACK REGISTRATION
 *===========================================================================*/

/**
 * @brief Set command output callback
 *
 * @param adapter Adapter instance
 * @param callback Command handler function
 * @param user_data User context
 * @return true on success
 */
bool motor_set_command_callback(motor_adapter_t* adapter,
                                 motor_command_callback_t callback,
                                 void* user_data);

/**
 * @brief Set completion callback
 *
 * @param adapter Adapter instance
 * @param callback Completion handler function
 * @param user_data User context
 * @return true on success
 */
bool motor_set_complete_callback(motor_adapter_t* adapter,
                                  motor_complete_callback_t callback,
                                  void* user_data);

/**
 * @brief Set event callback
 *
 * @param adapter Adapter instance
 * @param callback Event handler function
 * @param user_data User context
 * @return true on success
 */
bool motor_set_event_callback(motor_adapter_t* adapter,
                               motor_event_callback_t callback,
                               void* user_data);

/*=============================================================================
 * BIO-ASYNC COMMUNICATION
 *===========================================================================*/

/**
 * @brief Get bio-async module context
 *
 * WHAT: Returns the bio-async module context for motor cortex
 * WHY:  Allow external modules to send messages to motor cortex
 * HOW:  Returns internal bio_module_context_t
 *
 * @param adapter Adapter instance
 * @return Bio-async module context, or NULL if not enabled
 */
bio_module_context_t motor_get_bio_context(motor_adapter_t* adapter);

/**
 * @brief Process pending bio-async messages
 *
 * WHAT: Process messages in motor cortex inbox
 * WHY:  Handle incoming requests from other modules
 * HOW:  Calls bio_router_process_inbox and invokes handlers
 *
 * @param adapter Adapter instance
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t motor_process_bio_messages(motor_adapter_t* adapter, uint32_t max_messages);

/**
 * @brief Request movement execution asynchronously
 *
 * WHAT: Send movement request via bio-async
 * WHY:  Async communication with motor control
 * HOW:  Sends BIO_MSG_MOTOR_COMMAND_REQUEST, returns future
 *
 * @param adapter Adapter instance
 * @param goal Movement goal
 * @return Future for movement result, or NULL on failure
 */
nimcp_bio_future_t motor_request_movement_async(
    motor_adapter_t* adapter,
    const motor_goal_t* goal
);

/**
 * @brief Broadcast movement completion
 *
 * WHAT: Notify all modules that movement is complete
 * WHY:  Allow sensory systems, planning to sync
 * HOW:  Broadcasts BIO_MSG_MOTOR_COMPLETE
 *
 * @param adapter Adapter instance
 * @param result Movement result to broadcast
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t motor_broadcast_completion(
    motor_adapter_t* adapter,
    const motor_result_t* result
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MOTOR_ADAPTER_H */
