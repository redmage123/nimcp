/**
 * @file nimcp_dragonfly_collision.c
 * @brief Collision Avoidance During Pursuit Implementation
 *
 * WHAT: Detects obstacles and plans avoidance maneuvers
 * WHY:  Enables safe hunting in cluttered environments
 * HOW:  Obstacle detection with trajectory replanning
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#include "dragonfly/nimcp_dragonfly_collision.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_collision)

//=============================================================================
// Local Helpers
//=============================================================================

static inline uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static inline float vec3_length(const float v[3]) {
    return sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static inline float vec3_dot(const float a[3], const float b[3]) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

static inline void vec3_normalize(const float v[3], float out[3]) {
    float len = vec3_length(v);
    if (len > 1e-6f) {
        out[0] = v[0] / len;
        out[1] = v[1] / len;
        out[2] = v[2] / len;
    } else {
        out[0] = 0.0f;
        out[1] = 0.0f;
        out[2] = 1.0f;
    }
}

static inline void vec3_cross(const float a[3], const float b[3], float out[3]) {
    out[0] = a[1]*b[2] - a[2]*b[1];
    out[1] = a[2]*b[0] - a[0]*b[2];
    out[2] = a[0]*b[1] - a[1]*b[0];
}

//=============================================================================
// Internal Structure
//=============================================================================

struct dragonfly_collision_s {
    /* Configuration */
    collision_config_t config;

    /* Obstacle list */
    detected_obstacle_t obstacles[COLLISION_MAX_OBSTACLES];
    uint32_t num_obstacles;

    /* Current assessment */
    collision_summary_t summary;

    /* Self state cache */
    float self_position[3];
    float self_velocity[3];

    /* Statistics */
    collision_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Timing */
    uint64_t creation_time_us;
    uint64_t last_update_us;
};

//=============================================================================
// Name Functions
//=============================================================================

const char* dragonfly_obstacle_type_name(obstacle_type_t type) {
    switch (type) {
        case OBSTACLE_UNKNOWN:      return "unknown";
        case OBSTACLE_VEGETATION:   return "vegetation";
        case OBSTACLE_STRUCTURE:    return "structure";
        case OBSTACLE_WATER_SURFACE: return "water_surface";
        case OBSTACLE_PREDATOR:     return "predator";
        case OBSTACLE_COMPETITOR:   return "competitor";
        default:                    return "unknown";
    }
}

const char* dragonfly_threat_level_name(threat_level_t level) {
    switch (level) {
        case THREAT_NONE:     return "none";
        case THREAT_LOW:      return "low";
        case THREAT_MEDIUM:   return "medium";
        case THREAT_HIGH:     return "high";
        case THREAT_CRITICAL: return "critical";
        default:              return "unknown";
    }
}

const char* dragonfly_avoidance_action_name(avoidance_action_t action) {
    switch (action) {
        case AVOID_NONE:         return "none";
        case AVOID_SLIGHT_LEFT:  return "slight_left";
        case AVOID_SLIGHT_RIGHT: return "slight_right";
        case AVOID_HARD_LEFT:    return "hard_left";
        case AVOID_HARD_RIGHT:   return "hard_right";
        case AVOID_CLIMB:        return "climb";
        case AVOID_DIVE:         return "dive";
        case AVOID_BRAKE:        return "brake";
        case AVOID_ABORT:        return "abort";
        default:                 return "unknown";
    }
}

//=============================================================================
// Configuration Functions
//=============================================================================

collision_config_t collision_default_config(void) {
    collision_config_t config = {
        /* Detection */
        .detection_range_m = 5.0f,
        .detection_cone_rad = 1.2f,  /* ~70 degrees half-angle */
        .peripheral_range_m = 2.0f,

        /* Safety margins */
        .min_clearance_m = 0.2f,
        .ttc_warning_threshold_s = 0.5f,
        .ttc_critical_threshold_s = 0.2f,

        /* Response parameters */
        .max_avoidance_angle_rad = 0.8f,
        .avoidance_aggression = 0.7f,
        .allow_pursuit_abort = true,

        /* Prediction */
        .prediction_horizon_s = 1.0f,
        .use_obstacle_velocity = true,

        /* Priorities */
        .pursuit_vs_safety = 0.3f  /* Safety prioritized */
    };
    return config;
}

