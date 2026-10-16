/**
 * @file nimcp_omni_wm_parietal_bridge.c
 * @brief World Model Parietal Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-17
 *
 * WHAT: Bidirectional bridge connecting World Model (RSSM) with Parietal systems
 * WHY:  Enable physics-informed world modeling and spatial-aware predictions
 * HOW:  Integrate spatial attention, coordinate transforms, and physics constraints
 *
 * IMPLEMENTATION NOTES:
 * =====================
 * This implementation integrates several key concepts:
 *
 * 1. SPATIAL WORLD MODELS:
 *    - Predictions in multiple reference frames (egocentric/allocentric)
 *    - Coordinate transforms for motor planning
 *
 * 2. PHYSICS-INFORMED PREDICTIONS:
 *    - Conservation laws constrain trajectory forecasts
 *    - Collision detection and avoidance
 *
 * 3. SPATIAL ATTENTION GATING:
 *    - Attention maps weight prediction resources
 *    - Focus-based prediction prioritization
 */

#include "cognitive/omni/bridges/nimcp_omni_wm_parietal_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>

/* ============================================================================
 * Module-level Constants
 * ============================================================================ */

#define LOG_MODULE "wm_parietal_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(omni_wm_parietal_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_omni_wm_parietal_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_omni_wm_parietal_bridge_mesh_registry = NULL;

nimcp_error_t omni_wm_parietal_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_omni_wm_parietal_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "omni_wm_parietal_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "omni_wm_parietal_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_omni_wm_parietal_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_omni_wm_parietal_bridge_mesh_registry = registry;
    return err;
}

void omni_wm_parietal_bridge_mesh_unregister(void) {
    if (g_omni_wm_parietal_bridge_mesh_registry && g_omni_wm_parietal_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_omni_wm_parietal_bridge_mesh_registry, g_omni_wm_parietal_bridge_mesh_id);
        g_omni_wm_parietal_bridge_mesh_id = 0;
        g_omni_wm_parietal_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from omni_wm_parietal_bridge module (instance-level) */
static inline void omni_wm_parietal_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_omni_wm_parietal_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_omni_wm_parietal_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_omni_wm_parietal_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

void omni_wm_parietal_bridge_set_instance_health_agent(
    omni_wm_parietal_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "omni_wm_parietal_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Instance-level Training Functions
 * ============================================================================ */

int omni_wm_parietal_bridge_training_begin(omni_wm_parietal_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_parietal_bridge_training_begin: NULL argument");
        return -1;
    }
    omni_wm_parietal_bridge_heartbeat_instance(g_omni_wm_parietal_bridge_health_agent, "training_begin", 0.0f);
    (void)bridge;
    return 0;
}

int omni_wm_parietal_bridge_training_step(omni_wm_parietal_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_parietal_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    omni_wm_parietal_bridge_heartbeat_instance(g_omni_wm_parietal_bridge_health_agent, "training_step", progress);
    (void)bridge;
    return 0;
}

int omni_wm_parietal_bridge_training_end(omni_wm_parietal_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_parietal_bridge_training_end: NULL argument");
        return -1;
    }
    omni_wm_parietal_bridge_heartbeat_instance(g_omni_wm_parietal_bridge_health_agent, "training_end", 1.0f);
    (void)bridge;
    return 0;
}

/** Default tracked objects capacity */
#define DEFAULT_TRACKED_OBJECTS_CAPACITY 32

/** Default constraints capacity */
#define DEFAULT_CONSTRAINTS_CAPACITY 16

/** Default trajectory cache capacity */
#define DEFAULT_TRAJECTORY_CACHE_CAPACITY 16

#include "constants/nimcp_constants.h"
/** Small epsilon for floating point comparisons */
#define FLOAT_EPSILON NIMCP_EPSILON_NUMERICAL

/** Default gravity (Earth, m/s^2) */
#define DEFAULT_GRAVITY 9.81f

/* ============================================================================
 * Internal Physics Engine Structure
 * ============================================================================ */

/**
 * @brief Internal physics engine for constraint application
 */
struct wm_physics_engine {
    float gravity_magnitude;        /**< Gravity strength (m/s^2) */
    wm_parietal_vec3_t gravity_dir; /**< Gravity direction (normalized) */
    float time_accumulator;         /**< Accumulated physics time */
    float fixed_dt;                 /**< Fixed physics timestep */
    bool initialized;               /**< Engine initialized */
};

/* ============================================================================
 * Internal Helper Forward Declarations
 * ============================================================================ */

static nimcp_error_t allocate_tracked_objects(omni_wm_parietal_bridge_t* bridge);
static void free_tracked_objects(omni_wm_parietal_bridge_t* bridge);
static nimcp_error_t allocate_constraints(omni_wm_parietal_bridge_t* bridge);
static void free_constraints(omni_wm_parietal_bridge_t* bridge);
static nimcp_error_t allocate_attention_map(omni_wm_parietal_bridge_t* bridge);
static void free_attention_map(omni_wm_parietal_bridge_t* bridge);
static nimcp_error_t allocate_trajectory_cache(omni_wm_parietal_bridge_t* bridge);
static void free_trajectory_cache(omni_wm_parietal_bridge_t* bridge);
static nimcp_error_t create_physics_engine(omni_wm_parietal_bridge_t* bridge);
static void destroy_physics_engine(omni_wm_parietal_bridge_t* bridge);

static nimcp_error_t update_wm_to_parietal_effects(omni_wm_parietal_bridge_t* bridge);
static nimcp_error_t update_parietal_to_wm_effects(omni_wm_parietal_bridge_t* bridge);
static nimcp_error_t apply_physics_constraints(omni_wm_parietal_bridge_t* bridge,
                                                wm_parietal_spatial_state_t* state,
                                                float dt);
static int find_tracked_object(const omni_wm_parietal_bridge_t* bridge, uint32_t object_id);
static uint64_t get_current_time_us(void);

