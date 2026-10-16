/**
 * @file nimcp_spatial_reasoning.h
 * @brief Spatial reasoning and mental rotation for parietal lobe
 *
 * Implements spatial processing capabilities:
 * - Mental rotation (Shepard paradigm, ~53ms/degree)
 * - Coordinate transformations (egocentric ↔ allocentric)
 * - K-D tree spatial indexing (nearest neighbor, range queries)
 * - Spatial attention maps
 *
 * BIOLOGICAL BASIS:
 * The posterior parietal cortex (PPC) processes spatial information,
 * with mental rotation showing linear time complexity with angle
 * (Shepard & Metzler, 1971).
 *
 * USAGE:
 * ```c
 * spatial_reasoning_t* sr = spatial_reasoning_create();
 *
 * // Mental rotation
 * rotation_result_t rot = spatial_rotate_and_compare(sr, obj_a, obj_b);
 *
 * // Coordinate transform
 * vec3_t world = spatial_ego_to_allocentric(sr, local_pos, observer);
 *
 * // Nearest neighbor
 * spatial_add_object(sr, obj);
 * spatial_object_t* nearest = spatial_find_nearest(sr, query_pos);
 *
 * spatial_reasoning_destroy(sr);
 * ```
 */

#ifndef NIMCP_SPATIAL_REASONING_H
#define NIMCP_SPATIAL_REASONING_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Mental rotation rate (degrees per millisecond) */
#define SPATIAL_ROTATION_RATE_DEG_MS    (1.0f / 53.0f)

/** Maximum objects in spatial index */
#define SPATIAL_MAX_OBJECTS             10000

/** Dimensions for spatial coordinates */
#define SPATIAL_DIMENSIONS              3

/** Bio-async module ID for spatial reasoning */
#define BIO_MODULE_SPATIAL_REASONING    0x0382

/* ============================================================================
 * TYPES
 * ============================================================================ */

/** Opaque handle for spatial reasoning processor */
typedef struct spatial_reasoning spatial_reasoning_t;

/**
 * @brief 3D vector
 */
typedef struct {
    float x, y, z;
} vec3_t;

/**
 * @brief 4x4 homogeneous transformation matrix (column-major)
 */
typedef struct {
    float m[16];
} spatial_transform_t;

/**
 * @brief Quaternion for rotations
 */
typedef struct {
    float w, x, y, z;
} quaternion_t;

/**
 * @brief Spatial object representation
 */
typedef struct {
    uint32_t id;                    /**< Unique object ID */
    vec3_t position;                /**< Position in world space */
    quaternion_t orientation;       /**< Orientation as quaternion */
    vec3_t* vertices;               /**< Object vertices (NULL if point) */
    uint32_t num_vertices;          /**< Number of vertices */
    float bounding_radius;          /**< Bounding sphere radius */
    void* user_data;                /**< User-attached data */
} spatial_object_t;

/**
 * @brief Observer pose for coordinate transforms
 */
typedef struct {
    vec3_t position;                /**< Observer position */
    quaternion_t orientation;       /**< Observer orientation */
    float heading;                  /**< Heading angle (radians) */
} observer_pose_t;

/**
 * @brief Spatial reasoning configuration
 */
typedef struct {
    float rotation_rate_deg_ms;     /**< Mental rotation rate (default: 1/53) */
    float matching_threshold;       /**< Shape matching threshold (default: 0.9) */
    uint32_t max_objects;           /**< Maximum spatial objects (default: 10000) */
    bool enable_attention;          /**< Enable spatial attention (default: true) */
    bool enable_bio_async;          /**< Enable bio-async messaging (default: false) */

    /** Modulation parameters */
    float inflammation_sensitivity; /**< Inflammation effect on rotation (0-1) */
    float fatigue_sensitivity;      /**< Fatigue effect on accuracy (0-1) */
} spatial_config_t;

/**
 * @brief Mental rotation result
 */
typedef struct {
    bool is_match;                  /**< Objects match (same shape) */
    float rotation_angle;           /**< Estimated rotation angle (degrees) */
    vec3_t rotation_axis;           /**< Estimated rotation axis */
    float confidence;               /**< Match confidence [0,1] */
    uint64_t processing_time_ms;    /**< Simulated processing time */
    float shape_similarity;         /**< Shape similarity score [0,1] */
} rotation_result_t;

