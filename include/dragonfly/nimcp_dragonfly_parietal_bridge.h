/**
 * @file nimcp_dragonfly_parietal_bridge.h
 * @brief Parietal Lobe Bridge for Dragonfly Module
 *
 * WHAT: Connects dragonfly interception system to parietal cortex spatial processing
 * WHY:  Parietal cortex handles spatial cognition and visuomotor coordination
 * HOW:  Leverages spatial reasoning for coordinate transforms and spatial attention
 *
 * INTEGRATION PIPELINE:
 * Dragonfly Tracking → Parietal Spatial → Coordinate Transforms → Motor Commands
 *
 * BIOLOGICAL REFERENCE:
 * - Posterior Parietal Cortex (PPC) computes visuomotor transformations
 * - Lateral Intraparietal area (LIP) represents visual spatial attention
 * - Parietal reach region (PRR) transforms targets to motor coordinates
 * - Superior Colliculus receives parietal attention signals for saccades
 *
 * KEY FEATURES:
 * - Egocentric ↔ Allocentric coordinate transforms
 * - Spatial attention map integration
 * - K-D tree spatial indexing for multi-target tracking
 * - Visuomotor gain field computation
 * - Saccade/pursuit eye movement command generation
 *
 * @author NIMCP Team
 * @date 2024-12-28
 */

#ifndef NIMCP_DRAGONFLY_PARIETAL_BRIDGE_H
#define NIMCP_DRAGONFLY_PARIETAL_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "dragonfly/nimcp_dragonfly.h"

/* Forward declarations */
typedef struct spatial_reasoning spatial_reasoning_t;
typedef struct parietal_lobe parietal_lobe_t;
typedef struct dragonfly_parietal_bridge_s dragonfly_parietal_bridge_t;

//=============================================================================
// Constants
//=============================================================================

#define PARIETAL_BRIDGE_MAX_TARGETS 32      /**< Max tracked targets */
#define PARIETAL_BRIDGE_MAX_WAYPOINTS 64    /**< Max intercept waypoints */

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Coordinate frame types
 */
typedef enum {
    COORD_FRAME_EYE,          /**< Eye-centered (retinal) */
    COORD_FRAME_HEAD,         /**< Head-centered */
    COORD_FRAME_BODY,         /**< Body-centered (egocentric) */
    COORD_FRAME_WORLD,        /**< World-centered (allocentric) */
    COORD_FRAME_CAMERA        /**< Camera/sensor-centered */
} coordinate_frame_t;

/**
 * @brief Motor command types for visuomotor output
 */
typedef enum {
    MOTOR_CMD_SACCADE,        /**< Rapid eye movement to target */
    MOTOR_CMD_SMOOTH_PURSUIT, /**< Smooth tracking movement */
    MOTOR_CMD_HEAD_TURN,      /**< Head orientation toward target */
    MOTOR_CMD_BODY_ORIENT,    /**< Body orientation toward target */
    MOTOR_CMD_INTERCEPT_PATH  /**< Full body intercept trajectory */
} motor_command_type_t;

/**
 * @brief 3D position vector
 */
typedef struct {
    float x, y, z;
} parietal_vec3_t;

/**
 * @brief Orientation quaternion
 */
typedef struct {
    float w, x, y, z;
} parietal_quat_t;

/**
 * @brief Observer pose for coordinate transforms
 */
typedef struct {
    parietal_vec3_t position;     /**< Observer position */
    parietal_quat_t orientation;  /**< Observer orientation */
    float heading;                /**< Heading angle (radians) */
    float pitch;                  /**< Pitch angle (radians) */
    float roll;                   /**< Roll angle (radians) */
    coordinate_frame_t frame;     /**< Current coordinate frame */
} observer_state_t;

/**
 * @brief Spatial target representation (parietal format)
 */
typedef struct {
    uint32_t id;                  /**< Target ID (matches dragonfly track ID) */
    parietal_vec3_t position;     /**< Position in current frame */
    parietal_vec3_t velocity;     /**< Velocity in current frame */
    float azimuth;                /**< Azimuth from observer (radians) */
    float elevation;              /**< Elevation from observer (radians) */
    float distance;               /**< Distance from observer (meters) */
    float visual_angle;           /**< Angular size (radians) */
    float attention_weight;       /**< Spatial attention weight [0,1] */
    coordinate_frame_t frame;     /**< Reference frame */
    uint64_t timestamp_us;        /**< Last update timestamp */
} parietal_target_t;

/**
 * @brief Motor command output
 */