bool collision_validate_config(const collision_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collision_validate_config: config is NULL");
        return false;
    }

    if (config->detection_range_m <= 0.0f) {
        return false;
    }
    if (config->detection_cone_rad <= 0.0f || config->detection_cone_rad > M_PI) {
        return false;
    }
    if (config->peripheral_range_m < 0.0f) {
        return false;
    }

    if (config->min_clearance_m < 0.0f) {
        return false;
    }
    if (config->ttc_warning_threshold_s <= 0.0f) {
        return false;
    }
    if (config->ttc_critical_threshold_s <= 0.0f) {
        return false;
    }
    if (config->ttc_critical_threshold_s > config->ttc_warning_threshold_s) {
        return false;
    }

    if (config->max_avoidance_angle_rad <= 0.0f) {
        return false;
    }
    if (config->avoidance_aggression < 0.0f || config->avoidance_aggression > 1.0f) {
        return false;
    }

    if (config->prediction_horizon_s <= 0.0f) {
        return false;
    }
    if (config->pursuit_vs_safety < 0.0f || config->pursuit_vs_safety > 1.0f) {
        return false;
    }

    return true;
}

//=============================================================================
// Internal Helpers
//=============================================================================

static int find_obstacle_by_id(const dragonfly_collision_t collision, uint32_t id) {
    for (uint32_t i = 0; i < collision->num_obstacles; i++) {
        if (collision->obstacles[i].id == id) {
            return (int)i;
        }
    }
    return -1;  /* Not found - normal search miss */
}

static float compute_ttc(
    const float self_pos[3],
    const float self_vel[3],
    const float obs_pos[3],
    const float obs_vel[3],
    float obs_radius
) {
    /* Relative position and velocity */
    float rel_pos[3], rel_vel[3];
    for (int i = 0; i < 3; i++) {
        rel_pos[i] = obs_pos[i] - self_pos[i];
        rel_vel[i] = (obs_vel ? obs_vel[i] : 0.0f) - self_vel[i];
    }

    float a = vec3_dot(rel_vel, rel_vel);
    float b = 2.0f * vec3_dot(rel_pos, rel_vel);
    float c = vec3_dot(rel_pos, rel_pos) - obs_radius * obs_radius;

    if (a < 1e-6f) {
        /* No relative velocity */
        return (c < 0) ? 0.0f : INFINITY;
    }

    float discriminant = b*b - 4.0f*a*c;
    if (discriminant < 0) {
        return INFINITY;  /* No collision */
    }

    float t = (-b - sqrtf(discriminant)) / (2.0f * a);
    if (t < 0) {
        t = (-b + sqrtf(discriminant)) / (2.0f * a);
    }

    return (t > 0) ? t : INFINITY;
}

static float compute_closest_approach(
    const float self_pos[3],
    const float self_vel[3],
    const float obs_pos[3],
    const float obs_vel[3],
    float prediction_time
) {
    float min_dist = INFINITY;

    for (float t = 0.0f; t <= prediction_time; t += 0.05f) {
        float self_future[3], obs_future[3];
        for (int i = 0; i < 3; i++) {
            self_future[i] = self_pos[i] + self_vel[i] * t;
            obs_future[i] = obs_pos[i] + (obs_vel ? obs_vel[i] * t : 0.0f);
        }

        float dx = self_future[0] - obs_future[0];
        float dy = self_future[1] - obs_future[1];
        float dz = self_future[2] - obs_future[2];
        float dist = sqrtf(dx*dx + dy*dy + dz*dz);

        if (dist < min_dist) {
            min_dist = dist;
        }
    }

    return min_dist;
}

static threat_level_t assess_threat(
    const dragonfly_collision_t collision,
    detected_obstacle_t* obs,
    const float self_pos[3],
    const float self_vel[3]
) {
    /* Compute TTC */
    float obs_vel[3] = {0};
    if (collision->config.use_obstacle_velocity && obs->is_moving) {
        memcpy(obs_vel, obs->velocity, sizeof(obs_vel));
    }

    float radius = (obs->extent[0] + obs->extent[1] + obs->extent[2]) / 3.0f + collision->config.min_clearance_m;
    obs->time_to_collision_s = compute_ttc(self_pos, self_vel, obs->position, obs_vel, radius);

    /* Compute closest approach */
    obs->closest_approach_m = compute_closest_approach(
        self_pos, self_vel, obs->position, obs_vel, collision->config.prediction_horizon_s);

    /* Assess threat level */
    if (obs->time_to_collision_s < collision->config.ttc_critical_threshold_s) {
        return THREAT_CRITICAL;
    }
    if (obs->time_to_collision_s < collision->config.ttc_warning_threshold_s) {
        return THREAT_HIGH;
    }
    if (obs->closest_approach_m < collision->config.min_clearance_m * 2.0f) {
        return THREAT_MEDIUM;
    }
    if (obs->closest_approach_m < collision->config.min_clearance_m * 4.0f) {
        return THREAT_LOW;
    }
    return THREAT_NONE;
}

