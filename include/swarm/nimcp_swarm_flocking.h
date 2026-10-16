/**
 * @file nimcp_swarm_flocking.h
 * @brief Flocking Dynamics Engine for NIMCP Swarms
 *
 * Biologically-inspired flocking behavior based on Reynolds' boids algorithm.
 * Implements separation, alignment, and cohesion rules with advanced behaviors
 * including obstacle avoidance, predator evasion, and formation templates.
 *
 * Biological Inspiration:
 * - Bird flocks (geese V-formation, murmurations)
 * - Fish schools (coordinated swimming, predator evasion)
 * - Insect swarms (mosquitoes, bees)
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_SWARM_FLOCKING_H
#define NIMCP_SWARM_FLOCKING_H

#include "utils/validation/nimcp_common.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Formation templates for coordinated swarm behavior
 */
typedef enum {
    NIMCP_FORMATION_NONE,          /**< No specific formation */
    NIMCP_FORMATION_V,             /**< V-formation for energy efficiency */
    NIMCP_FORMATION_SPHERE,        /**< Defensive sphere formation */
    NIMCP_FORMATION_LINE,          /**< Single file line formation */
    NIMCP_FORMATION_GRID,          /**< Grid formation for area coverage */
    NIMCP_FORMATION_SPIRAL,        /**< Spiral formation for search */
    NIMCP_FORMATION_WEDGE,         /**< Wedge formation for offense */
    NIMCP_FORMATION_DIAMOND,       /**< Diamond formation for reconnaissance */
    NIMCP_FORMATION_CIRCLE,        /**< Circle formation for perimeter */
    NIMCP_FORMATION_CUSTOM         /**< User-defined formation */
} nimcp_formation_type_t;

/**
 * @brief Flocking behavior state
 */
typedef enum {
    NIMCP_FLOCK_IDLE,              /**< No flocking behavior */
    NIMCP_FLOCK_ACTIVE,            /**< Normal flocking */
    NIMCP_FLOCK_EVADING,           /**< Evading predator/threat */
    NIMCP_FLOCK_SEEKING,           /**< Seeking goal */
    NIMCP_FLOCK_FORMING,           /**< Transitioning to formation */
    NIMCP_FLOCK_HOLDING            /**< Maintaining formation */
} nimcp_flock_state_t;

/**
 * @brief 3D position vector
 */
typedef struct {
    float x;                        /**< X coordinate */
    float y;                        /**< Y coordinate */
    float z;                        /**< Z coordinate */
} nimcp_vec3_t;

/**
 * @brief Boid (bird-oid) agent in the flock
 */
typedef struct {
    uint32_t id;                    /**< Unique boid identifier */
    nimcp_vec3_t position;          /**< Current position */
    nimcp_vec3_t velocity;          /**< Current velocity */
    nimcp_vec3_t acceleration;      /**< Current acceleration */

    float mass;                     /**< Mass for physics */
    float max_speed;                /**< Maximum speed */
    float max_force;                /**< Maximum steering force */

    uint32_t neighbor_count;        /**< Number of neighbors detected */
    uint32_t *neighbor_ids;         /**< Array of neighbor IDs */
    size_t neighbor_capacity;       /**< Capacity of neighbor array */

    bool is_leader;                 /**< Leader flag for formations */
    uint32_t follow_id;             /**< ID of boid to follow */

    void *user_data;                /**< User-defined data */
} nimcp_boid_t;

/**
 * @brief Obstacle for avoidance
 */
typedef struct {
    nimcp_vec3_t position;          /**< Obstacle center */
    float radius;                   /**< Obstacle radius */
    bool is_active;                 /**< Active flag */
} nimcp_obstacle_t;

/**
 * @brief Flocking configuration parameters
 */
