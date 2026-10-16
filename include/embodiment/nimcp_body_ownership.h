/**
 * @file nimcp_body_ownership.h
 * @brief Body Schema and Ownership for NIMCP Embodied Cognition
 *
 * Biological Inspiration:
 * - Parietal cortex: Body schema representation and spatial awareness
 * - Rubber hand illusion: Demonstrates plasticity of body ownership
 * - Proprioceptive integration: Merging body position sense
 * - Peripersonal space: Representation of space near the body
 * - Interoceptive awareness: Internal body state sensing
 *
 * This module enables:
 * - Proprioceptive integration across body parts
 * - Body boundary detection and maintenance
 * - Rubber hand illusion-like ownership plasticity
 * - Body schema updating and adaptation
 * - Peripersonal space representation
 *
 * Key Features:
 * - Multi-limb body representation
 * - Proprioceptive prediction and error
 * - Visual-proprioceptive integration
 * - Ownership confidence tracking
 * - Body boundary adaptation
 * - Statistics tracking
 *
 * @version 1.0
 * @date 2025-01-13
 */

#ifndef NIMCP_BODY_OWNERSHIP_H
#define NIMCP_BODY_OWNERSHIP_H

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
 * @brief Maximum number of body parts
 */
#define NIMCP_BODY_MAX_PARTS 32

/**
 * @brief Maximum joints per body part
 */
#define NIMCP_BODY_MAX_JOINTS 8

/**
 * @brief Maximum number of external objects for ownership
 */
#define NIMCP_BODY_MAX_EXTERNAL_OBJECTS 16

/**
 * @brief Proprioceptive history size
 */
#define NIMCP_BODY_PROPRIO_HISTORY_SIZE 64

/**
 * @brief Peripersonal space grid resolution
 */
#define NIMCP_BODY_PERIPERSONAL_GRID_SIZE 16

/* ============================================================================
 * Error Codes
 * ============================================================================ */

/**
 * @brief Body ownership-specific error codes (9100-9199 range)
 */
typedef enum {
    NIMCP_BODY_OK = 0,                        /**< Operation successful */
    NIMCP_BODY_ERROR = 9100,                  /**< Generic body error */
    NIMCP_BODY_ERROR_NULL_PARAM = 9101,       /**< Null parameter provided */
    NIMCP_BODY_ERROR_INVALID_CONFIG = 9102,   /**< Invalid configuration */
    NIMCP_BODY_ERROR_NOT_INITIALIZED = 9103,  /**< System not initialized */
    NIMCP_BODY_ERROR_PART_LIMIT = 9104,       /**< Body part limit reached */
    NIMCP_BODY_ERROR_INVALID_PART = 9105,     /**< Invalid body part ID */
    NIMCP_BODY_ERROR_INVALID_JOINT = 9106,    /**< Invalid joint ID */
    NIMCP_BODY_ERROR_OWNERSHIP_FAILED = 9107, /**< Ownership update failed */
    NIMCP_BODY_ERROR_MEMORY = 9108,           /**< Memory allocation failed */
    NIMCP_BODY_ERROR_PREDICTION = 9109        /**< Prediction error */
} nimcp_body_error_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Standard body part types
 */
typedef enum {
    NIMCP_BODY_PART_UNKNOWN = 0,
    NIMCP_BODY_PART_HEAD,
    NIMCP_BODY_PART_TORSO,
    NIMCP_BODY_PART_LEFT_ARM,
    NIMCP_BODY_PART_RIGHT_ARM,
    NIMCP_BODY_PART_LEFT_HAND,
    NIMCP_BODY_PART_RIGHT_HAND,
    NIMCP_BODY_PART_LEFT_LEG,
    NIMCP_BODY_PART_RIGHT_LEG,
    NIMCP_BODY_PART_LEFT_FOOT,
    NIMCP_BODY_PART_RIGHT_FOOT,
    NIMCP_BODY_PART_NECK,
    NIMCP_BODY_PART_PELVIS,
    NIMCP_BODY_PART_FINGER,
    NIMCP_BODY_PART_TOE,
    NIMCP_BODY_PART_TOOL_EXTENSION,  /**< Tool incorporated into body schema */
    NIMCP_BODY_PART_PROSTHETIC,      /**< Prosthetic limb */
    NIMCP_BODY_PART_VIRTUAL,         /**< Virtual limb */
    NIMCP_BODY_PART_COUNT
} nimcp_body_part_type_t;

/**
 * @brief Joint types
 */
