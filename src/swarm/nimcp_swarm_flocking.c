/**
 * @file nimcp_swarm_flocking.c
 * @brief Implementation of Flocking Dynamics Engine for NIMCP Swarms
 *
 * Implements Reynolds' boids algorithm with separation, alignment, and cohesion.
 * Includes advanced behaviors: obstacle avoidance, predator evasion, goal seeking,
 * and formation templates inspired by biological swarms.
 */

#include "swarm/nimcp_swarm_flocking.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/memory/nimcp_memory.h"
#include "api/nimcp_api_exception.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_flocking)

/* ========================================================================
 * Constants and Defaults
 * ======================================================================== */

#define NIMCP_FLOCKING_DEFAULT_BOID_CAPACITY 1000
#define NIMCP_FLOCKING_DEFAULT_OBSTACLE_CAPACITY 100
#define NIMCP_FLOCKING_DEFAULT_NEIGHBOR_CAPACITY 20
#include "constants/nimcp_constants.h"
#define NIMCP_FLOCKING_EPSILON NIMCP_EPSILON_NUMERICAL

/* ========================================================================
 * Internal Helper Functions
 * ======================================================================== */

/**
 * @brief Find boid index by ID
 */
static int flocking_find_boid_index(const nimcp_flocking_engine_t *engine, uint32_t boid_id) {
    if (!engine || !engine->boids) {
        return -1;  /* Normal search miss on uninitialized engine */
    }

    for (uint32_t i = 0; i < engine->boid_count; i++) {
        if (engine->boids[i].id == boid_id) {
            return (int)i;
        }
    }
    return -1;  /* Not found - normal search miss */
}

/**
 * @brief Calculate formation position for V-formation
 */
static void calculate_v_formation_position(const nimcp_flocking_engine_t *engine,
                                          uint32_t boid_index,
                                          nimcp_vec3_t *position) {
    if (!engine || !position) return;

    const float spacing = engine->config.formation_spacing;
    const int row = boid_index / 2;
    const int side = (boid_index % 2 == 0) ? -1 : 1;

    position->x = engine->formation_center.x + side * row * spacing;
    position->y = engine->formation_center.y;
    position->z = engine->formation_center.z - row * spacing * 0.5F;
}

/**
 * @brief Calculate formation position for sphere formation
 */
static void calculate_sphere_formation_position(const nimcp_flocking_engine_t *engine,
                                               uint32_t boid_index,
                                               nimcp_vec3_t *position) {
    if (!engine || !position) return;

    const float radius = engine->config.formation_spacing * 2.0F;
    const float phi = acosf(1.0F - 2.0F * (boid_index + 0.5F) / engine->boid_count);
    const float theta = M_PI * (1.0F + sqrtf(5.0F)) * boid_index;

    position->x = engine->formation_center.x + radius * sinf(phi) * cosf(theta);
    position->y = engine->formation_center.y + radius * sinf(phi) * sinf(theta);
    position->z = engine->formation_center.z + radius * cosf(phi);
}

/**
 * @brief Calculate formation position for line formation
 */
static void calculate_line_formation_position(const nimcp_flocking_engine_t *engine,
                                             uint32_t boid_index,
                                             nimcp_vec3_t *position) {
    if (!engine || !position) return;

    const float spacing = engine->config.formation_spacing;

    position->x = engine->formation_center.x;
    position->y = engine->formation_center.y;
    position->z = engine->formation_center.z - boid_index * spacing;
}

/**
 * @brief Calculate formation position for grid formation
 */
static void calculate_grid_formation_position(const nimcp_flocking_engine_t *engine,
                                             uint32_t boid_index,
                                             nimcp_vec3_t *position) {
    if (!engine || !position) return;

    const float spacing = engine->config.formation_spacing;
    const uint32_t grid_size = (uint32_t)ceilf(sqrtf((float)engine->boid_count));
    const uint32_t row = boid_index / grid_size;
    const uint32_t col = boid_index % grid_size;

    position->x = engine->formation_center.x + (col - grid_size / 2.0F) * spacing;
    position->y = engine->formation_center.y;
    position->z = engine->formation_center.z + (row - grid_size / 2.0F) * spacing;
}

/**
 * @brief Calculate formation position for spiral formation
 */
static void calculate_spiral_formation_position(const nimcp_flocking_engine_t *engine,
                                               uint32_t boid_index,
                                               nimcp_vec3_t *position) {
    if (!engine || !position) return;

    const float spacing = engine->config.formation_spacing;
    const float angle = boid_index * 0.5F;
    const float radius = boid_index * spacing * 0.2F;

    position->x = engine->formation_center.x + radius * cosf(angle);
    position->y = engine->formation_center.y;
    position->z = engine->formation_center.z + radius * sinf(angle);
}

/**
 * @brief Calculate formation position for wedge formation
 */
static void calculate_wedge_formation_position(const nimcp_flocking_engine_t *engine,
                                              uint32_t boid_index,
                                              nimcp_vec3_t *position) {
    if (!engine || !position) return;

    const float spacing = engine->config.formation_spacing;
    const int row = (int)sqrtf((float)boid_index * 2);
    const int pos_in_row = boid_index - (row * (row + 1)) / 2;

    position->x = engine->formation_center.x + (pos_in_row - row / 2.0F) * spacing;
    position->y = engine->formation_center.y;
    position->z = engine->formation_center.z - row * spacing;
}

/* ========================================================================
 * Vector Utilities Implementation
 * ======================================================================== */