typedef struct {
    // Core flocking weights
    float separation_weight;        /**< Weight for separation rule (default: 1.5) */
    float alignment_weight;         /**< Weight for alignment rule (default: 1.0) */
    float cohesion_weight;          /**< Weight for cohesion rule (default: 1.0) */

    // Radii for neighbor detection
    float separation_radius;        /**< Separation distance (default: 2.0) */
    float alignment_radius;         /**< Alignment detection radius (default: 5.0) */
    float cohesion_radius;          /**< Cohesion detection radius (default: 5.0) */

    // Speed and force limits
    float max_speed;                /**< Maximum boid speed (default: 5.0) */
    float max_force;                /**< Maximum steering force (default: 0.5) */
    float max_acceleration;         /**< Maximum acceleration (default: 2.0) */

    // Advanced behavior weights
    float obstacle_weight;          /**< Obstacle avoidance weight (default: 2.0) */
    float goal_weight;              /**< Goal seeking weight (default: 1.0) */
    float boundary_weight;          /**< Boundary containment weight (default: 1.5) */
    float predator_weight;          /**< Predator evasion weight (default: 3.0) */

    // Obstacle avoidance
    float obstacle_detect_radius;   /**< Obstacle detection range (default: 10.0) */
    float obstacle_avoid_distance;  /**< Minimum distance from obstacles (default: 3.0) */

    // Boundary containment
    nimcp_vec3_t boundary_min;      /**< Minimum boundary */
    nimcp_vec3_t boundary_max;      /**< Maximum boundary */
    float boundary_margin;          /**< Margin before boundary force (default: 10.0) */

    // Formation parameters
    float formation_tightness;      /**< Formation tightness factor (default: 1.0) */
    float formation_spacing;        /**< Spacing between boids (default: 3.0) */

    // Neighbor detection
    uint32_t max_neighbors;         /**< Maximum neighbors to consider (default: 10) */

    // Update rate
    float update_dt;                /**< Time step for updates (default: 0.016) */
} nimcp_flocking_config_t;

/**
 * @brief Flocking engine state
 */
typedef struct {
    // Configuration
    nimcp_flocking_config_t config;

    // Boids
    nimcp_boid_t *boids;            /**< Array of boids */
    uint32_t boid_count;            /**< Number of boids */
    uint32_t boid_capacity;         /**< Capacity of boid array */

    // Obstacles
    nimcp_obstacle_t *obstacles;    /**< Array of obstacles */
    uint32_t obstacle_count;        /**< Number of obstacles */
    uint32_t obstacle_capacity;     /**< Capacity of obstacle array */

    // Goal and predator
    nimcp_vec3_t goal_position;     /**< Goal position for seeking */
    bool has_goal;                  /**< Goal active flag */

    nimcp_vec3_t predator_position; /**< Predator position */
    bool has_predator;              /**< Predator active flag */
    float predator_radius;          /**< Predator influence radius */

    // Formation
    nimcp_formation_type_t formation_type;
    uint32_t leader_id;             /**< Formation leader ID */
    nimcp_vec3_t formation_center;  /**< Formation center point */

    // State
    nimcp_flock_state_t state;
    uint64_t update_count;          /**< Number of updates performed */

    // Bio-async integration
    bio_module_context_t bio_ctx;   /**< Bio-async module context (pointer type) */
    bool bio_async_enabled;         /**< Bio-async enabled flag */

    // Thread safety
    nimcp_platform_mutex_t *mutex;  /**< Mutex for thread safety */

    void *user_data;                /**< User-defined data */
} nimcp_flocking_engine_t;

/**
 * @brief Flocking statistics
 */
typedef struct {
    float avg_speed;                /**< Average boid speed */
    float avg_neighbor_count;       /**< Average neighbors per boid */
    float cohesion_metric;          /**< Flock cohesion (0-1) */
    float alignment_metric;         /**< Flock alignment (0-1) */
    float formation_quality;        /**< Formation adherence (0-1) */
    nimcp_vec3_t center_of_mass;    /**< Flock center of mass */
    float bounding_radius;          /**< Flock bounding sphere radius */
} nimcp_flocking_stats_t;

/* ========================================================================
 * Creation and Destruction
 * ======================================================================== */

/**
 * @brief Create a flocking engine
 *
 * @param config Flocking configuration (NULL for defaults)
 * @return Flocking engine or NULL on failure
 */
nimcp_flocking_engine_t *nimcp_flocking_create(const nimcp_flocking_config_t *config);

/**
 * @brief Destroy a flocking engine
 *
 * @param engine Flocking engine to destroy
 */
void nimcp_flocking_destroy(nimcp_flocking_engine_t *engine);