typedef struct {
    motor_command_type_t type;    /**< Command type */
    parietal_vec3_t target_pos;   /**< Target position for command */
    parietal_vec3_t velocity;     /**< Movement velocity */
    float amplitude;              /**< Movement amplitude (degrees or meters) */
    float duration_ms;            /**< Expected movement duration */
    float urgency;                /**< Command urgency [0,1] */
    uint32_t target_id;           /**< Associated target ID */
} motor_command_t;

/**
 * @brief Spatial attention map (retinotopic/allocentric)
 */
typedef struct {
    float* weights;               /**< Attention weights (row-major) */
    uint32_t width;               /**< Map width (azimuth bins) */
    uint32_t height;              /**< Map height (elevation bins) */
    float azimuth_min;            /**< Minimum azimuth (radians) */
    float azimuth_max;            /**< Maximum azimuth (radians) */
    float elevation_min;          /**< Minimum elevation (radians) */
    float elevation_max;          /**< Maximum elevation (radians) */
    float total_attention;        /**< Sum of all weights */
    parietal_vec3_t peak_location; /**< Location of attention peak */
    float peak_weight;            /**< Weight at peak */
} parietal_attention_map_t;

/**
 * @brief Intercept waypoint for motor planning (parietal format)
 */
typedef struct {
    parietal_vec3_t position;     /**< Waypoint position */
    parietal_vec3_t velocity;     /**< Required velocity at waypoint */
    float time_ms;                /**< Time from start (ms) */
    float confidence;             /**< Path confidence [0,1] */
} parietal_waypoint_t;

/**
 * @brief Visuomotor gain field parameters
 */
typedef struct {
    float eye_gain[3];            /**< Gain field for eye position */
    float head_gain[3];           /**< Gain field for head position */
    float modulation_strength;    /**< Overall gain modulation */
    float preferred_direction[3]; /**< Preferred direction vector */
    float tuning_width;           /**< Tuning curve width (radians) */
} gain_field_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Coordinate transform */
    bool auto_transform;          /**< Automatically transform coordinates */
    coordinate_frame_t default_output_frame;  /**< Default output frame */

    /* Attention */
    bool enable_attention;        /**< Enable spatial attention integration */
    uint32_t attention_map_width; /**< Attention map width (default: 64) */
    uint32_t attention_map_height;/**< Attention map height (default: 32) */
    float attention_decay;        /**< Attention decay rate per update */

    /* Motor commands */
    bool generate_motor_commands; /**< Generate visuomotor outputs */
    float saccade_threshold;      /**< Threshold for saccade vs pursuit */
    float pursuit_gain;           /**< Smooth pursuit gain */
    float motor_latency_ms;       /**< Simulated motor latency */

    /* Gain fields */
    bool enable_gain_fields;      /**< Enable gain field modulation */
    float gain_field_sigma;       /**< Gain field tuning width */

    /* K-D tree indexing */
    bool enable_spatial_index;    /**< Use K-D tree for queries */
    float query_radius_default;   /**< Default query radius (meters) */
} parietal_bridge_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t targets_processed;
    uint64_t transforms_computed;
    uint64_t motor_commands_generated;
    uint64_t attention_updates;
    float avg_transform_time_us;
    float avg_motor_latency_ms;
    uint32_t current_targets;
} parietal_bridge_stats_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default configuration
 */
parietal_bridge_config_t parietal_bridge_default_config(void);

/**
 * @brief Validate configuration
 */