static float flocking_vec3_length(nimcp_vec3_t v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

static nimcp_vec3_t flocking_vec3_normalize(nimcp_vec3_t v) {
    float len = flocking_vec3_length(v);
    if (len < NIMCP_FLOCKING_EPSILON) {
        return nimcp_vec3_create(0, 0, 0);
    }
    return nimcp_vec3_div(v, len);
}

/* Public wrappers matching header declarations */
float nimcp_vec3_length(nimcp_vec3_t v) {
    return flocking_vec3_length(v);
}

nimcp_vec3_t nimcp_vec3_normalize(nimcp_vec3_t v) {
    return flocking_vec3_normalize(v);
}

float nimcp_vec3_distance(nimcp_vec3_t a, nimcp_vec3_t b) {
    return flocking_vec3_length(nimcp_vec3_sub(a, b));
}

nimcp_vec3_t nimcp_vec3_limit(nimcp_vec3_t v, float max) {
    float len = flocking_vec3_length(v);
    if (len > max) {
        return nimcp_vec3_mul(flocking_vec3_normalize(v), max);
    }
    return v;
}

/* ========================================================================
 * Creation and Destruction
 * ======================================================================== */

void nimcp_flocking_get_default_config(nimcp_flocking_config_t *config) {
    if (!config) return;

    memset(config, 0, sizeof(nimcp_flocking_config_t));

    // Core flocking weights
    config->separation_weight = 1.5F;
    config->alignment_weight = 1.0F;
    config->cohesion_weight = 1.0F;

    // Radii
    config->separation_radius = 2.0F;
    config->alignment_radius = 5.0F;
    config->cohesion_radius = 5.0F;

    // Speed and force limits
    config->max_speed = 5.0F;
    config->max_force = 0.5F;
    config->max_acceleration = 2.0F;

    // Advanced behavior weights
    config->obstacle_weight = 2.0F;
    config->goal_weight = 1.0F;
    config->boundary_weight = 1.5F;
    config->predator_weight = 3.0F;

    // Obstacle avoidance
    config->obstacle_detect_radius = 10.0F;
    config->obstacle_avoid_distance = 3.0F;

    // Boundary (default: large box)
    config->boundary_min = nimcp_vec3_create(-100.0F, -100.0F, -100.0F);
    config->boundary_max = nimcp_vec3_create(100.0F, 100.0F, 100.0F);
    config->boundary_margin = 10.0F;

    // Formation
    config->formation_tightness = 1.0F;
    config->formation_spacing = 3.0F;

    // Neighbor detection
    config->max_neighbors = 10;

    // Update rate (60 FPS)
    config->update_dt = 0.016F;
}

nimcp_flocking_engine_t *nimcp_flocking_create(const nimcp_flocking_config_t *config) {
    LOG_INFO("Creating flocking engine");

    nimcp_flocking_engine_t *engine = (nimcp_flocking_engine_t *)nimcp_calloc(1, sizeof(nimcp_flocking_engine_t));
    if (!engine) {
        LOG_ERROR("Failed to allocate flocking engine");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate flocking engine");
        return NULL;
    }

    // Set configuration
    if (config) {
        memcpy(&engine->config, config, sizeof(nimcp_flocking_config_t));
    } else {
        nimcp_flocking_get_default_config(&engine->config);
    }

    // Allocate boid array
    engine->boid_capacity = NIMCP_FLOCKING_DEFAULT_BOID_CAPACITY;
    engine->boids = (nimcp_boid_t *)nimcp_calloc(engine->boid_capacity, sizeof(nimcp_boid_t));
    if (!engine->boids) {
        LOG_ERROR("Failed to allocate boid array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate boid array");
        nimcp_free(engine);
        return NULL;
    }

    // Allocate obstacle array
    engine->obstacle_capacity = NIMCP_FLOCKING_DEFAULT_OBSTACLE_CAPACITY;
    engine->obstacles = (nimcp_obstacle_t *)nimcp_calloc(engine->obstacle_capacity, sizeof(nimcp_obstacle_t));
    if (!engine->obstacles) {
        LOG_ERROR("Failed to allocate obstacle array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate obstacle array");
        nimcp_free(engine->boids);
        nimcp_free(engine);
        return NULL;
    }

    // Create mutex
    engine->mutex = nimcp_platform_mutex_create();
    if (!engine->mutex) {
        LOG_ERROR("Failed to create mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to create flocking mutex");
        nimcp_free(engine->obstacles);
        nimcp_free(engine->boids);
        nimcp_free(engine);
        return NULL;
    }

    // Initialize state
    engine->state = NIMCP_FLOCK_IDLE;
    engine->formation_type = NIMCP_FORMATION_NONE;
    engine->has_goal = false;
    engine->has_predator = false;
    engine->bio_async_enabled = false;
    engine->update_count = 0;

    LOG_INFO("Flocking engine created successfully");
    return engine;
}

void nimcp_flocking_destroy(nimcp_flocking_engine_t *engine) {
    if (!engine) return;

    LOG_INFO("Destroying flocking engine");

    // Free boid neighbor arrays
    if (engine->boids) {
        for (uint32_t i = 0; i < engine->boid_count; i++) {
            if (engine->boids[i].neighbor_ids) {
                nimcp_free(engine->boids[i].neighbor_ids);
            }
        }
        nimcp_free(engine->boids);
    }

    if (engine->obstacles) {
        nimcp_free(engine->obstacles);
    }

    if (engine->mutex) {
        nimcp_platform_mutex_destroy(engine->mutex);
        nimcp_free(engine->mutex);
        engine->mutex = NULL;
    }

    nimcp_free(engine);
    LOG_INFO("Flocking engine destroyed");
}

/* ========================================================================
 * Boid Management
 * ======================================================================== */

uint32_t nimcp_flocking_add_boid(nimcp_flocking_engine_t *engine,
                                 const nimcp_vec3_t *position,
                                 const nimcp_vec3_t *velocity) {
    if (!engine || !position || !velocity) {
        LOG_ERROR("Invalid parameters for add_boid");
        return 0;
    }

    nimcp_platform_mutex_lock(engine->mutex);

    // Check capacity
    if (engine->boid_count >= engine->boid_capacity) {
        // Check for overflow before doubling capacity
        if (engine->boid_capacity > UINT32_MAX / 2) {
            LOG_ERROR("Boid capacity overflow");
            nimcp_platform_mutex_unlock(engine->mutex);
            return 0;
        }
        // Reallocate
        uint32_t new_capacity = engine->boid_capacity * 2;
        // Check for allocation size overflow
        if (new_capacity > SIZE_MAX / sizeof(nimcp_boid_t)) {
            LOG_ERROR("Boid array size overflow");
            nimcp_platform_mutex_unlock(engine->mutex);
            return 0;
        }
        nimcp_boid_t *new_boids = (nimcp_boid_t *)nimcp_realloc(engine->boids,
                                                          new_capacity * sizeof(nimcp_boid_t));
        if (!new_boids) {
            LOG_ERROR("Failed to reallocate boid array");
            nimcp_platform_mutex_unlock(engine->mutex);
            return 0;
        }
        engine->boids = new_boids;
        engine->boid_capacity = new_capacity;

        // Zero new memory
        memset(&engine->boids[engine->boid_count], 0,
               (new_capacity - engine->boid_count) * sizeof(nimcp_boid_t));
    }

    // Create new boid
    uint32_t boid_id = engine->boid_count + 1;
    nimcp_boid_t *boid = &engine->boids[engine->boid_count];

    boid->id = boid_id;
    boid->position = *position;
    boid->velocity = *velocity;
    boid->acceleration = nimcp_vec3_create(0, 0, 0);

    boid->mass = 1.0F;
    boid->max_speed = engine->config.max_speed;
    boid->max_force = engine->config.max_force;

    // Allocate neighbor array
    boid->neighbor_capacity = NIMCP_FLOCKING_DEFAULT_NEIGHBOR_CAPACITY;
    boid->neighbor_ids = (uint32_t *)nimcp_calloc(boid->neighbor_capacity, sizeof(uint32_t));
    if (!boid->neighbor_ids) {
        LOG_ERROR("Failed to allocate neighbor array");
        nimcp_platform_mutex_unlock(engine->mutex);
        return 0;
    }
    boid->neighbor_count = 0;

    boid->is_leader = false;
    boid->follow_id = 0;
    boid->user_data = NULL;

    engine->boid_count++;

    nimcp_platform_mutex_unlock(engine->mutex);

    LOG_DEBUG("Added boid %u at position (%.2f, %.2f, %.2f)",
                    boid_id, position->x, position->y, position->z);

    return boid_id;
}

int nimcp_flocking_remove_boid(nimcp_flocking_engine_t *engine, uint32_t boid_id) {
    if (!engine || boid_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_flocking_remove_boid: engine is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(engine->mutex);

    int index = flocking_find_boid_index(engine, boid_id);
    if (index < 0) {
        nimcp_platform_mutex_unlock(engine->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_flocking_remove_boid: validation failed");
        return -1;
    }

    // Free neighbor array
    if (engine->boids[index].neighbor_ids) {
        nimcp_free(engine->boids[index].neighbor_ids);
    }

    // Shift remaining boids
    if ((uint32_t)index < engine->boid_count - 1) {
        memmove(&engine->boids[index], &engine->boids[index + 1],
                (engine->boid_count - index - 1) * sizeof(nimcp_boid_t));
    }

    engine->boid_count--;

    nimcp_platform_mutex_unlock(engine->mutex);

    LOG_DEBUG("Removed boid %u", boid_id);
    return 0;
}

nimcp_boid_t *nimcp_flocking_get_boid(nimcp_flocking_engine_t *engine, uint32_t boid_id) {
    if (!engine || boid_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_flocking_get_boid: engine is NULL");
        return NULL;
    }

    int index = flocking_find_boid_index(engine, boid_id);
    if (index < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_flocking_get_boid: validation failed");
        return NULL;
    }

    return &engine->boids[index];
}

int nimcp_flocking_update_boid(nimcp_flocking_engine_t *engine,
                               uint32_t boid_id,
                               const nimcp_vec3_t *position,
                               const nimcp_vec3_t *velocity) {
    if (!engine || boid_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_flocking_update_boid: engine is NULL or boid_id is 0");
        return -1;
    }

    nimcp_platform_mutex_lock(engine->mutex);

    nimcp_boid_t *boid = nimcp_flocking_get_boid(engine, boid_id);
    if (!boid) {
        nimcp_platform_mutex_unlock(engine->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_flocking_update_boid: boid is NULL");
        return -1;
    }

    if (position) {
        boid->position = *position;
    }
    if (velocity) {
        boid->velocity = *velocity;
    }

    nimcp_platform_mutex_unlock(engine->mutex);
    return 0;
}

/* ========================================================================
 * Obstacle Management
 * ======================================================================== */

uint32_t nimcp_flocking_add_obstacle(nimcp_flocking_engine_t *engine,
                                     const nimcp_vec3_t *position,
                                     float radius) {
    if (!engine || !position || radius <= 0) {
        return 0;
    }

    nimcp_platform_mutex_lock(engine->mutex);

    if (engine->obstacle_count >= engine->obstacle_capacity) {
        uint32_t new_capacity = engine->obstacle_capacity * 2;
        nimcp_obstacle_t *new_obstacles = (nimcp_obstacle_t *)nimcp_realloc(
            engine->obstacles, new_capacity * sizeof(nimcp_obstacle_t));
        if (!new_obstacles) {
            nimcp_platform_mutex_unlock(engine->mutex);
            return 0;
        }
        engine->obstacles = new_obstacles;
        engine->obstacle_capacity = new_capacity;

        memset(&engine->obstacles[engine->obstacle_count], 0,
               (new_capacity - engine->obstacle_count) * sizeof(nimcp_obstacle_t));
    }

    uint32_t obstacle_id = engine->obstacle_count + 1;
    nimcp_obstacle_t *obstacle = &engine->obstacles[engine->obstacle_count];

    obstacle->position = *position;
    obstacle->radius = radius;
    obstacle->is_active = true;

    engine->obstacle_count++;

    nimcp_platform_mutex_unlock(engine->mutex);

    LOG_DEBUG("Added obstacle %u at (%.2f, %.2f, %.2f) radius %.2f",
                    obstacle_id, position->x, position->y, position->z, radius);

    return obstacle_id;
}

int nimcp_flocking_remove_obstacle(nimcp_flocking_engine_t *engine, uint32_t obstacle_id) {
    if (!engine || obstacle_id == 0 || obstacle_id > engine->obstacle_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_flocking_remove_obstacle: engine is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(engine->mutex);

    uint32_t index = obstacle_id - 1;

    if (index < engine->obstacle_count - 1) {
        memmove(&engine->obstacles[index], &engine->obstacles[index + 1],
                (engine->obstacle_count - index - 1) * sizeof(nimcp_obstacle_t));
    }

    engine->obstacle_count--;

    nimcp_platform_mutex_unlock(engine->mutex);
    return 0;
}

int nimcp_flocking_update_obstacle(nimcp_flocking_engine_t *engine,
                                   uint32_t obstacle_id,
                                   const nimcp_vec3_t *position) {
    if (!engine || !position || obstacle_id == 0 || obstacle_id > engine->obstacle_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_flocking_update_obstacle: required parameter is NULL (engine, position)");
        return -1;
    }

    nimcp_platform_mutex_lock(engine->mutex);
    engine->obstacles[obstacle_id - 1].position = *position;
    nimcp_platform_mutex_unlock(engine->mutex);

    return 0;
}

/* ========================================================================
 * Goal and Predator
 * ======================================================================== */

void nimcp_flocking_set_goal(nimcp_flocking_engine_t *engine, const nimcp_vec3_t *position) {
    if (!engine || !position) return;

    nimcp_platform_mutex_lock(engine->mutex);
    engine->goal_position = *position;
    engine->has_goal = true;
    engine->state = NIMCP_FLOCK_SEEKING;
    nimcp_platform_mutex_unlock(engine->mutex);

    LOG_DEBUG("Set goal at (%.2f, %.2f, %.2f)", position->x, position->y, position->z);
}

void nimcp_flocking_clear_goal(nimcp_flocking_engine_t *engine) {
    if (!engine) return;

    nimcp_platform_mutex_lock(engine->mutex);
    engine->has_goal = false;
    if (engine->state == NIMCP_FLOCK_SEEKING) {
        engine->state = NIMCP_FLOCK_ACTIVE;
    }
    nimcp_platform_mutex_unlock(engine->mutex);
}

void nimcp_flocking_set_predator(nimcp_flocking_engine_t *engine,
                                 const nimcp_vec3_t *position,
                                 float radius) {
    if (!engine || !position) return;

    nimcp_platform_mutex_lock(engine->mutex);
    engine->predator_position = *position;
    engine->predator_radius = radius;
    engine->has_predator = true;
    engine->state = NIMCP_FLOCK_EVADING;
    nimcp_platform_mutex_unlock(engine->mutex);

    LOG_DEBUG("Set predator at (%.2f, %.2f, %.2f) radius %.2f",
                    position->x, position->y, position->z, radius);
}

void nimcp_flocking_clear_predator(nimcp_flocking_engine_t *engine) {
    if (!engine) return;

    nimcp_platform_mutex_lock(engine->mutex);
    engine->has_predator = false;
    if (engine->state == NIMCP_FLOCK_EVADING) {
        engine->state = NIMCP_FLOCK_ACTIVE;
    }
    nimcp_platform_mutex_unlock(engine->mutex);
}

/* ========================================================================
 * Formation Control
 * ======================================================================== */

int nimcp_flocking_set_formation(nimcp_flocking_engine_t *engine,
                                 nimcp_formation_type_t formation,
                                 uint32_t leader_id) {
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_flocking_set_formation: engine is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(engine->mutex);

    engine->formation_type = formation;

    // Auto-select leader if not provided
    if (leader_id == 0 && engine->boid_count > 0) {
        leader_id = engine->boids[0].id;
    }

    engine->leader_id = leader_id;

    // Mark leader
    for (uint32_t i = 0; i < engine->boid_count; i++) {
        engine->boids[i].is_leader = (engine->boids[i].id == leader_id);
    }

    // Calculate formation center from leader or flock center
    if (leader_id != 0) {
        nimcp_boid_t *leader = nimcp_flocking_get_boid(engine, leader_id);
        if (leader) {
            engine->formation_center = leader->position;
        }
    } else {
        nimcp_flocking_center_of_mass(engine, &engine->formation_center);
    }

    engine->state = NIMCP_FLOCK_FORMING;

    nimcp_platform_mutex_unlock(engine->mutex);

    LOG_INFO("Set formation type %d with leader %u", formation, leader_id);
    return 0;
}

void nimcp_flocking_clear_formation(nimcp_flocking_engine_t *engine) {
    if (!engine) return;

    nimcp_platform_mutex_lock(engine->mutex);
    engine->formation_type = NIMCP_FORMATION_NONE;
    engine->leader_id = 0;

    for (uint32_t i = 0; i < engine->boid_count; i++) {
        engine->boids[i].is_leader = false;
        engine->boids[i].follow_id = 0;
    }

    engine->state = NIMCP_FLOCK_ACTIVE;
    nimcp_platform_mutex_unlock(engine->mutex);
}

/* Internal unlocked version for use within already-locked contexts */
static int flocking_get_formation_position_unlocked(nimcp_flocking_engine_t *engine,
                                                     uint32_t boid_id,
                                                     nimcp_vec3_t *position) {
    if (!engine || !position || boid_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "flocking_get_formation_position_unlocked: required parameter is NULL (engine, position)");
        return -1;
    }

    int index = flocking_find_boid_index(engine, boid_id);
    if (index < 0) {
        return -1;  /* Boid not found */
    }

    switch (engine->formation_type) {
        case NIMCP_FORMATION_V:
            calculate_v_formation_position(engine, index, position);
            break;
        case NIMCP_FORMATION_SPHERE:
            calculate_sphere_formation_position(engine, index, position);
            break;
        case NIMCP_FORMATION_LINE:
            calculate_line_formation_position(engine, index, position);
            break;
        case NIMCP_FORMATION_GRID:
            calculate_grid_formation_position(engine, index, position);
            break;
        case NIMCP_FORMATION_SPIRAL:
            calculate_spiral_formation_position(engine, index, position);
            break;
        case NIMCP_FORMATION_WEDGE:
            calculate_wedge_formation_position(engine, index, position);
            break;
        default:
            *position = engine->boids[index].position;
            break;
    }

    return 0;
}

int nimcp_flocking_get_formation_position(nimcp_flocking_engine_t *engine,
                                          uint32_t boid_id,
                                          nimcp_vec3_t *position) {
    if (!engine || !position || boid_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_flocking_get_formation_position: required parameter is NULL (engine, position)");
        return -1;
    }

    nimcp_platform_mutex_lock(engine->mutex);
    int result = flocking_get_formation_position_unlocked(engine, boid_id, position);
    nimcp_platform_mutex_unlock(engine->mutex);

    return result;
}

/* ========================================================================
 * Neighbor Detection
 * ======================================================================== */

int nimcp_flocking_find_neighbors(nimcp_flocking_engine_t *engine,
                                  uint32_t boid_id,
                                  float radius) {
    if (!engine || boid_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_flocking_find_neighbors: engine is NULL or boid_id is 0");
        return -1;
    }

    nimcp_boid_t *boid = nimcp_flocking_get_boid(engine, boid_id);
    if (!boid) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_flocking_find_neighbors: boid is NULL");
        return -1;
    }

    boid->neighbor_count = 0;
    float radius_sq = radius * radius;

    for (uint32_t i = 0; i < engine->boid_count; i++) {
        if (engine->boids[i].id == boid_id) continue;

        float dist_sq = nimcp_vec3_length_squared(
            nimcp_vec3_sub(engine->boids[i].position, boid->position));

        if (dist_sq <= radius_sq) {
            if (boid->neighbor_count >= boid->neighbor_capacity) {
                // Reallocate
                uint32_t new_capacity = boid->neighbor_capacity * 2;
                uint32_t *new_neighbors = (uint32_t *)nimcp_realloc(
                    boid->neighbor_ids, new_capacity * sizeof(uint32_t));
                if (!new_neighbors) {
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_flocking_find_neighbors: realloc failed for neighbor_ids");
                    return -1;
                }
                boid->neighbor_ids = new_neighbors;
                boid->neighbor_capacity = new_capacity;
            }

            boid->neighbor_ids[boid->neighbor_count++] = engine->boids[i].id;

            if (boid->neighbor_count >= engine->config.max_neighbors) {
                break;
            }
        }
    }

    return 0;
}

/* ========================================================================
 * Core Flocking Rules
 * ======================================================================== */

int nimcp_flocking_separation(nimcp_flocking_engine_t *engine,
                              const nimcp_boid_t *boid,
                              nimcp_vec3_t *force) {
    if (!engine || !boid || !force) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_flocking_separation: required parameter is NULL (engine, boid, force)");
        return -1;
    }

    *force = nimcp_vec3_create(0, 0, 0);

    int count = 0;
    float radius_sq = engine->config.separation_radius * engine->config.separation_radius;

    for (uint32_t i = 0; i < engine->boid_count; i++) {
        if (engine->boids[i].id == boid->id) continue;

        nimcp_vec3_t diff = nimcp_vec3_sub(boid->position, engine->boids[i].position);
        float dist_sq = nimcp_vec3_length_squared(diff);

        if (dist_sq > 0 && dist_sq < radius_sq) {
            float dist = sqrtf(dist_sq);
            // Weight by distance (closer = stronger repulsion)
            nimcp_vec3_t normalized = nimcp_vec3_div(diff, dist);
            normalized = nimcp_vec3_div(normalized, dist);
            *force = nimcp_vec3_add(*force, normalized);
            count++;
        }
    }

    if (count > 0) {
        *force = nimcp_vec3_div(*force, (float)count);

        // Steering force
        float len = nimcp_vec3_length(*force);
        if (len > NIMCP_FLOCKING_EPSILON) {
            *force = nimcp_vec3_mul(nimcp_vec3_normalize(*force), boid->max_speed);
            *force = nimcp_vec3_sub(*force, boid->velocity);
            *force = nimcp_vec3_limit(*force, boid->max_force);
        }
    }

    return 0;
}

int nimcp_flocking_alignment(nimcp_flocking_engine_t *engine,
                             const nimcp_boid_t *boid,
                             nimcp_vec3_t *force) {
    if (!engine || !boid || !force) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_flocking_alignment: required parameter is NULL (engine, boid, force)");
        return -1;
    }

    *force = nimcp_vec3_create(0, 0, 0);

    int count = 0;
    float radius_sq = engine->config.alignment_radius * engine->config.alignment_radius;

    for (uint32_t i = 0; i < engine->boid_count; i++) {
        if (engine->boids[i].id == boid->id) continue;

        float dist_sq = nimcp_vec3_length_squared(
            nimcp_vec3_sub(engine->boids[i].position, boid->position));

        if (dist_sq < radius_sq) {
            *force = nimcp_vec3_add(*force, engine->boids[i].velocity);
            count++;
        }
    }

    if (count > 0) {
        *force = nimcp_vec3_div(*force, (float)count);

        // Steering force
        float len = nimcp_vec3_length(*force);
        if (len > NIMCP_FLOCKING_EPSILON) {
            *force = nimcp_vec3_mul(nimcp_vec3_normalize(*force), boid->max_speed);
            *force = nimcp_vec3_sub(*force, boid->velocity);
            *force = nimcp_vec3_limit(*force, boid->max_force);
        }
    }

    return 0;
}

int nimcp_flocking_cohesion(nimcp_flocking_engine_t *engine,
                            const nimcp_boid_t *boid,
                            nimcp_vec3_t *force) {
    if (!engine || !boid || !force) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_flocking_cohesion: required parameter is NULL (engine, boid, force)");
        return -1;
    }

    *force = nimcp_vec3_create(0, 0, 0);

    int count = 0;
    float radius_sq = engine->config.cohesion_radius * engine->config.cohesion_radius;
    nimcp_vec3_t center = nimcp_vec3_create(0, 0, 0);

    for (uint32_t i = 0; i < engine->boid_count; i++) {
        if (engine->boids[i].id == boid->id) continue;

        float dist_sq = nimcp_vec3_length_squared(
            nimcp_vec3_sub(engine->boids[i].position, boid->position));

        if (dist_sq < radius_sq) {
            center = nimcp_vec3_add(center, engine->boids[i].position);
            count++;
        }
    }

    if (count > 0) {
        center = nimcp_vec3_div(center, (float)count);

        // Seek towards center
        nimcp_vec3_t desired = nimcp_vec3_sub(center, boid->position);
        float len = nimcp_vec3_length(desired);

        if (len > NIMCP_FLOCKING_EPSILON) {
            desired = nimcp_vec3_mul(nimcp_vec3_normalize(desired), boid->max_speed);
            *force = nimcp_vec3_sub(desired, boid->velocity);
            *force = nimcp_vec3_limit(*force, boid->max_force);
        }
    }

    return 0;
}