/**
 * @brief Spatial query result
 */
typedef struct {
    spatial_object_t** objects;     /**< Array of found objects */
    float* distances;               /**< Distances to query point */
    uint32_t count;                 /**< Number of results */
    uint32_t capacity;              /**< Allocated capacity */
} spatial_query_result_t;

/**
 * @brief Spatial attention map (2D grid)
 */
typedef struct {
    float* weights;                 /**< Attention weights [grid_w * grid_h] */
    uint32_t grid_width;            /**< Grid width */
    uint32_t grid_height;           /**< Grid height */
    vec3_t focus_point;             /**< Current focus point */
    float spread;                   /**< Attention spread (sigma) */
} spatial_attention_t;

/**
 * @brief Spatial reasoning statistics
 */
typedef struct {
    uint64_t rotations_performed;   /**< Total mental rotations */
    uint64_t transforms_performed;  /**< Total coordinate transforms */
    uint64_t spatial_queries;       /**< Total spatial queries */
    uint64_t objects_stored;        /**< Current objects in index */
    float avg_rotation_time_ms;     /**< Average rotation time */
    float avg_rotation_angle;       /**< Average rotation angle */
} spatial_stats_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Create spatial reasoning processor with default configuration
 * @return Handle or NULL on error
 */
spatial_reasoning_t* spatial_reasoning_create(void);

/**
 * @brief Create spatial reasoning processor with custom configuration
 * @param config Configuration (NULL for defaults)
 * @return Handle or NULL on error
 */
spatial_reasoning_t* spatial_reasoning_create_custom(const spatial_config_t* config);

/**
 * @brief Destroy spatial reasoning processor
 * @param sr Handle (NULL safe)
 */
void spatial_reasoning_destroy(spatial_reasoning_t* sr);

/**
 * @brief Get default configuration
 * @return Default configuration struct
 */
spatial_config_t spatial_default_config(void);

/**
 * @brief Validate configuration
 * @param config Configuration to validate
 * @return true if valid
 */
bool spatial_validate_config(const spatial_config_t* config);

/* ============================================================================
 * MENTAL ROTATION API
 * ============================================================================ */

/**
 * @brief Perform mental rotation comparison
 *
 * Compares two objects by mentally rotating one to match the other.
 * Processing time is linear with rotation angle (Shepard paradigm).
 *
 * @param sr Spatial reasoning handle
 * @param object_a First object
 * @param object_b Second object
 * @return Rotation comparison result
 */
rotation_result_t spatial_rotate_and_compare(
    spatial_reasoning_t* sr,
    const spatial_object_t* object_a,
    const spatial_object_t* object_b
);

/**
 * @brief Rotate object mentally by specified angle
 *
 * @param sr Spatial reasoning handle
 * @param object Object to rotate (modified in place)
 * @param axis Rotation axis (normalized)
 * @param angle_degrees Rotation angle in degrees
 * @return Processing time in milliseconds
 */
uint64_t spatial_mental_rotate(
    spatial_reasoning_t* sr,
    spatial_object_t* object,
    vec3_t axis,
    float angle_degrees
);

/**
 * @brief Compute shape similarity between two objects
 *
 * @param sr Spatial reasoning handle
 * @param object_a First object
 * @param object_b Second object
 * @return Similarity score [0,1]
 */
float spatial_shape_similarity(
    spatial_reasoning_t* sr,
    const spatial_object_t* object_a,
    const spatial_object_t* object_b
);

/* ============================================================================
 * COORDINATE TRANSFORMATION API
 * ============================================================================ */

/**
 * @brief Transform from egocentric to allocentric coordinates
 *
 * @param sr Spatial reasoning handle
 * @param local_pos Position in egocentric (body-centered) frame
 * @param observer Observer pose
 * @return Position in allocentric (world) coordinates
 */
vec3_t spatial_ego_to_allocentric(
    spatial_reasoning_t* sr,
    vec3_t local_pos,
    const observer_pose_t* observer
);