/* Bio-async handlers */
static nimcp_error_t handle_spatial_pred(const void* msg, size_t msg_size,
                                          nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_trajectory_pred(const void* msg, size_t msg_size,
                                             nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_coord_transform(const void* msg, size_t msg_size,
                                             nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_physics_query(const void* msg, size_t msg_size,
                                           nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_attention_update(const void* msg, size_t msg_size,
                                              nimcp_bio_promise_t promise, void* user_data);

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_current_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

/**
 * @brief Allocate tracked objects array
 */
static nimcp_error_t allocate_tracked_objects(omni_wm_parietal_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    uint32_t capacity = DEFAULT_TRACKED_OBJECTS_CAPACITY;

    bridge->tracked_objects = nimcp_calloc(capacity, sizeof(wm_parietal_spatial_state_t));
    if (!bridge->tracked_objects) return NIMCP_ERROR_NO_MEMORY;

    bridge->tracked_objects_capacity = capacity;
    bridge->num_tracked_objects = 0;

    return NIMCP_SUCCESS;
}

/**
 * @brief Free tracked objects array
 */
static void free_tracked_objects(omni_wm_parietal_bridge_t* bridge) {
    if (!bridge) return;

    nimcp_free(bridge->tracked_objects);
    bridge->tracked_objects = NULL;
    bridge->num_tracked_objects = 0;
    bridge->tracked_objects_capacity = 0;
}

/**
 * @brief Allocate constraints array
 */
static nimcp_error_t allocate_constraints(omni_wm_parietal_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    uint32_t capacity = DEFAULT_CONSTRAINTS_CAPACITY;

    bridge->constraints = nimcp_calloc(capacity, sizeof(wm_parietal_physics_constraint_t));
    if (!bridge->constraints) return NIMCP_ERROR_NO_MEMORY;

    bridge->constraints_capacity = capacity;
    bridge->num_constraints = 0;

    return NIMCP_SUCCESS;
}

/**
 * @brief Free constraints array
 */
static void free_constraints(omni_wm_parietal_bridge_t* bridge) {
    if (!bridge) return;

    nimcp_free(bridge->constraints);
    bridge->constraints = NULL;
    bridge->num_constraints = 0;
    bridge->constraints_capacity = 0;
}

/**
 * @brief Allocate attention map
 */
static nimcp_error_t allocate_attention_map(omni_wm_parietal_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    uint32_t dim = bridge->config.attention_resolution;
    if (dim == 0) dim = WM_PARIETAL_DEFAULT_ATTENTION_RES;

    uint32_t size = dim * dim * dim; /* 3D attention map */
    bridge->attention_map = nimcp_calloc(size, sizeof(float));
    if (!bridge->attention_map) return NIMCP_ERROR_NO_MEMORY;

    /* Initialize uniform attention */
    float uniform_weight = 1.0f / (float)size;
    for (uint32_t i = 0; i < size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && size > 256) {
            omni_wm_parietal_bridge_heartbeat("omni_wm_pari_loop",
                             (float)(i + 1) / (float)size);
        }

        bridge->attention_map[i] = uniform_weight;
    }

    bridge->attention_map_dim = dim;
    bridge->current_focus = (wm_parietal_vec3_t){0.0f, 0.0f, 0.0f};

    return NIMCP_SUCCESS;
}

/**
 * @brief Free attention map
 */
static void free_attention_map(omni_wm_parietal_bridge_t* bridge) {
    if (!bridge) return;

    nimcp_free(bridge->attention_map);
    bridge->attention_map = NULL;
    bridge->attention_map_dim = 0;
}

/**
 * @brief Allocate trajectory cache
 */
static nimcp_error_t allocate_trajectory_cache(omni_wm_parietal_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    uint32_t capacity = DEFAULT_TRAJECTORY_CACHE_CAPACITY;

    bridge->trajectory_cache = nimcp_calloc(capacity, sizeof(wm_parietal_trajectory_t*));
    if (!bridge->trajectory_cache) return NIMCP_ERROR_NO_MEMORY;

    bridge->trajectory_cache_capacity = capacity;
    bridge->trajectory_cache_size = 0;

    return NIMCP_SUCCESS;
}

/**
 * @brief Free trajectory cache
 */
static void free_trajectory_cache(omni_wm_parietal_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->trajectory_cache) {
        for (uint32_t i = 0; i < bridge->trajectory_cache_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->trajectory_cache_size > 256) {
                omni_wm_parietal_bridge_heartbeat("omni_wm_pari_loop",
                                 (float)(i + 1) / (float)bridge->trajectory_cache_size);
            }

            omni_wm_parietal_trajectory_destroy(bridge->trajectory_cache[i]);
        }
        nimcp_free(bridge->trajectory_cache);
        bridge->trajectory_cache = NULL;
    }
    bridge->trajectory_cache_size = 0;
    bridge->trajectory_cache_capacity = 0;
}

/**
 * @brief Create internal physics engine
 */
static nimcp_error_t create_physics_engine(omni_wm_parietal_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    bridge->physics_engine = nimcp_calloc(1, sizeof(wm_physics_engine_t));
    if (!bridge->physics_engine) return NIMCP_ERROR_NO_MEMORY;

    bridge->physics_engine->gravity_magnitude = bridge->config.gravity_magnitude;
    bridge->physics_engine->gravity_dir = (wm_parietal_vec3_t){0.0f, -1.0f, 0.0f};
    bridge->physics_engine->fixed_dt = bridge->config.physics_dt;
    bridge->physics_engine->time_accumulator = 0.0f;
    bridge->physics_engine->initialized = true;

    return NIMCP_SUCCESS;
}

/**
 * @brief Destroy physics engine
 */
static void destroy_physics_engine(omni_wm_parietal_bridge_t* bridge) {
    if (!bridge) return;

    nimcp_free(bridge->physics_engine);
    bridge->physics_engine = NULL;
}

/**
 * @brief Find tracked object by ID
 * @return Index or -1 if not found
 */
static int find_tracked_object(const omni_wm_parietal_bridge_t* bridge, uint32_t object_id) {
    if (!bridge || !bridge->tracked_objects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_tracked_object: required parameter is NULL (bridge, bridge->tracked_objects)");
        return -1;
    }

    for (uint32_t i = 0; i < bridge->num_tracked_objects; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_tracked_objects > 256) {
            omni_wm_parietal_bridge_heartbeat("omni_wm_pari_loop",
                             (float)(i + 1) / (float)bridge->num_tracked_objects);
        }

        if (bridge->tracked_objects[i].object_id == object_id) {
            return (int)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_tracked_object: validation failed");
    return -1;
}

/**
 * @brief Apply physics constraints to a spatial state
 */
static nimcp_error_t apply_physics_constraints(omni_wm_parietal_bridge_t* bridge,
                                                wm_parietal_spatial_state_t* state,
                                                float dt) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_NULL_POINTER, "state is NULL");
    if (!bridge->config.enable_physics_constraints) return NIMCP_SUCCESS;
    if (!bridge->physics_engine || !bridge->physics_engine->initialized) return NIMCP_SUCCESS;

    /* Apply gravity */
    if (bridge->config.gravity_magnitude > FLOAT_EPSILON) {
        wm_physics_engine_t* pe = bridge->physics_engine;
        state->velocity.vx += pe->gravity_dir.x * pe->gravity_magnitude * dt;
        state->velocity.vy += pe->gravity_dir.y * pe->gravity_magnitude * dt;
        state->velocity.vz += pe->gravity_dir.z * pe->gravity_magnitude * dt;
    }

    /* Apply each constraint */
    for (uint32_t i = 0; i < bridge->num_constraints; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_constraints > 256) {
            omni_wm_parietal_bridge_heartbeat("omni_wm_pari_loop",
                             (float)(i + 1) / (float)bridge->num_constraints);
        }

        wm_parietal_physics_constraint_t* c = &bridge->constraints[i];
        if (!c->enabled) continue;
        if (c->object_id != 0 && c->object_id != state->object_id) continue;

        switch (c->type) {
            case WM_PARIETAL_PHYSICS_GRAVITY:
                /* Custom gravity override */
                state->velocity.vy -= c->parameters[0] * c->strength * dt;
                break;

            case WM_PARIETAL_PHYSICS_FRICTION:
                /* Simple friction damping */
                {
                    float friction = c->parameters[0] * c->strength;
                    state->velocity.vx *= (1.0f - friction * dt);
                    state->velocity.vy *= (1.0f - friction * dt);
                    state->velocity.vz *= (1.0f - friction * dt);
                }
                break;

            case WM_PARIETAL_PHYSICS_BOUNDARY:
                /* Clamp to boundary */
                {
                    float min_x = c->parameters[0];
                    float max_x = c->parameters[1];
                    float min_y = c->parameters[2];
                    float max_y = c->parameters[3];
                    float min_z = c->parameters[4];
                    float max_z = c->parameters[5];

                    if (state->position.x < min_x) {
                        state->position.x = min_x;
                        state->velocity.vx = -state->velocity.vx * c->strength;
                    }
                    if (state->position.x > max_x) {
                        state->position.x = max_x;
                        state->velocity.vx = -state->velocity.vx * c->strength;
                    }
                    if (state->position.y < min_y) {
                        state->position.y = min_y;
                        state->velocity.vy = -state->velocity.vy * c->strength;
                    }
                    if (state->position.y > max_y) {
                        state->position.y = max_y;
                        state->velocity.vy = -state->velocity.vy * c->strength;
                    }
                    if (state->position.z < min_z) {
                        state->position.z = min_z;
                        state->velocity.vz = -state->velocity.vz * c->strength;
                    }
                    if (state->position.z > max_z) {
                        state->position.z = max_z;
                        state->velocity.vz = -state->velocity.vz * c->strength;
                    }
                }
                break;

            default:
                break;
        }
    }

    /* Update position from velocity */
    state->position.x += state->velocity.vx * dt;
    state->position.y += state->velocity.vy * dt;
    state->position.z += state->velocity.vz * dt;

    return NIMCP_SUCCESS;
}

/**
 * @brief Update effects flowing from WM to parietal systems
 */