/* ========================================================================
 * Advanced Behaviors
 * ======================================================================== */

int nimcp_flocking_obstacle_avoidance(nimcp_flocking_engine_t *engine,
                                      const nimcp_boid_t *boid,
                                      nimcp_vec3_t *force) {
    if (!engine || !boid || !force) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_flocking_obstacle_avoidance: required parameter is NULL (engine, boid, force)");
        return -1;
    }

    *force = nimcp_vec3_create(0, 0, 0);

    for (uint32_t i = 0; i < engine->obstacle_count; i++) {
        if (!engine->obstacles[i].is_active) continue;

        nimcp_vec3_t diff = nimcp_vec3_sub(boid->position, engine->obstacles[i].position);
        float dist = nimcp_vec3_length(diff);
        float min_dist = engine->obstacles[i].radius + engine->config.obstacle_avoid_distance;

        if (dist < engine->config.obstacle_detect_radius) {
            // Predictive avoidance
            nimcp_vec3_t future_pos = nimcp_vec3_add(boid->position,
                                                     nimcp_vec3_mul(boid->velocity, 2.0F));
            float future_dist = nimcp_vec3_distance(future_pos, engine->obstacles[i].position);

            if (future_dist < min_dist || dist < min_dist) {
                // Avoidance force inversely proportional to distance
                float strength = (min_dist - dist) / min_dist;
                strength = fmaxf(0.0F, fminf(1.0F, strength));

                nimcp_vec3_t avoid = nimcp_vec3_normalize(diff);
                avoid = nimcp_vec3_mul(avoid, boid->max_speed * (1.0F + strength));

                *force = nimcp_vec3_add(*force, avoid);
            }
        }
    }

    if (nimcp_vec3_length(*force) > NIMCP_FLOCKING_EPSILON) {
        *force = nimcp_vec3_limit(*force, boid->max_force);
    }

    return 0;
}

