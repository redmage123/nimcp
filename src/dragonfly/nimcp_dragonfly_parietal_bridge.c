/**
 * @file nimcp_dragonfly_parietal_bridge.c
 * @brief Parietal Lobe Bridge for Dragonfly Module - Implementation
 *
 * Spatial processing and visuomotor coordination for target tracking.
 *
 * @author NIMCP Team
 * @date 2024-12-28
 */

#include "dragonfly/nimcp_dragonfly_parietal_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_dimension_constants.h"  /* Centralized dimension constants */
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_learning_constants.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_parietal_bridge)

#define LOG_MODULE "DRAGONFLY_PARIETAL_BRIDGE"



//=============================================================================
// Internal Time Helper
//=============================================================================

static inline uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Parietal bridge internal state
 */
struct dragonfly_parietal_bridge_s {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */

    /* Connected systems */
    dragonfly_system_t* dragonfly;
    spatial_reasoning_t* spatial_reasoning;
    parietal_lobe_t* parietal;

    /* Configuration */
    parietal_bridge_config_t config;

    /* State */
    bool initialized;

    /* Observer state */
    observer_state_t observer;
    bool observer_set;

    /* Target cache */
    parietal_target_t targets[PARIETAL_BRIDGE_MAX_TARGETS];
    uint32_t num_targets;
    uint32_t primary_target_id;

    /* Attention map */
    parietal_attention_map_t* internal_attention;

    /* Statistics */
    parietal_bridge_stats_t stats;
    uint64_t transform_time_sum;
};

//=============================================================================
// Math Utilities
//=============================================================================

static float vec3_length(const parietal_vec3_t* v) {
    return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
}

static parietal_vec3_t vec3_normalize(const parietal_vec3_t* v) {
    float len = vec3_length(v);
    if (len < 1e-10f) {
        parietal_vec3_t zero = {0, 0, 0};
        return zero;
    }
    parietal_vec3_t result = {v->x / len, v->y / len, v->z / len};
    return result;
}

static parietal_vec3_t vec3_sub(const parietal_vec3_t* a, const parietal_vec3_t* b) {
    parietal_vec3_t result = {a->x - b->x, a->y - b->y, a->z - b->z};
    return result;
}

void dragonfly_parietal_quat_normalize(parietal_quat_t* q) {
    float len = sqrtf(q->w*q->w + q->x*q->x + q->y*q->y + q->z*q->z);
    if (len > 1e-10f) {
        q->w /= len;
        q->x /= len;
        q->y /= len;
        q->z /= len;
    }
}

parietal_quat_t dragonfly_parietal_quat_from_euler(float roll, float pitch, float yaw) {
    float cr = cosf(roll * 0.5f);
    float sr = sinf(roll * 0.5f);
    float cp = cosf(pitch * 0.5f);
    float sp = sinf(pitch * 0.5f);
    float cy = cosf(yaw * 0.5f);
    float sy = sinf(yaw * 0.5f);

    parietal_quat_t q;
    q.w = cr * cp * cy + sr * sp * sy;
    q.x = sr * cp * cy - cr * sp * sy;
    q.y = cr * sp * cy + sr * cp * sy;
    q.z = cr * cp * sy - sr * sp * cy;

    dragonfly_parietal_quat_normalize(&q);
    return q;
}

static parietal_quat_t quat_conjugate(const parietal_quat_t* q) {
    parietal_quat_t conj = {q->w, -q->x, -q->y, -q->z};
    return conj;
}

static parietal_quat_t quat_multiply(const parietal_quat_t* a, const parietal_quat_t* b) {
    parietal_quat_t result;
    result.w = a->w*b->w - a->x*b->x - a->y*b->y - a->z*b->z;
    result.x = a->w*b->x + a->x*b->w + a->y*b->z - a->z*b->y;
    result.y = a->w*b->y - a->x*b->z + a->y*b->w + a->z*b->x;
    result.z = a->w*b->z + a->x*b->y - a->y*b->x + a->z*b->w;
    return result;
}

parietal_vec3_t dragonfly_parietal_quat_rotate_vec(const parietal_quat_t* q, const parietal_vec3_t* v) {
    /* q * v * q^(-1) where v is treated as quaternion (0, vx, vy, vz) */
    parietal_quat_t v_quat = {0, v->x, v->y, v->z};
    parietal_quat_t q_conj = quat_conjugate(q);

    parietal_quat_t temp = quat_multiply(q, &v_quat);
    parietal_quat_t result = quat_multiply(&temp, &q_conj);

    parietal_vec3_t rotated = {result.x, result.y, result.z};
    return rotated;
}

//=============================================================================
// Configuration Functions
//=============================================================================