bool parietal_bridge_validate_config(const parietal_bridge_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create parietal bridge
 *
 * @param dragonfly Dragonfly system for target data
 * @param spatial_reasoning Spatial reasoning module (can be NULL)
 * @param parietal Parietal lobe module (can be NULL)
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
dragonfly_parietal_bridge_t* dragonfly_parietal_bridge_create(
    dragonfly_system_t* dragonfly,
    spatial_reasoning_t* spatial_reasoning,
    parietal_lobe_t* parietal,
    const parietal_bridge_config_t* config
);

/**
 * @brief Destroy bridge
 */
void dragonfly_parietal_bridge_destroy(dragonfly_parietal_bridge_t* bridge);

/**
 * @brief Reset bridge state
 */
int dragonfly_parietal_bridge_reset(dragonfly_parietal_bridge_t* bridge);

//=============================================================================
// Observer State Functions
//=============================================================================

/**
 * @brief Update observer (agent) state
 *
 * @param bridge Parietal bridge
 * @param observer Current observer pose
 * @return 0 on success, -1 on error
 */
int dragonfly_parietal_bridge_set_observer(
    dragonfly_parietal_bridge_t* bridge,
    const observer_state_t* observer
);

/**
 * @brief Get current observer state
 */
int dragonfly_parietal_bridge_get_observer(
    const dragonfly_parietal_bridge_t* bridge,
    observer_state_t* observer
);

//=============================================================================
// Coordinate Transform Functions
//=============================================================================

/**
 * @brief Transform target to specified coordinate frame
 *
 * @param bridge Parietal bridge
 * @param target Target to transform (modified in place)
 * @param target_frame Desired output coordinate frame
 * @return 0 on success, -1 on error
 */
int dragonfly_parietal_bridge_transform_target(
    const dragonfly_parietal_bridge_t* bridge,
    parietal_target_t* target,
    coordinate_frame_t target_frame
);

/**
 * @brief Transform position between coordinate frames
 *
 * @param bridge Parietal bridge
 * @param position Position to transform (modified in place)
 * @param from_frame Source frame
 * @param to_frame Target frame
 * @return 0 on success, -1 on error
 */
int dragonfly_parietal_bridge_transform_position(
    const dragonfly_parietal_bridge_t* bridge,
    parietal_vec3_t* position,
    coordinate_frame_t from_frame,
    coordinate_frame_t to_frame
);

/**
 * @brief Compute azimuth/elevation from observer to position
 *
 * @param bridge Parietal bridge
 * @param position Target position (in world coordinates)
 * @param azimuth Output: azimuth angle (radians)
 * @param elevation Output: elevation angle (radians)
 * @param distance Output: distance (meters)
 * @return 0 on success, -1 on error
 */
int dragonfly_parietal_bridge_compute_angles(
    const dragonfly_parietal_bridge_t* bridge,
    const parietal_vec3_t* position,
    float* azimuth,
    float* elevation,
    float* distance
);

//=============================================================================
// Target Integration Functions
//=============================================================================

/**
 * @brief Update targets from dragonfly tracking
 *
 * WHAT: Pull tracked targets from dragonfly and convert to parietal format
 * WHY:  Synchronize dragonfly tracking with parietal spatial representation
 *
 * @param bridge Parietal bridge
 * @return Number of targets updated, or -1 on error
 */
int dragonfly_parietal_bridge_sync_targets(dragonfly_parietal_bridge_t* bridge);

/**
 * @brief Get all tracked targets in specified frame
 *
 * @param bridge Parietal bridge
 * @param targets Output array (must have capacity for PARIETAL_BRIDGE_MAX_TARGETS)
 * @param frame Coordinate frame for output
 * @return Number of targets, or -1 on error
 */
int dragonfly_parietal_bridge_get_targets(
    const dragonfly_parietal_bridge_t* bridge,
    parietal_target_t* targets,
    coordinate_frame_t frame
);

/**
 * @brief Get primary (locked) target in specified frame
 *
 * @param bridge Parietal bridge
 * @param target Output target
 * @param frame Coordinate frame for output
 * @return 0 on success, 1 if no locked target, -1 on error
 */
int dragonfly_parietal_bridge_get_primary_target(
    const dragonfly_parietal_bridge_t* bridge,
    parietal_target_t* target,
    coordinate_frame_t frame
);

//=============================================================================
// Spatial Attention Functions
//=============================================================================

/**
 * @brief Create attention map
 *
 * @param width Map width
 * @param height Map height
 * @return Attention map or NULL on error
 */
parietal_attention_map_t* parietal_attention_map_create(uint32_t width, uint32_t height);

/**
 * @brief Destroy attention map
 */
void parietal_attention_map_destroy(parietal_attention_map_t* map);

/**
 * @brief Update spatial attention map from targets
 *
 * @param bridge Parietal bridge
 * @param map Attention map to update
 * @return 0 on success, -1 on error
 */
int dragonfly_parietal_bridge_update_attention(
    dragonfly_parietal_bridge_t* bridge,
    parietal_attention_map_t* map
);

/**
 * @brief Get attention weight at location
 *
 * @param map Attention map
 * @param azimuth Azimuth angle (radians)
 * @param elevation Elevation angle (radians)
 * @return Attention weight [0,1], or -1 on error
 */
float parietal_attention_map_sample(
    const parietal_attention_map_t* map,
    float azimuth,
    float elevation
);

/**
 * @brief Set attention weight at location
 */
int parietal_attention_map_set(
    parietal_attention_map_t* map,
    float azimuth,
    float elevation,
    float weight
);

/**
 * @brief Find attention peak
 *
 * @param map Attention map
 * @param azimuth Output: azimuth of peak
 * @param elevation Output: elevation of peak
 * @param weight Output: weight at peak
 * @return 0 on success, -1 on error
 */
int parietal_attention_map_find_peak(
    const parietal_attention_map_t* map,
    float* azimuth,
    float* elevation,
    float* weight
);

//=============================================================================
// Motor Command Functions
//=============================================================================

/**
 * @brief Generate motor command for target
 *
 * @param bridge Parietal bridge
 * @param target_id Target to generate command for
 * @param command Output motor command
 * @return 0 on success, -1 on error
 */
int dragonfly_parietal_bridge_generate_motor_command(
    dragonfly_parietal_bridge_t* bridge,
    uint32_t target_id,
    motor_command_t* command
);

/**
 * @brief Generate saccade command to target location
 *
 * @param bridge Parietal bridge
 * @param azimuth Target azimuth (radians)
 * @param elevation Target elevation (radians)
 * @param command Output saccade command
 * @return 0 on success, -1 on error
 */
int dragonfly_parietal_bridge_generate_saccade(
    dragonfly_parietal_bridge_t* bridge,
    float azimuth,
    float elevation,
    motor_command_t* command
);

/**
 * @brief Generate smooth pursuit command
 *
 * @param bridge Parietal bridge
 * @param target_id Target to pursue
 * @param command Output pursuit command
 * @return 0 on success, -1 on error
 */
int dragonfly_parietal_bridge_generate_pursuit(
    dragonfly_parietal_bridge_t* bridge,
    uint32_t target_id,
    motor_command_t* command
);

//=============================================================================
// Intercept Path Planning Functions
//=============================================================================

/**
 * @brief Compute intercept waypoints in body-centered coordinates
 *
 * @param bridge Parietal bridge
 * @param target_id Target to intercept
 * @param waypoints Output waypoint array
 * @param max_waypoints Maximum waypoints to generate
 * @return Number of waypoints generated, or -1 on error
 */
int dragonfly_parietal_bridge_compute_intercept_path(
    dragonfly_parietal_bridge_t* bridge,
    uint32_t target_id,
    parietal_waypoint_t* waypoints,
    uint32_t max_waypoints
);

//=============================================================================
// Gain Field Functions
//=============================================================================

/**
 * @brief Compute gain field modulation for target
 *
 * @param bridge Parietal bridge
 * @param target Target in current eye/head frame
 * @param gain_field Output gain field parameters
 * @return 0 on success, -1 on error
 */
int dragonfly_parietal_bridge_compute_gain_field(
    const dragonfly_parietal_bridge_t* bridge,
    const parietal_target_t* target,
    gain_field_t* gain_field
);

/**
 * @brief Apply gain field modulation to motor command
 *
 * @param command Motor command to modulate
 * @param gain_field Gain field to apply
 * @return 0 on success, -1 on error
 */
int dragonfly_parietal_bridge_apply_gain_field(
    motor_command_t* command,
    const gain_field_t* gain_field
);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get bridge statistics
 */
int dragonfly_parietal_bridge_get_stats(
    const dragonfly_parietal_bridge_t* bridge,
    parietal_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 */
int dragonfly_parietal_bridge_reset_stats(dragonfly_parietal_bridge_t* bridge);

//=============================================================================
// Configuration Update Functions
//=============================================================================

/**
 * @brief Update configuration
 */
int dragonfly_parietal_bridge_set_config(
    dragonfly_parietal_bridge_t* bridge,
    const parietal_bridge_config_t* config
);

/**
 * @brief Get current configuration
 */
int dragonfly_parietal_bridge_get_config(
    const dragonfly_parietal_bridge_t* bridge,
    parietal_bridge_config_t* config
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get coordinate frame name
 */
const char* dragonfly_parietal_frame_name(coordinate_frame_t frame);

/**
 * @brief Get motor command type name
 */
const char* dragonfly_parietal_motor_cmd_name(motor_command_type_t type);

/**
 * @brief Normalize quaternion
 */
void dragonfly_parietal_quat_normalize(parietal_quat_t* q);

/**
 * @brief Create quaternion from Euler angles (ZYX convention)
 */
parietal_quat_t dragonfly_parietal_quat_from_euler(float roll, float pitch, float yaw);

/**
 * @brief Rotate vector by quaternion
 */
parietal_vec3_t dragonfly_parietal_quat_rotate_vec(const parietal_quat_t* q, const parietal_vec3_t* v);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_PARIETAL_BRIDGE_H */