int nimcp_flocking_goal_seek(nimcp_flocking_engine_t *engine,
                             const nimcp_boid_t *boid,
                             nimcp_vec3_t *force) {
    if (!engine || !boid || !force) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_flocking_goal_seek: required parameter is NULL (engine, boid, force)");
        return -1;
    }

    *force = nimcp_vec3_create(0, 0, 0);

    if (!engine->has_goal) return 0;

    nimcp_vec3_t desired = nimcp_vec3_sub(engine->goal_position, boid->position);
    float dist = nimcp_vec3_length(desired);

    if (dist > NIMCP_FLOCKING_EPSILON) {
        desired = nimcp_vec3_normalize(desired);

        // Slow down when approaching goal
        if (dist < 10.0F) {
            float speed = boid->max_speed * (dist / 10.0F);
            desired = nimcp_vec3_mul(desired, speed);
        } else {
            desired = nimcp_vec3_mul(desired, boid->max_speed);
        }

        *force = nimcp_vec3_sub(desired, boid->velocity);
        *force = nimcp_vec3_limit(*force, boid->max_force);
    }

    return 0;
}

int nimcp_flocking_boundary_containment(nimcp_flocking_engine_t *engine,
                                        const nimcp_boid_t *boid,
                                        nimcp_vec3_t *force) {
    if (!engine || !boid || !force) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_flocking_boundary_containment: required parameter is NULL (engine, boid, force)");
        return -1;
    }

    *force = nimcp_vec3_create(0, 0, 0);

    const float margin = engine->config.boundary_margin;

    // Check each axis
    if (boid->position.x < engine->config.boundary_min.x + margin) {
        force->x = boid->max_speed;
    } else if (boid->position.x > engine->config.boundary_max.x - margin) {
        force->x = -boid->max_speed;
    }

    if (boid->position.y < engine->config.boundary_min.y + margin) {
        force->y = boid->max_speed;
    } else if (boid->position.y > engine->config.boundary_max.y - margin) {
        force->y = -boid->max_speed;
    }

    if (boid->position.z < engine->config.boundary_min.z + margin) {
        force->z = boid->max_speed;
    } else if (boid->position.z > engine->config.boundary_max.z - margin) {
        force->z = -boid->max_speed;
    }

    if (nimcp_vec3_length(*force) > NIMCP_FLOCKING_EPSILON) {
        *force = nimcp_vec3_limit(*force, boid->max_force);
    }

    return 0;
}

