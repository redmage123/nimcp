/**
 * @file nimcp_reasoning_visuospatial.h
 * @brief Visuospatial Reasoning -- spatial object representation, relations, and queries
 *
 * WHAT: Provides spatial reasoning capabilities for the reasoning chain engine.
 *       Supports spatial object representation, relation inference, geometric
 *       queries (distance, containment, intersection), and path finding.
 * WHY:  The brain's parietal lobe performs visuospatial reasoning -- understanding
 *       where objects are, how they relate spatially, and navigating through space.
 *       Connecting this to the reasoning chain enables spatial reasoning steps.
 * HOW:  Maintains a spatial scene with objects (positions + bounding boxes),
 *       infers spatial relations from geometry, supports queries via BFS/linear scan.
 *
 * BIOLOGICAL BASIS:
 * Models the posterior parietal cortex (PPC) and its role in spatial cognition:
 * - Object location encoding (vs_point_t positions)
 * - Spatial relation computation (above, below, near, inside, etc.)
 * - Mental rotation and navigation (path queries via relation graph)
 * - Integration with dorsal visual stream ("where" pathway)
 *
 * INTEGRATION:
 * - Registered as REASONING_STEP_VISUOSPATIAL in the reasoning chain
 * - Opt-in via enable_visuospatial_reasoning in reasoning_engine_config_t
 * - Can participate as convergent contributor (Tier 1 evidence producer)
 *
 * NOTE: All types prefixed with vs_ to avoid conflicts with the parietal
 *       spatial reasoning module (nimcp_spatial_reasoning.h).
 *
 * @version 1.0.0
 * @date 2026-02-26
 */

#ifndef NIMCP_REASONING_VISUOSPATIAL_H
#define NIMCP_REASONING_VISUOSPATIAL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Maximum objects in a visuospatial scene */
#define VS_MAX_OBJECTS 256

/** Maximum explicit relations */
#define VS_MAX_RELATIONS 1024

/** Maximum name length for visuospatial objects */
#define VS_MAX_NAME_LEN 64

/** Maximum spatial dimensions */
#define VS_MAX_DIMENSIONS 3

/** Default proximity threshold for NEAR/FAR classification */
#define VS_DEFAULT_PROXIMITY_THRESHOLD 1.0f

/** Maximum objects returned by region query */
#define VS_MAX_REGION_RESULTS 32

/** Maximum explanation string length */
#define VS_EXPLANATION_LEN 256

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief 3D spatial point for visuospatial reasoning
 */
typedef struct {
    float x;
    float y;
    float z;
} vs_point_t;

/**
 * @brief Axis-aligned bounding box for visuospatial reasoning
 */
typedef struct {
    vs_point_t min;
    vs_point_t max;
} vs_bounds_t;

/**
 * @brief Visuospatial object in the scene
 */
typedef struct {
    uint32_t id;
    char name[VS_MAX_NAME_LEN];
    vs_point_t position;
    vs_bounds_t bounds;
    bool has_bounds;
} vs_object_t;

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Type of spatial relation between two objects
 */
typedef enum {
    VS_RELATION_ABOVE = 0,
    VS_RELATION_BELOW,
    VS_RELATION_LEFT_OF,
    VS_RELATION_RIGHT_OF,
    VS_RELATION_IN_FRONT,
    VS_RELATION_BEHIND,
    VS_RELATION_INSIDE,
    VS_RELATION_OUTSIDE,
    VS_RELATION_NEAR,
    VS_RELATION_FAR,
    VS_RELATION_TOUCHING,
    VS_RELATION_OVERLAPPING,
    VS_RELATION_BETWEEN
} vs_relation_type_t;

/**
 * @brief Spatial relation between two (or three) objects
 */
typedef struct {
    vs_relation_type_t type;
    uint32_t object_a_id;
    uint32_t object_b_id;
    float confidence;
    uint32_t reference_id;  /**< For BETWEEN: the third reference object */
} vs_relation_t;

/**
 * @brief Type of spatial query
 */
