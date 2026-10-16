/**
 * @file nimcp_dragonfly_territory.c
 * @brief Territorial Behavior and Patrol System Implementation
 *
 * WHAT: Manages territorial boundaries and patrol behavior
 * WHY:  Enables realistic dragonfly behavior patterns
 * HOW:  Spatial memory with patrol route optimization
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#include "dragonfly/nimcp_dragonfly_territory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include <math.h>
#include <string.h>
#include <time.h>
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_territory)

//=============================================================================
// Constants
//=============================================================================


//=============================================================================
// Local Helpers
//=============================================================================

static inline uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static inline float vec3_distance(const float a[3], const float b[3]) {
    float dx = a[0] - b[0];
    float dy = a[1] - b[1];
    float dz = a[2] - b[2];
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

static inline void vec3_copy(float dst[3], const float src[3]) {
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

static inline void vec3_sub(float dst[3], const float a[3], const float b[3]) {
    dst[0] = a[0] - b[0];
    dst[1] = a[1] - b[1];
    dst[2] = a[2] - b[2];
}

static inline float vec3_length(const float v[3]) {
    return sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static inline void vec3_normalize(float v[3]) {
    float len = vec3_length(v);
    if (len > 0.0001f) {
        v[0] /= len;
        v[1] /= len;
        v[2] /= len;
    }
}

//=============================================================================
// Internal Structure
//=============================================================================

struct dragonfly_territory_s {
    /* Configuration */
    territory_config_t config;

    /* Territory definition */
    territory_boundary_t boundary;
    bool boundary_set;

    /* Landmarks */
    territory_landmark_t landmarks[TERRITORY_MAX_LANDMARKS];
    uint32_t num_landmarks;

    /* Patrol waypoints */
    patrol_waypoint_t waypoints[TERRITORY_MAX_WAYPOINTS];
    uint32_t num_waypoints;

    /* Current state */
    territory_state_t state;

    /* Tracked intruders */
    tracked_intruder_t intruders[TERRITORY_MAX_INTRUDERS];
    uint32_t num_intruders;

    /* Statistics */
    territory_stats_t stats;

    /* Timing */
    uint64_t patrol_start_us;
    uint64_t perch_start_us;
    uint64_t chase_start_us;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Timing */
    uint64_t creation_time_us;
    uint64_t last_update_us;
};

//=============================================================================
// Configuration Functions
//=============================================================================

territory_config_t territory_default_config(void) {
    territory_config_t config = {
        /* Territory definition */
        .default_radius_m = 10.0f,
        .auto_learn_boundaries = true,

        /* Patrol settings */
        .patrol_speed_m_s = 3.0f,
        .waypoint_tolerance_m = 0.5f,
        .scan_time_s = 2.0f,

        /* Intruder response */
        .intruder_detect_radius_m = 5.0f,
        .chase_abandon_distance_m = 15.0f,
        .chase_max_duration_s = 10.0f,

        /* Energy management */
        .perch_energy_threshold = 0.3f,
        .perch_duration_s = 30.0f,

        /* Learning */
        .enable_route_optimization = true,
        .landmark_decay_rate = 0.01f
    };
    return config;
}

bool territory_validate_config(const territory_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "territory_validate_config: config is NULL");
        return false;
    }

    if (config->default_radius_m <= 0.0f) {
        return false;
    }
    if (config->patrol_speed_m_s <= 0.0f) {
        return false;
    }
    if (config->waypoint_tolerance_m <= 0.0f) {
        return false;
    }
    if (config->scan_time_s < 0.0f) {
        return false;
    }

    if (config->intruder_detect_radius_m <= 0.0f) {
        return false;
    }
    if (config->chase_abandon_distance_m <= 0.0f) {
        return false;
    }
    if (config->chase_max_duration_s <= 0.0f) {
        return false;
    }

    if (config->perch_energy_threshold < 0.0f ||
        config->perch_energy_threshold > 1.0f) return false;
    if (config->perch_duration_s < 0.0f) {
        return false;
    }

    if (config->landmark_decay_rate < 0.0f ||
        config->landmark_decay_rate > 1.0f) return false;

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