static int get_sector(const float direction[3]) {
    float angle = atan2f(direction[1], direction[0]);
    if (angle < 0) angle += 2.0f * M_PI;
    return (int)(angle / (2.0f * M_PI) * COLLISION_SECTORS) % COLLISION_SECTORS;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

dragonfly_collision_t dragonfly_collision_create(const collision_config_t* config) {
    collision_config_t cfg = config ? *config : collision_default_config();

    if (!collision_validate_config(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "dragonfly_collision_create: invalid configuration");
        return NULL;
    }

    dragonfly_collision_t collision = nimcp_calloc(1, sizeof(struct dragonfly_collision_s));
    if (!collision) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "dragonfly_collision_create: failed to allocate collision");
        return NULL;
    }

    collision->config = cfg;
    collision->creation_time_us = get_time_us();

    /* Initialize sector clearances to maximum */
    for (int i = 0; i < COLLISION_SECTORS; i++) {
        collision->summary.sector_clearance[i] = cfg.detection_range_m;
        collision->summary.sector_threat[i] = THREAT_NONE;
    }
    collision->summary.path_clear = true;

    collision->mutex = nimcp_mutex_create(NULL);
    if (!collision->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "dragonfly_collision_create: failed to create mutex");
        nimcp_free(collision);
        return NULL;
    }

    return collision;
}

void dragonfly_collision_destroy(dragonfly_collision_t collision) {
    if (!collision) return;

    if (collision->mutex) {
        nimcp_mutex_destroy(collision->mutex);
    }

    nimcp_free(collision);
}

int dragonfly_collision_reset(dragonfly_collision_t collision) {
    if (!collision) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_collision_reset: collision is NULL");
        return -1;
    }

    nimcp_mutex_lock(collision->mutex);

    collision->num_obstacles = 0;
    memset(&collision->summary, 0, sizeof(collision->summary));

    for (int i = 0; i < COLLISION_SECTORS; i++) {
        collision->summary.sector_clearance[i] = collision->config.detection_range_m;
        collision->summary.sector_threat[i] = THREAT_NONE;
    }
    collision->summary.path_clear = true;

    nimcp_mutex_unlock(collision->mutex);

    return 0;
}

//=============================================================================
// Detection Functions
//=============================================================================

int dragonfly_collision_add_obstacle(
    dragonfly_collision_t collision,
    const detected_obstacle_t* obstacle
) {
    if (!collision || !obstacle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_collision_add_obstacle: required parameter is NULL (collision, obstacle)");
        return -1;
    }

    nimcp_mutex_lock(collision->mutex);

    int idx = find_obstacle_by_id(collision, obstacle->id);

    if (idx >= 0) {
        /* Update existing */
        collision->obstacles[idx] = *obstacle;
        collision->obstacles[idx].last_seen_us = get_time_us();
    } else if (collision->num_obstacles < COLLISION_MAX_OBSTACLES) {
        /* Add new */
        collision->obstacles[collision->num_obstacles] = *obstacle;
        collision->obstacles[collision->num_obstacles].first_seen_us = get_time_us();
        collision->obstacles[collision->num_obstacles].last_seen_us = get_time_us();
        collision->num_obstacles++;
        collision->stats.obstacles_detected++;
    }

    nimcp_mutex_unlock(collision->mutex);

    return 0;
}

int dragonfly_collision_remove_obstacle(
    dragonfly_collision_t collision,
    uint32_t obstacle_id
) {
    if (!collision) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_collision_remove_obstacle: collision is NULL");
        return -1;
    }

    nimcp_mutex_lock(collision->mutex);

    int idx = find_obstacle_by_id(collision, obstacle_id);
    if (idx < 0) {
        nimcp_mutex_unlock(collision->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_collision_remove_obstacle: validation failed");
        return -1;
    }

    /* Shift remaining */
    for (uint32_t i = idx; i < collision->num_obstacles - 1; i++) {
        collision->obstacles[i] = collision->obstacles[i + 1];
    }
    collision->num_obstacles--;

    nimcp_mutex_unlock(collision->mutex);

    return 0;
}