static nimcp_error_t update_wm_to_parietal_effects(omni_wm_parietal_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    omni_wm_to_parietal_effects_t* effects = &bridge->wm_to_parietal;

    /* Update timestamp */
    effects->prediction_timestamp = (double)get_current_time_us() / 1000000.0;

    /* Update prediction counts */
    effects->num_predicted_states = bridge->num_tracked_objects;

    /* Calculate physics analysis */
    effects->total_kinetic_energy = 0.0f;
    effects->total_potential_energy = 0.0f;
    effects->center_of_mass = (wm_parietal_vec3_t){0.0f, 0.0f, 0.0f};
    effects->total_momentum = (wm_parietal_vec3_t){0.0f, 0.0f, 0.0f};

    float total_mass = 0.0f;
    for (uint32_t i = 0; i < bridge->num_tracked_objects; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_tracked_objects > 256) {
            omni_wm_parietal_bridge_heartbeat("omni_wm_pari_loop",
                             (float)(i + 1) / (float)bridge->num_tracked_objects);
        }

        wm_parietal_spatial_state_t* obj = &bridge->tracked_objects[i];

        /* Kinetic energy: 0.5 * m * v^2 */
        float v_squared = obj->velocity.vx * obj->velocity.vx +
                         obj->velocity.vy * obj->velocity.vy +
                         obj->velocity.vz * obj->velocity.vz;
        effects->total_kinetic_energy += 0.5f * obj->mass * v_squared;

        /* Potential energy: m * g * h (assuming y is up) */
        effects->total_potential_energy += obj->mass *
            bridge->config.gravity_magnitude * obj->position.y;

        /* Center of mass contribution */
        effects->center_of_mass.x += obj->mass * obj->position.x;
        effects->center_of_mass.y += obj->mass * obj->position.y;
        effects->center_of_mass.z += obj->mass * obj->position.z;
        total_mass += obj->mass;

        /* Momentum: m * v */
        effects->total_momentum.x += obj->mass * obj->velocity.vx;
        effects->total_momentum.y += obj->mass * obj->velocity.vy;
        effects->total_momentum.z += obj->mass * obj->velocity.vz;
    }

    if (total_mass > FLOAT_EPSILON) {
        effects->center_of_mass.x /= total_mass;
        effects->center_of_mass.y /= total_mass;
        effects->center_of_mass.z /= total_mass;
    }

    /* Compute overall confidence based on tracked object confidences */
    float confidence_sum = 0.0f;
    for (uint32_t i = 0; i < bridge->num_tracked_objects; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_tracked_objects > 256) {
            omni_wm_parietal_bridge_heartbeat("omni_wm_pari_loop",
                             (float)(i + 1) / (float)bridge->num_tracked_objects);
        }

        confidence_sum += bridge->tracked_objects[i].confidence;
    }
    if (bridge->num_tracked_objects > 0) {
        effects->overall_confidence = confidence_sum / (float)bridge->num_tracked_objects;
    } else {
        effects->overall_confidence = 0.0f;
    }

    /* Physics consistency score */
    effects->physics_consistency = 1.0f; /* Placeholder */

    return NIMCP_SUCCESS;
}

/**
 * @brief Update effects flowing from parietal systems to WM
 */
static nimcp_error_t update_parietal_to_wm_effects(omni_wm_parietal_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    parietal_to_omni_wm_effects_t* effects = &bridge->parietal_to_wm;

    /* Update attention information */
    effects->attention_focus = bridge->current_focus;
    effects->attention_map_size = bridge->attention_map_dim;

    /* Update constraint information */
    effects->num_constraints = bridge->num_constraints;

    /* Compute constraint violation */
    effects->constraint_violation = 0.0f; /* Placeholder */

    /* Update observed states count */
    effects->num_observed_states = bridge->num_tracked_objects;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async Message Handlers
 * ============================================================================ */

/**
 * @brief Handle spatial prediction request
 */
static nimcp_error_t handle_spatial_pred(const void* msg, size_t msg_size,
                                          nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_parietal_bridge_t* bridge = (omni_wm_parietal_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.spatial_predictions_made++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle trajectory prediction request
 */
static nimcp_error_t handle_trajectory_pred(const void* msg, size_t msg_size,
                                             nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_parietal_bridge_t* bridge = (omni_wm_parietal_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.trajectory_predictions_made++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle coordinate transform request
 */
static nimcp_error_t handle_coord_transform(const void* msg, size_t msg_size,
                                             nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_parietal_bridge_t* bridge = (omni_wm_parietal_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.coordinate_transforms++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle physics query
 */
static nimcp_error_t handle_physics_query(const void* msg, size_t msg_size,
                                           nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_parietal_bridge_t* bridge = (omni_wm_parietal_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.physics_queries++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle attention update
 */
static nimcp_error_t handle_attention_update(const void* msg, size_t msg_size,
                                              nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_parietal_bridge_t* bridge = (omni_wm_parietal_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.attention_updates++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_parietal_bridge_default_config(
    omni_wm_parietal_bridge_config_t* config) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_default_config", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    memset(config, 0, sizeof(omni_wm_parietal_bridge_config_t));

    /* General settings */
    config->enable_modulation = true;
    config->sensitivity = 1.0f;

    /* Spatial prediction settings */
    config->enable_spatial_prediction = true;
    config->max_prediction_horizon = WM_PARIETAL_MAX_TRAJECTORY_HORIZON;
    config->prediction_dt = 0.016f; /* ~60Hz */
    config->default_frame = WM_PARIETAL_FRAME_ALLOCENTRIC;
    config->enable_coordinate_transforms = true;

    /* Physics integration settings */
    config->enable_physics_constraints = true;
    config->physics_dt = WM_PARIETAL_DEFAULT_PHYSICS_DT;
    config->gravity_magnitude = DEFAULT_GRAVITY;
    config->enable_collision_prediction = true;
    config->collision_epsilon = 0.01f;
    config->enable_momentum_conservation = true;
    config->enable_energy_constraints = false;

    /* Spatial attention settings */
    config->enable_attention_gating = true;
    config->attention_resolution = WM_PARIETAL_DEFAULT_ATTENTION_RES;
    config->attention_decay_rate = 0.05f;
    config->salience_threshold = 0.1f;

    /* Mathematical reasoning settings */
    config->enable_math_reasoning = true;
    config->enable_pattern_extrapolation = true;
    config->enable_numerical_estimation = true;

    /* Training settings */
    config->enable_physics_learning = false;
    config->physics_learning_rate = 0.001f;

    /* Bio-async settings */
    config->enable_bio_async = true;

    return NIMCP_SUCCESS;
}

omni_wm_parietal_bridge_t* omni_wm_parietal_bridge_create(
    const omni_wm_parietal_bridge_config_t* config) {

    /* Allocate bridge structure */
    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_create", 0.0f);


    omni_wm_parietal_bridge_t* bridge = nimcp_calloc(1, sizeof(omni_wm_parietal_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate WM parietal bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "update_parietal_to_wm_effects: bridge is NULL");
        return NULL;
    }

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, BIO_MODULE_WM_PARIETAL_BRIDGE,
                         "wm_parietal_bridge") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to initialize bridge base");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: operation failed");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        omni_wm_parietal_bridge_default_config(&bridge->config);
    }

    /* Allocate tracked objects */
    nimcp_error_t err = allocate_tracked_objects(bridge);
    if (err != NIMCP_SUCCESS) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to allocate tracked objects");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: validation failed");
        return NULL;
    }

    /* Allocate constraints */
    err = allocate_constraints(bridge);
    if (err != NIMCP_SUCCESS) {
        free_tracked_objects(bridge);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to allocate constraints");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: validation failed");
        return NULL;
    }

    /* Allocate attention map */
    err = allocate_attention_map(bridge);
    if (err != NIMCP_SUCCESS) {
        free_constraints(bridge);
        free_tracked_objects(bridge);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to allocate attention map");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: validation failed");
        return NULL;
    }

    /* Allocate trajectory cache */
    err = allocate_trajectory_cache(bridge);
    if (err != NIMCP_SUCCESS) {
        free_attention_map(bridge);
        free_constraints(bridge);
        free_tracked_objects(bridge);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to allocate trajectory cache");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: validation failed");
        return NULL;
    }

    /* Create physics engine */
    err = create_physics_engine(bridge);
    if (err != NIMCP_SUCCESS) {
        free_trajectory_cache(bridge);
        free_attention_map(bridge);
        free_constraints(bridge);
        free_tracked_objects(bridge);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to create physics engine");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: validation failed");
        return NULL;
    }

    NIMCP_LOGGING_INFO("WM Parietal Bridge created successfully");
    return bridge;
}

void omni_wm_parietal_bridge_destroy(omni_wm_parietal_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        omni_wm_parietal_bridge_disconnect_bio_async(bridge);
    }

    /* Free WM to parietal effects dynamic arrays */
    nimcp_free(bridge->wm_to_parietal.predicted_states);
    if (bridge->wm_to_parietal.trajectories) {
        for (uint32_t i = 0; i < bridge->wm_to_parietal.num_trajectories; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->wm_to_parietal.num_trajectories > 256) {
                omni_wm_parietal_bridge_heartbeat("omni_wm_pari_loop",
                                 (float)(i + 1) / (float)bridge->wm_to_parietal.num_trajectories);
            }

            omni_wm_parietal_trajectory_destroy(bridge->wm_to_parietal.trajectories[i]);
        }
        nimcp_free(bridge->wm_to_parietal.trajectories);
    }

    /* Free parietal to WM effects dynamic arrays */
    nimcp_free(bridge->parietal_to_wm.attention_map);
    nimcp_free(bridge->parietal_to_wm.constraints);
    nimcp_free(bridge->parietal_to_wm.observed_states);

    /* Free internal structures */
    destroy_physics_engine(bridge);
    free_trajectory_cache(bridge);
    free_attention_map(bridge);
    free_constraints(bridge);
    free_tracked_objects(bridge);

    /* Cleanup base and free */
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;

    NIMCP_LOGGING_INFO("WM Parietal Bridge destroyed");
}