typedef enum {
    NIMCP_JOINT_UNKNOWN = 0,
    NIMCP_JOINT_BALL,           /**< Ball joint (3 DOF) */
    NIMCP_JOINT_HINGE,          /**< Hinge joint (1 DOF) */
    NIMCP_JOINT_SADDLE,         /**< Saddle joint (2 DOF) */
    NIMCP_JOINT_PIVOT,          /**< Pivot joint (1 DOF rotation) */
    NIMCP_JOINT_GLIDING,        /**< Gliding joint */
    NIMCP_JOINT_CONDYLOID,      /**< Condyloid joint (2 DOF) */
    NIMCP_JOINT_COUNT
} nimcp_joint_type_t;

/**
 * @brief Ownership state
 */
typedef enum {
    NIMCP_OWNERSHIP_NONE = 0,        /**< No ownership */
    NIMCP_OWNERSHIP_UNCERTAIN,       /**< Ownership uncertain */
    NIMCP_OWNERSHIP_PARTIAL,         /**< Partial ownership */
    NIMCP_OWNERSHIP_FULL,            /**< Full ownership */
    NIMCP_OWNERSHIP_EXTENDED,        /**< Extended (tool) ownership */
    NIMCP_OWNERSHIP_COUNT
} nimcp_ownership_state_t;

/**
 * @brief Sensory modality for body perception
 */
typedef enum {
    NIMCP_SENSORY_PROPRIOCEPTION = 0, /**< Joint position/movement sense */
    NIMCP_SENSORY_VISION,             /**< Visual feedback */
    NIMCP_SENSORY_TACTILE,            /**< Touch sensation */
    NIMCP_SENSORY_VESTIBULAR,         /**< Balance/orientation */
    NIMCP_SENSORY_INTEROCEPTIVE,      /**< Internal body state */
    NIMCP_SENSORY_COUNT
} nimcp_sensory_modality_t;

/**
 * @brief Body schema update type
 */
typedef enum {
    NIMCP_SCHEMA_UPDATE_NONE = 0,
    NIMCP_SCHEMA_UPDATE_POSITION,     /**< Position update */
    NIMCP_SCHEMA_UPDATE_SIZE,         /**< Size/dimension update */
    NIMCP_SCHEMA_UPDATE_BOUNDARY,     /**< Boundary update */
    NIMCP_SCHEMA_UPDATE_OWNERSHIP,    /**< Ownership change */
    NIMCP_SCHEMA_UPDATE_EXTENSION,    /**< Tool extension */
    NIMCP_SCHEMA_UPDATE_COUNT
} nimcp_schema_update_type_t;

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
} nimcp_body_position_t;

/**
 * @brief Quaternion for orientation
 */
typedef struct {
    double w;
    double x;
    double y;
    double z;
} nimcp_body_quaternion_t;

/**
 * @brief Joint state
 */
typedef struct {
    uint32_t joint_id;                /**< Joint identifier */
    nimcp_joint_type_t type;          /**< Joint type */

    /* Joint angles (radians) */
    double angles[3];                 /**< Joint angles (up to 3 DOF) */
    double velocities[3];             /**< Angular velocities */
    double limits_min[3];             /**< Minimum angle limits */
    double limits_max[3];             /**< Maximum angle limits */

    /* Proprioceptive state */
    double proprioceptive_confidence; /**< Confidence in joint sense [0-1] */
    double predicted_angles[3];       /**< Predicted joint angles */
    double prediction_error;          /**< Prediction error magnitude */

    /* Timing */
    uint64_t last_update;             /**< Last update timestamp */
} nimcp_joint_state_t;

/**
 * @brief Body part representation
 */
typedef struct {
    uint32_t part_id;                 /**< Body part identifier */
    nimcp_body_part_type_t type;      /**< Part type */
    char name[32];                    /**< Human-readable name */

    /* Spatial properties */
    nimcp_body_position_t position;   /**< Position in body frame */
    nimcp_body_quaternion_t orientation; /**< Orientation */
    double dimensions[3];             /**< Bounding box (width, height, depth) */
    double mass;                      /**< Estimated mass (kg) */

    /* Joints */
    nimcp_joint_state_t joints[NIMCP_BODY_MAX_JOINTS];
    uint32_t num_joints;              /**< Number of joints */

    /* Ownership */
    nimcp_ownership_state_t ownership_state; /**< Current ownership */
    double ownership_confidence;      /**< Ownership confidence [0-1] */
    double agency;                    /**< Sense of agency [0-1] */

    /* Connectivity */
    uint32_t parent_part_id;          /**< Parent in kinematic chain */
    uint32_t child_part_ids[8];       /**< Child parts */
    uint32_t num_children;            /**< Number of child parts */

    /* Sensory weighting */
    double sensory_weights[NIMCP_SENSORY_COUNT]; /**< Modality weights */

    /* Timing */
    uint64_t creation_time;           /**< When part was added */
    uint64_t last_update;             /**< Last update timestamp */
    bool is_active;                   /**< Part is active */
} nimcp_body_part_t;

