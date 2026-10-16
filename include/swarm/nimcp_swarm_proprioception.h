/**
 * @file nimcp_swarm_proprioception.h
 * @brief Collective Proprioception System for NIMCP Swarms
 *
 * Biological Inspiration:
 * - Spider web distributed sensing: Vibrations and tensions propagate through
 *   the web, allowing the spider to sense position, movement, and disturbances
 * - Organism proprioception: Sense of body position and movement without
 *   external reference
 * - Fish schooling: Each fish maintains awareness of neighbors' positions
 *   and adjusts behavior to maintain formation
 *
 * This system enables swarms to:
 * - Know their collective shape and geometry
 * - Detect deformations and disturbances
 * - Estimate positions without GPS
 * - Maintain formation awareness
 * - Respond to collective changes
 *
 * Key Features:
 * - Relative positioning using local coordinate frames
 * - Shape classification and fitness metrics
 * - Deformation detection and quantification
 * - Boundary awareness and edge detection
 * - Density mapping and gradient calculation
 * - Center-of-mass tracking (distributed)
 * - Formation quality metrics
 * - Vibration sensing and source localization
 * - Bio-async message integration
 *
 * @version 1.0
 * @date 2025-01-08
 */

#ifndef NIMCP_SWARM_PROPRIOCEPTION_H
#define NIMCP_SWARM_PROPRIOCEPTION_H

#include "utils/validation/nimcp_common.h"
#include "async/nimcp_bio_messages.h"
#include <stdint.h>
#include <stdbool.h>

#include "async/nimcp_bio_router.h"
#include "utils/thread/nimcp_thread.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum number of neighbors tracked per drone
 */
#define NIMCP_SWARM_PROPRIO_MAX_NEIGHBORS 32

/**
 * @brief Maximum number of historical positions tracked
 */
#define NIMCP_SWARM_PROPRIO_HISTORY_SIZE 64

/**
 * @brief Maximum number of vibration frequencies tracked
 */
#define NIMCP_SWARM_PROPRIO_MAX_FREQUENCIES 16

/**
 * @brief Shape classification types
 */
typedef enum {
    NIMCP_SWARM_SHAPE_UNKNOWN = 0,
    NIMCP_SWARM_SHAPE_SPHERE,       /**< Spherical formation */
    NIMCP_SWARM_SHAPE_ELLIPSOID,    /**< Ellipsoidal formation */
    NIMCP_SWARM_SHAPE_LINE,         /**< Linear formation */
    NIMCP_SWARM_SHAPE_WEDGE,        /**< V-shaped wedge */
    NIMCP_SWARM_SHAPE_WALL,         /**< Planar wall formation */
    NIMCP_SWARM_SHAPE_RING,         /**< Circular ring */
    NIMCP_SWARM_SHAPE_LATTICE,      /**< Grid/lattice structure */
    NIMCP_SWARM_SHAPE_CLUSTER,      /**< Irregular cluster */
    NIMCP_SWARM_SHAPE_DISPERSED,    /**< Highly dispersed */
    NIMCP_SWARM_SHAPE_COUNT
} nimcp_swarm_shape_t;

/**
 * @brief Deformation types
 */
typedef enum {
    NIMCP_SWARM_DEFORM_NONE = 0,
    NIMCP_SWARM_DEFORM_STRETCH,     /**< Formation stretched */
    NIMCP_SWARM_DEFORM_COMPRESS,    /**< Formation compressed */
    NIMCP_SWARM_DEFORM_SHEAR,       /**< Shearing deformation */
    NIMCP_SWARM_DEFORM_TWIST,       /**< Torsional deformation */
    NIMCP_SWARM_DEFORM_SPLIT,       /**< Formation splitting */
    NIMCP_SWARM_DEFORM_ASYMMETRIC,  /**< Asymmetric distortion */
    NIMCP_SWARM_DEFORM_COUNT
} nimcp_swarm_deformation_t;

/**
 * @brief Boundary role within swarm
 */
typedef enum {
    NIMCP_SWARM_ROLE_INTERIOR = 0,  /**< Interior drone */
    NIMCP_SWARM_ROLE_EDGE,          /**< Edge/boundary drone */
    NIMCP_SWARM_ROLE_VERTEX,        /**< Corner/vertex drone */
    NIMCP_SWARM_ROLE_ISOLATED,      /**< Isolated from swarm */
    NIMCP_SWARM_ROLE_COUNT
} nimcp_swarm_boundary_role_t;