parietal_bridge_config_t parietal_bridge_default_config(void) {
    parietal_bridge_config_t config = {0};

    /* Coordinate transform */
    config.auto_transform = true;
    config.default_output_frame = COORD_FRAME_BODY;

    /* Attention - use centralized dimension constants */
    config.enable_attention = true;
    config.attention_map_width = NIMCP_ATTENTION_MAP_WIDTH;   /* Was: 64 */
    config.attention_map_height = NIMCP_ATTENTION_MAP_WIDTH / 2;  /* Was: 32 */
    config.attention_decay = NIMCP_ELIGIBILITY_DECAY_DEFAULT;

    /* Motor commands */
    config.generate_motor_commands = true;
    config.saccade_threshold = 0.1f;  /* ~6 degrees */
    config.pursuit_gain = 0.95f;
    config.motor_latency_ms = 150.0f;  /* 150ms typical motor latency */

    /* Gain fields */
    config.enable_gain_fields = true;
    config.gain_field_sigma = 0.5f;

    /* K-D tree indexing */
    config.enable_spatial_index = true;
    config.query_radius_default = 10.0f;

    return config;
}

bool parietal_bridge_validate_config(const parietal_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_bridge_validate_config: config is NULL");
        return false;
    }

    /* Use centralized constants for validation limits */
    if (config->attention_map_width == 0 || config->attention_map_width > NIMCP_ATTENTION_MAP_MAX_WIDTH) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parietal_bridge_validate_config: config->attention_map_width is zero");
        return false;
    }
    if (config->attention_map_height == 0 || config->attention_map_height > NIMCP_ATTENTION_MAP_MAX_HEIGHT / 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parietal_bridge_validate_config: config->attention_map_height is zero");
        return false;
    }
    if (config->attention_decay < 0 || config->attention_decay > 1.0f) {
        return false;
    }
    if (config->saccade_threshold < 0 || config->saccade_threshold > M_PI) {
        return false;
    }
    if (config->pursuit_gain < 0 || config->pursuit_gain > 2.0f) {
        return false;
    }
    if (config->motor_latency_ms < 0 || config->motor_latency_ms > 1000.0f) {
        return false;
    }
    if (config->gain_field_sigma <= 0) {
        return false;
    }
    if (config->query_radius_default <= 0) {
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

dragonfly_parietal_bridge_t* dragonfly_parietal_bridge_create(
    dragonfly_system_t* dragonfly,
    spatial_reasoning_t* spatial_reasoning,
    parietal_lobe_t* parietal,
    const parietal_bridge_config_t* config
) {
    dragonfly_parietal_bridge_t* bridge = nimcp_calloc(1, sizeof(dragonfly_parietal_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "dragonfly_parietal_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Store connected systems */
    bridge->dragonfly = dragonfly;
    bridge->spatial_reasoning = spatial_reasoning;
    bridge->parietal = parietal;

    /* Apply configuration */
    if (config) {
        if (!parietal_bridge_validate_config(config)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "dragonfly_parietal_bridge_create: invalid configuration");
            nimcp_free(bridge);
            return NULL;
        }
        bridge->config = *config;
    } else {
        bridge->config = parietal_bridge_default_config();
    }

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "dragonfly_parietal") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "dragonfly_parietal_bridge_create: failed to initialize base bridge");
        nimcp_free(bridge);
        return NULL;
    }

    /* Initialize observer to default (origin, looking forward) */
    bridge->observer.position = (parietal_vec3_t){0, 0, 0};
    bridge->observer.orientation = (parietal_quat_t){1, 0, 0, 0};  /* Identity */
    bridge->observer.heading = 0;
    bridge->observer.pitch = 0;
    bridge->observer.roll = 0;
    bridge->observer.frame = COORD_FRAME_WORLD;
    bridge->observer_set = false;

    /* Create internal attention map */
    if (bridge->config.enable_attention) {
        bridge->internal_attention = parietal_attention_map_create(
            bridge->config.attention_map_width,
            bridge->config.attention_map_height
        );
    }

    bridge->initialized = true;
    return bridge;
}