dragonfly_territory_t dragonfly_territory_create(const territory_config_t* config) {
    territory_config_t cfg = config ? *config : territory_default_config();

    if (!territory_validate_config(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_territory_create: invalid config");
        return NULL;
    }

    dragonfly_territory_t territory = nimcp_calloc(1, sizeof(struct dragonfly_territory_s));
    if (!territory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dragonfly_territory_create: failed to allocate territory");
        return NULL;
    }

    territory->config = cfg;
    territory->state.activity = TERRITORY_PATROLLING;
    territory->creation_time_us = get_time_us();

    territory->mutex = nimcp_mutex_create(NULL);
    if (!territory->mutex) {
        nimcp_free(territory);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dragonfly_territory_create: failed to create mutex");
        return NULL;
    }

    return territory;
}

void dragonfly_territory_destroy(dragonfly_territory_t territory) {
    if (!territory) return;

    if (territory->mutex) {
        nimcp_mutex_destroy(territory->mutex);
    }

    nimcp_free(territory);
}

int dragonfly_territory_reset(dragonfly_territory_t territory) {
    if (!territory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_territory_reset: territory is NULL");
        return -1;
    }

    nimcp_mutex_lock(territory->mutex);

    territory->boundary_set = false;
    territory->num_landmarks = 0;
    territory->num_waypoints = 0;
    memset(&territory->state, 0, sizeof(territory->state));
    territory->state.activity = TERRITORY_PATROLLING;
    territory->num_intruders = 0;

    nimcp_mutex_unlock(territory->mutex);

    return 0;
}

//=============================================================================
// Territory Definition Functions
//=============================================================================

int dragonfly_territory_set_center(
    dragonfly_territory_t territory,
    const float center[3],
    float radius_m
) {
    if (!territory || !center || radius_m <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_territory_set_center: territory or center is NULL, or radius_m <= 0");
        return -1;
    }

    nimcp_mutex_lock(territory->mutex);

    vec3_copy(territory->boundary.center, center);
    territory->boundary.radius_m = radius_m;
    territory->boundary.num_boundary_points = 0;  /* Circular territory */
    territory->boundary_set = true;

    nimcp_mutex_unlock(territory->mutex);

    return 0;
}

int dragonfly_territory_add_landmark(
    dragonfly_territory_t territory,
    const territory_landmark_t* landmark
) {
    if (!territory || !landmark) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_territory_add_landmark: territory or landmark is NULL");
        return -1;
    }

    nimcp_mutex_lock(territory->mutex);

    if (territory->num_landmarks >= TERRITORY_MAX_LANDMARKS) {
        /* Find lowest importance landmark to replace */
        float min_importance = landmark->importance;
        int32_t replace_idx = -1;
        for (uint32_t i = 0; i < territory->num_landmarks; i++) {
            if (territory->landmarks[i].importance < min_importance) {
                min_importance = territory->landmarks[i].importance;
                replace_idx = (int32_t)i;
            }
        }
        if (replace_idx < 0) {
            nimcp_mutex_unlock(territory->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "dragonfly_territory_add_landmark: landmark not important enough to replace existing");
            return -1;  /* New landmark not important enough */
        }
        territory->landmarks[replace_idx] = *landmark;
    } else {
        territory->landmarks[territory->num_landmarks++] = *landmark;
    }

    nimcp_mutex_unlock(territory->mutex);

    return 0;
}

int dragonfly_territory_add_waypoint(
    dragonfly_territory_t territory,
    const patrol_waypoint_t* waypoint
) {
    if (!territory || !waypoint) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_territory_add_waypoint: territory or waypoint is NULL");
        return -1;
    }

    nimcp_mutex_lock(territory->mutex);

    if (territory->num_waypoints >= TERRITORY_MAX_WAYPOINTS) {
        nimcp_mutex_unlock(territory->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "dragonfly_territory_add_waypoint: max waypoints reached");
        return -1;
    }

    territory->waypoints[territory->num_waypoints++] = *waypoint;

    nimcp_mutex_unlock(territory->mutex);

    return 0;
}