int nimcp_flocking_predator_evasion(nimcp_flocking_engine_t *engine,
                                    const nimcp_boid_t *boid,
                                    nimcp_vec3_t *force) {
    if (!engine || !boid || !force) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_flocking_predator_evasion: required parameter is NULL (engine, boid, force)");
        return -1;
    }

    *force = nimcp_vec3_create(0, 0, 0);

    if (!engine->has_predator) return 0;

    float dist = nimcp_vec3_distance(boid->position, engine->predator_position);

    if (dist < engine->predator_radius) {
        // Flee from predator
        nimcp_vec3_t desired = nimcp_vec3_sub(boid->position, engine->predator_position);
        desired = nimcp_vec3_normalize(desired);

        // Panic response: max speed
        float panic = 1.0F - (dist / engine->predator_radius);
        float speed = boid->max_speed * (1.0F + panic * 2.0F);

        desired = nimcp_vec3_mul(desired, speed);
        *force = nimcp_vec3_sub(desired, boid->velocity);
        *force = nimcp_vec3_limit(*force, boid->max_force * 2.0F);  // Extra force for evasion
    }

    return 0;
}

/* ========================================================================
 * Update and Simulation
 * ======================================================================== */

int nimcp_flocking_calculate_force(nimcp_flocking_engine_t *engine,
                                   uint32_t boid_id,
                                   nimcp_vec3_t *force) {
    if (!engine || !force || boid_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_flocking_calculate_force: required parameter is NULL (engine, force)");
        return -1;
    }

    nimcp_boid_t *boid = nimcp_flocking_get_boid(engine, boid_id);
    if (!boid) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_flocking_calculate_force: boid is NULL");
        return -1;
    }

    *force = nimcp_vec3_create(0, 0, 0);

    // Core flocking rules
    nimcp_vec3_t sep_force, align_force, coh_force;
    nimcp_flocking_separation(engine, boid, &sep_force);
    nimcp_flocking_alignment(engine, boid, &align_force);
    nimcp_flocking_cohesion(engine, boid, &coh_force);

    *force = nimcp_vec3_add(*force, nimcp_vec3_mul(sep_force, engine->config.separation_weight));
    *force = nimcp_vec3_add(*force, nimcp_vec3_mul(align_force, engine->config.alignment_weight));
    *force = nimcp_vec3_add(*force, nimcp_vec3_mul(coh_force, engine->config.cohesion_weight));

    // Advanced behaviors
    nimcp_vec3_t obs_force, goal_force, bound_force, pred_force;

    nimcp_flocking_obstacle_avoidance(engine, boid, &obs_force);
    *force = nimcp_vec3_add(*force, nimcp_vec3_mul(obs_force, engine->config.obstacle_weight));

    if (engine->has_goal) {
        nimcp_flocking_goal_seek(engine, boid, &goal_force);
        *force = nimcp_vec3_add(*force, nimcp_vec3_mul(goal_force, engine->config.goal_weight));
    }

    nimcp_flocking_boundary_containment(engine, boid, &bound_force);
    *force = nimcp_vec3_add(*force, nimcp_vec3_mul(bound_force, engine->config.boundary_weight));

    if (engine->has_predator) {
        nimcp_flocking_predator_evasion(engine, boid, &pred_force);
        *force = nimcp_vec3_add(*force, nimcp_vec3_mul(pred_force, engine->config.predator_weight));
    }

    // Formation seeking
    if (engine->formation_type != NIMCP_FORMATION_NONE && !boid->is_leader) {
        nimcp_vec3_t formation_pos;
        /* Use unlocked version since we're called from within locked context */
        if (flocking_get_formation_position_unlocked(engine, boid_id, &formation_pos) == 0) {
            nimcp_vec3_t formation_force = nimcp_vec3_sub(formation_pos, boid->position);
            float dist = nimcp_vec3_length(formation_force);
            if (dist > NIMCP_FLOCKING_EPSILON) {
                formation_force = nimcp_vec3_normalize(formation_force);
                formation_force = nimcp_vec3_mul(formation_force, boid->max_speed);
                formation_force = nimcp_vec3_sub(formation_force, boid->velocity);
                formation_force = nimcp_vec3_limit(formation_force, boid->max_force);
                formation_force = nimcp_vec3_mul(formation_force, engine->config.formation_tightness);
                *force = nimcp_vec3_add(*force, formation_force);
            }
        }
    }

    return 0;
}