/**
 * @brief Transform from allocentric to egocentric coordinates
 *
 * @param sr Spatial reasoning handle
 * @param world_pos Position in allocentric (world) frame
 * @param observer Observer pose
 * @return Position in egocentric (body-centered) coordinates
 */
vec3_t spatial_allocentric_to_ego(
    spatial_reasoning_t* sr,
    vec3_t world_pos,
    const observer_pose_t* observer
);

/**
 * @brief Create transformation matrix from pose
 *
 * @param pose Observer pose
 * @return 4x4 transformation matrix
 */
spatial_transform_t spatial_pose_to_transform(const observer_pose_t* pose);

/**
 * @brief Apply transformation to point
 *
 * @param transform Transformation matrix
 * @param point Point to transform
 * @return Transformed point
 */
vec3_t spatial_transform_point(
    const spatial_transform_t* transform,
    vec3_t point
);

/**
 * @brief Invert transformation matrix
 *
 * @param transform Transformation to invert
 * @return Inverted transformation
 */
spatial_transform_t spatial_transform_inverse(const spatial_transform_t* transform);

/* ============================================================================
 * QUATERNION UTILITIES
 * ============================================================================ */

/**
 * @brief Create quaternion from axis-angle
 */
quaternion_t quaternion_from_axis_angle(vec3_t axis, float angle_radians);

/**
 * @brief Create identity quaternion
 */
quaternion_t quaternion_identity(void);

/**
 * @brief Multiply two quaternions
 */
quaternion_t quaternion_multiply(quaternion_t a, quaternion_t b);

/**
 * @brief Rotate vector by quaternion
 */
vec3_t quaternion_rotate_vector(quaternion_t q, vec3_t v);

/**
 * @brief Compute angle between two quaternions
 */
float quaternion_angle_between(quaternion_t a, quaternion_t b);

/**
 * @brief Normalize quaternion
 */
quaternion_t quaternion_normalize(quaternion_t q);

/* ============================================================================
 * SPATIAL INDEXING API
 * ============================================================================ */

/**
 * @brief Add object to spatial index
 *
 * @param sr Spatial reasoning handle
 * @param object Object to add (copied)
 * @return Object ID or 0 on error
 */
uint32_t spatial_add_object(
    spatial_reasoning_t* sr,
    const spatial_object_t* object
);

/**
 * @brief Remove object from spatial index
 *
 * @param sr Spatial reasoning handle
 * @param object_id Object ID to remove
 * @return 0 on success
 */
int spatial_remove_object(
    spatial_reasoning_t* sr,
    uint32_t object_id
);

/**
 * @brief Update object position
 *
 * @param sr Spatial reasoning handle
 * @param object_id Object ID
 * @param new_position New position
 * @return 0 on success
 */
int spatial_update_position(
    spatial_reasoning_t* sr,
    uint32_t object_id,
    vec3_t new_position
);

/**
 * @brief Find nearest object to query point
 *
 * @param sr Spatial reasoning handle
 * @param query_pos Query position
 * @return Nearest object or NULL if none
 */
spatial_object_t* spatial_find_nearest(
    spatial_reasoning_t* sr,
    vec3_t query_pos
);

/**
 * @brief Find k nearest objects to query point
 *
 * @param sr Spatial reasoning handle
 * @param query_pos Query position
 * @param k Number of neighbors to find
 * @param result Output result (pre-allocated)
 * @return Number of objects found
 */
uint32_t spatial_find_k_nearest(
    spatial_reasoning_t* sr,
    vec3_t query_pos,
    uint32_t k,
    spatial_query_result_t* result
);

/**
 * @brief Find all objects within radius
 *
 * @param sr Spatial reasoning handle
 * @param center Query center
 * @param radius Search radius
 * @param result Output result (pre-allocated)
 * @return Number of objects found
 */
uint32_t spatial_find_in_radius(
    spatial_reasoning_t* sr,
    vec3_t center,
    float radius,
    spatial_query_result_t* result
);

/**
 * @brief Create query result structure
 *
 * @param capacity Maximum results
 * @return Query result or NULL on error
 */
