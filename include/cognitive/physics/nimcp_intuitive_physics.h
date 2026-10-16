/**
 * @file nimcp_intuitive_physics.h
 * @brief Intuitive Physics Engine — rigid body simulation for world model grounding
 *
 * WHAT: Deterministic rigid-body physics simulator with collision detection,
 *       contact resolution, support graphs, and trajectory prediction.
 * WHY:  Provides the "ground truth" physics prior that constrains the learned
 *       world model (RSSM) to make physically plausible predictions. Infants
 *       develop intuitive physics by ~4 months; this module gives the brain
 *       the same inductive bias.
 * HOW:  Symplectic Euler integration, GJK-lite collision detection, sequential
 *       impulse contact solver (Erin Catto / Box2D style).
 *
 * THEORETICAL FOUNDATION:
 *   - Core Knowledge hypothesis (Spelke, 1990): objects are cohesive, move on
 *     continuous paths, cannot pass through each other.
 *   - Physics-Informed Neural Networks (Raissi et al., 2019): L = L_data + λL_physics
 *   - Hamiltonian Neural Networks (Greydanus et al., 2019): energy conservation
 */

#ifndef NIMCP_INTUITIVE_PHYSICS_H
#define NIMCP_INTUITIVE_PHYSICS_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/omni/bridges/nimcp_omni_wm_parietal_bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define IP_MAX_OBJECTS              128
#define IP_MAX_CONTACTS             512
#define IP_MAX_SUPPORT_EDGES        256
#define IP_DEFAULT_DT               0.01f
#define IP_DEFAULT_GRAVITY          9.81f
#define IP_SOLVER_ITERATIONS        8
#define IP_CONTACT_THRESHOLD        0.02f   /* penetration tolerance (m) */
#define IP_SUPPORT_NORMAL_THRESHOLD 0.7f    /* cos(angle) for "supported from below" */
#define IP_RESTITUTION_DEFAULT      0.3f
#define IP_FRICTION_DEFAULT         0.5f
#define IP_ENERGY_DRIFT_WARN        0.05f   /* warn if >5% energy drift */

/* ============================================================================
 * Shape Types
 * ============================================================================ */

typedef enum {
    IP_SHAPE_SPHERE   = 0,
    IP_SHAPE_BOX      = 1,
    IP_SHAPE_CYLINDER  = 2,
    IP_SHAPE_PLANE     = 3,   /* infinite ground/wall */
    IP_SHAPE_COUNT
} ip_shape_type_t;

/**
 * @brief Shape definition (tagged union)
 */
typedef struct {
    ip_shape_type_t type;
    union {
        struct { float radius; } sphere;
        struct { float hx, hy, hz; } box;           /* half-extents */
        struct { float radius, half_height; } cylinder;
        struct { wm_parietal_vec3_t normal; float offset; } plane;
    };
} ip_shape_t;

/* ============================================================================
 * Object
 * ============================================================================ */

typedef struct {
    uint32_t            id;
    wm_parietal_vec3_t  position;
    wm_parietal_velocity_t velocity;
    wm_parietal_quaternion_t orientation;
    wm_parietal_vec3_t  angular_velocity;
    float               mass;           /* 0 = static/infinite mass */
    float               inv_mass;       /* precomputed 1/mass (0 for static) */
    float               restitution;    /* bounciness [0..1] */
    float               friction;       /* surface friction coefficient */
    ip_shape_t          shape;
    /* Inertia tensor (diagonal approximation for simplicity) */
    wm_parietal_vec3_t  inv_inertia;    /* inverse diagonal inertia tensor */
    /* Relations (updated each step) */
    uint32_t            supported_by;   /* object id, UINT32_MAX = none/ground */
    uint32_t            contained_in;   /* object id, UINT32_MAX = none */
    /* Visibility / permanence */
    bool                is_static;
    bool                visible;
    bool                active;         /* false = slot unused */
    float               last_seen_time;
} ip_object_t;

/* ============================================================================
 * Contact
 * ============================================================================ */

typedef struct {
    uint32_t            obj_a;          /* object index */
    uint32_t            obj_b;          /* object index */
    wm_parietal_vec3_t  normal;         /* contact normal (A -> B) */
    wm_parietal_vec3_t  point;          /* contact point (world space) */
    float               penetration;    /* overlap depth */
    float               normal_impulse; /* accumulated normal impulse */
    float               tangent_impulse;/* accumulated friction impulse */
} ip_contact_t;