nimcp_error_t omni_wm_parietal_bridge_reset(omni_wm_parietal_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_reset", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset effects */
    memset(&bridge->wm_to_parietal, 0, sizeof(omni_wm_to_parietal_effects_t));
    memset(&bridge->parietal_to_wm, 0, sizeof(parietal_to_omni_wm_effects_t));

    /* Clear tracked objects */
    bridge->num_tracked_objects = 0;

    /* Clear constraints */
    bridge->num_constraints = 0;

    /* Reset attention to uniform */
    if (bridge->attention_map && bridge->attention_map_dim > 0) {
        uint32_t size = bridge->attention_map_dim * bridge->attention_map_dim *
                       bridge->attention_map_dim;
        float uniform = 1.0f / (float)size;
        for (uint32_t i = 0; i < size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && size > 256) {
                omni_wm_parietal_bridge_heartbeat("omni_wm_pari_loop",
                                 (float)(i + 1) / (float)size);
            }

            bridge->attention_map[i] = uniform;
        }
    }
    bridge->current_focus = (wm_parietal_vec3_t){0.0f, 0.0f, 0.0f};

    /* Clear trajectory cache */
    for (uint32_t i = 0; i < bridge->trajectory_cache_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->trajectory_cache_size > 256) {
            omni_wm_parietal_bridge_heartbeat("omni_wm_pari_loop",
                             (float)(i + 1) / (float)bridge->trajectory_cache_size);
        }

        omni_wm_parietal_trajectory_destroy(bridge->trajectory_cache[i]);
        bridge->trajectory_cache[i] = NULL;
    }
    bridge->trajectory_cache_size = 0;

    /* Reset physics engine */
    if (bridge->physics_engine) {
        bridge->physics_engine->time_accumulator = 0.0f;
    }

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(omni_wm_parietal_bridge_stats_t));

    /* Reset base bridge (unlocked since we already hold the mutex) */
    bridge_base_reset_unlocked(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_parietal_bridge_connect(
    omni_wm_parietal_bridge_t* bridge,
    omni_world_model_t* world_model,
    parietal_lobe_t* parietal_lobe,
    parietal_adapter_t* parietal_adapter,
    spatial_reasoning_t* spatial_reasoning) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_connect", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(world_model, NIMCP_ERROR_INVALID_PARAM, "world_model is required");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->world_model = world_model;
    bridge->parietal_lobe = parietal_lobe;
    bridge->parietal_adapter = parietal_adapter;
    bridge->spatial_reasoning = spatial_reasoning;

    /* Update base connection state */
    bridge->base.system_a = world_model;
    bridge->base.system_a_connected = true;
    bridge->base.bridge_active = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("WM Parietal Bridge connected: WM=%p, Lobe=%p, Adapter=%p, Spatial=%p",
                       (void*)world_model, (void*)parietal_lobe,
                       (void*)parietal_adapter, (void*)spatial_reasoning);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_parietal_bridge_connect_world_model(
    omni_wm_parietal_bridge_t* bridge,
    omni_world_model_t* world_model) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_connect_world_model", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(world_model, NIMCP_ERROR_NULL_POINTER, "world_model is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->world_model = world_model;
    bridge->base.system_a = world_model;
    bridge->base.system_a_connected = true;
    bridge->base.bridge_active = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_parietal_bridge_connect_parietal_lobe(
    omni_wm_parietal_bridge_t* bridge,
    parietal_lobe_t* parietal_lobe) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_connect_parietal_lob", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(parietal_lobe, NIMCP_ERROR_NULL_POINTER, "parietal_lobe is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->parietal_lobe = parietal_lobe;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_parietal_bridge_connect_parietal_adapter(
    omni_wm_parietal_bridge_t* bridge,
    parietal_adapter_t* parietal_adapter) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_connect_parietal_ada", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(parietal_adapter, NIMCP_ERROR_NULL_POINTER, "parietal_adapter is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->parietal_adapter = parietal_adapter;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_parietal_bridge_connect_spatial_reasoning(
    omni_wm_parietal_bridge_t* bridge,
    spatial_reasoning_t* spatial_reasoning) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_connect_spatial_reas", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(spatial_reasoning, NIMCP_ERROR_NULL_POINTER, "spatial_reasoning is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->spatial_reasoning = spatial_reasoning;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

bool omni_wm_parietal_bridge_is_connected(const omni_wm_parietal_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_is_connected", 0.0f);


    return bridge->world_model != NULL;
}

/* ============================================================================
 * Update API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_parietal_bridge_update(
    omni_wm_parietal_bridge_t* bridge,
    float dt) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_update", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Validate dt - negative timesteps are invalid */
    if (dt < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "omni_wm_parietal_bridge_update: negative dt (%.4f)", dt);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Must be connected (world_model set) to perform updates */
    if (!bridge->world_model) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED,
            "omni_wm_parietal_bridge_update: bridge not connected (world_model is NULL)");
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    if (!bridge->config.enable_modulation) return NIMCP_SUCCESS;

    uint64_t start_time = get_current_time_us();

    nimcp_mutex_lock(bridge->base.mutex);

    /* Apply physics step if enabled */
    if (bridge->config.enable_physics_constraints && bridge->physics_engine) {
        bridge->physics_engine->time_accumulator += dt;

        /* Fixed timestep physics integration */
        while (bridge->physics_engine->time_accumulator >= bridge->physics_engine->fixed_dt) {
            for (uint32_t i = 0; i < bridge->num_tracked_objects; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && bridge->num_tracked_objects > 256) {
                    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_loop",
                                     (float)(i + 1) / (float)bridge->num_tracked_objects);
                }

                apply_physics_constraints(bridge, &bridge->tracked_objects[i],
                                         bridge->physics_engine->fixed_dt);
            }
            bridge->physics_engine->time_accumulator -= bridge->physics_engine->fixed_dt;
        }
    }

    /* Apply attention decay */
    if (bridge->config.enable_attention_gating && bridge->attention_map) {
        float decay = bridge->config.attention_decay_rate * dt;
        uint32_t size = bridge->attention_map_dim * bridge->attention_map_dim *
                       bridge->attention_map_dim;
        float uniform = 1.0f / (float)size;
        for (uint32_t i = 0; i < size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && size > 256) {
                omni_wm_parietal_bridge_heartbeat("omni_wm_pari_loop",
                                 (float)(i + 1) / (float)size);
            }

            bridge->attention_map[i] = bridge->attention_map[i] * (1.0f - decay) +
                                       uniform * decay;
        }
    }

    /* Update effects in both directions */
    update_wm_to_parietal_effects(bridge);
    update_parietal_to_wm_effects(bridge);

    /* Update timing statistics */
    bridge->stats.total_updates++;
    uint64_t elapsed = get_current_time_us() - start_time;
    bridge->stats.total_processing_time_ms += (double)elapsed / 1000.0;
    bridge->stats.mean_update_time_ms = bridge->stats.total_processing_time_ms /
                                         (double)bridge->stats.total_updates;
    bridge->stats.last_update_time_us = start_time;

    /* Record base update */
    bridge_base_record_update(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Spatial Prediction API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_parietal_bridge_predict_spatial_state(
    omni_wm_parietal_bridge_t* bridge,
    uint32_t object_id,
    uint32_t horizon_steps,
    wm_parietal_frame_t target_frame,
    wm_parietal_spatial_state_t* predicted_state) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_predict_spatial_stat", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(predicted_state, NIMCP_ERROR_NULL_POINTER, "predicted_state is NULL");
    if (!bridge->config.enable_spatial_prediction) return NIMCP_SUCCESS;
    if (horizon_steps > bridge->config.max_prediction_horizon) {
        horizon_steps = bridge->config.max_prediction_horizon;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Find the object */
    int idx = find_tracked_object(bridge, object_id);
    if (idx < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Copy current state as starting point */
    *predicted_state = bridge->tracked_objects[idx];

    /* Simulate forward by horizon_steps */
    float dt = bridge->config.prediction_dt;
    for (uint32_t step = 0; step < horizon_steps; step++) {
        /* Phase 8: Loop progress heartbeat */
        if ((step & 0xFF) == 0 && horizon_steps > 256) {
            omni_wm_parietal_bridge_heartbeat("omni_wm_pari_loop",
                             (float)(step + 1) / (float)horizon_steps);
        }

        apply_physics_constraints(bridge, predicted_state, dt);
    }

    /* Transform to target frame if needed */
    if (target_frame != predicted_state->frame) {
        /* Placeholder: would use spatial_reasoning for transform */
        predicted_state->frame = target_frame;
    }

    /* Update timestamp */
    predicted_state->timestamp = (double)get_current_time_us() / 1000000.0;

    /* Update statistics */
    bridge->stats.spatial_predictions_made++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_parietal_bridge_predict_trajectory(
    omni_wm_parietal_bridge_t* bridge,
    uint32_t object_id,
    uint32_t horizon_steps,
    float dt,
    wm_parietal_frame_t target_frame,
    wm_parietal_trajectory_t* trajectory) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_predict_trajectory", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(trajectory, NIMCP_ERROR_NULL_POINTER, "trajectory is NULL");
    NIMCP_CHECK_THROW(trajectory->states, NIMCP_ERROR_INVALID_PARAM, "trajectory states is NULL");
    if (!bridge->config.enable_spatial_prediction) return NIMCP_SUCCESS;

    if (horizon_steps > bridge->config.max_prediction_horizon) {
        horizon_steps = bridge->config.max_prediction_horizon;
    }
    if (dt <= 0.0f) dt = bridge->config.prediction_dt;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Find the object */
    int idx = find_tracked_object(bridge, object_id);
    if (idx < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Initialize trajectory */
    trajectory->object_id = object_id;
    trajectory->dt = dt;
    trajectory->total_duration = dt * (float)horizon_steps;
    trajectory->frame = target_frame;
    trajectory->physics_constrained = bridge->config.enable_physics_constraints;

    /* Copy initial state */
    wm_parietal_spatial_state_t current = bridge->tracked_objects[idx];

    /* Roll out trajectory */
    float confidence_sum = 0.0f;
    for (uint32_t step = 0; step < horizon_steps; step++) {
        /* Phase 8: Loop progress heartbeat */
        if ((step & 0xFF) == 0 && horizon_steps > 256) {
            omni_wm_parietal_bridge_heartbeat("omni_wm_pari_loop",
                             (float)(step + 1) / (float)horizon_steps);
        }

        /* Store current state */
        trajectory->states[step] = current;

        /* Apply physics for next step */
        apply_physics_constraints(bridge, &current, dt);

        /* Decay confidence over time */
        current.confidence *= 0.99f;
        confidence_sum += current.confidence;
    }

    trajectory->length = horizon_steps;
    trajectory->overall_confidence = confidence_sum / (float)horizon_steps;

    /* Update statistics */
    bridge->stats.trajectory_predictions_made++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_parietal_bridge_predict_joint_trajectories(
    omni_wm_parietal_bridge_t* bridge,
    const uint32_t* object_ids,
    uint32_t num_objects,
    uint32_t horizon_steps,
    wm_parietal_trajectory_t** trajectories) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_predict_joint_trajec", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(object_ids, NIMCP_ERROR_NULL_POINTER, "object_ids is NULL");
    NIMCP_CHECK_THROW(trajectories, NIMCP_ERROR_NULL_POINTER, "trajectories is NULL");
    if (num_objects == 0) return NIMCP_SUCCESS;

    /* Predict each trajectory individually for now */
    /* In full implementation, would consider interactions */
    for (uint32_t i = 0; i < num_objects; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_objects > 256) {
            omni_wm_parietal_bridge_heartbeat("omni_wm_pari_loop",
                             (float)(i + 1) / (float)num_objects);
        }

        if (!trajectories[i]) continue;

        nimcp_error_t err = omni_wm_parietal_bridge_predict_trajectory(
            bridge, object_ids[i], horizon_steps,
            bridge->config.prediction_dt,
            bridge->config.default_frame,
            trajectories[i]);

        if (err != NIMCP_SUCCESS) {
            NIMCP_LOGGING_WARN("Failed to predict trajectory for object %u", object_ids[i]);
        }
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Coordinate Transform API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_parietal_bridge_transform_position(
    omni_wm_parietal_bridge_t* bridge,
    const wm_parietal_vec3_t* position,
    wm_parietal_frame_t from_frame,
    wm_parietal_frame_t to_frame,
    wm_parietal_vec3_t* result) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_transform_position", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(position, NIMCP_ERROR_NULL_POINTER, "position is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_NULL_POINTER, "result is NULL");
    if (!bridge->config.enable_coordinate_transforms) {
        *result = *position;
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Simple identity transform for same frame */
    if (from_frame == to_frame) {
        *result = *position;
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_SUCCESS;
    }

    /* Placeholder: simple transform (would use spatial_reasoning in full impl) */
    /* For now, just copy with potential offset based on frame type */
    *result = *position;

    /* Apply frame-specific offsets (placeholder) */
    switch (to_frame) {
        case WM_PARIETAL_FRAME_EGOCENTRIC:
            /* Convert to body-centered (subtract body position) */
            /* Placeholder: assume body at origin */
            break;
        case WM_PARIETAL_FRAME_ALLOCENTRIC:
            /* Already world-centered, no transform needed */
            break;
        case WM_PARIETAL_FRAME_HEAD:
            /* Placeholder: offset by head position */
            break;
        case WM_PARIETAL_FRAME_HAND:
            /* Placeholder: offset by hand position */
            if (bridge->parietal_to_wm.reaching_active) {
                result->x -= bridge->parietal_to_wm.hand_position.x;
                result->y -= bridge->parietal_to_wm.hand_position.y;
                result->z -= bridge->parietal_to_wm.hand_position.z;
            }
            break;
        default:
            break;
    }

    /* Update statistics */
    bridge->stats.coordinate_transforms++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_parietal_bridge_transform_state(
    omni_wm_parietal_bridge_t* bridge,
    const wm_parietal_spatial_state_t* state,
    wm_parietal_frame_t to_frame,
    wm_parietal_spatial_state_t* result) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_transform_state", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_NULL_POINTER, "state is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_NULL_POINTER, "result is NULL");

    /* Copy base state */
    *result = *state;

    /* Transform position */
    nimcp_error_t err = omni_wm_parietal_bridge_transform_position(
        bridge, &state->position, state->frame, to_frame, &result->position);
    if (err != NIMCP_SUCCESS) return err;

    /* Update frame */
    result->frame = to_frame;

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_parietal_bridge_get_transform_matrix(
    omni_wm_parietal_bridge_t* bridge,
    wm_parietal_frame_t from_frame,
    wm_parietal_frame_t to_frame,
    float* matrix) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_get_transform_matrix", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(matrix, NIMCP_ERROR_NULL_POINTER, "matrix is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Initialize to identity matrix */
    memset(matrix, 0, 16 * sizeof(float));
    matrix[0] = 1.0f;
    matrix[5] = 1.0f;
    matrix[10] = 1.0f;
    matrix[15] = 1.0f;

    /* Placeholder: would compute actual transform matrix */
    (void)from_frame;
    (void)to_frame;

    bridge->stats.coordinate_transforms++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Physics Constraint API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_parietal_bridge_add_physics_constraint(
    omni_wm_parietal_bridge_t* bridge,
    const wm_parietal_physics_constraint_t* constraint) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_add_physics_constrai", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(constraint, NIMCP_ERROR_NULL_POINTER, "constraint is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->num_constraints >= bridge->constraints_capacity) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NO_MEMORY;
    }

    bridge->constraints[bridge->num_constraints] = *constraint;
    bridge->num_constraints++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_parietal_bridge_remove_physics_constraint(
    omni_wm_parietal_bridge_t* bridge,
    wm_parietal_physics_type_t type,
    uint32_t object_id) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_remove_physics_const", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    for (uint32_t i = 0; i < bridge->num_constraints; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_constraints > 256) {
            omni_wm_parietal_bridge_heartbeat("omni_wm_pari_loop",
                             (float)(i + 1) / (float)bridge->num_constraints);
        }

        if (bridge->constraints[i].type == type &&
            bridge->constraints[i].object_id == object_id) {
            /* Shift remaining constraints down */
            for (uint32_t j = i; j < bridge->num_constraints - 1; j++) {
                bridge->constraints[j] = bridge->constraints[j + 1];
            }
            bridge->num_constraints--;
            nimcp_mutex_unlock(bridge->base.mutex);
            return NIMCP_SUCCESS;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_ERROR_NOT_FOUND;
}