/**
 * @brief Get default flocking configuration
 *
 * @param config Configuration structure to fill
 */
void nimcp_flocking_get_default_config(nimcp_flocking_config_t *config);

/* ========================================================================
 * Boid Management
 * ======================================================================== */

/**
 * @brief Add a boid to the flock
 *
 * @param engine Flocking engine
 * @param position Initial position
 * @param velocity Initial velocity
 * @return Boid ID or 0 on failure
 */
uint32_t nimcp_flocking_add_boid(nimcp_flocking_engine_t *engine,
                                 const nimcp_vec3_t *position,
                                 const nimcp_vec3_t *velocity);

/**
 * @brief Remove a boid from the flock
 *
 * @param engine Flocking engine
 * @param boid_id Boid ID to remove
 * @return 0 on success, -1 on failure
 */
int nimcp_flocking_remove_boid(nimcp_flocking_engine_t *engine, uint32_t boid_id);

/**
 * @brief Get a boid by ID
 *
 * @param engine Flocking engine
 * @param boid_id Boid ID
 * @return Boid pointer or NULL if not found
 */
nimcp_boid_t *nimcp_flocking_get_boid(nimcp_flocking_engine_t *engine, uint32_t boid_id);

/**
 * @brief Update boid position and velocity manually
 *
 * @param engine Flocking engine
 * @param boid_id Boid ID
 * @param position New position (NULL to keep current)
 * @param velocity New velocity (NULL to keep current)
 * @return 0 on success, -1 on failure
 */
int nimcp_flocking_update_boid(nimcp_flocking_engine_t *engine,
                               uint32_t boid_id,
                               const nimcp_vec3_t *position,
                               const nimcp_vec3_t *velocity);

/* ========================================================================
 * Obstacle Management
 * ======================================================================== */

/**
 * @brief Add an obstacle
 *
 * @param engine Flocking engine
 * @param position Obstacle position
 * @param radius Obstacle radius
 * @return Obstacle ID or 0 on failure
 */
uint32_t nimcp_flocking_add_obstacle(nimcp_flocking_engine_t *engine,
                                     const nimcp_vec3_t *position,
                                     float radius);

/**
 * @brief Remove an obstacle
 *
 * @param engine Flocking engine
 * @param obstacle_id Obstacle ID
 * @return 0 on success, -1 on failure
 */
int nimcp_flocking_remove_obstacle(nimcp_flocking_engine_t *engine, uint32_t obstacle_id);

/**
 * @brief Update obstacle position
 *
 * @param engine Flocking engine
 * @param obstacle_id Obstacle ID
 * @param position New position
 * @return 0 on success, -1 on failure
 */
int nimcp_flocking_update_obstacle(nimcp_flocking_engine_t *engine,
                                   uint32_t obstacle_id,
                                   const nimcp_vec3_t *position);

/* ========================================================================
 * Goal and Predator
 * ======================================================================== */

/**
 * @brief Set goal position for goal-seeking behavior
 *
 * @param engine Flocking engine
 * @param position Goal position
 */
void nimcp_flocking_set_goal(nimcp_flocking_engine_t *engine, const nimcp_vec3_t *position);

/**
 * @brief Clear goal
 *
 * @param engine Flocking engine
 */
void nimcp_flocking_clear_goal(nimcp_flocking_engine_t *engine);

/**
 * @brief Set predator position for evasion behavior
 *
 * @param engine Flocking engine
 * @param position Predator position
 * @param radius Predator influence radius
 */
void nimcp_flocking_set_predator(nimcp_flocking_engine_t *engine,
                                 const nimcp_vec3_t *position,
                                 float radius);

/**
 * @brief Clear predator
 *
 * @param engine Flocking engine
 */
void nimcp_flocking_clear_predator(nimcp_flocking_engine_t *engine);

/* ========================================================================
 * Formation Control
 * ======================================================================== */

/**
 * @brief Set formation type
 *
 * @param engine Flocking engine
 * @param formation Formation type
 * @param leader_id Leader boid ID (0 for auto-select)
 * @return 0 on success, -1 on failure
 */
int nimcp_flocking_set_formation(nimcp_flocking_engine_t *engine,
                                 nimcp_formation_type_t formation,
                                 uint32_t leader_id);