/**
 * @brief Proprioceptive signal
 */
typedef struct {
    uint32_t part_id;                 /**< Associated body part */
    uint32_t joint_id;                /**< Associated joint (if applicable) */

    double position[3];               /**< Sensed position */
    double velocity[3];               /**< Sensed velocity */
    double force[3];                  /**< Sensed force/resistance */

    double confidence;                /**< Signal confidence [0-1] */
    uint64_t timestamp;               /**< Signal timestamp */
} nimcp_proprio_signal_t;

/**
 * @brief Visual feedback for body part
 */
typedef struct {
    uint32_t part_id;                 /**< Associated body part */

    nimcp_body_position_t seen_position; /**< Visually perceived position */
    nimcp_body_quaternion_t seen_orientation; /**< Perceived orientation */

    double confidence;                /**< Visual confidence [0-1] */
    bool is_visible;                  /**< Part is currently visible */
    bool is_occluded;                 /**< Part is occluded */
    uint64_t timestamp;               /**< Observation timestamp */
} nimcp_visual_feedback_t;

/**
 * @brief External object for ownership incorporation
 *
 * WHAT: Represents external objects that may be incorporated into body schema
 * WHY:  Enables rubber hand illusion and tool incorporation
 */
typedef struct {
    uint32_t object_id;               /**< Object identifier */

    nimcp_body_position_t position;   /**< Object position */
    nimcp_body_quaternion_t orientation; /**< Object orientation */
    double dimensions[3];             /**< Object size */

    /* Ownership state */
    double ownership_score;           /**< How "owned" the object feels [0-1] */
    bool is_incorporated;             /**< Incorporated into body schema */
    uint32_t replaces_part_id;        /**< Which body part it replaces (if any) */

    /* Synchrony metrics (for rubber hand illusion) */
    double visual_tactile_sync;       /**< Visual-tactile synchrony [0-1] */
    double sync_duration;             /**< Duration of synchronous stimulation */

    uint64_t first_contact;           /**< First synchronous contact time */
    uint64_t last_update;             /**< Last update timestamp */
} nimcp_external_object_t;

/**
 * @brief Peripersonal space representation
 *
 * WHAT: Represents the space immediately surrounding the body
 * WHY:  Important for threat detection and action planning
 */
typedef struct {
    /* 3D grid of activation */
    double activation[NIMCP_BODY_PERIPERSONAL_GRID_SIZE]
                     [NIMCP_BODY_PERIPERSONAL_GRID_SIZE]
                     [NIMCP_BODY_PERIPERSONAL_GRID_SIZE];

    double grid_origin[3];            /**< Grid origin in body frame */
    double grid_resolution;           /**< Resolution (meters per cell) */
    double extent[3];                 /**< Total extent in meters */

    /* Summary metrics */
    double nearest_object_distance;   /**< Distance to nearest object */
    double nearest_object_direction[3]; /**< Direction to nearest */
    double threat_level;              /**< Overall threat assessment [0-1] */

    uint64_t last_update;             /**< Last update timestamp */
} nimcp_peripersonal_space_t;

/**
 * @brief Body schema update event
 */
typedef struct {
    nimcp_schema_update_type_t type;  /**< Update type */
    uint32_t part_id;                 /**< Affected part */
    double old_value[3];              /**< Previous value */
    double new_value[3];              /**< New value */
    double confidence;                /**< Update confidence */
    uint64_t timestamp;               /**< Event timestamp */
} nimcp_schema_update_t;

/**
 * @brief Body ownership statistics
 */
typedef struct {
    uint64_t total_proprio_updates;   /**< Total proprioceptive updates */
    uint64_t total_visual_updates;    /**< Total visual updates */
    uint64_t total_integrations;      /**< Total multimodal integrations */
    uint64_t ownership_changes;       /**< Ownership state changes */
    uint64_t schema_updates;          /**< Body schema updates */
    uint64_t prediction_errors;       /**< Prediction error count */

    double avg_prediction_error;      /**< Average prediction error */
    double avg_ownership_confidence;  /**< Average ownership confidence */
    double avg_agency;                /**< Average sense of agency */

    uint32_t active_parts;            /**< Currently active body parts */
    uint32_t incorporated_objects;    /**< Incorporated external objects */

    uint64_t creation_time;           /**< System creation timestamp */
} nimcp_body_stats_t;