int dragonfly_territory_generate_route(dragonfly_territory_t territory) {
    if (!territory || !territory->boundary_set) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_territory_generate_route: territory is NULL or boundary not set");
        return -1;
    }

    nimcp_mutex_lock(territory->mutex);

    /* Generate circular patrol route around territory */
    uint32_t num_points = 8;
    if (num_points > TERRITORY_MAX_WAYPOINTS) {
        num_points = TERRITORY_MAX_WAYPOINTS;
    }

    territory->num_waypoints = num_points;
    float radius = territory->boundary.radius_m * 0.8f;  /* Patrol inside boundary */

    for (uint32_t i = 0; i < num_points; i++) {
        float angle = 2.0f * (float)M_PI * (float)i / (float)num_points;
        territory->waypoints[i].position[0] = territory->boundary.center[0] +
                                               radius * cosf(angle);
        territory->waypoints[i].position[1] = territory->boundary.center[1] +
                                               radius * sinf(angle);
        territory->waypoints[i].position[2] = territory->boundary.center[2];
        territory->waypoints[i].loiter_time_s = territory->config.scan_time_s;
        territory->waypoints[i].scan_radius_m = territory->config.intruder_detect_radius_m;
        territory->waypoints[i].priority = 1;
    }

    /* Add landmarks as higher-priority waypoints */
    for (uint32_t i = 0; i < territory->num_landmarks &&
         territory->num_waypoints < TERRITORY_MAX_WAYPOINTS; i++) {
        if (territory->landmarks[i].type == LANDMARK_PERCH ||
            territory->landmarks[i].type == LANDMARK_PREY_RICH) {
            patrol_waypoint_t wp;
            vec3_copy(wp.position, territory->landmarks[i].position);
            wp.loiter_time_s = territory->config.scan_time_s * 2.0f;
            wp.scan_radius_m = territory->config.intruder_detect_radius_m;
            wp.priority = 2;
            territory->waypoints[territory->num_waypoints++] = wp;
        }
    }

    nimcp_mutex_unlock(territory->mutex);

    return 0;
}

//=============================================================================
// Update Functions
//=============================================================================

int dragonfly_territory_update(
    dragonfly_territory_t territory,
    const float current_pos[3],
    float dt_s
) {
    if (!territory || !current_pos || dt_s <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_territory_update: territory or current_pos is NULL, or dt_s <= 0");
        return -1;
    }

    nimcp_mutex_lock(territory->mutex);

    uint64_t now = get_time_us();
    vec3_copy(territory->state.current_position, current_pos);

    /* Update time tracking */
    switch (territory->state.activity) {
        case TERRITORY_PATROLLING:
            territory->state.time_on_patrol_us += (uint64_t)(dt_s * 1000000.0f);
            break;
        case TERRITORY_PERCHING:
            territory->state.time_on_perch_us += (uint64_t)(dt_s * 1000000.0f);
            break;
        default:
            break;
    }

    /* Check for intruder timeout */
    for (uint32_t i = 0; i < territory->num_intruders; i++) {
        if (now - territory->intruders[i].last_seen_us > 5000000) {  /* 5 second timeout */
            /* Remove stale intruder */
            territory->intruders[i] = territory->intruders[territory->num_intruders - 1];
            territory->num_intruders--;
            i--;
        }
    }

    /* Update state machine */
    switch (territory->state.activity) {
        case TERRITORY_PATROLLING:
            /* Check if at current waypoint */
            if (territory->num_waypoints > 0) {
                uint32_t wp_idx = territory->state.current_waypoint;
                float dist = vec3_distance(current_pos,
                                           territory->waypoints[wp_idx].position);
                if (dist < territory->config.waypoint_tolerance_m) {
                    /* Advance to next waypoint */
                    territory->state.current_waypoint =
                        (wp_idx + 1) % territory->num_waypoints;
                    territory->state.patrol_progress =
                        (float)territory->state.current_waypoint /
                        (float)territory->num_waypoints;

                    if (territory->state.current_waypoint == 0) {
                        territory->stats.patrols_completed++;
                    }
                }
            }

            /* Check for intruders to chase */
            if (territory->num_intruders > 0) {
                /* Find highest threat */
                float max_threat = 0.0f;
                uint32_t max_threat_id = 0;
                for (uint32_t i = 0; i < territory->num_intruders; i++) {
                    if (territory->intruders[i].threat_level > max_threat &&
                        territory->intruders[i].response == RESPONSE_CHASE) {
                        max_threat = territory->intruders[i].threat_level;
                        max_threat_id = territory->intruders[i].id;
                    }
                }
                if (max_threat > 0.3f) {
                    territory->state.activity = TERRITORY_CHASING;
                    territory->state.primary_intruder_id = max_threat_id;
                    territory->chase_start_us = now;
                    territory->stats.chases_initiated++;
                }
            }
            break;

        case TERRITORY_CHASING:
            {
                /* Check chase timeout */
                float chase_duration = (float)(now - territory->chase_start_us) / 1000000.0f;
                if (chase_duration > territory->config.chase_max_duration_s) {
                    territory->state.activity = TERRITORY_PATROLLING;
                    break;
                }

                /* Find the intruder we're chasing */
                bool intruder_found = false;
                for (uint32_t i = 0; i < territory->num_intruders; i++) {
                    if (territory->intruders[i].id == territory->state.primary_intruder_id) {
                        intruder_found = true;
                        float dist = vec3_distance(current_pos,
                                                   territory->intruders[i].position);
                        if (dist > territory->config.chase_abandon_distance_m) {
                            /* Intruder has fled */
                            territory->state.activity = TERRITORY_PATROLLING;
                            territory->stats.chases_successful++;
                        }
                        break;
                    }
                }
                if (!intruder_found) {
                    /* Intruder gone */
                    territory->state.activity = TERRITORY_PATROLLING;
                    territory->stats.chases_successful++;
                }
            }
            break;

        case TERRITORY_PERCHING:
            {
                float perch_duration = (float)(now - territory->perch_start_us) / 1000000.0f;
                if (perch_duration > territory->config.perch_duration_s) {
                    territory->state.activity = TERRITORY_PATROLLING;
                    territory->patrol_start_us = now;
                }
            }
            break;

        default:
            break;
    }

    territory->state.num_intruders = territory->num_intruders;
    territory->last_update_us = now;

    nimcp_mutex_unlock(territory->mutex);

    return 0;
}