/**
 * @brief Clear formation (return to free flocking)
 *
 * @param engine Flocking engine
 */
void nimcp_flocking_clear_formation(nimcp_flocking_engine_t *engine);

/**
 * @brief Get ideal position for boid in formation
 *
 * @param engine Flocking engine
 * @param boid_id Boid ID
 * @param position Output position
 * @return 0 on success, -1 on failure
 */
int nimcp_flocking_get_formation_position(nimcp_flocking_engine_t *engine,
                                          uint32_t boid_id,
                                          nimcp_vec3_t *position);

/* ========================================================================
 * Update and Simulation
 * ======================================================================== */

/**
 * @brief Update flocking simulation for one time step
 *
 * @param engine Flocking engine
 * @param dt Time step (0 to use config.update_dt)
 * @return 0 on success, -1 on failure
 */
int nimcp_flocking_update(nimcp_flocking_engine_t *engine, float dt);

/**
 * @brief Calculate flocking force for a boid
 *
 * @param engine Flocking engine
 * @param boid_id Boid ID
 * @param force Output force vector
 * @return 0 on success, -1 on failure
 */
int nimcp_flocking_calculate_force(nimcp_flocking_engine_t *engine,
                                   uint32_t boid_id,
                                   nimcp_vec3_t *force);

/* ========================================================================
 * Core Flocking Rules
 * ======================================================================== */

/**
 * @brief Calculate separation force (avoid crowding neighbors)
 *
 * @param engine Flocking engine
 * @param boid Boid to calculate force for
 * @param force Output force vector
 * @return 0 on success, -1 on failure
 */
int nimcp_flocking_separation(nimcp_flocking_engine_t *engine,
                              const nimcp_boid_t *boid,
                              nimcp_vec3_t *force);

/**
 * @brief Calculate alignment force (steer towards average heading)
 *
 * @param engine Flocking engine
 * @param boid Boid to calculate force for
 * @param force Output force vector
 * @return 0 on success, -1 on failure
 */
int nimcp_flocking_alignment(nimcp_flocking_engine_t *engine,
                             const nimcp_boid_t *boid,
                             nimcp_vec3_t *force);

/**
 * @brief Calculate cohesion force (steer towards center of mass)
 *
 * @param engine Flocking engine
 * @param boid Boid to calculate force for
 * @param force Output force vector
 * @return 0 on success, -1 on failure
 */
int nimcp_flocking_cohesion(nimcp_flocking_engine_t *engine,
                            const nimcp_boid_t *boid,
                            nimcp_vec3_t *force);

/* ========================================================================
 * Advanced Behaviors
 * ======================================================================== */

/**
 * @brief Calculate obstacle avoidance force
 *
 * @param engine Flocking engine
 * @param boid Boid to calculate force for
 * @param force Output force vector
 * @return 0 on success, -1 on failure
 */
int nimcp_flocking_obstacle_avoidance(nimcp_flocking_engine_t *engine,
                                      const nimcp_boid_t *boid,
                                      nimcp_vec3_t *force);

/**
 * @brief Calculate goal seeking force
 *
 * @param engine Flocking engine
 * @param boid Boid to calculate force for
 * @param force Output force vector
 * @return 0 on success, -1 on failure
 */
int nimcp_flocking_goal_seek(nimcp_flocking_engine_t *engine,
                             const nimcp_boid_t *boid,
                             nimcp_vec3_t *force);

/**
 * @brief Calculate boundary containment force
 *
 * @param engine Flocking engine
 * @param boid Boid to calculate force for
 * @param force Output force vector
 * @return 0 on success, -1 on failure
 */
int nimcp_flocking_boundary_containment(nimcp_flocking_engine_t *engine,
                                        const nimcp_boid_t *boid,
                                        nimcp_vec3_t *force);

/**
 * @brief Calculate predator evasion force
 *
 * @param engine Flocking engine
 * @param boid Boid to calculate force for
 * @param force Output force vector
 * @return 0 on success, -1 on failure
 */
int nimcp_flocking_predator_evasion(nimcp_flocking_engine_t *engine,
                                    const nimcp_boid_t *boid,
                                    nimcp_vec3_t *force);