int nimcp_flocking_update(nimcp_flocking_engine_t *engine, float dt) {
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_flocking_update: engine is NULL");
        return -1;
    }

    if (dt <= 0) {
        dt = engine->config.update_dt;
    }

    nimcp_platform_mutex_lock(engine->mutex);

    if (engine->boid_count == 0) {
        nimcp_platform_mutex_unlock(engine->mutex);
        return 0;
    }

    // Update formation center if we have a leader
    if (engine->formation_type != NIMCP_FORMATION_NONE && engine->leader_id != 0) {
        nimcp_boid_t *leader = nimcp_flocking_get_boid(engine, engine->leader_id);
        if (leader) {
            engine->formation_center = leader->position;
        }
    }

    // Calculate forces and update velocities
    for (uint32_t i = 0; i < engine->boid_count; i++) {
        nimcp_boid_t *boid = &engine->boids[i];

        // Find neighbors
        float search_radius = fmaxf(engine->config.separation_radius,
                                   fmaxf(engine->config.alignment_radius,
                                         engine->config.cohesion_radius));
        nimcp_flocking_find_neighbors(engine, boid->id, search_radius);

        // Calculate total force
        nimcp_vec3_t force;
        nimcp_flocking_calculate_force(engine, boid->id, &force);

        // Update acceleration
        boid->acceleration = nimcp_vec3_div(force, boid->mass);
        boid->acceleration = nimcp_vec3_limit(boid->acceleration, engine->config.max_acceleration);
    }

    // Update positions
    for (uint32_t i = 0; i < engine->boid_count; i++) {
        nimcp_boid_t *boid = &engine->boids[i];

        // Update velocity
        boid->velocity = nimcp_vec3_add(boid->velocity,
                                        nimcp_vec3_mul(boid->acceleration, dt));
        boid->velocity = nimcp_vec3_limit(boid->velocity, boid->max_speed);

        // Update position
        boid->position = nimcp_vec3_add(boid->position,
                                        nimcp_vec3_mul(boid->velocity, dt));

        // Hard boundary enforcement
        if (boid->position.x < engine->config.boundary_min.x) {
            boid->position.x = engine->config.boundary_min.x;
            boid->velocity.x *= -0.5F;
        }
        if (boid->position.x > engine->config.boundary_max.x) {
            boid->position.x = engine->config.boundary_max.x;
            boid->velocity.x *= -0.5F;
        }
        if (boid->position.y < engine->config.boundary_min.y) {
            boid->position.y = engine->config.boundary_min.y;
            boid->velocity.y *= -0.5F;
        }
        if (boid->position.y > engine->config.boundary_max.y) {
            boid->position.y = engine->config.boundary_max.y;
            boid->velocity.y *= -0.5F;
        }
        if (boid->position.z < engine->config.boundary_min.z) {
            boid->position.z = engine->config.boundary_min.z;
            boid->velocity.z *= -0.5F;
        }
        if (boid->position.z > engine->config.boundary_max.z) {
            boid->position.z = engine->config.boundary_max.z;
            boid->velocity.z *= -0.5F;
        }
    }

    engine->update_count++;

    // Update state
    if (engine->state == NIMCP_FLOCK_IDLE && engine->boid_count > 0) {
        engine->state = NIMCP_FLOCK_ACTIVE;
    }

    nimcp_platform_mutex_unlock(engine->mutex);

    return 0;
}