/**
 * @brief 3D position vector
 */
typedef struct {
    double x;                       /**< X coordinate (meters) */
    double y;                       /**< Y coordinate (meters) */
    double z;                       /**< Z coordinate (meters) */
} nimcp_swarm_position_t;

/**
 * @brief 3D velocity vector
 */
typedef struct {
    double vx;                      /**< X velocity (m/s) */
    double vy;                      /**< Y velocity (m/s) */
    double vz;                      /**< Z velocity (m/s) */
} nimcp_swarm_velocity_t;

/**
 * @brief Neighbor information
 */
typedef struct {
    uint32_t drone_id;              /**< Neighbor drone ID */
    nimcp_swarm_position_t relative_pos; /**< Relative position */
    nimcp_swarm_velocity_t relative_vel; /**< Relative velocity */
    double distance;                /**< Distance to neighbor (meters) */
    double signal_strength;         /**< Communication signal strength */
    uint64_t last_update_time;      /**< Last position update (timestamp) */
    bool is_active;                 /**< Neighbor is active */
} nimcp_swarm_neighbor_t;

/**
 * @brief Shape descriptor
 */
typedef struct {
    nimcp_swarm_shape_t shape_type; /**< Classified shape type */
    double fitness;                 /**< Shape fitness score [0-1] */
    double symmetry;                /**< Symmetry measure [0-1] */
    double compactness;             /**< Compactness ratio [0-1] */
    double aspect_ratio;            /**< Aspect ratio */
    double principal_axes[3];       /**< Principal component lengths */
    double orientation[3];          /**< Orientation angles (radians) */
    uint64_t classification_time;   /**< Time of classification */
} nimcp_swarm_shape_descriptor_t;

/**
 * @brief Deformation metrics
 */
typedef struct {
    nimcp_swarm_deformation_t deform_type; /**< Type of deformation */
    double magnitude;               /**< Deformation magnitude [0-1] */
    double strain_tensor[9];        /**< 3x3 strain tensor */
    nimcp_swarm_position_t deform_center; /**< Center of deformation */
    nimcp_swarm_position_t deform_direction; /**< Deformation direction */
    double recovery_estimate;       /**< Estimated recovery time (seconds) */
    uint64_t detection_time;        /**< Time of detection */
} nimcp_swarm_deformation_metrics_t;

/**
 * @brief Boundary descriptor
 */
typedef struct {
    nimcp_swarm_boundary_role_t role; /**< Boundary role */
    double distance_to_boundary;    /**< Distance to nearest boundary */
    nimcp_swarm_position_t boundary_normal; /**< Normal vector to boundary */
    uint32_t boundary_neighbors;    /**< Number of boundary neighbors */
    double curvature;               /**< Local boundary curvature */
    bool is_convex_hull;            /**< Part of convex hull */
} nimcp_swarm_boundary_descriptor_t;

/**
 * @brief Density information
 */
typedef struct {
    double local_density;           /**< Local drone density (drones/m³) */
    double gradient[3];             /**< Density gradient vector */
    double laplacian;               /**< Density Laplacian */
    nimcp_swarm_position_t dense_center; /**< Nearest dense region center */
    nimcp_swarm_position_t sparse_direction; /**< Direction to sparse region */
    double uniformity;              /**< Density uniformity [0-1] */
} nimcp_swarm_density_info_t;

/**
 * @brief Center-of-mass estimate
 */
typedef struct {
    nimcp_swarm_position_t position; /**< Estimated COM position */
    nimcp_swarm_velocity_t velocity; /**< Estimated COM velocity */
    double confidence;              /**< Estimate confidence [0-1] */
    uint32_t contributing_drones;   /**< Number of drones in estimate */
    uint64_t update_time;           /**< Last update time */
} nimcp_swarm_com_estimate_t;

/**
 * @brief Formation metrics
 */
typedef struct {
    double avg_neighbor_distance;   /**< Average distance to neighbors */
    double min_neighbor_distance;   /**< Minimum neighbor distance */
    double max_neighbor_distance;   /**< Maximum neighbor distance */
    double distance_variance;       /**< Distance variance */
    double connectivity;            /**< Graph connectivity [0-1] */
    double formation_quality;       /**< Overall quality score [0-1] */
    uint32_t active_connections;    /**< Number of active connections */
    uint32_t isolated_drones;       /**< Number of isolated drones */
} nimcp_swarm_formation_metrics_t;

/**
 * @brief Vibration frequency component
 */