/**
 * @brief Configuration parameters
 */
typedef struct {
    /* Integration parameters */
    double proprio_weight;            /**< Proprioception weight [0-1] */
    double visual_weight;             /**< Vision weight [0-1] */
    double tactile_weight;            /**< Tactile weight [0-1] */

    /* Prediction parameters */
    double prediction_learning_rate;  /**< How fast to update predictions */
    double prediction_error_threshold; /**< Error threshold for updates */

    /* Ownership parameters */
    double ownership_threshold;       /**< Threshold for ownership [0-1] */
    double ownership_decay_rate;      /**< Decay rate per second */
    double sync_window_ms;            /**< Synchrony window for RHI (ms) */

    /* Body schema */
    bool enable_tool_incorporation;   /**< Allow tool incorporation */
    double incorporation_threshold;   /**< Threshold for incorporation */
    double boundary_margin;           /**< Body boundary margin (meters) */

    /* Peripersonal space */
    bool enable_peripersonal;         /**< Enable peripersonal tracking */
    double peripersonal_range;        /**< Range of peripersonal space (m) */

    /* Limits */
    uint32_t max_body_parts;          /**< Maximum body parts */

    /* Update rate */
    double update_rate_hz;            /**< Processing update rate */
} nimcp_body_config_t;

/**
 * @brief Body ownership context (opaque)
 */
typedef struct nimcp_body_context nimcp_body_context_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Configuration to fill
 */
void nimcp_body_default_config(nimcp_body_config_t* config);

/**
 * @brief Create body ownership context
 *
 * @param config Configuration parameters
 * @return Context pointer or NULL on failure
 */
nimcp_body_context_t* nimcp_body_create(const nimcp_body_config_t* config);

/**
 * @brief Initialize existing context
 *
 * @param ctx Context to initialize
 * @param config Configuration parameters
 * @return NIMCP_BODY_OK on success
 */
nimcp_body_error_t nimcp_body_init(
    nimcp_body_context_t* ctx,
    const nimcp_body_config_t* config
);

/**
 * @brief Reset body context
 *
 * @param ctx Context to reset
 * @return NIMCP_BODY_OK on success
 */
nimcp_body_error_t nimcp_body_reset(nimcp_body_context_t* ctx);

/**
 * @brief Destroy body context
 *
 * @param ctx Context to destroy
 */
void nimcp_body_destroy(nimcp_body_context_t* ctx);

/* ============================================================================
 * Body Part Management API
 * ============================================================================ */

/**
 * @brief Add body part
 *
 * @param ctx Body context
 * @param part Body part to add
 * @return NIMCP_BODY_OK on success
 */
nimcp_body_error_t nimcp_body_add_part(
    nimcp_body_context_t* ctx,
    const nimcp_body_part_t* part
);

/**
 * @brief Update body part
 *
 * @param ctx Body context
 * @param part Updated body part
 * @return NIMCP_BODY_OK on success
 */
nimcp_body_error_t nimcp_body_update_part(
    nimcp_body_context_t* ctx,
    const nimcp_body_part_t* part
);

/**
 * @brief Remove body part
 *
 * @param ctx Body context
 * @param part_id Part to remove
 * @return NIMCP_BODY_OK on success
 */
nimcp_body_error_t nimcp_body_remove_part(
    nimcp_body_context_t* ctx,
    uint32_t part_id
);

/**
 * @brief Get body part
 *
 * @param ctx Body context
 * @param part_id Part to retrieve
 * @param part Output part
 * @return NIMCP_BODY_OK on success
 */
nimcp_body_error_t nimcp_body_get_part(
    const nimcp_body_context_t* ctx,
    uint32_t part_id,
    nimcp_body_part_t* part
);

/**
 * @brief Get all body parts
 *
 * @param ctx Body context
 * @param parts Output array
 * @param max_parts Maximum to return
 * @param num_parts Output count
 * @return NIMCP_BODY_OK on success
 */
nimcp_body_error_t nimcp_body_get_all_parts(
    const nimcp_body_context_t* ctx,
    nimcp_body_part_t* parts,
    uint32_t max_parts,
    uint32_t* num_parts
);