/* ========================================================================
 * Statistics and Analysis
 * ======================================================================== */

int nimcp_flocking_center_of_mass(nimcp_flocking_engine_t *engine,
                                  nimcp_vec3_t *center) {
    if (!engine || !center || engine->boid_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_flocking_center_of_mass: required parameter is NULL (engine, center)");
        return -1;
    }

    *center = nimcp_vec3_create(0, 0, 0);

    for (uint32_t i = 0; i < engine->boid_count; i++) {
        *center = nimcp_vec3_add(*center, engine->boids[i].position);
    }

    *center = nimcp_vec3_div(*center, (float)engine->boid_count);
    return 0;
}

int nimcp_flocking_get_stats(nimcp_flocking_engine_t *engine,
                             nimcp_flocking_stats_t *stats) {
    if (!engine || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_flocking_get_stats: required parameter is NULL (engine, stats)");
        return -1;
    }

    memset(stats, 0, sizeof(nimcp_flocking_stats_t));

    if (engine->boid_count == 0) return 0;

    nimcp_platform_mutex_lock(engine->mutex);

    // Calculate center of mass
    nimcp_flocking_center_of_mass(engine, &stats->center_of_mass);

    // Calculate average speed and neighbor count
    float total_speed = 0;
    float total_neighbors = 0;

    for (uint32_t i = 0; i < engine->boid_count; i++) {
        total_speed += nimcp_vec3_length(engine->boids[i].velocity);
        total_neighbors += engine->boids[i].neighbor_count;
    }

    stats->avg_speed = total_speed / engine->boid_count;
    stats->avg_neighbor_count = total_neighbors / engine->boid_count;

    // Calculate bounding radius
    float max_dist_sq = 0;
    for (uint32_t i = 0; i < engine->boid_count; i++) {
        float dist_sq = nimcp_vec3_length_squared(
            nimcp_vec3_sub(engine->boids[i].position, stats->center_of_mass));
        if (dist_sq > max_dist_sq) {
            max_dist_sq = dist_sq;
        }
    }
    stats->bounding_radius = sqrtf(max_dist_sq);

    // Calculate cohesion metric (inverse of average distance from center)
    float total_dist = 0;
    for (uint32_t i = 0; i < engine->boid_count; i++) {
        total_dist += nimcp_vec3_distance(engine->boids[i].position, stats->center_of_mass);
    }
    float avg_dist = total_dist / engine->boid_count;
    stats->cohesion_metric = 1.0F / (1.0F + avg_dist / 10.0F);

    // Calculate alignment metric (average velocity alignment)
    nimcp_vec3_t avg_velocity = nimcp_vec3_create(0, 0, 0);
    for (uint32_t i = 0; i < engine->boid_count; i++) {
        avg_velocity = nimcp_vec3_add(avg_velocity, engine->boids[i].velocity);
    }
    avg_velocity = nimcp_vec3_div(avg_velocity, (float)engine->boid_count);

    float alignment_sum = 0;
    for (uint32_t i = 0; i < engine->boid_count; i++) {
        nimcp_vec3_t vel_norm = nimcp_vec3_normalize(engine->boids[i].velocity);
        nimcp_vec3_t avg_norm = nimcp_vec3_normalize(avg_velocity);
        float dot = nimcp_vec3_dot(vel_norm, avg_norm);
        alignment_sum += (dot + 1.0F) / 2.0F;  // Normalize to 0-1
    }
    stats->alignment_metric = alignment_sum / engine->boid_count;

    // Calculate formation quality
    if (engine->formation_type != NIMCP_FORMATION_NONE) {
        float error_sum = 0;
        for (uint32_t i = 0; i < engine->boid_count; i++) {
            nimcp_vec3_t ideal_pos;
            /* Use unlocked version since we're already holding the mutex */
            if (flocking_get_formation_position_unlocked(engine, engine->boids[i].id, &ideal_pos) == 0) {
                float error = nimcp_vec3_distance(engine->boids[i].position, ideal_pos);
                error_sum += error;
            }
        }
        float avg_error = error_sum / engine->boid_count;
        stats->formation_quality = 1.0F / (1.0F + avg_error / engine->config.formation_spacing);
    } else {
        stats->formation_quality = 0;
    }

    nimcp_platform_mutex_unlock(engine->mutex);

    return 0;
}