typedef struct {
    double frequency;               /**< Frequency (Hz) */
    double amplitude;               /**< Amplitude */
    double phase;                   /**< Phase (radians) */
    bool is_active;                 /**< Frequency is active */
} nimcp_swarm_vibration_frequency_t;

/**
 * @brief Vibration sensing data
 */
typedef struct {
    nimcp_swarm_position_t source_estimate; /**< Estimated vibration source */
    double source_confidence;       /**< Source estimate confidence [0-1] */
    nimcp_swarm_vibration_frequency_t frequencies[NIMCP_SWARM_PROPRIO_MAX_FREQUENCIES];
    uint32_t num_frequencies;       /**< Number of detected frequencies */
    double total_energy;            /**< Total vibration energy */
    uint64_t detection_time;        /**< Time of detection */
    double propagation_speed;       /**< Estimated propagation speed (m/s) */
} nimcp_swarm_vibration_data_t;

/**
 * @brief Position history entry
 */
typedef struct {
    nimcp_swarm_position_t position; /**< Position at time */
    uint64_t timestamp;             /**< Timestamp */
    bool is_valid;                  /**< Entry is valid */
} nimcp_swarm_position_history_t;

/**
 * @brief Configuration parameters
 */
typedef struct {
    double neighbor_radius;         /**< Radius for neighbor detection (meters) */
    double position_update_rate;    /**< Position update rate (Hz) */
    double shape_classification_interval; /**< Shape classification interval (s) */
    double deformation_threshold;   /**< Deformation detection threshold */
    double density_kernel_width;    /**< Density estimation kernel width (m) */
    double vibration_sensitivity;   /**< Vibration detection sensitivity */
    bool enable_history;            /**< Enable position history tracking */
    bool enable_vibration;          /**< Enable vibration sensing */
    uint32_t max_neighbors;         /**< Maximum neighbors to track */
} nimcp_swarm_proprio_config_t;

/**
 * @brief Proprioception state (opaque structure)
 */
typedef struct nimcp_swarm_proprioception nimcp_swarm_proprioception_t;

/**
 * @brief Bio-async message types for proprioception
 */
typedef enum {
    NIMCP_SWARM_MSG_POSITION_SHARE = 0x5000,
    NIMCP_SWARM_MSG_FORMATION_STATE,
    NIMCP_SWARM_MSG_DEFORMATION_ALERT,
    NIMCP_SWARM_MSG_DENSITY_UPDATE,
    NIMCP_SWARM_MSG_COM_ESTIMATE,
    NIMCP_SWARM_MSG_VIBRATION_DETECT,
    NIMCP_SWARM_MSG_BOUNDARY_UPDATE
} nimcp_swarm_proprio_msg_t;

/* ============================= Core API ============================= */

/**
 * @brief Create proprioception system instance
 *
 * @param drone_id This drone's unique ID
 * @param config Configuration parameters
 * @param bio_ctx Bio-async context for messaging (can be NULL)
 * @return Proprioception instance or NULL on failure
 */
nimcp_swarm_proprioception_t* nimcp_swarm_proprioception_create(
    uint32_t drone_id,
    const nimcp_swarm_proprio_config_t* config,
    void* bio_ctx
);

/**
 * @brief Destroy proprioception system instance
 *
 * @param proprio Proprioception instance
 */
void nimcp_swarm_proprioception_destroy(nimcp_swarm_proprioception_t* proprio);

/**
 * @brief Reset proprioception state
 *
 * @param proprio Proprioception instance
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_swarm_proprioception_reset(nimcp_swarm_proprioception_t* proprio);

/* ===================== Relative Positioning ======================== */

/**
 * @brief Update own position in local coordinate frame
 *
 * @param proprio Proprioception instance
 * @param position Current position
 * @param velocity Current velocity (can be NULL)
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_swarm_proprio_update_position(
    nimcp_swarm_proprioception_t* proprio,
    const nimcp_swarm_position_t* position,
    const nimcp_swarm_velocity_t* velocity
);

/**
 * @brief Update neighbor position
 *
 * @param proprio Proprioception instance
 * @param neighbor_id Neighbor drone ID
 * @param relative_position Position relative to this drone
 * @param signal_strength Communication signal strength
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_swarm_proprio_update_neighbor(
    nimcp_swarm_proprioception_t* proprio,
    uint32_t neighbor_id,
    const nimcp_swarm_position_t* relative_position,
    double signal_strength
);

/**
 * @brief Get neighbor information
 *
 * @param proprio Proprioception instance
 * @param neighbor_id Neighbor drone ID
 * @param neighbor Output neighbor information
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_swarm_proprio_get_neighbor(
    const nimcp_swarm_proprioception_t* proprio,
    uint32_t neighbor_id,
    nimcp_swarm_neighbor_t* neighbor
);

/**
 * @brief Get all neighbors
 *
 * @param proprio Proprioception instance
 * @param neighbors Output array of neighbors
 * @param max_neighbors Maximum neighbors to return
 * @param num_neighbors Output number of neighbors returned
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_swarm_proprio_get_neighbors(
    const nimcp_swarm_proprioception_t* proprio,
    nimcp_swarm_neighbor_t* neighbors,
    uint32_t max_neighbors,
    uint32_t* num_neighbors
);

/* ==================== Swarm Geometry Sensing ======================= */