int dragonfly_collision_clear(dragonfly_collision_t collision) {
    if (!collision) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_collision_clear: collision is NULL");
        return -1;
    }

    nimcp_mutex_lock(collision->mutex);
    collision->num_obstacles = 0;
    nimcp_mutex_unlock(collision->mutex);

    return 0;
}

int dragonfly_collision_update_depth(
    dragonfly_collision_t collision,
    const float* depth_map,
    uint32_t width,
    uint32_t height,
    float fov_rad
) {
    if (!collision || !depth_map) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_collision_update_depth: required parameter is NULL (collision, depth_map)");
        return -1;
    }
    (void)width;
    (void)height;
    (void)fov_rad;

    /* Placeholder for depth map processing */
    /* Would convert depth pixels to obstacle detections */

    return 0;
}

//=============================================================================
// Analysis Functions
//=============================================================================

int dragonfly_collision_analyze(
    dragonfly_collision_t collision,
    const float self_position[3],
    const float self_velocity[3],
    collision_summary_t* summary
) {
    if (!collision || !self_position || !self_velocity || !summary) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_collision_analyze: required parameter is NULL (collision, self_position, self_velocity, summary)");
        return -1;
    }

    nimcp_mutex_lock(collision->mutex);

    memcpy(collision->self_position, self_position, sizeof(collision->self_position));
    memcpy(collision->self_velocity, self_velocity, sizeof(collision->self_velocity));

    /* Reset summary */
    collision->summary.max_threat = THREAT_NONE;
    collision->summary.obstacle_count = collision->num_obstacles;
    collision->summary.min_ttc_s = INFINITY;
    collision->summary.has_primary_threat = false;
    collision->summary.path_clear = true;
    collision->summary.safe_path_fraction = 1.0f;

    for (int i = 0; i < COLLISION_SECTORS; i++) {
        collision->summary.sector_clearance[i] = collision->config.detection_range_m;
        collision->summary.sector_threat[i] = THREAT_NONE;
    }

    /* Analyze each obstacle */
    uint64_t now = get_time_us();

    for (uint32_t i = 0; i < collision->num_obstacles; i++) {
        detected_obstacle_t* obs = &collision->obstacles[i];

        /* Skip stale obstacles */
        if ((now - obs->last_seen_us) > 500000) {  /* 500ms timeout */
            continue;
        }

        /* Assess threat */
        obs->threat = assess_threat(collision, obs, self_position, self_velocity);

        /* Update summary */
        if (obs->threat > collision->summary.max_threat) {
            collision->summary.max_threat = obs->threat;
            collision->summary.has_primary_threat = true;
            collision->summary.primary_obstacle_id = obs->id;
            collision->summary.primary_ttc_s = obs->time_to_collision_s;
        }

        if (obs->time_to_collision_s < collision->summary.min_ttc_s) {
            collision->summary.min_ttc_s = obs->time_to_collision_s;
        }

        /* Update sector information */
        float dir[3];
        for (int j = 0; j < 3; j++) {
            dir[j] = obs->position[j] - self_position[j];
        }
        int sector = get_sector(dir);

        if (obs->distance_m < collision->summary.sector_clearance[sector]) {
            collision->summary.sector_clearance[sector] = obs->distance_m;
        }
        if (obs->threat > collision->summary.sector_threat[sector]) {
            collision->summary.sector_threat[sector] = obs->threat;
        }

        /* Check if path is obstructed */
        if (obs->threat >= THREAT_HIGH) {
            collision->summary.path_clear = false;
            collision->summary.safe_path_fraction *= 0.5f;
            collision->stats.critical_threats++;
        }
    }

    *summary = collision->summary;

    collision->last_update_us = now;

    nimcp_mutex_unlock(collision->mutex);

    return 0;
}