/* ========================================================================
 * Bio-Async Integration
 * ======================================================================== */

/**
 * @brief Message type for flocking state broadcasts
 */
typedef struct {
    bio_message_header_t header;  /**< Bio-async message header (must be first) */
    uint32_t boid_id;             /**< Boid identifier */
    nimcp_vec3_t position;        /**< Boid position */
    nimcp_vec3_t velocity;        /**< Boid velocity */
} flocking_state_msg_t;

/**
 * @brief Bio-async message handler for flocking module
 */
static nimcp_error_t flocking_bioasync_handler(
    const void *msg,
    size_t size,
    nimcp_bio_promise_t response_promise,
    void *user_data
) {
    nimcp_flocking_engine_t *engine = (nimcp_flocking_engine_t *)user_data;
    if (!engine || !msg || size < sizeof(bio_message_header_t)) {
        return NIMCP_INVALID_PARAM;
    }

    (void)response_promise;  // Not used for broadcasts

    // Handle incoming boid state updates from other agents
    nimcp_platform_mutex_lock(engine->mutex);
    // Could update tracked boid positions from network here
    nimcp_platform_mutex_unlock(engine->mutex);

    return NIMCP_SUCCESS;
}

/* ========================================================================
 * KG-Driven Wiring Callback
 * ======================================================================== */

/**
 * @brief Handler map for KG-driven wiring
 */
DEFINE_HANDLER_MAP_BEGIN(flocking)
    HANDLER_MAP_ENTRY(BIO_MSG_BRAIN_STATE_QUERY, flocking_bioasync_handler)
DEFINE_HANDLER_MAP_END()

/**
 * @brief KG-driven wiring callback for flocking module
 *
 * WHAT: Register message handlers based on KG-discovered wiring
 * WHY:  Enable dynamic handler registration from knowledge graph
 * HOW:  Iterate discovered message types and register matching handlers
 *
 * @param bio_ctx Bio-async module context
 * @param message_types Array of message types discovered from KG
 * @param message_count Number of message types in array
 * @param user_data User data (flocking engine pointer)
 * @return 0 on success, -1 on failure
 */
static int flocking_wiring_handler_callback(
    bio_module_context_t bio_ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    (void)user_data;  // Engine context available if needed

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        for (size_t j = 0; j < HANDLER_MAP_SIZE(flocking); j++) {
            if (g_flocking_handler_map[j].message_type == message_types[i]) {
                bio_router_register_handler(
                    bio_ctx,
                    message_types[i],
                    g_flocking_handler_map[j].handler
                );
                registered++;
                break;
            }
        }
    }

    return (registered > 0) ? 0 : -1;
}

int nimcp_flocking_register_bioasync(nimcp_flocking_engine_t *engine,
                                     bio_router_t *router) {
    if (!engine || !router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_flocking_register_bioasync: required parameter is NULL (engine, router)");
        return -1;
    }

    nimcp_platform_mutex_lock(engine->mutex);

    // Create module info for registration
    bio_module_info_t info = {
        .module_id = BIO_MODULE_SWARM_FLOCKING,
        .module_name = "swarm_flocking",
        .inbox_capacity = 0,  // Use default
        .user_data = engine
    };

    // Register with bio-async router
    engine->bio_ctx = bio_router_register_module(&info);

    if (engine->bio_ctx != NULL) {
        // Try KG-driven wiring callback registration
        nimcp_error_t wiring_result = bio_router_register_wiring_callback(
            BIO_MODULE_SWARM_FLOCKING,
            (void*)flocking_wiring_handler_callback,
            engine
        );

        if (wiring_result != NIMCP_SUCCESS) {
            // Fallback to legacy hardcoded registration
            LEGACY_HANDLER_REGISTRATION(
                bio_router_register_handler(engine->bio_ctx, BIO_MSG_BRAIN_STATE_QUERY, flocking_bioasync_handler)
            );
            LOG_DEBUG("Flocking using legacy handler registration (wiring callback unavailable)");
        } else {
            LOG_DEBUG("Flocking registered KG-driven wiring callback");
        }

        engine->bio_async_enabled = true;
        LOG_INFO("Flocking engine registered with bio-async router");
    } else {
        LOG_ERROR("Failed to register flocking engine with bio-async router");
    }

    nimcp_platform_mutex_unlock(engine->mutex);

    return (engine->bio_ctx != NULL) ? 0 : -1;
}

int nimcp_flocking_process_messages(nimcp_flocking_engine_t *engine) {
    if (!engine || !engine->bio_async_enabled) return 0;

    nimcp_platform_mutex_lock(engine->mutex);

    int count = 0;

    // Process pending messages from bio-async router
    if (engine->bio_ctx) {
        nimcp_error_t result = bio_router_process_inbox(engine->bio_ctx, 10);  // Process up to 10 messages
        if (result == NIMCP_SUCCESS) {
            count = 1;  // At least one processed
        }
    }

    nimcp_platform_mutex_unlock(engine->mutex);

    return count;
}

int nimcp_flocking_broadcast_state(nimcp_flocking_engine_t *engine, uint32_t boid_id) {
    if (!engine || !engine->bio_async_enabled || boid_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_flocking_broadcast_state: required parameter is NULL (engine, engine->bio_async_enabled)");
        return -1;
    }

    nimcp_boid_t *boid = nimcp_flocking_get_boid(engine, boid_id);
    if (!boid) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_flocking_broadcast_state: boid is NULL");
        return -1;
    }

    // Create and send position/velocity message
    flocking_state_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.header.type = BIO_MSG_BRAIN_STATE_QUERY;  // Use as generic swarm message
    msg.header.source_module = BIO_MODULE_UNKNOWN;
    msg.header.target_module = 0;  // Broadcast
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.boid_id = boid_id;
    msg.position = boid->position;
    msg.velocity = boid->velocity;

    // Broadcast to all registered modules
    nimcp_error_t result = bio_router_broadcast(engine->bio_ctx, &msg, sizeof(msg));

    return (result == NIMCP_SUCCESS) ? 0 : -1;
}

/* ========================================================================
 * KG Self-Awareness Integration
 * ======================================================================== */

/**
 * @brief Query knowledge graph for module self-knowledge
 *
 * WHAT: Introspect module identity from knowledge graph
 * WHY:  Enable self-awareness and runtime reflection
 * HOW:  Query KG for Swarm_Flocking entity and its relations
 *
 * @param kg Knowledge graph reader
 * @return 1 if self-knowledge found, 0 otherwise
 */
int swarm_flocking_query_self_knowledge(kg_reader_t* kg)
{
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Swarm_Flocking");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            LOG_DEBUG("Flocking self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Swarm_Flocking");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Swarm_Flocking");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}