/**
 * @brief Classify swarm shape
 *
 * @param proprio Proprioception instance
 * @param descriptor Output shape descriptor
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_swarm_proprio_classify_shape(
    nimcp_swarm_proprioception_t* proprio,
    nimcp_swarm_shape_descriptor_t* descriptor
);

/**
 * @brief Calculate shape fitness for target shape
 *
 * @param proprio Proprioception instance
 * @param target_shape Target shape type
 * @param fitness Output fitness score [0-1]
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_swarm_proprio_shape_fitness(
    const nimcp_swarm_proprioception_t* proprio,
    nimcp_swarm_shape_t target_shape,
    double* fitness
);

/* ==================== Deformation Detection ======================== */

/**
 * @brief Detect formation deformation
 *
 * @param proprio Proprioception instance
 * @param metrics Output deformation metrics
 * @return NIMCP_OK on success, NIMCP_NO_DATA if no deformation detected
 */
nimcp_result_t nimcp_swarm_proprio_detect_deformation(
    nimcp_swarm_proprioception_t* proprio,
    nimcp_swarm_deformation_metrics_t* metrics
);

/**
 * @brief Calculate deviation from target shape
 *
 * @param proprio Proprioception instance
 * @param target_shape Target shape type
 * @param deviation Output deviation magnitude
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_swarm_proprio_shape_deviation(
    const nimcp_swarm_proprioception_t* proprio,
    nimcp_swarm_shape_t target_shape,
    double* deviation
);

/* ===================== Boundary Awareness ========================== */

/**
 * @brief Determine boundary role
 *
 * @param proprio Proprioception instance
 * @param descriptor Output boundary descriptor
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_swarm_proprio_boundary_role(
    nimcp_swarm_proprioception_t* proprio,
    nimcp_swarm_boundary_descriptor_t* descriptor
);

/**
 * @brief Get boundary drones
 *
 * @param proprio Proprioception instance
 * @param boundary_ids Output array of boundary drone IDs
 * @param max_ids Maximum IDs to return
 * @param num_ids Output number of boundary drones
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_swarm_proprio_get_boundary_drones(
    const nimcp_swarm_proprioception_t* proprio,
    uint32_t* boundary_ids,
    uint32_t max_ids,
    uint32_t* num_ids
);

/* ======================= Density Mapping =========================== */

/**
 * @brief Calculate local density
 *
 * @param proprio Proprioception instance
 * @param density_info Output density information
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_swarm_proprio_density(
    nimcp_swarm_proprioception_t* proprio,
    nimcp_swarm_density_info_t* density_info
);

/**
 * @brief Identify sparse and dense regions
 *
 * @param proprio Proprioception instance
 * @param sparse_direction Output direction to sparse region
 * @param dense_direction Output direction to dense region
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_swarm_proprio_density_regions(
    const nimcp_swarm_proprioception_t* proprio,
    nimcp_swarm_position_t* sparse_direction,
    nimcp_swarm_position_t* dense_direction
);

/* =================== Center-of-Mass Tracking ======================= */

/**
 * @brief Estimate swarm center-of-mass
 *
 * @param proprio Proprioception instance
 * @param com_estimate Output COM estimate
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_swarm_proprio_estimate_com(
    nimcp_swarm_proprioception_t* proprio,
    nimcp_swarm_com_estimate_t* com_estimate
);

/**
 * @brief Update COM estimate from neighbor
 *
 * @param proprio Proprioception instance
 * @param neighbor_com Neighbor's COM estimate
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_swarm_proprio_merge_com_estimate(
    nimcp_swarm_proprioception_t* proprio,
    const nimcp_swarm_com_estimate_t* neighbor_com
);

/* ===================== Formation Metrics =========================== */

