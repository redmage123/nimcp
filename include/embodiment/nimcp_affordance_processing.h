/**
 * @file nimcp_affordance_processing.h
 * @brief Affordance Detection and Processing for NIMCP Embodied Cognition
 *
 * Biological Inspiration:
 * - Gibson's ecological psychology: Affordances are action possibilities
 *   directly perceived in the environment
 * - Dorsal visual stream: Object properties processed for action guidance
 * - Premotor cortex: Object-action associations and affordance competition
 * - Mirror neuron system: Action understanding through motor simulation
 *
 * This module enables:
 * - Detection of action possibilities in environment
 * - Object-action relationship encoding
 * - Affordance competition and selection
 * - Context-dependent affordance modulation
 * - Motor readiness preparation
 *
 * Key Features:
 * - Multi-affordance detection per object
 * - Saliency-based affordance ranking
 * - Goal-directed affordance filtering
 * - Temporal affordance dynamics
 * - Affordance-motor coupling
 * - Statistics tracking
 *
 * @version 1.0
 * @date 2025-01-13
 */

#ifndef NIMCP_AFFORDANCE_PROCESSING_H
#define NIMCP_AFFORDANCE_PROCESSING_H

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
 * @brief Maximum number of affordances per object
 */
#define NIMCP_AFFORDANCE_MAX_PER_OBJECT 16

/**
 * @brief Maximum number of objects tracked
 */
#define NIMCP_AFFORDANCE_MAX_OBJECTS 64

/**
 * @brief Maximum number of action types
 */
#define NIMCP_AFFORDANCE_MAX_ACTION_TYPES 32

/**
 * @brief Affordance history size for temporal dynamics
 */
#define NIMCP_AFFORDANCE_HISTORY_SIZE 32

/**
 * @brief Maximum motor programs per affordance
 */
#define NIMCP_AFFORDANCE_MAX_MOTOR_PROGRAMS 8

/* ============================================================================
 * Error Codes
 * ============================================================================ */

/**
 * @brief Affordance-specific error codes (9000-9099 range)
 */
typedef enum {
    NIMCP_AFFORDANCE_OK = 0,                     /**< Operation successful */
    NIMCP_AFFORDANCE_ERROR = 9000,               /**< Generic affordance error */
    NIMCP_AFFORDANCE_ERROR_NULL_PARAM = 9001,    /**< Null parameter provided */
    NIMCP_AFFORDANCE_ERROR_INVALID_CONFIG = 9002,/**< Invalid configuration */
    NIMCP_AFFORDANCE_ERROR_NOT_INITIALIZED = 9003,/**< System not initialized */
    NIMCP_AFFORDANCE_ERROR_OBJECT_LIMIT = 9004,  /**< Object limit reached */
    NIMCP_AFFORDANCE_ERROR_AFFORDANCE_LIMIT = 9005, /**< Affordance limit reached */
    NIMCP_AFFORDANCE_ERROR_INVALID_OBJECT = 9006,/**< Invalid object ID */
    NIMCP_AFFORDANCE_ERROR_INVALID_ACTION = 9007,/**< Invalid action type */
    NIMCP_AFFORDANCE_ERROR_NO_AFFORDANCES = 9008,/**< No affordances detected */
    NIMCP_AFFORDANCE_ERROR_MEMORY = 9009,        /**< Memory allocation failed */
    NIMCP_AFFORDANCE_ERROR_COMPETITION = 9010    /**< Competition resolution failed */
} nimcp_affordance_error_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Standard action types for affordance encoding
 *
 * WHAT: Categories of actions that objects can afford
 * WHY:  Enables structured affordance representation
 */