spatial_query_result_t* spatial_query_result_create(uint32_t capacity);

/**
 * @brief Destroy query result structure
 *
 * @param result Query result (NULL safe)
 */
void spatial_query_result_destroy(spatial_query_result_t* result);

/* ============================================================================
 * SPATIAL ATTENTION API
 * ============================================================================ */

/**
 * @brief Create spatial attention map
 *
 * @param sr Spatial reasoning handle
 * @param grid_width Grid width
 * @param grid_height Grid height
 * @return Attention map or NULL on error
 */
spatial_attention_t* spatial_attention_create(
    spatial_reasoning_t* sr,
    uint32_t grid_width,
    uint32_t grid_height
);

/**
 * @brief Destroy spatial attention map
 *
 * @param attention Attention map (NULL safe)
 */
void spatial_attention_destroy(spatial_attention_t* attention);

/**
 * @brief Set attention focus point
 *
 * @param attention Attention map
 * @param focus Focus point
 * @param spread Gaussian spread (sigma)
 * @return 0 on success
 */
int spatial_attention_set_focus(
    spatial_attention_t* attention,
    vec3_t focus,
    float spread
);

/**
 * @brief Get attention weight at position
 *
 * @param attention Attention map
 * @param pos Query position
 * @return Attention weight [0,1]
 */
float spatial_attention_at(
    const spatial_attention_t* attention,
    vec3_t pos
);

/**
 * @brief Update attention map (decay + shift)
 *
 * @param attention Attention map
 * @param decay_rate Decay rate [0,1]
 * @return 0 on success
 */
int spatial_attention_update(
    spatial_attention_t* attention,
    float decay_rate
);

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

/**
 * @brief Set inflammation level
 *
 * @param sr Spatial reasoning handle
 * @param level Inflammation level [0,1]
 * @return 0 on success
 */
int spatial_set_inflammation(
    spatial_reasoning_t* sr,
    float level
);

/**
 * @brief Set fatigue level
 *
 * @param sr Spatial reasoning handle
 * @param level Fatigue level [0,1]
 * @return 0 on success
 */
int spatial_set_fatigue(
    spatial_reasoning_t* sr,
    float level
);

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

/**
 * @brief Get statistics
 *
 * @param sr Spatial reasoning handle
 * @param stats Output statistics
 * @return 0 on success
 */
int spatial_get_stats(
    spatial_reasoning_t* sr,
    spatial_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param sr Spatial reasoning handle
 */
void spatial_reset_stats(spatial_reasoning_t* sr);

/**
 * @brief Get last error message
 * @return Thread-local error message
 */
const char* spatial_get_last_error(void);

/* ============================================================================
 * VECTOR UTILITIES
 * ============================================================================ */

/** Create vector */
static inline vec3_t vec3_create(float x, float y, float z) {
    vec3_t v = {x, y, z};
    return v;
}

/** Add vectors */
static inline vec3_t vec3_add(vec3_t a, vec3_t b) {
    return vec3_create(a.x + b.x, a.y + b.y, a.z + b.z);
}

/** Subtract vectors */
static inline vec3_t vec3_sub(vec3_t a, vec3_t b) {
    return vec3_create(a.x - b.x, a.y - b.y, a.z - b.z);
}

/** Scale vector */
static inline vec3_t vec3_scale(vec3_t v, float s) {
    return vec3_create(v.x * s, v.y * s, v.z * s);
}

/** Dot product */
static inline float vec3_dot(vec3_t a, vec3_t b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

/** Cross product */
static inline vec3_t vec3_cross(vec3_t a, vec3_t b) {
    return vec3_create(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

/** Length */
static inline float vec3_length(vec3_t v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

/** Normalize */
static inline vec3_t vec3_normalize(vec3_t v) {
    float len = vec3_length(v);
    if (len < 1e-10f) return vec3_create(0, 0, 0);
    return vec3_scale(v, 1.0f / len);
}

/** Distance */
static inline float vec3_distance(vec3_t a, vec3_t b) {
    return vec3_length(vec3_sub(a, b));
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SPATIAL_REASONING_H */