nimcp_error_t omni_wm_parietal_bridge_clear_physics_constraints(
    omni_wm_parietal_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_clear_physics_constr", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->num_constraints = 0;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_parietal_bridge_check_collision(
    omni_wm_parietal_bridge_t* bridge,
    uint32_t object_a,
    uint32_t object_b,
    float horizon_seconds,
    bool* will_collide,
    float* time_to_collision,
    wm_parietal_vec3_t* collision_point) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_check_collision", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Null outputs are handled gracefully - return success with no writes */
    if (!will_collide) {
        return NIMCP_SUCCESS;
    }

    *will_collide = false;
    if (time_to_collision) *time_to_collision = -1.0f;

    if (!bridge->config.enable_collision_prediction) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    int idx_a = find_tracked_object(bridge, object_a);
    int idx_b = find_tracked_object(bridge, object_b);

    if (idx_a < 0 || idx_b < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Copy states for simulation */
    wm_parietal_spatial_state_t state_a = bridge->tracked_objects[idx_a];
    wm_parietal_spatial_state_t state_b = bridge->tracked_objects[idx_b];

    float dt = bridge->config.physics_dt;
    float time = 0.0f;

    while (time < horizon_seconds) {
        /* Step physics */
        apply_physics_constraints(bridge, &state_a, dt);
        apply_physics_constraints(bridge, &state_b, dt);

        /* Check distance */
        float dx = state_a.position.x - state_b.position.x;
        float dy = state_a.position.y - state_b.position.y;
        float dz = state_a.position.z - state_b.position.z;
        float dist = sqrtf(dx*dx + dy*dy + dz*dz);

        float threshold = state_a.bounding_radius + state_b.bounding_radius +
                         bridge->config.collision_epsilon;

        if (dist < threshold) {
            *will_collide = true;
            if (time_to_collision) *time_to_collision = time;
            if (collision_point) {
                /* Midpoint between objects */
                collision_point->x = (state_a.position.x + state_b.position.x) / 2.0f;
                collision_point->y = (state_a.position.y + state_b.position.y) / 2.0f;
                collision_point->z = (state_a.position.z + state_b.position.z) / 2.0f;
            }
            bridge->stats.collisions_predicted++;
            break;
        }

        time += dt;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_parietal_bridge_physics_step(
    omni_wm_parietal_bridge_t* bridge,
    float dt) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_physics_step", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Validate dt - negative timesteps are invalid for physics */
    if (dt < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "omni_wm_parietal_bridge_physics_step: negative dt (%.4f)", dt);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    for (uint32_t i = 0; i < bridge->num_tracked_objects; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_tracked_objects > 256) {
            omni_wm_parietal_bridge_heartbeat("omni_wm_pari_loop",
                             (float)(i + 1) / (float)bridge->num_tracked_objects);
        }

        apply_physics_constraints(bridge, &bridge->tracked_objects[i], dt);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Spatial Attention API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_parietal_bridge_update_attention(
    omni_wm_parietal_bridge_t* bridge,
    const float* attention_map,
    uint32_t map_dim) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_update_attention", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(attention_map, NIMCP_ERROR_NULL_POINTER, "attention_map is NULL");
    NIMCP_CHECK_THROW(map_dim > 0, NIMCP_ERROR_INVALID_PARAM, "update_attention: map_dim is zero");
    if (!bridge->config.enable_attention_gating) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t current_dim = bridge->attention_map_dim;
    uint32_t copy_dim = (map_dim < current_dim) ? map_dim : current_dim;
    uint32_t copy_size = copy_dim * copy_dim * copy_dim;

    memcpy(bridge->attention_map, attention_map, copy_size * sizeof(float));

    bridge->stats.attention_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_parietal_bridge_set_attention_focus(
    omni_wm_parietal_bridge_t* bridge,
    const wm_parietal_vec3_t* focus,
    float spread) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_set_attention_focus", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(focus, NIMCP_ERROR_NULL_POINTER, "focus is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    wm_parietal_vec3_t old_focus = bridge->current_focus;
    bridge->current_focus = *focus;

    /* Check if focus shifted significantly */
    float dx = focus->x - old_focus.x;
    float dy = focus->y - old_focus.y;
    float dz = focus->z - old_focus.z;
    float shift_dist = sqrtf(dx*dx + dy*dy + dz*dz);

    if (shift_dist > spread * 0.5f) {
        bridge->stats.attention_shifts++;
    }

    /* Update attention map with Gaussian around focus */
    if (bridge->attention_map && bridge->attention_map_dim > 0) {
        uint32_t dim = bridge->attention_map_dim;
        float sigma_sq = spread * spread;
        float total = 0.0f;

        for (uint32_t z = 0; z < dim; z++) {
            /* Phase 8: Loop progress heartbeat */
            if ((z & 0xFF) == 0 && dim > 256) {
                omni_wm_parietal_bridge_heartbeat("omni_wm_pari_loop",
                                 (float)(z + 1) / (float)dim);
            }

            for (uint32_t y = 0; y < dim; y++) {
                /* Phase 8: Loop progress heartbeat */
                if ((y & 0xFF) == 0 && dim > 256) {
                    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_loop",
                                     (float)(y + 1) / (float)dim);
                }

                for (uint32_t x = 0; x < dim; x++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((x & 0xFF) == 0 && dim > 256) {
                        omni_wm_parietal_bridge_heartbeat("omni_wm_pari_loop",
                                         (float)(x + 1) / (float)dim);
                    }

                    /* Map grid to spatial coordinates */
                    float gx = (dim > 1) ? (float)x / (float)(dim - 1) - 0.5f : 0.0f;
                    float gy = (dim > 1) ? (float)y / (float)(dim - 1) - 0.5f : 0.0f;
                    float gz = (dim > 1) ? (float)z / (float)(dim - 1) - 0.5f : 0.0f;

                    /* Distance from focus */
                    float fx = gx - focus->x;
                    float fy = gy - focus->y;
                    float fz = gz - focus->z;
                    float dist_sq = fx*fx + fy*fy + fz*fz;

                    /* Gaussian weight */
                    float weight = expf(-0.5f * dist_sq / sigma_sq);

                    uint32_t idx = z * dim * dim + y * dim + x;
                    bridge->attention_map[idx] = weight;
                    total += weight;
                }
            }
        }

        /* Normalize */
        if (total > FLOAT_EPSILON) {
            uint32_t size = dim * dim * dim;
            for (uint32_t i = 0; i < size; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && size > 256) {
                    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_loop",
                                     (float)(i + 1) / (float)size);
                }

                bridge->attention_map[i] /= total;
            }
        }
    }

    /* Update parietal effects */
    bridge->parietal_to_wm.attention_focus = *focus;
    bridge->parietal_to_wm.attention_spread = spread;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