typedef enum {
    VS_QUERY_RELATION = 0,     /**< Check if a relation holds */
    VS_QUERY_DISTANCE,          /**< Compute distance between objects */
    VS_QUERY_NEAREST,           /**< Find nearest object to target */
    VS_QUERY_CONTAINS,          /**< Check if A is inside B's bounds */
    VS_QUERY_PATH,              /**< Check if A can reach B via NEAR chain */
    VS_QUERY_OBJECTS_IN_REGION  /**< Find all objects in a region */
} vs_query_type_t;

/**
 * @brief Spatial query specification
 */
typedef struct {
    vs_query_type_t type;
    uint32_t object_a_id;
    uint32_t object_b_id;
    vs_relation_type_t relation;
    vs_bounds_t region;
} vs_query_t;

/**
 * @brief Result of a spatial query
 */
typedef struct {
    bool holds;                                    /**< Relation holds */
    float confidence;                              /**< Confidence [0-1] */
    float distance;                                /**< Distance result */
    uint32_t nearest_id;                           /**< Nearest object ID */
    uint32_t objects_in_region[VS_MAX_REGION_RESULTS]; /**< Objects in region */
    uint32_t num_objects_in_region;                /**< Count of objects */
    char explanation[VS_EXPLANATION_LEN];           /**< Human-readable explanation */
} vs_result_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Configuration for visuospatial reasoning
 */
typedef struct {
    uint32_t max_objects;         /**< Maximum objects (default VS_MAX_OBJECTS) */
    float proximity_threshold;    /**< Distance threshold for NEAR (default 1.0) */
    bool enable_3d;               /**< Enable 3D reasoning (default false) */
} visuospatial_config_t;

/**
 * @brief Statistics for visuospatial reasoning
 */
typedef struct {
    uint32_t num_objects;
    uint32_t num_relations;
    uint32_t num_queries;
    float avg_query_time_us;
} visuospatial_stats_t;

/*=============================================================================
 * OPAQUE TYPE
 *===========================================================================*/

typedef struct reasoning_visuospatial reasoning_visuospatial_t;

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

reasoning_visuospatial_t* reasoning_visuospatial_create(
    const visuospatial_config_t* config);

void reasoning_visuospatial_destroy(reasoning_visuospatial_t* vs);

visuospatial_config_t reasoning_visuospatial_default_config(void);

/*=============================================================================
 * OBJECT MANAGEMENT
 *===========================================================================*/

int reasoning_visuospatial_add_object(reasoning_visuospatial_t* vs,
                                       const char* name,
                                       vs_point_t position);

int reasoning_visuospatial_add_object_with_bounds(reasoning_visuospatial_t* vs,
                                                    const char* name,
                                                    vs_point_t position,
                                                    vs_bounds_t bounds);

int reasoning_visuospatial_remove_object(reasoning_visuospatial_t* vs,
                                          uint32_t object_id);

int reasoning_visuospatial_move_object(reasoning_visuospatial_t* vs,
                                        uint32_t object_id,
                                        vs_point_t new_position);

int reasoning_visuospatial_get_object(const reasoning_visuospatial_t* vs,
                                       uint32_t object_id,
                                       vs_object_t* out);

/*=============================================================================
 * RELATIONS
 *===========================================================================*/

int reasoning_visuospatial_add_relation(reasoning_visuospatial_t* vs,
                                         vs_relation_type_t type,
                                         uint32_t obj_a, uint32_t obj_b,
                                         float confidence);

int reasoning_visuospatial_infer_relations(reasoning_visuospatial_t* vs);

/*=============================================================================
 * QUERIES
 *===========================================================================*/

int reasoning_visuospatial_query(reasoning_visuospatial_t* vs,
                                  const vs_query_t* query,
                                  vs_result_t* result);

float reasoning_visuospatial_distance(const reasoning_visuospatial_t* vs,
                                       uint32_t obj_a, uint32_t obj_b);

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

int reasoning_visuospatial_get_stats(const reasoning_visuospatial_t* vs,
                                      visuospatial_stats_t* stats);

/*=============================================================================
 * UTILITY
 *===========================================================================*/

const char* reasoning_visuospatial_get_relation_name(vs_relation_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_REASONING_VISUOSPATIAL_H */