int dragonfly_territory_report_intruder(
    dragonfly_territory_t territory,
    uint32_t id,
    const float position[3],
    const float velocity[3],
    float size,
    float threat_level
) {
    if (!territory || !position || !velocity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_territory_report_intruder: territory, position, or velocity is NULL");
        return -1;
    }

    nimcp_mutex_lock(territory->mutex);

    uint64_t now = get_time_us();

    /* Check if intruder already tracked */
    for (uint32_t i = 0; i < territory->num_intruders; i++) {
        if (territory->intruders[i].id == id) {
            vec3_copy(territory->intruders[i].position, position);
            vec3_copy(territory->intruders[i].velocity, velocity);
            territory->intruders[i].threat_level = threat_level;
            territory->intruders[i].last_seen_us = now;
            nimcp_mutex_unlock(territory->mutex);
            return 0;
        }
    }

    /* Add new intruder */
    if (territory->num_intruders >= TERRITORY_MAX_INTRUDERS) {
        nimcp_mutex_unlock(territory->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "dragonfly_territory_report_intruder: max intruders reached");
        return -1;  /* No room */
    }

    tracked_intruder_t* intruder = &territory->intruders[territory->num_intruders++];
    intruder->id = id;
    vec3_copy(intruder->position, position);
    vec3_copy(intruder->velocity, velocity);
    intruder->threat_level = threat_level;
    intruder->first_seen_us = now;
    intruder->last_seen_us = now;

    /* Classify intruder and determine response */
    if (size > 0.1f) {
        intruder->type = INTRUDER_PREDATOR;
        intruder->response = RESPONSE_FLEE;
    } else if (size > 0.03f && threat_level > 0.5f) {
        intruder->type = INTRUDER_MALE_SAME;
        intruder->response = RESPONSE_CHASE;
    } else if (size > 0.02f) {
        intruder->type = INTRUDER_MALE_OTHER;
        intruder->response = RESPONSE_DISPLAY;
    } else {
        intruder->type = INTRUDER_UNKNOWN;
        intruder->response = RESPONSE_MONITOR;
    }

    territory->stats.intruders_detected++;
    territory->state.last_intruder_us = now;

    nimcp_mutex_unlock(territory->mutex);

    return 0;
}

int dragonfly_territory_get_next_waypoint(
    const dragonfly_territory_t territory,
    float waypoint[3]
) {
    if (!territory || !waypoint) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_territory_get_next_waypoint: territory or waypoint is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)territory->mutex);

    if (territory->num_waypoints == 0) {
        /* No waypoints - return center */
        if (territory->boundary_set) {
            vec3_copy(waypoint, territory->boundary.center);
        } else {
            memset(waypoint, 0, sizeof(float) * 3);
        }
        nimcp_mutex_unlock((nimcp_mutex_t*)territory->mutex);
        return 0;
    }

    if (territory->state.activity == TERRITORY_CHASING) {
        /* Return intruder position as waypoint */
        for (uint32_t i = 0; i < territory->num_intruders; i++) {
            if (territory->intruders[i].id == territory->state.primary_intruder_id) {
                vec3_copy(waypoint, territory->intruders[i].position);
                nimcp_mutex_unlock((nimcp_mutex_t*)territory->mutex);
                return 0;
            }
        }
    }

    vec3_copy(waypoint, territory->waypoints[territory->state.current_waypoint].position);

    nimcp_mutex_unlock((nimcp_mutex_t*)territory->mutex);

    return 0;
}

