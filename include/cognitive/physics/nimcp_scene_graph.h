/**
 * @file nimcp_scene_graph.h
 * @brief Scene Graph — spatial relation reasoning (support, containment, proximity)
 *
 * WHAT: Maintains a graph of spatial relations between objects: ON_TOP_OF,
 *       INSIDE, NEXT_TO, ABOVE, BELOW, ATTACHED_TO, BLOCKING (occlusion).
 * WHY:  Enables relational reasoning about physical scenes — "if I remove the
 *       table, the cup falls" requires knowing the support graph.
 * HOW:  Rebuilt each physics step from contact data (cheap at ≤128 objects).
 *       Support relations derived from contact normals, containment from AABB
 *       inclusion, proximity from distance.
 */

#ifndef NIMCP_SCENE_GRAPH_H
#define NIMCP_SCENE_GRAPH_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/omni/bridges/nimcp_omni_wm_parietal_bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct intuitive_physics_engine intuitive_physics_engine_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SG_MAX_RELATIONS        512
#define SG_MAX_CHAIN_DEPTH      16
#define SG_PROXIMITY_THRESHOLD  1.0f    /* meters — "next to" distance */

/* ============================================================================
 * Relation Types
 * ============================================================================ */

typedef enum {
    SCENE_REL_ON_TOP_OF   = 0,  /* A rests on B (derived from upward contact normal) */
    SCENE_REL_INSIDE      = 1,  /* A is contained within B (AABB inclusion) */
    SCENE_REL_NEXT_TO     = 2,  /* A and B are close but not touching */
    SCENE_REL_ABOVE       = 3,  /* A is above B (no contact) */
    SCENE_REL_BELOW       = 4,  /* A is below B (no contact) */
    SCENE_REL_ATTACHED_TO = 5,  /* A is fixed to B (joint/constraint) */
    SCENE_REL_BLOCKING    = 6,  /* A occludes B from viewpoint */
    SCENE_REL_TOUCHING    = 7,  /* A and B are in contact */
    SCENE_REL_COUNT
} scene_relation_type_t;

/* ============================================================================
 * Relation Edge
 * ============================================================================ */

typedef struct {
    uint32_t                subject_id; /* object A */
    uint32_t                object_id;  /* object B */
    scene_relation_type_t   type;
    float                   confidence; /* [0..1] */
    float                   strength;   /* e.g. contact force magnitude */
} scene_relation_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    uint32_t    max_relations;
    float       proximity_threshold;    /* distance for NEXT_TO (m) */
    float       support_normal_thresh;  /* cos(angle) for ON_TOP_OF */
    bool        enable_occlusion;       /* compute BLOCKING relations */
    wm_parietal_vec3_t viewpoint;       /* camera/eye position for occlusion */
} scene_graph_config_t;

/* ============================================================================
 * Scene Graph
 * ============================================================================ */

typedef struct scene_graph {
    scene_relation_t*   relations;
    uint32_t            num_relations;
    uint32_t            capacity;

    scene_graph_config_t config;

    /* Per-object adjacency (support children) — for fast chain queries */
    /* support_children[i] = head of linked list of objects supported by i */
    /* Stored as flat arrays for cache efficiency */
    uint32_t*           support_children;       /* [max_objects * max_children_per] */
    uint32_t*           support_children_count;  /* [max_objects] */
    uint32_t            max_objects;

    /* Statistics */
    uint64_t            rebuild_count;

    bool                initialized;
} scene_graph_t;

/* ============================================================================
 * API
 * ============================================================================ */

/** Create scene graph */
scene_graph_t* scene_graph_create(const scene_graph_config_t* config);

/** Destroy scene graph */
void scene_graph_destroy(scene_graph_t* graph);

/** Rebuild all relations from current physics state.
 *  This is the primary update method — called each physics step */
int scene_graph_rebuild(scene_graph_t* graph, const intuitive_physics_engine_t* physics);

/** Query: get all relations involving a specific object */
uint32_t scene_graph_get_relations_for(const scene_graph_t* graph, uint32_t object_id,
                                        scene_relation_t* buf, uint32_t buf_size);

/** Query: does relation exist? */
bool scene_graph_has_relation(const scene_graph_t* graph,
                               uint32_t subject, uint32_t object,
                               scene_relation_type_t type);

/** Query: get full support chain (who supports whom, bottom to top) */
uint32_t scene_graph_get_support_chain(const scene_graph_t* graph, uint32_t object_id,
                                        uint32_t* chain_buf, uint32_t buf_size);

/** Query: predict which objects are affected if object_id is removed.
 *  Returns count of affected objects (those that lose support) */
uint32_t scene_graph_predict_removal_cascade(const scene_graph_t* graph,
                                              uint32_t object_id,
                                              uint32_t* affected_buf, uint32_t buf_size);

/** Query: is this stack stable? (all support chains terminate at static objects) */
bool scene_graph_is_stack_stable(const scene_graph_t* graph, uint32_t top_object_id);

/** Query: what is object A's relation to object B? Returns type or -1 */
int scene_graph_relation_between(const scene_graph_t* graph,
                                  uint32_t a, uint32_t b);

/** Get total relation count */
uint32_t scene_graph_count(const scene_graph_t* graph);

/** Default config */
scene_graph_config_t scene_graph_default_config(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SCENE_GRAPH_H */