float omni_wm_parietal_bridge_get_attention_at(
    const omni_wm_parietal_bridge_t* bridge,
    const wm_parietal_vec3_t* position) {

    if (!bridge || !position) return 0.0f;
    if (!bridge->attention_map || bridge->attention_map_dim == 0) return 1.0f;

    /* Map position to grid coordinates (assuming -0.5 to 0.5 range) */
    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_get_attention_at", 0.0f);


    uint32_t dim = bridge->attention_map_dim;
    float px = (position->x + 0.5f) * (float)(dim - 1);
    float py = (position->y + 0.5f) * (float)(dim - 1);
    float pz = (position->z + 0.5f) * (float)(dim - 1);

    /* Clamp to valid range */
    uint32_t x = (uint32_t)fmaxf(0.0f, fminf(px, (float)(dim - 1)));
    uint32_t y = (uint32_t)fmaxf(0.0f, fminf(py, (float)(dim - 1)));
    uint32_t z = (uint32_t)fmaxf(0.0f, fminf(pz, (float)(dim - 1)));

    uint32_t idx = z * dim * dim + y * dim + x;
    return bridge->attention_map[idx];
}

/* ============================================================================
 * Object Tracking API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_parietal_bridge_track_object(
    omni_wm_parietal_bridge_t* bridge,
    const wm_parietal_spatial_state_t* initial_state) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_track_object", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(initial_state, NIMCP_ERROR_NULL_POINTER, "initial_state is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if already tracking */
    int idx = find_tracked_object(bridge, initial_state->object_id);
    if (idx >= 0) {
        /* Update existing */
        bridge->tracked_objects[idx] = *initial_state;
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_SUCCESS;
    }

    /* Check capacity */
    if (bridge->num_tracked_objects >= bridge->tracked_objects_capacity) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Add new object */
    bridge->tracked_objects[bridge->num_tracked_objects] = *initial_state;
    bridge->num_tracked_objects++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_parietal_bridge_update_object(
    omni_wm_parietal_bridge_t* bridge,
    uint32_t object_id,
    const wm_parietal_spatial_state_t* new_state) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_update_object", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(new_state, NIMCP_ERROR_NULL_POINTER, "new_state is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    int idx = find_tracked_object(bridge, object_id);
    if (idx < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    bridge->tracked_objects[idx] = *new_state;
    bridge->tracked_objects[idx].timestamp = (double)get_current_time_us() / 1000000.0;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_parietal_bridge_untrack_object(
    omni_wm_parietal_bridge_t* bridge,
    uint32_t object_id) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_untrack_object", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    int idx = find_tracked_object(bridge, object_id);
    if (idx < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Shift remaining objects down */
    for (uint32_t i = (uint32_t)idx; i < bridge->num_tracked_objects - 1; i++) {
        bridge->tracked_objects[i] = bridge->tracked_objects[i + 1];
    }
    bridge->num_tracked_objects--;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_parietal_bridge_get_object_state(
    const omni_wm_parietal_bridge_t* bridge,
    uint32_t object_id,
    wm_parietal_spatial_state_t* state) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_get_object_state", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_NULL_POINTER, "state is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    int idx = find_tracked_object(bridge, object_id);
    if (idx < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    *state = bridge->tracked_objects[idx];

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Mathematical Reasoning API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_parietal_bridge_math_predict(
    omni_wm_parietal_bridge_t* bridge,
    const float* observation_sequence,
    uint32_t sequence_length,
    uint32_t prediction_steps,
    float* predictions,
    float* confidence) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_math_predict", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(observation_sequence, NIMCP_ERROR_NULL_POINTER, "observation_sequence is NULL");
    NIMCP_CHECK_THROW(predictions, NIMCP_ERROR_NULL_POINTER, "predictions is NULL");
    NIMCP_CHECK_THROW(sequence_length > 0, NIMCP_ERROR_INVALID_PARAM, "sequence_length must be greater than 0");
    NIMCP_CHECK_THROW(prediction_steps > 0, NIMCP_ERROR_INVALID_PARAM, "prediction_steps must be greater than 0");
    if (!bridge->config.enable_math_reasoning) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Simple linear extrapolation as placeholder */
    /* In full implementation, would use parietal_lobe pattern detection */

    float last = observation_sequence[sequence_length - 1];
    float second_last = (sequence_length > 1) ? observation_sequence[sequence_length - 2] : last;
    float slope = last - second_last;

    for (uint32_t i = 0; i < prediction_steps; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && prediction_steps > 256) {
            omni_wm_parietal_bridge_heartbeat("omni_wm_pari_loop",
                             (float)(i + 1) / (float)prediction_steps);
        }

        predictions[i] = last + slope * (float)(i + 1);
    }

    if (confidence) {
        /* Confidence decreases with prediction horizon */
        *confidence = 0.9f / (1.0f + 0.1f * (float)prediction_steps);
    }

    bridge->stats.math_predictions++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_parietal_bridge_estimate_quantity(
    omni_wm_parietal_bridge_t* bridge,
    const float* values,
    uint32_t num_values,
    float* estimate,
    float* confidence) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_estimate_quantity", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(values, NIMCP_ERROR_NULL_POINTER, "values is NULL");
    NIMCP_CHECK_THROW(estimate, NIMCP_ERROR_NULL_POINTER, "estimate is NULL");
    NIMCP_CHECK_THROW(num_values > 0, NIMCP_ERROR_INVALID_PARAM, "num_values must be greater than 0");
    if (!bridge->config.enable_numerical_estimation) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Simple mean estimation as placeholder */
    /* In full implementation, would use parietal_lobe number sense */

    float sum = 0.0f;
    for (uint32_t i = 0; i < num_values; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_values > 256) {
            omni_wm_parietal_bridge_heartbeat("omni_wm_pari_loop",
                             (float)(i + 1) / (float)num_values);
        }

        sum += values[i];
    }
    *estimate = sum / (float)num_values;

    if (confidence) {
        /* Confidence based on value spread */
        float variance = 0.0f;
        for (uint32_t i = 0; i < num_values; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_values > 256) {
                omni_wm_parietal_bridge_heartbeat("omni_wm_pari_loop",
                                 (float)(i + 1) / (float)num_values);
            }

            float diff = values[i] - *estimate;
            variance += diff * diff;
        }
        variance /= (float)num_values;

        /* Low variance = high confidence */
        *confidence = 1.0f / (1.0f + sqrtf(variance));
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

const omni_wm_to_parietal_effects_t* omni_wm_parietal_bridge_get_wm_effects(
    const omni_wm_parietal_bridge_t* bridge) {

    if (!bridge) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return NULL;


    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_get_wm_effects", 0.0f);


    return &bridge->wm_to_parietal;
}

const parietal_to_omni_wm_effects_t* omni_wm_parietal_bridge_get_parietal_effects(
    const omni_wm_parietal_bridge_t* bridge) {

    if (!bridge) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return NULL;


    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_get_parietal_effects", 0.0f);


    return &bridge->parietal_to_wm;
}

nimcp_error_t omni_wm_parietal_bridge_get_stats(
    const omni_wm_parietal_bridge_t* bridge,
    omni_wm_parietal_bridge_stats_t* stats) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_get_stats", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_parietal_bridge_reset_stats(
    omni_wm_parietal_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_reset_stats", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(omni_wm_parietal_bridge_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_parietal_bridge_connect_bio_async(
    omni_wm_parietal_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_connect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_bio_async) return NIMCP_SUCCESS;
    if (bridge->base.bio_async_enabled) return NIMCP_SUCCESS;

    /* Check if router is initialized */
    if (!bio_router_is_initialized()) {
        NIMCP_LOGGING_DEBUG("Bio-async router not initialized, skipping registration");
        return NIMCP_SUCCESS;
    }

    /* Register module with router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_WM_PARIETAL_BRIDGE,
        .module_name = "wm_parietal_bridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (!bridge->base.bio_ctx) {
        NIMCP_LOGGING_WARN("Failed to register with bio-async router");
        return NIMCP_SUCCESS;
    }

    /* Register message handlers */
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_PARIETAL_SPATIAL_PRED,
                                handle_spatial_pred);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_PARIETAL_TRAJECTORY_PRED,
                                handle_trajectory_pred);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_PARIETAL_COORD_TRANSFORM,
                                handle_coord_transform);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_PARIETAL_PHYSICS_QUERY,
                                handle_physics_query);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_PARIETAL_ATTENTION_UPDATE,
                                handle_attention_update);

    bridge->base.bio_async_enabled = true;
    NIMCP_LOGGING_INFO("WM Parietal Bridge connected to bio-async router");

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_parietal_bridge_disconnect_bio_async(
    omni_wm_parietal_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_disconnect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->base.bio_async_enabled) return NIMCP_SUCCESS;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("WM Parietal Bridge disconnected from bio-async router");

    return NIMCP_SUCCESS;
}