/**
 * @brief Initialize standard human body schema
 *
 * @param ctx Body context
 * @return NIMCP_BODY_OK on success
 */
nimcp_body_error_t nimcp_body_init_human_schema(nimcp_body_context_t* ctx);

/* ============================================================================
 * Proprioceptive Integration API
 * ============================================================================ */

/**
 * @brief Process proprioceptive signal
 *
 * WHAT: Integrates proprioceptive input and updates body schema
 * WHY:  Core proprioceptive processing
 *
 * @param ctx Body context
 * @param signal Proprioceptive signal
 * @return NIMCP_BODY_OK on success
 */
nimcp_body_error_t nimcp_body_process_proprio(
    nimcp_body_context_t* ctx,
    const nimcp_proprio_signal_t* signal
);

/**
 * @brief Process visual feedback
 *
 * WHAT: Integrates visual observation of body part
 * WHY:  Visual-proprioceptive integration
 *
 * @param ctx Body context
 * @param feedback Visual feedback
 * @return NIMCP_BODY_OK on success
 */
nimcp_body_error_t nimcp_body_process_visual(
    nimcp_body_context_t* ctx,
    const nimcp_visual_feedback_t* feedback
);

/**
 * @brief Update joint state
 *
 * @param ctx Body context
 * @param part_id Body part
 * @param joint Joint state to update
 * @return NIMCP_BODY_OK on success
 */
nimcp_body_error_t nimcp_body_update_joint(
    nimcp_body_context_t* ctx,
    uint32_t part_id,
    const nimcp_joint_state_t* joint
);

/**
 * @brief Get joint prediction error
 *
 * @param ctx Body context
 * @param part_id Body part
 * @param joint_id Joint
 * @param error Output prediction error
 * @return NIMCP_BODY_OK on success
 */
nimcp_body_error_t nimcp_body_get_prediction_error(
    const nimcp_body_context_t* ctx,
    uint32_t part_id,
    uint32_t joint_id,
    double* error
);

/* ============================================================================
 * Body Ownership API
 * ============================================================================ */

/**
 * @brief Get ownership state for body part
 *
 * @param ctx Body context
 * @param part_id Body part
 * @param state Output ownership state
 * @param confidence Output confidence
 * @return NIMCP_BODY_OK on success
 */
nimcp_body_error_t nimcp_body_get_ownership(
    const nimcp_body_context_t* ctx,
    uint32_t part_id,
    nimcp_ownership_state_t* state,
    double* confidence
);

/**
 * @brief Update ownership based on synchronous stimulation
 *
 * WHAT: Updates ownership based on visual-tactile synchrony
 * WHY:  Implements rubber hand illusion mechanism
 *
 * @param ctx Body context
 * @param part_id Body part
 * @param visual_position Visual position of stimulation
 * @param tactile_position Tactile position of stimulation
 * @param is_synchronous Whether stimulations are synchronous
 * @return NIMCP_BODY_OK on success
 */
nimcp_body_error_t nimcp_body_update_ownership_sync(
    nimcp_body_context_t* ctx,
    uint32_t part_id,
    const nimcp_body_position_t* visual_position,
    const nimcp_body_position_t* tactile_position,
    bool is_synchronous
);

/**
 * @brief Process external object for ownership
 *
 * WHAT: Evaluates whether external object should be incorporated
 * WHY:  Enables rubber hand illusion and tool use
 *
 * @param ctx Body context
 * @param object External object
 * @return NIMCP_BODY_OK on success
 */
nimcp_body_error_t nimcp_body_process_external_object(
    nimcp_body_context_t* ctx,
    const nimcp_external_object_t* object
);

/**
 * @brief Incorporate tool into body schema
 *
 * WHAT: Extends body schema to include tool
 * WHY:  Tool use modifies body representation
 *
 * @param ctx Body context
 * @param object_id Tool object ID
 * @param attach_part_id Part tool attaches to
 * @return NIMCP_BODY_OK on success
 */
nimcp_body_error_t nimcp_body_incorporate_tool(
    nimcp_body_context_t* ctx,
    uint32_t object_id,
    uint32_t attach_part_id
);

/**
 * @brief Release incorporated tool
 *
 * @param ctx Body context
 * @param object_id Tool to release
 * @return NIMCP_BODY_OK on success
 */
nimcp_body_error_t nimcp_body_release_tool(
    nimcp_body_context_t* ctx,
    uint32_t object_id
);

/* ============================================================================
 * Body Boundary API
 * ============================================================================ */