bool dragonfly_collision_path_clear(
    dragonfly_collision_t collision,
    const float start[3],
    const float end[3],
    float* min_clearance
) {
    if (!collision || !start || !end) return true;

    nimcp_mutex_lock(collision->mutex);

    float direction[3];
    for (int i = 0; i < 3; i++) {
        direction[i] = end[i] - start[i];
    }
    float path_length = vec3_length(direction);
    if (path_length < 1e-6f) {
        nimcp_mutex_unlock(collision->mutex);
        if (min_clearance) *min_clearance = collision->config.detection_range_m;
        return true;
    }

    for (int i = 0; i < 3; i++) {
        direction[i] /= path_length;
    }

    float closest = collision->config.detection_range_m;
    bool clear = true;

    for (uint32_t i = 0; i < collision->num_obstacles; i++) {
        detected_obstacle_t* obs = &collision->obstacles[i];

        /* Project obstacle onto path */
        float to_obs[3];
        for (int j = 0; j < 3; j++) {
            to_obs[j] = obs->position[j] - start[j];
        }

        float proj = vec3_dot(to_obs, direction);
        if (proj < 0 || proj > path_length) continue;

        /* Distance from path */
        float closest_point[3];
        for (int j = 0; j < 3; j++) {
            closest_point[j] = start[j] + direction[j] * proj;
        }

        float dx = obs->position[0] - closest_point[0];
        float dy = obs->position[1] - closest_point[1];
        float dz = obs->position[2] - closest_point[2];
        float dist = sqrtf(dx*dx + dy*dy + dz*dz);

        float obs_radius = (obs->extent[0] + obs->extent[1] + obs->extent[2]) / 3.0f;

        if (dist < closest) {
            closest = dist;
        }

        if (dist < obs_radius + collision->config.min_clearance_m) {
            clear = false;
        }
    }

    if (min_clearance) *min_clearance = closest;

    nimcp_mutex_unlock(collision->mutex);

    return clear;
}

int dragonfly_collision_get_avoidance(
    dragonfly_collision_t collision,
    const float self_position[3],
    const float self_velocity[3],
    const float pursuit_direction[3],
    avoidance_command_t* command
) {
    if (!collision || !self_position || !self_velocity || !command) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_collision_get_avoidance: required parameter is NULL (collision, self_position, self_velocity, command)");
        return -1;
    }

    nimcp_mutex_lock(collision->mutex);

    /* Default: no avoidance */
    command->action = AVOID_NONE;
    command->urgency = 0.0f;
    command->heading_offset_rad = 0.0f;
    command->pitch_offset_rad = 0.0f;
    command->speed_factor = 1.0f;
    command->pursuit_compatible = true;
    command->requires_immediate_action = false;
    command->react_deadline_ms = 1000.0f;

    if (pursuit_direction) {
        memcpy(command->safe_direction, pursuit_direction, sizeof(command->safe_direction));
    } else {
        float vel_norm[3];
        vec3_normalize(self_velocity, vel_norm);
        memcpy(command->safe_direction, vel_norm, sizeof(command->safe_direction));
    }

    /* Check for threats */
    if (collision->summary.max_threat == THREAT_NONE) {
        nimcp_mutex_unlock(collision->mutex);
        return 0;
    }

    /* Find safest direction */
    int best_sector = 0;
    float best_clearance = 0.0f;

    for (int i = 0; i < COLLISION_SECTORS; i++) {
        if (collision->summary.sector_clearance[i] > best_clearance) {
            best_clearance = collision->summary.sector_clearance[i];
            best_sector = i;
        }
    }

    /* Compute safe direction */
    float sector_angle = (float)best_sector / COLLISION_SECTORS * 2.0f * M_PI;
    command->safe_direction[0] = cosf(sector_angle);
    command->safe_direction[1] = sinf(sector_angle);
    command->safe_direction[2] = 0.0f;

    /* Determine avoidance action based on threat level */
    switch (collision->summary.max_threat) {
        case THREAT_LOW:
            command->action = AVOID_SLIGHT_LEFT;  /* Or right based on sector */
            command->urgency = 0.3f;
            command->heading_offset_rad = 0.1f * collision->config.avoidance_aggression;
            break;

        case THREAT_MEDIUM:
            if (best_sector < COLLISION_SECTORS / 2) {
                command->action = AVOID_SLIGHT_RIGHT;
                command->heading_offset_rad = -0.3f * collision->config.avoidance_aggression;
            } else {
                command->action = AVOID_SLIGHT_LEFT;
                command->heading_offset_rad = 0.3f * collision->config.avoidance_aggression;
            }
            command->urgency = 0.5f;
            break;

        case THREAT_HIGH:
            if (best_sector < COLLISION_SECTORS / 2) {
                command->action = AVOID_HARD_RIGHT;
                command->heading_offset_rad = -0.6f * collision->config.avoidance_aggression;
            } else {
                command->action = AVOID_HARD_LEFT;
                command->heading_offset_rad = 0.6f * collision->config.avoidance_aggression;
            }
            command->urgency = 0.8f;
            command->speed_factor = 0.8f;
            break;

        case THREAT_CRITICAL:
            if (collision->config.allow_pursuit_abort) {
                command->action = AVOID_ABORT;
                command->pursuit_compatible = false;
            } else {
                command->action = AVOID_BRAKE;
                command->speed_factor = 0.3f;
            }
            command->urgency = 1.0f;
            command->requires_immediate_action = true;
            command->react_deadline_ms = collision->summary.primary_ttc_s * 1000.0f;
            collision->stats.pursuits_aborted++;
            break;

        default:
            break;
    }

    collision->stats.avoidance_commands++;

    /* Check near misses */
    if (collision->summary.min_ttc_s < collision->config.ttc_critical_threshold_s * 2.0f) {
        collision->stats.near_misses++;
    }

    /* Update average clearance */
    float min_clear = collision->config.detection_range_m;
    for (int i = 0; i < COLLISION_SECTORS; i++) {
        if (collision->summary.sector_clearance[i] < min_clear) {
            min_clear = collision->summary.sector_clearance[i];
        }
    }
    collision->stats.avg_min_clearance_m =
        (collision->stats.avg_min_clearance_m * (collision->stats.avoidance_commands - 1) + min_clear) /
        collision->stats.avoidance_commands;

    nimcp_mutex_unlock(collision->mutex);

    return 0;
}