bool omni_wm_parietal_bridge_is_bio_async_connected(
    const omni_wm_parietal_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_is_bio_async_connect", 0.0f);


    return bridge_base_is_bio_async_connected(bridge ? &bridge->base : NULL);
}

/* ============================================================================
 * Utility Functions Implementation
 * ============================================================================ */

const char* omni_wm_parietal_msg_type_to_string(omni_wm_parietal_msg_type_t msg_type) {
    switch (msg_type) {
        case BIO_MSG_WM_PARIETAL_SPATIAL_PRED:
            return "SPATIAL_PRED";
        case BIO_MSG_WM_PARIETAL_SPATIAL_PRED_RESULT:
            return "SPATIAL_PRED_RESULT";
        case BIO_MSG_WM_PARIETAL_TRAJECTORY_PRED:
            return "TRAJECTORY_PRED";
        case BIO_MSG_WM_PARIETAL_TRAJECTORY_RESULT:
            return "TRAJECTORY_RESULT";
        case BIO_MSG_WM_PARIETAL_COORD_TRANSFORM:
            return "COORD_TRANSFORM";
        case BIO_MSG_WM_PARIETAL_COORD_RESULT:
            return "COORD_RESULT";
        case BIO_MSG_WM_PARIETAL_FRAME_UPDATE:
            return "FRAME_UPDATE";
        case BIO_MSG_WM_PARIETAL_PHYSICS_QUERY:
            return "PHYSICS_QUERY";
        case BIO_MSG_WM_PARIETAL_PHYSICS_RESULT:
            return "PHYSICS_RESULT";
        case BIO_MSG_WM_PARIETAL_PHYSICS_CONSTRAINT:
            return "PHYSICS_CONSTRAINT";
        case BIO_MSG_WM_PARIETAL_COLLISION_CHECK:
            return "COLLISION_CHECK";
        case BIO_MSG_WM_PARIETAL_ATTENTION_UPDATE:
            return "ATTENTION_UPDATE";
        case BIO_MSG_WM_PARIETAL_SALIENCE_MAP:
            return "SALIENCE_MAP";
        case BIO_MSG_WM_PARIETAL_FOCUS_SHIFT:
            return "FOCUS_SHIFT";
        case BIO_MSG_WM_PARIETAL_MATH_PRED:
            return "MATH_PRED";
        case BIO_MSG_WM_PARIETAL_NUMERICAL_EST:
            return "NUMERICAL_EST";
        case BIO_MSG_WM_PARIETAL_PATTERN_EXTRAP:
            return "PATTERN_EXTRAP";
        case BIO_MSG_WM_PARIETAL_BRIDGE_STATUS:
            return "BRIDGE_STATUS";
        case BIO_MSG_WM_PARIETAL_BRIDGE_ERROR:
            return "BRIDGE_ERROR";
        case BIO_MSG_WM_PARIETAL_STATS_UPDATE:
            return "STATS_UPDATE";
        default:
            return "UNKNOWN";
    }
}