typedef enum {
    NIMCP_ACTION_NONE = 0,
    NIMCP_ACTION_GRASP,           /**< Grasping/holding */
    NIMCP_ACTION_PUSH,            /**< Pushing/moving */
    NIMCP_ACTION_PULL,            /**< Pulling toward self */
    NIMCP_ACTION_LIFT,            /**< Lifting upward */
    NIMCP_ACTION_ROTATE,          /**< Rotating/turning */
    NIMCP_ACTION_PRESS,           /**< Pressing/button activation */
    NIMCP_ACTION_THROW,           /**< Throwing/releasing */
    NIMCP_ACTION_PLACE,           /**< Placing/positioning */
    NIMCP_ACTION_INSERT,          /**< Inserting into receptacle */
    NIMCP_ACTION_EXTRACT,         /**< Extracting from container */
    NIMCP_ACTION_CUT,             /**< Cutting/severing */
    NIMCP_ACTION_POUR,            /**< Pouring liquids */
    NIMCP_ACTION_OPEN,            /**< Opening containers/doors */
    NIMCP_ACTION_CLOSE,           /**< Closing containers/doors */
    NIMCP_ACTION_REACH,           /**< Reaching toward */
    NIMCP_ACTION_MANIPULATE,      /**< General manipulation */
    NIMCP_ACTION_SUPPORT,         /**< Supporting weight */
    NIMCP_ACTION_CONTAIN,         /**< Containing objects */
    NIMCP_ACTION_TRAVERSE,        /**< Walking/climbing on */
    NIMCP_ACTION_SIT,             /**< Sitting on */
    NIMCP_ACTION_COUNT
} nimcp_action_type_t;

/**
 * @brief Affordance state
 *
 * WHAT: Current state of an affordance
 * WHY:  Tracks affordance lifecycle and competition
 */
typedef enum {
    NIMCP_AFFORDANCE_STATE_INACTIVE = 0, /**< Not currently relevant */
    NIMCP_AFFORDANCE_STATE_DETECTED,     /**< Detected but not selected */
    NIMCP_AFFORDANCE_STATE_COMPETING,    /**< In competition phase */
    NIMCP_AFFORDANCE_STATE_SELECTED,     /**< Won competition, ready for action */
    NIMCP_AFFORDANCE_STATE_EXECUTING,    /**< Action in progress */
    NIMCP_AFFORDANCE_STATE_COMPLETED,    /**< Action completed */
    NIMCP_AFFORDANCE_STATE_INHIBITED,    /**< Suppressed by other affordance */
    NIMCP_AFFORDANCE_STATE_COUNT
} nimcp_affordance_state_t;

/**
 * @brief Object category for affordance priors
 *
 * WHAT: Categories of objects with typical affordances
 * WHY:  Enables prior-based affordance prediction
 */
typedef enum {
    NIMCP_OBJECT_CATEGORY_UNKNOWN = 0,
    NIMCP_OBJECT_CATEGORY_TOOL,          /**< Tools and implements */
    NIMCP_OBJECT_CATEGORY_CONTAINER,     /**< Containers and vessels */
    NIMCP_OBJECT_CATEGORY_SURFACE,       /**< Surfaces and platforms */
    NIMCP_OBJECT_CATEGORY_SUPPORT,       /**< Supporting structures */
    NIMCP_OBJECT_CATEGORY_MANIPULANDUM,  /**< Objects for manipulation */
    NIMCP_OBJECT_CATEGORY_BUTTON,        /**< Buttons and switches */
    NIMCP_OBJECT_CATEGORY_HANDLE,        /**< Handles and grips */
    NIMCP_OBJECT_CATEGORY_OPENING,       /**< Doors, lids, openings */
    NIMCP_OBJECT_CATEGORY_PROJECTILE,    /**< Objects for throwing */
    NIMCP_OBJECT_CATEGORY_COUNT
} nimcp_object_category_t;

/**
 * @brief Competition resolution strategy
 */