/**
 * @brief Calculate formation metrics
 *
 * @param proprio Proprioception instance
 * @param metrics Output formation metrics
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_swarm_proprio_formation_metrics(
    nimcp_swarm_proprioception_t* proprio,
    nimcp_swarm_formation_metrics_t* metrics
);

/**
 * @brief Get connectivity graph
 *
 * @param proprio Proprioception instance
 * @param adjacency_matrix Output adjacency matrix (flattened)
 * @param matrix_size Size of matrix (num_drones x num_drones)
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_swarm_proprio_connectivity_graph(
    const nimcp_swarm_proprioception_t* proprio,
    bool* adjacency_matrix,
    uint32_t matrix_size
);

/* ===================== Vibration Sensing =========================== */

/**
 * @brief Detect vibration/disturbance
 *
 * @param proprio Proprioception instance
 * @param signal Input signal array (position changes or accelerations)
 * @param signal_length Length of signal array
 * @param vibration_data Output vibration data
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_swarm_proprio_detect_vibration(
    nimcp_swarm_proprioception_t* proprio,
    const double* signal,
    uint32_t signal_length,
    nimcp_swarm_vibration_data_t* vibration_data
);

/**
 * @brief Localize vibration source
 *
 * @param proprio Proprioception instance
 * @param arrival_times Vibration arrival times from neighbors
 * @param num_neighbors Number of neighbors
 * @param source_position Output estimated source position
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_swarm_proprio_localize_vibration(
    nimcp_swarm_proprioception_t* proprio,
    const uint64_t* arrival_times,
    uint32_t num_neighbors,
    nimcp_swarm_position_t* source_position
);

/* ==================== Bio-Async Integration ======================== */

/**
 * @brief Broadcast position to neighbors
 *
 * @param proprio Proprioception instance
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_swarm_proprio_broadcast_position(
    nimcp_swarm_proprioception_t* proprio
);

/**
 * @brief Broadcast formation state
 *
 * @param proprio Proprioception instance
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_swarm_proprio_broadcast_formation_state(
    nimcp_swarm_proprioception_t* proprio
);

/**
 * @brief Send deformation alert
 *
 * @param proprio Proprioception instance
 * @param metrics Deformation metrics to broadcast
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_swarm_proprio_send_deformation_alert(
    nimcp_swarm_proprioception_t* proprio,
    const nimcp_swarm_deformation_metrics_t* metrics
);

/**
 * @brief Process received bio-async message
 *
 * @param proprio Proprioception instance
 * @param msg Received message
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_swarm_proprio_process_message(
    nimcp_swarm_proprioception_t* proprio,
    const bio_message_header_t* msg
);

/* ========================= Utility Functions ======================= */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 */
void nimcp_swarm_proprio_default_config(nimcp_swarm_proprio_config_t* config);

/**
 * @brief Get shape name string
 *
 * @param shape Shape type
 * @return Shape name string
 */
const char* nimcp_swarm_shape_name(nimcp_swarm_shape_t shape);

/**
 * @brief Get deformation name string
 *
 * @param deform Deformation type
 * @return Deformation name string
 */
const char* nimcp_swarm_deformation_name(nimcp_swarm_deformation_t deform);

/**
 * @brief Calculate distance between two positions
 *
 * @param pos1 First position
 * @param pos2 Second position
 * @return Distance in meters
 */
double nimcp_swarm_position_distance(
    const nimcp_swarm_position_t* pos1,
    const nimcp_swarm_position_t* pos2
);

/**
 * @brief Normalize position vector
 *
 * @param pos Position vector to normalize
 */
void nimcp_swarm_position_normalize(nimcp_swarm_position_t* pos);

/**
 * @brief Calculate dot product of two position vectors
 *
 * @param pos1 First position
 * @param pos2 Second position
 * @return Dot product
 */
double nimcp_swarm_position_dot(
    const nimcp_swarm_position_t* pos1,
    const nimcp_swarm_position_t* pos2
);

/**
 * @brief Calculate cross product of two position vectors
 *
 * @param pos1 First position
 * @param pos2 Second position
 * @param result Output cross product
 */
void nimcp_swarm_position_cross(
    const nimcp_swarm_position_t* pos1,
    const nimcp_swarm_position_t* pos2,
    nimcp_swarm_position_t* result
);

#include "async/nimcp_bio_router.h"
#include "utils/thread/nimcp_thread.h"
#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_PROPRIOCEPTION_H */