/**
 * @brief Update body boundaries
 *
 * WHAT: Recalculates body boundaries based on current schema
 * WHY:  Boundaries define self/non-self distinction
 *
 * @param ctx Body context
 * @return NIMCP_BODY_OK on success
 */
nimcp_body_error_t nimcp_body_update_boundaries(nimcp_body_context_t* ctx);

/**
 * @brief Check if position is inside body boundary
 *
 * @param ctx Body context
 * @param position Position to check
 * @param is_inside Output: true if inside
 * @param nearest_distance Output: distance to nearest boundary
 * @return NIMCP_BODY_OK on success
 */
nimcp_body_error_t nimcp_body_check_boundary(
    const nimcp_body_context_t* ctx,
    const nimcp_body_position_t* position,
    bool* is_inside,
    double* nearest_distance
);

/**
 * @brief Get body center of mass
 *
 * @param ctx Body context
 * @param center_of_mass Output position
 * @return NIMCP_BODY_OK on success
 */
nimcp_body_error_t nimcp_body_get_center_of_mass(
    const nimcp_body_context_t* ctx,
    nimcp_body_position_t* center_of_mass
);

/* ============================================================================
 * Peripersonal Space API
 * ============================================================================ */

/**
 * @brief Update peripersonal space
 *
 * WHAT: Updates peripersonal space representation
 * WHY:  Tracks objects near body for threat/action
 *
 * @param ctx Body context
 * @param object_positions Array of nearby object positions
 * @param num_objects Number of objects
 * @return NIMCP_BODY_OK on success
 */
nimcp_body_error_t nimcp_body_update_peripersonal(
    nimcp_body_context_t* ctx,
    const nimcp_body_position_t* object_positions,
    uint32_t num_objects
);

/**
 * @brief Get peripersonal space
 *
 * @param ctx Body context
 * @param space Output peripersonal space
 * @return NIMCP_BODY_OK on success
 */
nimcp_body_error_t nimcp_body_get_peripersonal(
    const nimcp_body_context_t* ctx,
    nimcp_peripersonal_space_t* space
);

/**
 * @brief Check if object is in peripersonal space
 *
 * @param ctx Body context
 * @param position Object position
 * @param is_in_space Output: true if in peripersonal space
 * @param activation Output: activation level at position
 * @return NIMCP_BODY_OK on success
 */
nimcp_body_error_t nimcp_body_check_peripersonal(
    const nimcp_body_context_t* ctx,
    const nimcp_body_position_t* position,
    bool* is_in_space,
    double* activation
);

/* ============================================================================
 * Update and Processing API
 * ============================================================================ */

/**
 * @brief Process one update cycle
 *
 * @param ctx Body context
 * @param delta_time Time since last update (seconds)
 * @return NIMCP_BODY_OK on success
 */
nimcp_body_error_t nimcp_body_update(
    nimcp_body_context_t* ctx,
    double delta_time
);

/**
 * @brief Predict body state
 *
 * WHAT: Generates prediction of body state at future time
 * WHY:  Predictive body representation
 *
 * @param ctx Body context
 * @param delta_time Time ahead to predict (seconds)
 * @param predicted_parts Output predicted parts
 * @param max_parts Maximum parts
 * @param num_predicted Output count
 * @return NIMCP_BODY_OK on success
 */
nimcp_body_error_t nimcp_body_predict(
    const nimcp_body_context_t* ctx,
    double delta_time,
    nimcp_body_part_t* predicted_parts,
    uint32_t max_parts,
    uint32_t* num_predicted
);

/* ============================================================================
 * Statistics and Utility API
 * ============================================================================ */

/**
 * @brief Get statistics
 *
 * @param ctx Body context
 * @param stats Output statistics
 * @return NIMCP_BODY_OK on success
 */
nimcp_body_error_t nimcp_body_get_stats(
    const nimcp_body_context_t* ctx,
    nimcp_body_stats_t* stats
);

/**
 * @brief Get body part type name
 *
 * @param type Part type
 * @return String name
 */
const char* nimcp_body_part_type_name(nimcp_body_part_type_t type);

/**
 * @brief Get ownership state name
 *
 * @param state Ownership state
 * @return String name
 */
const char* nimcp_body_ownership_state_name(nimcp_ownership_state_t state);

/**
 * @brief Get joint type name
 *
 * @param type Joint type
 * @return String name
 */
const char* nimcp_body_joint_type_name(nimcp_joint_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BODY_OWNERSHIP_H */