typedef enum {
    NIMCP_COMPETITION_WINNER_TAKE_ALL = 0, /**< Highest saliency wins */
    NIMCP_COMPETITION_SOFTMAX,             /**< Probabilistic selection */
    NIMCP_COMPETITION_THRESHOLD,           /**< All above threshold activate */
    NIMCP_COMPETITION_GOAL_DIRECTED,       /**< Goal compatibility determines */
    NIMCP_COMPETITION_COUNT
} nimcp_competition_strategy_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Motor program specification
 *
 * WHAT: Describes a motor program associated with an affordance
 * WHY:  Enables affordance-motor coupling
 */
typedef struct {
    uint32_t program_id;           /**< Motor program identifier */
    nimcp_action_type_t action;    /**< Associated action type */
    double complexity;             /**< Motor complexity [0-1] */
    double duration_estimate;      /**< Estimated execution time (seconds) */
    double effort_estimate;        /**< Estimated effort [0-1] */
    double success_probability;    /**< Historical success rate [0-1] */
    bool requires_bimanual;        /**< Requires both hands */
    bool requires_tool;            /**< Requires tool use */
} nimcp_motor_program_t;

/**
 * @brief Object properties for affordance computation
 *
 * WHAT: Physical and semantic properties of perceived objects
 * WHY:  Forms basis for affordance detection
 */
typedef struct {
    uint32_t object_id;            /**< Unique object identifier */
    nimcp_object_category_t category; /**< Object category */

    /* Spatial properties */
    double position[3];            /**< Position (x, y, z) in body-centered coords */
    double orientation[4];         /**< Orientation quaternion */
    double dimensions[3];          /**< Bounding box (width, height, depth) */
    double distance;               /**< Distance from body */

    /* Physical properties */
    double estimated_mass;         /**< Estimated mass (kg) */
    double surface_friction;       /**< Surface friction coefficient */
    double rigidity;               /**< Rigidity [0=soft, 1=rigid] */
    bool is_graspable;             /**< Size compatible with grasp */
    bool is_movable;               /**< Can be moved by agent */
    bool has_handle;               /**< Has graspable handle */
    bool is_container;             /**< Can contain other objects */

    /* Temporal properties */
    uint64_t first_seen_time;      /**< When first detected */
    uint64_t last_update_time;     /**< Last property update */
    bool is_stationary;            /**< Object is stationary */
    double velocity[3];            /**< Object velocity if moving */
} nimcp_object_properties_t;

/**
 * @brief Single affordance representation
 *
 * WHAT: Represents one action possibility for an object
 * WHY:  Core unit of affordance processing
 */
typedef struct {
    uint32_t affordance_id;        /**< Unique affordance identifier */
    uint32_t object_id;            /**< Associated object */
    nimcp_action_type_t action;    /**< Action type afforded */
    nimcp_affordance_state_t state; /**< Current state */

    /* Affordance strength */
    double saliency;               /**< Perceptual saliency [0-1] */
    double goal_relevance;         /**< Relevance to current goal [0-1] */
    double motor_readiness;        /**< Motor preparation level [0-1] */
    double competition_score;      /**< Score in competition [0-1] */

    /* Action parameters */
    double approach_vector[3];     /**< Direction of approach */
    double grasp_point[3];         /**< Optimal grasp location */
    double action_axis[3];         /**< Primary action axis */

    /* Feasibility assessment */
    double reachability;           /**< Can reach the object [0-1] */
    double manipulability;         /**< Ease of manipulation [0-1] */
    double effort_estimate;        /**< Estimated effort [0-1] */
    double success_probability;    /**< Estimated success [0-1] */

    /* Motor coupling */
    nimcp_motor_program_t motor_programs[NIMCP_AFFORDANCE_MAX_MOTOR_PROGRAMS];
    uint32_t num_motor_programs;   /**< Number of associated programs */
    uint32_t selected_program;     /**< Currently selected program index */

    /* Timing */
    uint64_t detection_time;       /**< When affordance detected */
    uint64_t selection_time;       /**< When selected (if selected) */
    double decay_rate;             /**< Saliency decay rate */
} nimcp_affordance_t;