/* ========================================================================
 * Statistics and Analysis
 * ======================================================================== */

/**
 * @brief Get flocking statistics
 *
 * @param engine Flocking engine
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int nimcp_flocking_get_stats(nimcp_flocking_engine_t *engine,
                             nimcp_flocking_stats_t *stats);

/**
 * @brief Calculate center of mass of the flock
 *
 * @param engine Flocking engine
 * @param center Output center of mass
 * @return 0 on success, -1 on failure
 */
int nimcp_flocking_center_of_mass(nimcp_flocking_engine_t *engine,
                                  nimcp_vec3_t *center);

/* ========================================================================
 * Neighbor Detection
 * ======================================================================== */

/**
 * @brief Find neighbors for a boid
 *
 * @param engine Flocking engine
 * @param boid_id Boid ID
 * @param radius Search radius
 * @return 0 on success, -1 on failure
 */
int nimcp_flocking_find_neighbors(nimcp_flocking_engine_t *engine,
                                  uint32_t boid_id,
                                  float radius);

/* ========================================================================
 * Bio-Async Integration
 * ======================================================================== */

/**
 * @brief Register flocking engine with bio-async router
 *
 * @param engine Flocking engine
 * @param router Bio-async router
 * @return 0 on success, -1 on failure
 */
int nimcp_flocking_register_bioasync(nimcp_flocking_engine_t *engine,
                                     bio_router_t *router);

/**
 * @brief Process bio-async messages
 *
 * @param engine Flocking engine
 * @return Number of messages processed, -1 on failure
 */
int nimcp_flocking_process_messages(nimcp_flocking_engine_t *engine);

/**
 * @brief Broadcast boid state to neighbors
 *
 * @param engine Flocking engine
 * @param boid_id Boid ID
 * @return 0 on success, -1 on failure
 */
int nimcp_flocking_broadcast_state(nimcp_flocking_engine_t *engine, uint32_t boid_id);

/* ========================================================================
 * Vector Utilities
 * ======================================================================== */

/**
 * @brief Create a 3D vector
 */
static inline nimcp_vec3_t nimcp_vec3_create(float x, float y, float z) {
    nimcp_vec3_t v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

/**
 * @brief Add two vectors
 */
static inline nimcp_vec3_t nimcp_vec3_add(nimcp_vec3_t a, nimcp_vec3_t b) {
    return nimcp_vec3_create(a.x + b.x, a.y + b.y, a.z + b.z);
}

/**
 * @brief Subtract two vectors
 */
static inline nimcp_vec3_t nimcp_vec3_sub(nimcp_vec3_t a, nimcp_vec3_t b) {
    return nimcp_vec3_create(a.x - b.x, a.y - b.y, a.z - b.z);
}

/**
 * @brief Multiply vector by scalar
 */
static inline nimcp_vec3_t nimcp_vec3_mul(nimcp_vec3_t v, float s) {
    return nimcp_vec3_create(v.x * s, v.y * s, v.z * s);
}

/**
 * @brief Divide vector by scalar
 */
static inline nimcp_vec3_t nimcp_vec3_div(nimcp_vec3_t v, float s) {
    if (s == 0.0f) return nimcp_vec3_create(0, 0, 0);
    return nimcp_vec3_create(v.x / s, v.y / s, v.z / s);
}

/**
 * @brief Calculate vector length
 */
float nimcp_vec3_length(nimcp_vec3_t v);

/**
 * @brief Calculate squared vector length (faster)
 */
static inline float nimcp_vec3_length_squared(nimcp_vec3_t v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

/**
 * @brief Normalize vector
 */
nimcp_vec3_t nimcp_vec3_normalize(nimcp_vec3_t v);

/**
 * @brief Calculate distance between two points
 */
float nimcp_vec3_distance(nimcp_vec3_t a, nimcp_vec3_t b);

/**
 * @brief Calculate dot product
 */
static inline float nimcp_vec3_dot(nimcp_vec3_t a, nimcp_vec3_t b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

/**
 * @brief Limit vector magnitude
 */
nimcp_vec3_t nimcp_vec3_limit(nimcp_vec3_t v, float max);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_FLOCKING_H */