/* ============================================================================
 * Support Edge (DAG)
 * ============================================================================ */

typedef struct {
    uint32_t            supporter;      /* object providing support */
    uint32_t            supported;      /* object being supported */
    float               contact_force;  /* normal force magnitude */
} ip_support_edge_t;

/* ============================================================================
 * Scene
 * ============================================================================ */

typedef struct {
    ip_object_t*        objects;
    uint32_t            num_objects;
    uint32_t            capacity;

    ip_contact_t*       contacts;
    uint32_t            num_contacts;
    uint32_t            contacts_capacity;

    ip_support_edge_t*  support_graph;
    uint32_t            num_support_edges;
    uint32_t            support_capacity;

    wm_parietal_vec3_t  gravity;
    float               time;
    float               dt;
} ip_scene_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    uint32_t            max_objects;
    uint32_t            max_contacts;
    float               dt;
    float               gravity_magnitude;
    wm_parietal_vec3_t  gravity_direction;
    uint32_t            solver_iterations;
    float               restitution_default;
    float               friction_default;
} ip_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    uint64_t            step_count;
    uint64_t            collision_checks;
    uint64_t            contacts_resolved;
    float               total_kinetic_energy;
    float               total_potential_energy;
    float               initial_total_energy;
    float               energy_drift;       /* (current - initial) / initial */
    uint32_t            active_objects;
    uint32_t            active_contacts;
} ip_stats_t;

/* ============================================================================
 * Engine
 * ============================================================================ */

typedef struct intuitive_physics_engine {
    ip_scene_t          scene;
    ip_config_t         config;
    ip_stats_t          stats;
    bool                initialized;
} intuitive_physics_engine_t;

/* ============================================================================
 * API
 * ============================================================================ */

/** Create and initialize the physics engine */
intuitive_physics_engine_t* intuitive_physics_create(const ip_config_t* config);

/** Destroy and free all resources */
void intuitive_physics_destroy(intuitive_physics_engine_t* engine);

/** Add an object to the scene. Returns object id, or UINT32_MAX on failure */
uint32_t intuitive_physics_add_object(intuitive_physics_engine_t* engine,
                                       const ip_object_t* obj);

/** Remove an object by id */
void intuitive_physics_remove_object(intuitive_physics_engine_t* engine, uint32_t id);

/** Get object by id (NULL if not found) */
ip_object_t* intuitive_physics_get_object(intuitive_physics_engine_t* engine, uint32_t id);

/** Step the simulation forward by dt seconds */
int intuitive_physics_step(intuitive_physics_engine_t* engine, float dt);

/**
 * Predict trajectory of an object over `duration` seconds.
 * Runs a copy of the simulation forward without modifying the real scene.
 * Caller must free trajectory->states when done.
 */
int intuitive_physics_predict_trajectory(intuitive_physics_engine_t* engine,
                                          uint32_t object_id,
                                          float duration,
                                          wm_parietal_trajectory_t* out_traj);

/** Query: will objects a and b collide within `within_time`? */
bool intuitive_physics_will_collide(intuitive_physics_engine_t* engine,
                                     uint32_t obj_a, uint32_t obj_b,
                                     float within_time, float* collision_time);

/** Query: is the given object supported (resting on something)? */
bool intuitive_physics_is_supported(const intuitive_physics_engine_t* engine, uint32_t id);

/** Query: is the full scene in static equilibrium? */
bool intuitive_physics_is_stable(const intuitive_physics_engine_t* engine);

/** Get support chain for an object (who supports who). Returns chain length */
uint32_t intuitive_physics_get_support_chain(const intuitive_physics_engine_t* engine,
                                              uint32_t object_id,
                                              uint32_t* chain_buf, uint32_t buf_size);

/** Predict what happens if object is removed (collapse cascade).
 *  Returns number of objects that would fall. */
uint32_t intuitive_physics_predict_removal(intuitive_physics_engine_t* engine,
                                            uint32_t object_id,
                                            uint32_t* affected_buf, uint32_t buf_size);

/** Get current statistics */
ip_stats_t intuitive_physics_get_stats(const intuitive_physics_engine_t* engine);

/** Create a default configuration */
ip_config_t intuitive_physics_default_config(void);

/** Add a ground plane at y=0 */
uint32_t intuitive_physics_add_ground(intuitive_physics_engine_t* engine);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INTUITIVE_PHYSICS_H */