/**
 * @brief Current goal state for goal-directed filtering
 */
typedef struct {
    uint32_t goal_id;              /**< Goal identifier */
    nimcp_action_type_t target_action; /**< Target action type */
    uint32_t target_object_id;     /**< Target object (0 = any) */
    nimcp_object_category_t target_category; /**< Target category */
    double urgency;                /**< Goal urgency [0-1] */
    double priority;               /**< Goal priority [0-1] */
    bool is_active;                /**< Goal is currently active */
} nimcp_affordance_goal_t;

/**
 * @brief Competition result
 */
typedef struct {
    uint32_t winner_affordance_id; /**< Winning affordance ID */
    uint32_t winner_object_id;     /**< Associated object ID */
    nimcp_action_type_t winner_action; /**< Winning action type */
    double winner_score;           /**< Winning score */
    uint32_t num_competitors;      /**< Number of competitors */
    double competition_duration;   /**< Time to resolve (seconds) */
    nimcp_competition_strategy_t strategy_used; /**< Strategy used */
} nimcp_competition_result_t;

/**
 * @brief Affordance history entry
 */
typedef struct {
    uint32_t affordance_id;        /**< Affordance that was selected */
    uint32_t object_id;            /**< Associated object */
    nimcp_action_type_t action;    /**< Action taken */
    bool success;                  /**< Action succeeded */
    double actual_effort;          /**< Actual effort expended */
    double actual_duration;        /**< Actual duration */
    uint64_t timestamp;            /**< When action completed */
} nimcp_affordance_history_t;

/**
 * @brief Affordance processing statistics
 */
typedef struct {
    uint64_t total_detections;     /**< Total affordances detected */
    uint64_t total_competitions;   /**< Total competitions resolved */
    uint64_t total_selections;     /**< Total affordances selected */
    uint64_t total_executions;     /**< Total actions executed */
    uint64_t successful_actions;   /**< Successful action count */
    uint64_t failed_actions;       /**< Failed action count */
    double avg_detection_time;     /**< Average detection latency */
    double avg_competition_time;   /**< Average competition duration */
    double avg_saliency;           /**< Average winning saliency */
    double action_success_rate;    /**< Overall success rate */
    uint64_t objects_tracked;      /**< Current objects being tracked */
    uint64_t active_affordances;   /**< Currently active affordances */
    uint64_t creation_time;        /**< System creation timestamp */
} nimcp_affordance_stats_t;

/**
 * @brief Configuration parameters
 */
typedef struct {
    /* Detection parameters */
    double detection_threshold;    /**< Minimum saliency to detect [0-1] */
    double goal_weight;            /**< Weight of goal relevance [0-1] */
    double motor_weight;           /**< Weight of motor readiness [0-1] */
    double decay_rate;             /**< Saliency decay per second */

    /* Competition parameters */
    nimcp_competition_strategy_t strategy; /**< Competition strategy */
    double competition_threshold;  /**< Threshold for threshold strategy */
    double softmax_temperature;    /**< Temperature for softmax */
    uint32_t max_competition_cycles; /**< Max iterations for competition */

    /* Motor coupling */
    bool enable_motor_coupling;    /**< Enable motor program coupling */
    double motor_readiness_threshold; /**< Threshold for motor prep */

    /* Limits */
    uint32_t max_objects;          /**< Maximum tracked objects */
    uint32_t max_affordances_per_object; /**< Max affordances per object */

    /* Timing */
    double update_rate_hz;         /**< Processing update rate */
    bool enable_history;           /**< Enable history tracking */
} nimcp_affordance_config_t;

/**
 * @brief Affordance processing context (opaque)
 */
typedef struct nimcp_affordance_context nimcp_affordance_context_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Fills config with sensible defaults
 * WHY:  Simplifies initialization
 *
 * @param config Configuration to fill
 */