const char* omni_wm_parietal_frame_to_string(wm_parietal_frame_t frame) {
    switch (frame) {
        case WM_PARIETAL_FRAME_EGOCENTRIC:
            return "EGOCENTRIC";
        case WM_PARIETAL_FRAME_ALLOCENTRIC:
            return "ALLOCENTRIC";
        case WM_PARIETAL_FRAME_OBJECT:
            return "OBJECT";
        case WM_PARIETAL_FRAME_RETINOTOPIC:
            return "RETINOTOPIC";
        case WM_PARIETAL_FRAME_HEAD:
            return "HEAD";
        case WM_PARIETAL_FRAME_HAND:
            return "HAND";
        default:
            return "UNKNOWN";
    }
}

const char* omni_wm_parietal_physics_type_to_string(wm_parietal_physics_type_t type) {
    switch (type) {
        case WM_PARIETAL_PHYSICS_GRAVITY:
            return "GRAVITY";
        case WM_PARIETAL_PHYSICS_COLLISION:
            return "COLLISION";
        case WM_PARIETAL_PHYSICS_MOMENTUM:
            return "MOMENTUM";
        case WM_PARIETAL_PHYSICS_ENERGY:
            return "ENERGY";
        case WM_PARIETAL_PHYSICS_FRICTION:
            return "FRICTION";
        case WM_PARIETAL_PHYSICS_BOUNDARY:
            return "BOUNDARY";
        case WM_PARIETAL_PHYSICS_CUSTOM:
            return "CUSTOM";
        default:
            return "UNKNOWN";
    }
}

nimcp_error_t omni_wm_parietal_bridge_validate_config(
    const omni_wm_parietal_bridge_config_t* config) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_validate_config", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    /* Validate sensitivity range */
    if (config->sensitivity < 0.5f || config->sensitivity > 2.0f) {
        NIMCP_LOGGING_WARN("Sensitivity %.2f out of range [0.5, 2.0]",
                          config->sensitivity);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Validate prediction settings */
    if (config->enable_spatial_prediction) {
        if (config->max_prediction_horizon == 0 ||
            config->max_prediction_horizon > WM_PARIETAL_MAX_TRAJECTORY_HORIZON) {
            NIMCP_LOGGING_WARN("Invalid max_prediction_horizon: %u",
                              config->max_prediction_horizon);
            return NIMCP_ERROR_INVALID_PARAM;
        }
        if (config->prediction_dt <= 0.0f || config->prediction_dt > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid prediction_dt: %.4f",
                              config->prediction_dt);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate physics settings */
    if (config->enable_physics_constraints) {
        if (config->physics_dt <= 0.0f || config->physics_dt > 0.1f) {
            NIMCP_LOGGING_WARN("Invalid physics_dt: %.4f",
                              config->physics_dt);
            return NIMCP_ERROR_INVALID_PARAM;
        }
        if (config->gravity_magnitude < 0.0f) {
            NIMCP_LOGGING_WARN("Invalid gravity_magnitude: %.2f",
                              config->gravity_magnitude);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate attention settings */
    if (config->enable_attention_gating) {
        if (config->attention_resolution == 0 ||
            config->attention_resolution > 64) {
            NIMCP_LOGGING_WARN("Invalid attention_resolution: %u",
                              config->attention_resolution);
            return NIMCP_ERROR_INVALID_PARAM;
        }
        if (config->attention_decay_rate < 0.0f ||
            config->attention_decay_rate > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid attention_decay_rate: %.2f",
                              config->attention_decay_rate);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    return NIMCP_SUCCESS;
}

wm_parietal_spatial_state_t omni_wm_parietal_create_state(
    uint32_t object_id,
    float x, float y, float z,
    wm_parietal_frame_t frame) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_omni_wm_parietal_cre", 0.0f);


    wm_parietal_spatial_state_t state;
    memset(&state, 0, sizeof(wm_parietal_spatial_state_t));

    state.object_id = object_id;
    state.position.x = x;
    state.position.y = y;
    state.position.z = z;
    state.velocity.vx = 0.0f;
    state.velocity.vy = 0.0f;
    state.velocity.vz = 0.0f;
    state.orientation.w = 1.0f;
    state.orientation.x = 0.0f;
    state.orientation.y = 0.0f;
    state.orientation.z = 0.0f;
    state.angular_vel.x = 0.0f;
    state.angular_vel.y = 0.0f;
    state.angular_vel.z = 0.0f;
    state.mass = 1.0f;
    state.bounding_radius = 0.1f;
    state.frame = frame;
    state.confidence = 1.0f;
    state.timestamp = (double)get_current_time_us() / 1000000.0;

    return state;
}

wm_parietal_trajectory_t* omni_wm_parietal_trajectory_create(uint32_t max_length) {
    if (max_length == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_parietal_trajectory_create: max_length is zero");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_omni_wm_parietal_tra", 0.0f);


    wm_parietal_trajectory_t* traj = nimcp_calloc(1, sizeof(wm_parietal_trajectory_t));
    if (!traj) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate traj");

        return NULL;

    }

    traj->states = nimcp_calloc(max_length, sizeof(wm_parietal_spatial_state_t));
    if (!traj->states) {
        nimcp_free(traj);
        traj = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_wm_parietal_trajectory_create: traj->states is NULL");
        return NULL;
    }

    traj->length = 0;
    traj->dt = 0.0f;
    traj->total_duration = 0.0f;
    traj->frame = WM_PARIETAL_FRAME_ALLOCENTRIC;
    traj->overall_confidence = 0.0f;
    traj->physics_constrained = false;

    return traj;
}

void omni_wm_parietal_trajectory_destroy(wm_parietal_trajectory_t* trajectory) {
    if (!trajectory) return;

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_omni_wm_parietal_tra", 0.0f);


    nimcp_free(trajectory->states);
    nimcp_free(trajectory);
    trajectory = NULL;
}

float omni_wm_parietal_distance(
    const wm_parietal_vec3_t* a,
    const wm_parietal_vec3_t* b) {

    if (!a || !b) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_omni_wm_parietal_dis", 0.0f);


    float dx = a->x - b->x;
    float dy = a->y - b->y;
    float dz = a->z - b->z;

    return sqrtf(dx*dx + dy*dy + dz*dz);
}

nimcp_error_t omni_wm_parietal_normalize(
    const wm_parietal_vec3_t* v,
    wm_parietal_vec3_t* result) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_parietal_bridge_heartbeat("omni_wm_pari_omni_wm_parietal_nor", 0.0f);


    NIMCP_CHECK_THROW(v, NIMCP_ERROR_NULL_POINTER, "v is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_NULL_POINTER, "result is NULL");

    float len = sqrtf(v->x*v->x + v->y*v->y + v->z*v->z);

    if (len < FLOAT_EPSILON) {
        result->x = 0.0f;
        result->y = 0.0f;
        result->z = 0.0f;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "omni_wm_parietal_normalize: zero-length vector cannot be normalized");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    result->x = v->x / (fabsf(len) > 1e-7f ? len : 1e-7f);
    result->y = v->y / (fabsf(len) > 1e-7f ? len : 1e-7f);
    result->z = v->z / (fabsf(len) > 1e-7f ? len : 1e-7f);

    return NIMCP_SUCCESS;
}