int dragonfly_collision_find_safe_direction(
    dragonfly_collision_t collision,
    const float self_position[3],
    const float preferred_direction[3],
    float safe_direction[3]
) {
    if (!collision || !self_position || !safe_direction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_collision_find_safe_direction: required parameter is NULL (collision, self_position, safe_direction)");
        return -1;
    }

    nimcp_mutex_lock(collision->mutex);

    /* Find sector with best clearance close to preferred direction */
    int preferred_sector = 0;
    if (preferred_direction) {
        preferred_sector = get_sector(preferred_direction);
    }

    int best_sector = preferred_sector;
    float best_score = -INFINITY;

    for (int i = 0; i < COLLISION_SECTORS; i++) {
        /* Score based on clearance and closeness to preferred */
        int dist_from_pref = abs(i - preferred_sector);
        if (dist_from_pref > COLLISION_SECTORS / 2) {
            dist_from_pref = COLLISION_SECTORS - dist_from_pref;
        }

        float score = collision->summary.sector_clearance[i] -
                      (float)dist_from_pref * 0.5f;

        if (collision->summary.sector_threat[i] >= THREAT_HIGH) {
            score -= 10.0f;
        }

        if (score > best_score) {
            best_score = score;
            best_sector = i;
        }
    }

    float sector_angle = (float)best_sector / COLLISION_SECTORS * 2.0f * M_PI;
    safe_direction[0] = cosf(sector_angle);
    safe_direction[1] = sinf(sector_angle);
    safe_direction[2] = 0.0f;

    nimcp_mutex_unlock(collision->mutex);

    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

int dragonfly_collision_get_obstacles(
    const dragonfly_collision_t collision,
    detected_obstacle_t* obstacles,
    uint32_t max_obstacles,
    uint32_t* num_obstacles
) {
    if (!collision || !obstacles || !num_obstacles) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_collision_get_obstacles: required parameter is NULL (collision, obstacles, num_obstacles)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)collision->mutex);

    uint32_t count = collision->num_obstacles < max_obstacles ?
                     collision->num_obstacles : max_obstacles;
    memcpy(obstacles, collision->obstacles, count * sizeof(detected_obstacle_t));
    *num_obstacles = count;

    nimcp_mutex_unlock((nimcp_mutex_t*)collision->mutex);

    return 0;
}

int dragonfly_collision_get_stats(
    const dragonfly_collision_t collision,
    collision_stats_t* stats
) {
    if (!collision || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_collision_get_stats: required parameter is NULL (collision, stats)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)collision->mutex);
    *stats = collision->stats;
    nimcp_mutex_unlock((nimcp_mutex_t*)collision->mutex);

    return 0;
}