void nimcp_affordance_default_config(nimcp_affordance_config_t* config);

/**
 * @brief Create affordance processing context
 *
 * WHAT: Allocates and initializes affordance processor
 * WHY:  Entry point for affordance processing
 *
 * @param config Configuration parameters
 * @return Context pointer or NULL on failure
 */
nimcp_affordance_context_t* nimcp_affordance_create(
    const nimcp_affordance_config_t* config
);

/**
 * @brief Initialize existing context
 *
 * WHAT: Initializes pre-allocated context
 * WHY:  Alternative to create for static allocation
 *
 * @param ctx Context to initialize
 * @param config Configuration parameters
 * @return NIMCP_AFFORDANCE_OK on success
 */
nimcp_affordance_error_t nimcp_affordance_init(
    nimcp_affordance_context_t* ctx,
    const nimcp_affordance_config_t* config
);

/**
 * @brief Reset affordance context
 *
 * WHAT: Clears all state, keeps configuration
 * WHY:  Allows reuse without reallocation
 *
 * @param ctx Context to reset
 * @return NIMCP_AFFORDANCE_OK on success
 */
nimcp_affordance_error_t nimcp_affordance_reset(
    nimcp_affordance_context_t* ctx
);

/**
 * @brief Destroy affordance context
 *
 * WHAT: Frees all resources
 * WHY:  Cleanup
 *
 * @param ctx Context to destroy
 */
void nimcp_affordance_destroy(nimcp_affordance_context_t* ctx);

/* ============================================================================
 * Object Management API
 * ============================================================================ */

/**
 * @brief Register a new object
 *
 * WHAT: Adds object to tracking and detects affordances
 * WHY:  Entry point for perceived objects
 *
 * @param ctx Affordance context
 * @param properties Object properties
 * @return NIMCP_AFFORDANCE_OK on success
 */
nimcp_affordance_error_t nimcp_affordance_register_object(
    nimcp_affordance_context_t* ctx,
    const nimcp_object_properties_t* properties
);

/**
 * @brief Update object properties
 *
 * WHAT: Updates tracked object and recomputes affordances
 * WHY:  Handles object motion and property changes
 *
 * @param ctx Affordance context
 * @param properties Updated properties
 * @return NIMCP_AFFORDANCE_OK on success
 */
nimcp_affordance_error_t nimcp_affordance_update_object(
    nimcp_affordance_context_t* ctx,
    const nimcp_object_properties_t* properties
);

/**
 * @brief Remove object from tracking
 *
 * WHAT: Removes object and associated affordances
 * WHY:  Object no longer in scene
 *
 * @param ctx Affordance context
 * @param object_id Object to remove
 * @return NIMCP_AFFORDANCE_OK on success
 */
nimcp_affordance_error_t nimcp_affordance_remove_object(
    nimcp_affordance_context_t* ctx,
    uint32_t object_id
);

/**
 * @brief Get object properties
 *
 * @param ctx Affordance context
 * @param object_id Object to query
 * @param properties Output properties
 * @return NIMCP_AFFORDANCE_OK on success
 */
nimcp_affordance_error_t nimcp_affordance_get_object(
    const nimcp_affordance_context_t* ctx,
    uint32_t object_id,
    nimcp_object_properties_t* properties
);

/* ============================================================================
 * Affordance Detection API
 * ============================================================================ */

/**
 * @brief Detect affordances for an object
 *
 * WHAT: Analyzes object and detects action possibilities
 * WHY:  Core affordance detection functionality
 *
 * @param ctx Affordance context
 * @param object_id Object to analyze
 * @param affordances Output array of detected affordances
 * @param max_affordances Maximum to return
 * @param num_detected Output count of detected affordances
 * @return NIMCP_AFFORDANCE_OK on success
 */