void dragonfly_parietal_bridge_destroy(dragonfly_parietal_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "dragonfly_parietal");

    if (bridge->internal_attention) {
        parietal_attention_map_destroy(bridge->internal_attention);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int dragonfly_parietal_bridge_reset(dragonfly_parietal_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_parietal_bridge_reset: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    memset(bridge->targets, 0, sizeof(bridge->targets));
    bridge->num_targets = 0;
    bridge->primary_target_id = 0;

    /* Reset attention map */
    if (bridge->internal_attention) {
        memset(bridge->internal_attention->weights, 0,
               bridge->config.attention_map_width * bridge->config.attention_map_height * sizeof(float));
        bridge->internal_attention->total_attention = 0;
        bridge->internal_attention->peak_weight = 0;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Observer State Functions
//=============================================================================

int dragonfly_parietal_bridge_set_observer(
    dragonfly_parietal_bridge_t* bridge,
    const observer_state_t* observer
) {
    if (!bridge || !bridge->initialized || !observer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_parietal_bridge_set_observer: required parameter is NULL (bridge, bridge->initialized, observer)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->observer = *observer;
    bridge->observer_set = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dragonfly_parietal_bridge_get_observer(
    const dragonfly_parietal_bridge_t* bridge,
    observer_state_t* observer
) {
    if (!bridge || !bridge->initialized || !observer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_parietal_bridge_get_observer: required parameter is NULL (bridge, bridge->initialized, observer)");
        return -1;
    }

    nimcp_mutex_lock(((dragonfly_parietal_bridge_t*)bridge)->base.mutex);
    *observer = bridge->observer;
    nimcp_mutex_unlock(((dragonfly_parietal_bridge_t*)bridge)->base.mutex);

    return 0;
}

//=============================================================================
// Coordinate Transform Functions
//=============================================================================

int dragonfly_parietal_bridge_transform_position(
    const dragonfly_parietal_bridge_t* bridge,
    parietal_vec3_t* position,
    coordinate_frame_t from_frame,
    coordinate_frame_t to_frame
) {
    if (!bridge || !bridge->initialized || !position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_parietal_bridge_transform_position: required parameter is NULL (bridge, bridge->initialized, position)");
        return -1;
    }
    if (from_frame == to_frame) return 0;  /* No transform needed */

    dragonfly_parietal_bridge_t* mutable_bridge = (dragonfly_parietal_bridge_t*)bridge;
    nimcp_mutex_lock(mutable_bridge->base.mutex);

    /* Get observer state */
    observer_state_t obs = bridge->observer;

    /* Transform to world first (if not already) */
    parietal_vec3_t world_pos = *position;

    if (from_frame == COORD_FRAME_BODY || from_frame == COORD_FRAME_HEAD) {
        /* Body/head to world: rotate by observer orientation and translate */
        world_pos = dragonfly_parietal_quat_rotate_vec(&obs.orientation, &world_pos);
        world_pos.x += obs.position.x;
        world_pos.y += obs.position.y;
        world_pos.z += obs.position.z;
    } else if (from_frame == COORD_FRAME_EYE) {
        /* Eye to world: similar but may have additional eye offset */
        world_pos = dragonfly_parietal_quat_rotate_vec(&obs.orientation, &world_pos);
        world_pos.x += obs.position.x;
        world_pos.y += obs.position.y;
        world_pos.z += obs.position.z;
    }
    /* COORD_FRAME_WORLD and COORD_FRAME_CAMERA assumed same for now */

    /* Transform from world to target frame */
    if (to_frame == COORD_FRAME_BODY || to_frame == COORD_FRAME_HEAD) {
        /* World to body: translate then inverse rotate */
        world_pos.x -= obs.position.x;
        world_pos.y -= obs.position.y;
        world_pos.z -= obs.position.z;
        parietal_quat_t inv_orient = quat_conjugate(&obs.orientation);
        world_pos = dragonfly_parietal_quat_rotate_vec(&inv_orient, &world_pos);
    } else if (to_frame == COORD_FRAME_EYE) {
        world_pos.x -= obs.position.x;
        world_pos.y -= obs.position.y;
        world_pos.z -= obs.position.z;
        parietal_quat_t inv_orient = quat_conjugate(&obs.orientation);
        world_pos = dragonfly_parietal_quat_rotate_vec(&inv_orient, &world_pos);
    }

    *position = world_pos;
    mutable_bridge->stats.transforms_computed++;

    nimcp_mutex_unlock(mutable_bridge->base.mutex);
    return 0;
}

int dragonfly_parietal_bridge_compute_angles(
    const dragonfly_parietal_bridge_t* bridge,
    const parietal_vec3_t* position,
    float* azimuth,
    float* elevation,
    float* distance
) {
    if (!bridge || !bridge->initialized || !position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_parietal_bridge_compute_angles: required parameter is NULL (bridge, bridge->initialized, position)");
        return -1;
    }

    dragonfly_parietal_bridge_t* mutable_bridge = (dragonfly_parietal_bridge_t*)bridge;
    nimcp_mutex_lock(mutable_bridge->base.mutex);

    /* Compute relative position in body frame */
    parietal_vec3_t rel_pos = vec3_sub(position, &bridge->observer.position);
    parietal_quat_t inv_orient = quat_conjugate(&bridge->observer.orientation);
    rel_pos = dragonfly_parietal_quat_rotate_vec(&inv_orient, &rel_pos);

    /* Compute spherical coordinates */
    float dist = vec3_length(&rel_pos);
    if (distance) *distance = dist;

    if (dist > 1e-10f) {
        if (azimuth) {
            *azimuth = atan2f(rel_pos.x, rel_pos.z);  /* x is right, z is forward */
        }
        if (elevation) {
            *elevation = asinf(rel_pos.y / dist);  /* y is up */
        }
    } else {
        if (azimuth) *azimuth = 0;
        if (elevation) *elevation = 0;
    }

    nimcp_mutex_unlock(mutable_bridge->base.mutex);
    return 0;
}

int dragonfly_parietal_bridge_transform_target(
    const dragonfly_parietal_bridge_t* bridge,
    parietal_target_t* target,
    coordinate_frame_t target_frame
) {
    if (!bridge || !bridge->initialized || !target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_parietal_bridge_transform_target: required parameter is NULL (bridge, bridge->initialized, target)");
        return -1;
    }
    if (target->frame == target_frame) return 0;

    /* Transform position */
    int ret = dragonfly_parietal_bridge_transform_position(bridge, &target->position,
                                                            target->frame, target_frame);
    if (ret != 0) return ret;

    /* Transform velocity (rotation only, no translation) */
    if (target->frame != target_frame) {
        dragonfly_parietal_bridge_t* mutable_bridge = (dragonfly_parietal_bridge_t*)bridge;
        nimcp_mutex_lock(mutable_bridge->base.mutex);

        if (target_frame == COORD_FRAME_BODY || target_frame == COORD_FRAME_HEAD) {
            parietal_quat_t inv_orient = quat_conjugate(&bridge->observer.orientation);
            target->velocity = dragonfly_parietal_quat_rotate_vec(&inv_orient, &target->velocity);
        } else if (target->frame == COORD_FRAME_BODY || target->frame == COORD_FRAME_HEAD) {
            target->velocity = dragonfly_parietal_quat_rotate_vec(&bridge->observer.orientation, &target->velocity);
        }

        nimcp_mutex_unlock(mutable_bridge->base.mutex);
    }

    /* Recompute angles */
    dragonfly_parietal_bridge_compute_angles(bridge, &target->position,
                                              &target->azimuth, &target->elevation, &target->distance);

    target->frame = target_frame;
    return 0;
}

//=============================================================================
// Target Integration Functions
//=============================================================================

int dragonfly_parietal_bridge_sync_targets(dragonfly_parietal_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_parietal_bridge_sync_targets: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t now = get_time_us();
    bridge->num_targets = 0;

    /* If we have a dragonfly system, get its targets */
    if (bridge->dragonfly) {
        dragonfly_stats_t stats;
        if (dragonfly_get_stats(bridge->dragonfly, &stats) == 0) {
            /* Get intercept solution for primary target */
            intercept_solution_t solution;
            if (dragonfly_get_intercept_solution(bridge->dragonfly, &solution) == 0 &&
                solution.feasibility == INTERCEPT_FEASIBLE) {

                parietal_target_t* pt = &bridge->targets[bridge->num_targets];
                pt->id = 1;  /* Primary target */
                pt->position.x = solution.intercept_point[0];
                pt->position.y = solution.intercept_point[1];
                pt->position.z = solution.intercept_point[2];
                /* Velocity from required accel (approximate) */
                pt->velocity.x = solution.required_accel[0];
                pt->velocity.y = solution.required_accel[1];
                pt->velocity.z = solution.required_accel[2];
                pt->attention_weight = 1.0f;  /* Primary target gets full attention */
                pt->frame = COORD_FRAME_WORLD;
                pt->timestamp_us = now;

                /* Compute angles */
                dragonfly_parietal_bridge_compute_angles(bridge, &pt->position,
                    &pt->azimuth, &pt->elevation, &pt->distance);

                /* Estimate visual angle (assuming 0.1m target size) */
                if (pt->distance > 0) {
                    pt->visual_angle = 2.0f * atanf(0.05f / pt->distance);
                } else {
                    pt->visual_angle = 0;
                }

                bridge->primary_target_id = pt->id;
                bridge->num_targets++;
            }
        }
    }

    bridge->stats.targets_processed += bridge->num_targets;

    nimcp_mutex_unlock(bridge->base.mutex);
    return (int)bridge->num_targets;
}

int dragonfly_parietal_bridge_get_targets(
    const dragonfly_parietal_bridge_t* bridge,
    parietal_target_t* targets,
    coordinate_frame_t frame
) {
    if (!bridge || !bridge->initialized || !targets) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_parietal_bridge_get_targets: required parameter is NULL (bridge, bridge->initialized, targets)");
        return -1;
    }

    dragonfly_parietal_bridge_t* mutable_bridge = (dragonfly_parietal_bridge_t*)bridge;
    nimcp_mutex_lock(mutable_bridge->base.mutex);

    /* Copy and transform targets */
    for (uint32_t i = 0; i < bridge->num_targets; i++) {
        targets[i] = bridge->targets[i];
        if (targets[i].frame != frame) {
            dragonfly_parietal_bridge_transform_target(bridge, &targets[i], frame);
        }
    }

    int count = (int)bridge->num_targets;
    nimcp_mutex_unlock(mutable_bridge->base.mutex);
    return count;
}

int dragonfly_parietal_bridge_get_primary_target(
    const dragonfly_parietal_bridge_t* bridge,
    parietal_target_t* target,
    coordinate_frame_t frame
) {
    if (!bridge || !bridge->initialized || !target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_parietal_bridge_get_primary_target: required parameter is NULL (bridge, bridge->initialized, target)");
        return -1;
    }

    dragonfly_parietal_bridge_t* mutable_bridge = (dragonfly_parietal_bridge_t*)bridge;
    nimcp_mutex_lock(mutable_bridge->base.mutex);

    /* Find primary target */
    bool found = false;
    for (uint32_t i = 0; i < bridge->num_targets; i++) {
        if (bridge->targets[i].id == bridge->primary_target_id) {
            *target = bridge->targets[i];
            found = true;
            break;
        }
    }

    if (!found) {
        nimcp_mutex_unlock(mutable_bridge->base.mutex);
        return 1;  /* No primary target */
    }

    /* Transform if needed */
    if (target->frame != frame) {
        dragonfly_parietal_bridge_transform_target(bridge, target, frame);
    }

    nimcp_mutex_unlock(mutable_bridge->base.mutex);
    return 0;
}

//=============================================================================
// Attention Map Functions
//=============================================================================

parietal_attention_map_t* parietal_attention_map_create(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parietal_attention_map_create: width or height is zero");
        return NULL;
    }

    parietal_attention_map_t* map = nimcp_calloc(1, sizeof(parietal_attention_map_t));
    if (!map) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parietal_attention_map_create: map is NULL");
        return NULL;
    }

    map->weights = nimcp_calloc(width * height, sizeof(float));
    if (!map->weights) {
        nimcp_free(map);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parietal_attention_map_create: map->weights is NULL");
        return NULL;
    }

    map->width = width;
    map->height = height;
    map->azimuth_min = -M_PI;
    map->azimuth_max = M_PI;
    map->elevation_min = -M_PI / 2;
    map->elevation_max = M_PI / 2;

    return map;
}

void parietal_attention_map_destroy(parietal_attention_map_t* map) {
    if (!map) return;
    if (map->weights) nimcp_free(map->weights);
    nimcp_free(map);
}

float parietal_attention_map_sample(
    const parietal_attention_map_t* map,
    float azimuth,
    float elevation
) {
    if (!map || !map->weights) return -1.0f;

    /* Normalize to [0,1] */
    float az_norm = (azimuth - map->azimuth_min) / (map->azimuth_max - map->azimuth_min);
    float el_norm = (elevation - map->elevation_min) / (map->elevation_max - map->elevation_min);

    /* Clamp */
    if (az_norm < 0) az_norm = 0;
    if (az_norm > 1) az_norm = 1;
    if (el_norm < 0) el_norm = 0;
    if (el_norm > 1) el_norm = 1;

    /* Map to indices */
    uint32_t x = (uint32_t)(az_norm * (map->width - 1));
    uint32_t y = (uint32_t)(el_norm * (map->height - 1));

    return map->weights[y * map->width + x];
}

int parietal_attention_map_set(
    parietal_attention_map_t* map,
    float azimuth,
    float elevation,
    float weight
) {
    if (!map || !map->weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_attention_map_set: required parameter is NULL (map, map->weights)");
        return -1;
    }

    /* Normalize to [0,1] */
    float az_norm = (azimuth - map->azimuth_min) / (map->azimuth_max - map->azimuth_min);
    float el_norm = (elevation - map->elevation_min) / (map->elevation_max - map->elevation_min);

    /* Clamp */
    if (az_norm < 0 || az_norm > 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parietal_attention_map_set: validation failed");
        return -1;
    }
    if (el_norm < 0 || el_norm > 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parietal_attention_map_set: validation failed");
        return -1;
    }

    /* Map to indices */
    uint32_t x = (uint32_t)(az_norm * (map->width - 1));
    uint32_t y = (uint32_t)(el_norm * (map->height - 1));

    /* Update weight */
    float old_weight = map->weights[y * map->width + x];
    map->weights[y * map->width + x] = weight;
    map->total_attention += weight - old_weight;

    /* Update peak if needed */
    if (weight > map->peak_weight) {
        map->peak_weight = weight;
        map->peak_location.x = azimuth;
        map->peak_location.y = elevation;
        map->peak_location.z = 0;
    }

    return 0;
}

int parietal_attention_map_find_peak(
    const parietal_attention_map_t* map,
    float* azimuth,
    float* elevation,
    float* weight
) {
    if (!map || !map->weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_attention_map_find_peak: required parameter is NULL (map, map->weights)");
        return -1;
    }

    float max_weight = -1.0f;
    uint32_t max_x = 0, max_y = 0;

    for (uint32_t y = 0; y < map->height; y++) {
        for (uint32_t x = 0; x < map->width; x++) {
            float w = map->weights[y * map->width + x];
            if (w > max_weight) {
                max_weight = w;
                max_x = x;
                max_y = y;
            }
        }
    }

    if (azimuth) {
        float az_norm = (map->width > 1) ? (float)max_x / (map->width - 1) : 0.5f;
        *azimuth = map->azimuth_min + az_norm * (map->azimuth_max - map->azimuth_min);
    }
    if (elevation) {
        float el_norm = (map->height > 1) ? (float)max_y / (map->height - 1) : 0.5f;
        *elevation = map->elevation_min + el_norm * (map->elevation_max - map->elevation_min);
    }
    if (weight) {
        *weight = max_weight;
    }

    return 0;
}

int dragonfly_parietal_bridge_update_attention(
    dragonfly_parietal_bridge_t* bridge,
    parietal_attention_map_t* map
) {
    if (!bridge || !bridge->initialized || !map) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_parietal_bridge_update_attention: required parameter is NULL (bridge, bridge->initialized, map)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Decay existing attention */
    float decay = bridge->config.attention_decay;
    for (uint32_t i = 0; i < map->width * map->height; i++) {
        map->weights[i] *= decay;
    }

    /* Add attention at target locations */
    for (uint32_t i = 0; i < bridge->num_targets; i++) {
        parietal_target_t* t = &bridge->targets[i];
        parietal_attention_map_set(map, t->azimuth, t->elevation, t->attention_weight);
    }

    /* Recompute peak */
    parietal_attention_map_find_peak(map, &map->peak_location.x, &map->peak_location.y, &map->peak_weight);

    /* Recompute total */
    map->total_attention = 0;
    for (uint32_t i = 0; i < map->width * map->height; i++) {
        map->total_attention += map->weights[i];
    }

    bridge->stats.attention_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Motor Command Functions
//=============================================================================

int dragonfly_parietal_bridge_generate_motor_command(
    dragonfly_parietal_bridge_t* bridge,
    uint32_t target_id,
    motor_command_t* command
) {
    if (!bridge || !bridge->initialized || !command) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_parietal_bridge_generate_motor_command: required parameter is NULL (bridge, bridge->initialized, command)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Find target */
    parietal_target_t* target = NULL;
    for (uint32_t i = 0; i < bridge->num_targets; i++) {
        if (bridge->targets[i].id == target_id) {
            target = &bridge->targets[i];
            break;
        }
    }

    if (!target) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_parietal_bridge_generate_motor_command: target is NULL");
        return -1;
    }

    memset(command, 0, sizeof(motor_command_t));
    command->target_id = target_id;

    /* Compute angular distance */
    float angular_dist = sqrtf(target->azimuth * target->azimuth +
                               target->elevation * target->elevation);

    /* Decide saccade vs pursuit */
    if (angular_dist > bridge->config.saccade_threshold) {
        /* Saccade to bring target to fovea */
        command->type = MOTOR_CMD_SACCADE;
        command->target_pos = target->position;
        command->amplitude = angular_dist * 180.0f / M_PI;  /* degrees */
        /* Saccade duration: ~25ms + 2.5ms per degree */
        command->duration_ms = 25.0f + 2.5f * command->amplitude;
    } else {
        /* Smooth pursuit */
        command->type = MOTOR_CMD_SMOOTH_PURSUIT;
        command->target_pos = target->position;
        command->velocity.x = target->velocity.x * bridge->config.pursuit_gain;
        command->velocity.y = target->velocity.y * bridge->config.pursuit_gain;
        command->velocity.z = target->velocity.z * bridge->config.pursuit_gain;
        command->duration_ms = 16.67f;  /* ~60Hz update rate */
    }

    command->urgency = target->attention_weight;
    bridge->stats.motor_commands_generated++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int dragonfly_parietal_bridge_generate_saccade(
    dragonfly_parietal_bridge_t* bridge,
    float azimuth,
    float elevation,
    motor_command_t* command
) {
    if (!bridge || !bridge->initialized || !command) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_parietal_bridge_generate_saccade: required parameter is NULL (bridge, bridge->initialized, command)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    memset(command, 0, sizeof(motor_command_t));
    command->type = MOTOR_CMD_SACCADE;

    /* Convert angles to target position (at 1m distance) */
    command->target_pos.x = sinf(azimuth) * cosf(elevation);
    command->target_pos.y = sinf(elevation);
    command->target_pos.z = cosf(azimuth) * cosf(elevation);

    float amplitude = sqrtf(azimuth * azimuth + elevation * elevation);
    command->amplitude = amplitude * 180.0f / M_PI;
    command->duration_ms = 25.0f + 2.5f * command->amplitude;
    command->urgency = 0.5f;

    bridge->stats.motor_commands_generated++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int dragonfly_parietal_bridge_generate_pursuit(
    dragonfly_parietal_bridge_t* bridge,
    uint32_t target_id,
    motor_command_t* command
) {
    if (!bridge || !bridge->initialized || !command) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_parietal_bridge_generate_pursuit: required parameter is NULL (bridge, bridge->initialized, command)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Find target */
    parietal_target_t* target = NULL;
    for (uint32_t i = 0; i < bridge->num_targets; i++) {
        if (bridge->targets[i].id == target_id) {
            target = &bridge->targets[i];
            break;
        }
    }

    if (!target) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_parietal_bridge_generate_pursuit: target is NULL");
        return -1;
    }

    memset(command, 0, sizeof(motor_command_t));
    command->type = MOTOR_CMD_SMOOTH_PURSUIT;
    command->target_id = target_id;
    command->target_pos = target->position;
    command->velocity.x = target->velocity.x * bridge->config.pursuit_gain;
    command->velocity.y = target->velocity.y * bridge->config.pursuit_gain;
    command->velocity.z = target->velocity.z * bridge->config.pursuit_gain;
    command->duration_ms = 16.67f;
    command->urgency = target->attention_weight;

    bridge->stats.motor_commands_generated++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Intercept Path Planning Functions
//=============================================================================

int dragonfly_parietal_bridge_compute_intercept_path(
    dragonfly_parietal_bridge_t* bridge,
    uint32_t target_id,
    parietal_waypoint_t* waypoints,
    uint32_t max_waypoints
) {
    if (!bridge || !bridge->initialized || !waypoints || max_waypoints == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_parietal_bridge_compute_intercept_path: required parameter is NULL (bridge, bridge->initialized, waypoints)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get intercept solution from dragonfly */
    if (!bridge->dragonfly) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    intercept_solution_t solution;
    if (dragonfly_get_intercept_solution(bridge->dragonfly, &solution) != 0 ||
        solution.feasibility != INTERCEPT_FEASIBLE) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Generate waypoints by interpolating from observer to intercept point */
    uint32_t num_waypoints = max_waypoints;
    if (num_waypoints > PARIETAL_BRIDGE_MAX_WAYPOINTS) num_waypoints = PARIETAL_BRIDGE_MAX_WAYPOINTS;
    if (num_waypoints < 2) num_waypoints = 2;

    /* Start from observer position, end at intercept point */
    parietal_vec3_t start = bridge->observer.position;
    parietal_vec3_t end = {
        solution.intercept_point[0],
        solution.intercept_point[1],
        solution.intercept_point[2]
    };

    float total_time_ms = solution.intercept_time_s * 1000.0f;
    float dt_ms = total_time_ms / (num_waypoints - 1);

    for (uint32_t i = 0; i < num_waypoints; i++) {
        float t = (float)i / (num_waypoints - 1);

        /* Linear interpolation from start to end */
        waypoints[i].position.x = start.x + t * (end.x - start.x);
        waypoints[i].position.y = start.y + t * (end.y - start.y);
        waypoints[i].position.z = start.z + t * (end.z - start.z);

        /* Constant velocity toward intercept */
        if (dt_ms > 0) {
            float vel_scale = 1000.0f / dt_ms;  /* Convert to per-second */
            waypoints[i].velocity.x = (end.x - start.x) / (num_waypoints - 1) * vel_scale;
            waypoints[i].velocity.y = (end.y - start.y) / (num_waypoints - 1) * vel_scale;
            waypoints[i].velocity.z = (end.z - start.z) / (num_waypoints - 1) * vel_scale;
        } else {
            waypoints[i].velocity = (parietal_vec3_t){0, 0, 0};
        }

        waypoints[i].time_ms = i * dt_ms;
        waypoints[i].confidence = solution.confidence;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return (int)num_waypoints;
}

//=============================================================================
// Gain Field Functions
//=============================================================================

int dragonfly_parietal_bridge_compute_gain_field(
    const dragonfly_parietal_bridge_t* bridge,
    const parietal_target_t* target,
    gain_field_t* gain_field
) {
    if (!bridge || !bridge->initialized || !target || !gain_field) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_parietal_bridge_compute_gain_field: required parameter is NULL (bridge, bridge->initialized, target, gain_field)");
        return -1;
    }

    memset(gain_field, 0, sizeof(gain_field_t));

    /* Compute preferred direction from target position */
    parietal_vec3_t dir = vec3_normalize(&target->position);
    gain_field->preferred_direction[0] = dir.x;
    gain_field->preferred_direction[1] = dir.y;
    gain_field->preferred_direction[2] = dir.z;

    gain_field->tuning_width = bridge->config.gain_field_sigma;

    /* Eye position gain (simplified model) */
    float eye_azimuth = target->azimuth;
    float eye_elevation = target->elevation;
    gain_field->eye_gain[0] = expf(-eye_azimuth * eye_azimuth / (2 * gain_field->tuning_width * gain_field->tuning_width));
    gain_field->eye_gain[1] = expf(-eye_elevation * eye_elevation / (2 * gain_field->tuning_width * gain_field->tuning_width));
    gain_field->eye_gain[2] = 1.0f;

    /* Head gain (identity for now) */
    gain_field->head_gain[0] = 1.0f;
    gain_field->head_gain[1] = 1.0f;
    gain_field->head_gain[2] = 1.0f;

    /* Overall modulation */
    gain_field->modulation_strength = gain_field->eye_gain[0] * gain_field->eye_gain[1];

    return 0;
}

int dragonfly_parietal_bridge_apply_gain_field(
    motor_command_t* command,
    const gain_field_t* gain_field
) {
    if (!command || !gain_field) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_parietal_bridge_apply_gain_field: required parameter is NULL (command, gain_field)");
        return -1;
    }

    /* Modulate velocity by gain field */
    command->velocity.x *= gain_field->modulation_strength;
    command->velocity.y *= gain_field->modulation_strength;
    command->velocity.z *= gain_field->modulation_strength;

    /* Modulate urgency */
    command->urgency *= gain_field->modulation_strength;

    return 0;
}

//=============================================================================
// Statistics Functions
//=============================================================================

int dragonfly_parietal_bridge_get_stats(
    const dragonfly_parietal_bridge_t* bridge,
    parietal_bridge_stats_t* stats
) {
    if (!bridge || !bridge->initialized || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_parietal_bridge_get_stats: required parameter is NULL (bridge, bridge->initialized, stats)");
        return -1;
    }

    nimcp_mutex_lock(((dragonfly_parietal_bridge_t*)bridge)->base.mutex);
    *stats = bridge->stats;
    stats->current_targets = bridge->num_targets;
    nimcp_mutex_unlock(((dragonfly_parietal_bridge_t*)bridge)->base.mutex);

    return 0;
}

int dragonfly_parietal_bridge_reset_stats(dragonfly_parietal_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_parietal_bridge_reset_stats: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(parietal_bridge_stats_t));
    bridge->transform_time_sum = 0;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Configuration Update Functions
//=============================================================================

int dragonfly_parietal_bridge_set_config(
    dragonfly_parietal_bridge_t* bridge,
    const parietal_bridge_config_t* config
) {
    if (!bridge || !bridge->initialized || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_parietal_bridge_set_config: required parameter is NULL (bridge, bridge->initialized, config)");
        return -1;
    }
    if (!parietal_bridge_validate_config(config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_parietal_bridge_set_config: parietal_bridge_validate_config is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config = *config;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dragonfly_parietal_bridge_get_config(
    const dragonfly_parietal_bridge_t* bridge,
    parietal_bridge_config_t* config
) {
    if (!bridge || !bridge->initialized || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_parietal_bridge_get_config: required parameter is NULL (bridge, bridge->initialized, config)");
        return -1;
    }

    nimcp_mutex_lock(((dragonfly_parietal_bridge_t*)bridge)->base.mutex);
    *config = bridge->config;
    nimcp_mutex_unlock(((dragonfly_parietal_bridge_t*)bridge)->base.mutex);

    return 0;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* dragonfly_parietal_frame_name(coordinate_frame_t frame) {
    switch (frame) {
        case COORD_FRAME_EYE: return "Eye";
        case COORD_FRAME_HEAD: return "Head";
        case COORD_FRAME_BODY: return "Body";
        case COORD_FRAME_WORLD: return "World";
        case COORD_FRAME_CAMERA: return "Camera";
        default: return "Unknown";
    }
}

const char* dragonfly_parietal_motor_cmd_name(motor_command_type_t type) {
    switch (type) {
        case MOTOR_CMD_SACCADE: return "Saccade";
        case MOTOR_CMD_SMOOTH_PURSUIT: return "SmoothPursuit";
        case MOTOR_CMD_HEAD_TURN: return "HeadTurn";
        case MOTOR_CMD_BODY_ORIENT: return "BodyOrient";
        case MOTOR_CMD_INTERCEPT_PATH: return "InterceptPath";
        default: return "Unknown";
    }
}