int dragonfly_territory_get_response(
    const dragonfly_territory_t territory,
    uint32_t intruder_id,
    intruder_response_t* response,
    float chase_vector[3]
) {
    if (!territory || !response) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_territory_get_response: territory or response is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)territory->mutex);

    for (uint32_t i = 0; i < territory->num_intruders; i++) {
        if (territory->intruders[i].id == intruder_id) {
            *response = territory->intruders[i].response;

            if (chase_vector) {
                vec3_sub(chase_vector, territory->intruders[i].position,
                         territory->state.current_position);
                vec3_normalize(chase_vector);
            }

            nimcp_mutex_unlock((nimcp_mutex_t*)territory->mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)territory->mutex);
    return -1;  /* Intruder not found */
}

//=============================================================================
// Query Functions
//=============================================================================

bool dragonfly_territory_contains(
    const dragonfly_territory_t territory,
    const float position[3]
) {
    if (!territory || !position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_territory_contains: territory or position is NULL");
        return false;
    }
    if (!territory->boundary_set) {
        return false;  /* No boundary defined - normal condition */
    }

    float dist = vec3_distance(position, territory->boundary.center);
    return dist <= territory->boundary.radius_m;
}

int dragonfly_territory_get_state(
    const dragonfly_territory_t territory,
    territory_state_t* state
) {
    if (!territory || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_territory_get_state: territory or state is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)territory->mutex);
    *state = territory->state;
    nimcp_mutex_unlock((nimcp_mutex_t*)territory->mutex);

    return 0;
}

int dragonfly_territory_get_boundary(
    const dragonfly_territory_t territory,
    territory_boundary_t* boundary
) {
    if (!territory || !boundary) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_territory_get_boundary: territory or boundary is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)territory->mutex);
    *boundary = territory->boundary;
    nimcp_mutex_unlock((nimcp_mutex_t*)territory->mutex);

    return 0;
}

int dragonfly_territory_get_intruders(
    const dragonfly_territory_t territory,
    tracked_intruder_t* intruders,
    uint32_t max_intruders,
    uint32_t* num_intruders
) {
    if (!territory || !intruders || !num_intruders) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_territory_get_intruders: territory, intruders, or num_intruders is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)territory->mutex);

    uint32_t count = territory->num_intruders;
    if (count > max_intruders) count = max_intruders;

    memcpy(intruders, territory->intruders, sizeof(tracked_intruder_t) * count);
    *num_intruders = count;

    nimcp_mutex_unlock((nimcp_mutex_t*)territory->mutex);

    return 0;
}

//=============================================================================
// Statistics Functions
//=============================================================================

int dragonfly_territory_get_stats(
    const dragonfly_territory_t territory,
    territory_stats_t* stats
) {
    if (!territory || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_territory_get_stats: territory or stats is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)territory->mutex);
    *stats = territory->stats;
    nimcp_mutex_unlock((nimcp_mutex_t*)territory->mutex);

    return 0;
}

const char* dragonfly_territory_activity_name(territory_activity_t activity) {
    switch (activity) {
        case TERRITORY_PATROLLING: return "PATROLLING";
        case TERRITORY_PERCHING:   return "PERCHING";
        case TERRITORY_CHASING:    return "CHASING";
        case TERRITORY_DISPLAYING: return "DISPLAYING";
        case TERRITORY_HUNTING:    return "HUNTING";
        case TERRITORY_ABSENT:     return "ABSENT";
        default:                   return "UNKNOWN";
    }
}

const char* dragonfly_intruder_type_name(intruder_type_t type) {
    switch (type) {
        case INTRUDER_MALE_SAME:  return "MALE_SAME_SPECIES";
        case INTRUDER_MALE_OTHER: return "MALE_OTHER_SPECIES";
        case INTRUDER_FEMALE:     return "FEMALE";
        case INTRUDER_PREDATOR:   return "PREDATOR";
        case INTRUDER_UNKNOWN:    return "UNKNOWN";
        default:                  return "INVALID";
    }
}