nimcp_affordance_error_t nimcp_affordance_detect(
    nimcp_affordance_context_t* ctx,
    uint32_t object_id,
    nimcp_affordance_t* affordances,
    uint32_t max_affordances,
    uint32_t* num_detected
);

/**
 * @brief Detect all affordances in scene
 *
 * WHAT: Detects affordances for all tracked objects
 * WHY:  Scene-wide affordance analysis
 *
 * @param ctx Affordance context
 * @param affordances Output array
 * @param max_affordances Maximum to return
 * @param num_detected Output count
 * @return NIMCP_AFFORDANCE_OK on success
 */
nimcp_affordance_error_t nimcp_affordance_detect_all(
    nimcp_affordance_context_t* ctx,
    nimcp_affordance_t* affordances,
    uint32_t max_affordances,
    uint32_t* num_detected
);

/**
 * @brief Get specific affordance
 *
 * @param ctx Affordance context
 * @param affordance_id Affordance to retrieve
 * @param affordance Output affordance
 * @return NIMCP_AFFORDANCE_OK on success
 */
nimcp_affordance_error_t nimcp_affordance_get(
    const nimcp_affordance_context_t* ctx,
    uint32_t affordance_id,
    nimcp_affordance_t* affordance
);

/**
 * @brief Get affordances by action type
 *
 * @param ctx Affordance context
 * @param action Action type to filter by
 * @param affordances Output array
 * @param max_affordances Maximum to return
 * @param num_found Output count
 * @return NIMCP_AFFORDANCE_OK on success
 */
nimcp_affordance_error_t nimcp_affordance_get_by_action(
    const nimcp_affordance_context_t* ctx,
    nimcp_action_type_t action,
    nimcp_affordance_t* affordances,
    uint32_t max_affordances,
    uint32_t* num_found
);

/* ============================================================================
 * Competition and Selection API
 * ============================================================================ */

/**
 * @brief Run affordance competition
 *
 * WHAT: Resolves competition among detected affordances
 * WHY:  Selects action from multiple possibilities
 *
 * @param ctx Affordance context
 * @param result Output competition result
 * @return NIMCP_AFFORDANCE_OK on success
 */
nimcp_affordance_error_t nimcp_affordance_compete(
    nimcp_affordance_context_t* ctx,
    nimcp_competition_result_t* result
);

/**
 * @brief Run goal-directed competition
 *
 * WHAT: Competition filtered by current goal
 * WHY:  Goal-oriented affordance selection
 *
 * @param ctx Affordance context
 * @param goal Current goal state
 * @param result Output result
 * @return NIMCP_AFFORDANCE_OK on success
 */
nimcp_affordance_error_t nimcp_affordance_compete_for_goal(
    nimcp_affordance_context_t* ctx,
    const nimcp_affordance_goal_t* goal,
    nimcp_competition_result_t* result
);

/**
 * @brief Manually select an affordance
 *
 * WHAT: Directly selects specified affordance
 * WHY:  Override automatic competition
 *
 * @param ctx Affordance context
 * @param affordance_id Affordance to select
 * @return NIMCP_AFFORDANCE_OK on success
 */
nimcp_affordance_error_t nimcp_affordance_select(
    nimcp_affordance_context_t* ctx,
    uint32_t affordance_id
);

/**
 * @brief Inhibit an affordance
 *
 * WHAT: Suppresses specified affordance
 * WHY:  Prevents unwanted action activation
 *
 * @param ctx Affordance context
 * @param affordance_id Affordance to inhibit
 * @return NIMCP_AFFORDANCE_OK on success
 */
nimcp_affordance_error_t nimcp_affordance_inhibit(
    nimcp_affordance_context_t* ctx,
    uint32_t affordance_id
);

/* ============================================================================
 * Motor Coupling API
 * ============================================================================ */

/**
 * @brief Get motor programs for affordance
 *
 * WHAT: Retrieves motor programs associated with affordance
 * WHY:  Enables action execution
 *
 * @param ctx Affordance context
 * @param affordance_id Affordance to query
 * @param programs Output array
 * @param max_programs Maximum to return
 * @param num_programs Output count
 * @return NIMCP_AFFORDANCE_OK on success
 */
nimcp_affordance_error_t nimcp_affordance_get_motor_programs(
    const nimcp_affordance_context_t* ctx,
    uint32_t affordance_id,
    nimcp_motor_program_t* programs,
    uint32_t max_programs,
    uint32_t* num_programs
);

/**
 * @brief Update motor readiness
 *
 * WHAT: Updates motor preparation level for affordance
 * WHY:  Tracks motor system state
 *
 * @param ctx Affordance context
 * @param affordance_id Affordance to update
 * @param readiness New readiness level [0-1]
 * @return NIMCP_AFFORDANCE_OK on success
 */
nimcp_affordance_error_t nimcp_affordance_update_motor_readiness(
    nimcp_affordance_context_t* ctx,
    uint32_t affordance_id,
    double readiness
);

/**
 * @brief Report action outcome
 *
 * WHAT: Records result of executed action
 * WHY:  Updates success probabilities and history
 *
 * @param ctx Affordance context
 * @param affordance_id Executed affordance
 * @param success Whether action succeeded
 * @param actual_effort Actual effort expended [0-1]
 * @param actual_duration Actual duration (seconds)
 * @return NIMCP_AFFORDANCE_OK on success
 */
nimcp_affordance_error_t nimcp_affordance_report_outcome(
    nimcp_affordance_context_t* ctx,
    uint32_t affordance_id,
    bool success,
    double actual_effort,
    double actual_duration
);

/* ============================================================================
 * Update and Processing API
 * ============================================================================ */

/**
 * @brief Process one update cycle
 *
 * WHAT: Updates affordance states, runs decay, etc.
 * WHY:  Main processing loop entry point
 *
 * @param ctx Affordance context
 * @param delta_time Time since last update (seconds)
 * @return NIMCP_AFFORDANCE_OK on success
 */
nimcp_affordance_error_t nimcp_affordance_update(
    nimcp_affordance_context_t* ctx,
    double delta_time
);

/**
 * @brief Set current goal
 *
 * WHAT: Sets goal for goal-directed processing
 * WHY:  Enables goal-based affordance filtering
 *
 * @param ctx Affordance context
 * @param goal Goal to set
 * @return NIMCP_AFFORDANCE_OK on success
 */
nimcp_affordance_error_t nimcp_affordance_set_goal(
    nimcp_affordance_context_t* ctx,
    const nimcp_affordance_goal_t* goal
);

/**
 * @brief Clear current goal
 *
 * @param ctx Affordance context
 * @return NIMCP_AFFORDANCE_OK on success
 */
nimcp_affordance_error_t nimcp_affordance_clear_goal(
    nimcp_affordance_context_t* ctx
);

/* ============================================================================
 * Statistics and Utility API
 * ============================================================================ */

/**
 * @brief Get statistics
 *
 * @param ctx Affordance context
 * @param stats Output statistics
 * @return NIMCP_AFFORDANCE_OK on success
 */
nimcp_affordance_error_t nimcp_affordance_get_stats(
    const nimcp_affordance_context_t* ctx,
    nimcp_affordance_stats_t* stats
);

/**
 * @brief Get action type name
 *
 * @param action Action type
 * @return String name
 */
const char* nimcp_affordance_action_name(nimcp_action_type_t action);

/**
 * @brief Get state name
 *
 * @param state Affordance state
 * @return String name
 */
const char* nimcp_affordance_state_name(nimcp_affordance_state_t state);

/**
 * @brief Get object category name
 *
 * @param category Object category
 * @return String name
 */
const char* nimcp_affordance_category_name(nimcp_object_category_t category);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AFFORDANCE_PROCESSING_H */
