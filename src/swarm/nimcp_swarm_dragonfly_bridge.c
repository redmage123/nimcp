/**
 * @file nimcp_swarm_dragonfly_bridge.c
 * @brief Swarm Coordination Bridge for Dragonfly Interception Implementation
 *
 * WHAT: Connects dragonfly targeting system with drone swarm coordination
 * WHY:  Enable coordinated multi-drone pursuit and target sharing
 * HOW:  Collective workspace items + task scheduling + formation logic
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#include "swarm/nimcp_swarm_dragonfly_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_dragonfly_bridge)

#define LOG_MODULE "SWARM_DRAGONFLY_BRIDGE"


//=============================================================================
// Constants
//=============================================================================

#define WORKSPACE_ITEM_TARGET_BASE 200  /* Custom workspace item type for targets */
#define TARGET_CONTENT_POSITION_X  0    /* Content vector index for position X */
#define TARGET_CONTENT_POSITION_Y  1    /* Content vector index for position Y */
#define TARGET_CONTENT_POSITION_Z  2    /* Content vector index for position Z */
#define TARGET_CONTENT_VELOCITY_X  3    /* Content vector index for velocity X */
#define TARGET_CONTENT_VELOCITY_Y  4    /* Content vector index for velocity Y */
#define TARGET_CONTENT_VELOCITY_Z  5    /* Content vector index for velocity Z */
#define TARGET_CONTENT_SIZE        6    /* Content vector index for size */
#define TARGET_CONTENT_PRIORITY    7    /* Content vector index for priority */
#define TARGET_CONTENT_STATUS      8    /* Content vector index for status */
#define TARGET_CONTENT_CLUSTER     9    /* Content vector index for cluster ID */
#define TARGET_CONTENT_POSITION   10    /* Content vector index for swarm position */
#define TARGET_CONTENT_DIFFICULTY 11    /* Content vector index for difficulty */

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

static inline void vec3_normalize(float v[3]) {
    float len = vec3_length(v);
    if (len > 0.0001f) {
        v[0] /= len;
        v[1] /= len;
        v[2] /= len;
    }
}

static inline void vec3_scale(float v[3], float s) {
    v[0] *= s;
    v[1] *= s;
    v[2] *= s;
}

static inline void vec3_add(float dst[3], const float a[3], const float b[3]) {
    dst[0] = a[0] + b[0];
    dst[1] = a[1] + b[1];
    dst[2] = a[2] + b[2];
}

static inline void vec3_sub(float dst[3], const float a[3], const float b[3]) {
    dst[0] = a[0] - b[0];
    dst[1] = a[1] - b[1];
    dst[2] = a[2] - b[2];
}

//=============================================================================
// Internal Structures
//=============================================================================

#define SWARM_DRAGONFLY_MAX_PEERS 32  /**< Maximum tracked peer positions */

/**
 * @brief Peer position record for formation tracking
 */
typedef struct {
    uint64_t drone_id;                /**< Peer drone ID */
    float position[3];                /**< Last known position */
    uint64_t last_update_us;          /**< Timestamp of last update */
    bool valid;                       /**< Position data is valid */
} peer_position_t;

/**
 * @brief Internal target record
 */
typedef struct {
    shared_target_t target;
    uint32_t workspace_item_id;       /**< Collective workspace item ID */
    bool locally_detected;            /**< Detected by this drone? */
    uint64_t last_update_us;
    bool active;
} target_record_t;

/**
 * @brief Internal bridge structure
 */
struct swarm_dragonfly_bridge_s {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    swarm_dragonfly_bridge_config_t config;

    /* Connections */
    dragonfly_system_t* dragonfly;
    collective_workspace_t* workspace;
    swarm_task_scheduler_t* scheduler;

    /* Shared targets */
    target_record_t targets[SWARM_DRAGONFLY_MAX_SHARED_TARGETS];
    uint32_t num_targets;
    uint32_t next_local_id;           /**< For generating target IDs */

    /* Current assignment */
    swarm_target_assignment_t current_assignment;
    bool has_assignment;

    /* Formation state */
    formation_state_t current_formation;
    bool in_formation;

    /* Local state */
    float local_position[3];
    float local_velocity[3];
    bool local_position_set;

    /* Statistics */
    swarm_dragonfly_bridge_stats_t stats;

    /* Peer position tracking for formation */
    peer_position_t peer_positions[SWARM_DRAGONFLY_MAX_PEERS];
    uint32_t num_tracked_peers;

    /* Timing */
    uint64_t last_share_time_us;
    uint64_t last_assignment_time_us;
    uint64_t creation_time_us;

};

//=============================================================================
// Configuration Functions
//=============================================================================

swarm_dragonfly_bridge_config_t swarm_dragonfly_bridge_default_config(void) {
    swarm_dragonfly_bridge_config_t config = {
        .local_drone_id = 0,
        .swarm_size = 16,

        /* Sharing policy */
        .share_threshold = 0.3f,
        .salience_boost = 0.2f,
        .share_all_targets = false,

        /* Assignment policy */
        .max_pursuit_distance_m = 100.0f,
        .min_intercept_probability = 0.3f,
        .max_simultaneous_pursuits = 1,

        /* Formation preferences */
        .preferred_formation = PURSUIT_FORMATION_WEDGE,
        .formation_spread_m = 5.0f,
        .enable_coordinated_pursuit = true,

        /* Handoff settings */
        .handoff_threshold_s = 2.0f,
        .handoff_overlap_s = 0.5f,

        /* Update rates */
        .share_update_interval_ms = 100.0f,
        .assignment_interval_ms = 200.0f,

        /* Bio-async */
        .use_bio_async = false,
        .broadcast_urgency = 0.5f
    };
    return config;
}

bool swarm_dragonfly_bridge_validate_config(
    const swarm_dragonfly_bridge_config_t* config
) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_validate_config: config is NULL");
        return false;
    }

    if (config->local_drone_id >= config->swarm_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_dragonfly_bridge_validate_config: capacity exceeded");
        return false;
    }
    if (config->swarm_size == 0 ||
        config->swarm_size > COLLECTIVE_WORKSPACE_MAX_SWARM_SIZE) return false;

    if (config->share_threshold < 0.0f ||
        config->share_threshold > 1.0f) return false;
    if (config->salience_boost < 0.0f ||
        config->salience_boost > 1.0f) return false;

    if (config->max_pursuit_distance_m <= 0.0f) {
        return false;
    }
    if (config->min_intercept_probability < 0.0f ||
        config->min_intercept_probability > 1.0f) return false;

    if (config->formation_spread_m <= 0.0f) {
        return false;
    }
    if (config->handoff_threshold_s <= 0.0f) {
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

swarm_dragonfly_bridge_t* swarm_dragonfly_bridge_create(
    dragonfly_system_t* dragonfly,
    collective_workspace_t* workspace,
    swarm_task_scheduler_t* scheduler,
    const swarm_dragonfly_bridge_config_t* config
) {
    swarm_dragonfly_bridge_config_t cfg = config ? *config :
        swarm_dragonfly_bridge_default_config();

    if (!swarm_dragonfly_bridge_validate_config(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_create: swarm_dragonfly_bridge_validate_config is NULL");
        return NULL;
    }

    swarm_dragonfly_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    bridge->config = cfg;
    bridge->dragonfly = dragonfly;
    bridge->workspace = workspace;
    bridge->scheduler = scheduler;
    bridge->creation_time_us = get_time_us();

    if (bridge_base_init(&bridge->base, 0, "swarm_dragonfly") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_create: bridge->base is NULL");
        return NULL;
    }

    return bridge;
}

void swarm_dragonfly_bridge_destroy(swarm_dragonfly_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "swarm_dragonfly");

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

int swarm_dragonfly_bridge_reset(swarm_dragonfly_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_reset: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    memset(bridge->targets, 0, sizeof(bridge->targets));
    bridge->num_targets = 0;
    bridge->next_local_id = 1;
    bridge->has_assignment = false;
    bridge->in_formation = false;
    bridge->local_position_set = false;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Find target by ID
 */
static target_record_t* find_target(swarm_dragonfly_bridge_t* bridge, uint32_t target_id) {
    for (uint32_t i = 0; i < SWARM_DRAGONFLY_MAX_SHARED_TARGETS; i++) {
        if (bridge->targets[i].active &&
            bridge->targets[i].target.target_id == target_id) {
            return &bridge->targets[i];
        }
    }
    return NULL;  /* Not found - normal search miss */
}

/**
 * @brief Find free target slot
 */
static target_record_t* find_free_slot(swarm_dragonfly_bridge_t* bridge) {
    for (uint32_t i = 0; i < SWARM_DRAGONFLY_MAX_SHARED_TARGETS; i++) {
        if (!bridge->targets[i].active) {
            return &bridge->targets[i];
        }
    }
    return NULL;  /* No free slot available */
}

/**
 * @brief Generate target ID from drone and local ID
 */
static uint32_t make_target_id(uint16_t drone_id, uint16_t local_id) {
    return ((uint32_t)drone_id << 16) | local_id;
}

/**
 * @brief Encode target to workspace content vector
 */
static void encode_target_content(
    const shared_target_t* target,
    float content[COLLECTIVE_WORKSPACE_CONTENT_DIM]
) {
    memset(content, 0, sizeof(float) * COLLECTIVE_WORKSPACE_CONTENT_DIM);

    content[TARGET_CONTENT_POSITION_X] = target->position[0];
    content[TARGET_CONTENT_POSITION_Y] = target->position[1];
    content[TARGET_CONTENT_POSITION_Z] = target->position[2];
    content[TARGET_CONTENT_VELOCITY_X] = target->velocity[0];
    content[TARGET_CONTENT_VELOCITY_Y] = target->velocity[1];
    content[TARGET_CONTENT_VELOCITY_Z] = target->velocity[2];
    content[TARGET_CONTENT_SIZE] = target->size_estimate;
    content[TARGET_CONTENT_PRIORITY] = target->priority;
    content[TARGET_CONTENT_STATUS] = (float)target->status;
    content[TARGET_CONTENT_CLUSTER] = (float)target->prey_cluster_id;
    content[TARGET_CONTENT_POSITION] = (float)target->swarm_position;
    content[TARGET_CONTENT_DIFFICULTY] = target->intercept_difficulty;
}

/**
 * @brief Decode target from workspace content vector
 */
static void decode_target_content(
    const float content[COLLECTIVE_WORKSPACE_CONTENT_DIM],
    shared_target_t* target
) {
    target->position[0] = content[TARGET_CONTENT_POSITION_X];
    target->position[1] = content[TARGET_CONTENT_POSITION_Y];
    target->position[2] = content[TARGET_CONTENT_POSITION_Z];
    target->velocity[0] = content[TARGET_CONTENT_VELOCITY_X];
    target->velocity[1] = content[TARGET_CONTENT_VELOCITY_Y];
    target->velocity[2] = content[TARGET_CONTENT_VELOCITY_Z];
    target->size_estimate = content[TARGET_CONTENT_SIZE];
    target->priority = content[TARGET_CONTENT_PRIORITY];
    target->status = (swarm_target_status_t)(int)content[TARGET_CONTENT_STATUS];
    target->prey_cluster_id = (uint32_t)content[TARGET_CONTENT_CLUSTER];
    target->swarm_position = (swarm_position_t)(int)content[TARGET_CONTENT_POSITION];
    target->intercept_difficulty = content[TARGET_CONTENT_DIFFICULTY];
}

//=============================================================================
// Target Sharing Functions
//=============================================================================

int swarm_dragonfly_bridge_share_target(
    swarm_dragonfly_bridge_t* bridge,
    const shared_target_t* target
) {
    if (!bridge || !target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_share_target: required parameter is NULL (bridge, target)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Find or create slot */
    target_record_t* record = find_target(bridge, target->target_id);
    if (!record) {
        record = find_free_slot(bridge);
        if (!record) {
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_share_target: record is NULL");
            return -1;  /* No space */
        }
        bridge->num_targets++;
    }

    /* Store target */
    record->target = *target;
    record->target.detecting_drone = bridge->config.local_drone_id;
    record->locally_detected = true;
    record->last_update_us = get_time_us();
    record->active = true;

    /* Share to collective workspace if available */
    if (bridge->workspace) {
        collective_workspace_item_t item = {0};

        item.item_id = target->target_id;
        item.salience = target->priority + bridge->config.salience_boost;
        item.salience = nimcp_clampf(item.salience, 0.0f, 1.0f);
        item.type = (workspace_item_type_t)(WORKSPACE_ITEM_CUSTOM +
                    WORKSPACE_ITEM_TARGET_BASE);
        item.source_drone = bridge->config.local_drone_id;
        item.timestamp_ms = get_time_us() / 1000;

        encode_target_content(target, item.content);

        if (collective_workspace_add_item(bridge->workspace, &item)) {
            record->workspace_item_id = item.item_id;
            bridge->stats.targets_shared++;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int swarm_dragonfly_bridge_share_track(
    swarm_dragonfly_bridge_t* bridge,
    uint32_t track_id,
    float priority
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_share_track: bridge is NULL");
        return -1;
    }

    /* Get track from dragonfly system */
    if (!bridge->dragonfly) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_share_track: bridge->dragonfly is NULL");
        return -1;
    }

    /* TODO: Get track info from dragonfly system */
    /* For now, create basic shared target */
    shared_target_t target = {0};
    target.target_id = make_target_id(bridge->config.local_drone_id,
                                       bridge->next_local_id++);
    target.detecting_drone = bridge->config.local_drone_id;
    target.priority = priority;
    target.status = TARGET_STATUS_DETECTED;
    target.detection_time_us = get_time_us();
    target.update_time_us = target.detection_time_us;

    return swarm_dragonfly_bridge_share_target(bridge, &target);
}

int swarm_dragonfly_bridge_update_target(
    swarm_dragonfly_bridge_t* bridge,
    uint32_t target_id,
    const float position[3],
    const float velocity[3],
    swarm_target_status_t status
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_update_target: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    target_record_t* record = find_target(bridge, target_id);
    if (!record) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_update_target: record is NULL");
        return -1;
    }

    if (position) {
        vec3_copy(record->target.position, position);
    }
    if (velocity) {
        vec3_copy(record->target.velocity, velocity);
    }
    record->target.status = status;
    record->target.update_time_us = get_time_us();
    record->last_update_us = record->target.update_time_us;

    /* Update workspace item if we own it */
    if (bridge->workspace && record->locally_detected) {
        collective_workspace_item_t item = {0};
        item.item_id = target_id;
        item.salience = record->target.priority + bridge->config.salience_boost;
        item.type = (workspace_item_type_t)(WORKSPACE_ITEM_CUSTOM +
                    WORKSPACE_ITEM_TARGET_BASE);
        item.source_drone = bridge->config.local_drone_id;
        item.timestamp_ms = get_time_us() / 1000;

        encode_target_content(&record->target, item.content);
        collective_workspace_add_item(bridge->workspace, &item);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int swarm_dragonfly_bridge_remove_target(
    swarm_dragonfly_bridge_t* bridge,
    uint32_t target_id,
    swarm_target_status_t reason
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_remove_target: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    target_record_t* record = find_target(bridge, target_id);
    if (!record) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_remove_target: record is NULL");
        return -1;
    }

    record->target.status = reason;
    record->active = false;
    bridge->num_targets--;

    /* Clear assignment if for this target */
    if (bridge->has_assignment &&
        bridge->current_assignment.target_id == target_id) {
        bridge->has_assignment = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Target Reception Functions
//=============================================================================

int swarm_dragonfly_bridge_process_updates(swarm_dragonfly_bridge_t* bridge) {
    if (!bridge || !bridge->workspace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_process_updates: required parameter is NULL (bridge, bridge->workspace)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    int updates = 0;

    /* Get items from collective workspace */
    collective_workspace_item_t items[COLLECTIVE_WORKSPACE_MAX_ITEMS];
    uint32_t count;

    if (collective_workspace_get_top_items(bridge->workspace, items,
                                           COLLECTIVE_WORKSPACE_MAX_ITEMS,
                                           &count)) {
        for (uint32_t i = 0; i < count; i++) {
            /* Check if this is a target item */
            if (items[i].type >= WORKSPACE_ITEM_CUSTOM + WORKSPACE_ITEM_TARGET_BASE &&
                items[i].type < WORKSPACE_ITEM_CUSTOM + WORKSPACE_ITEM_TARGET_BASE + 100) {

                /* Skip our own targets */
                if (items[i].source_drone == bridge->config.local_drone_id) {
                    continue;
                }

                /* Find or create record */
                target_record_t* record = find_target(bridge, items[i].item_id);
                if (!record) {
                    record = find_free_slot(bridge);
                    if (!record) continue;

                    record->target.target_id = items[i].item_id;
                    record->locally_detected = false;
                    record->active = true;
                    bridge->num_targets++;
                    bridge->stats.targets_received++;
                }

                /* Decode content */
                decode_target_content(items[i].content, &record->target);
                record->target.detecting_drone = items[i].source_drone;
                record->target.update_time_us = items[i].timestamp_ms * 1000;
                record->workspace_item_id = items[i].item_id;
                record->last_update_us = get_time_us();

                updates++;
            }
        }
    }

    /* Update staleness */
    uint64_t now = get_time_us();
    for (uint32_t i = 0; i < SWARM_DRAGONFLY_MAX_SHARED_TARGETS; i++) {
        if (bridge->targets[i].active) {
            bridge->targets[i].target.staleness_s =
                (float)(now - bridge->targets[i].last_update_us) / 1000000.0f;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return updates;
}

int swarm_dragonfly_bridge_get_shared_targets(
    const swarm_dragonfly_bridge_t* bridge,
    shared_target_t* targets,
    uint32_t max_targets,
    uint32_t* num_targets
) {
    if (!bridge || !targets || !num_targets) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_get_shared_targets: required parameter is NULL (bridge, targets, num_targets)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    uint32_t count = 0;
    for (uint32_t i = 0; i < SWARM_DRAGONFLY_MAX_SHARED_TARGETS && count < max_targets; i++) {
        if (bridge->targets[i].active) {
            targets[count++] = bridge->targets[i].target;
        }
    }
    *num_targets = count;

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

int swarm_dragonfly_bridge_get_target(
    const swarm_dragonfly_bridge_t* bridge,
    uint32_t target_id,
    shared_target_t* target
) {
    if (!bridge || !target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_get_target: required parameter is NULL (bridge, target)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    const target_record_t* record = NULL;
    for (uint32_t i = 0; i < SWARM_DRAGONFLY_MAX_SHARED_TARGETS; i++) {
        if (bridge->targets[i].active &&
            bridge->targets[i].target.target_id == target_id) {
            record = &bridge->targets[i];
            break;
        }
    }

    if (!record) {
        nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_get_target: record is NULL");
        return -1;
    }

    *target = record->target;

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

//=============================================================================
// Target Assignment Functions
//=============================================================================

bool swarm_dragonfly_bridge_get_assignment(
    const swarm_dragonfly_bridge_t* bridge,
    swarm_target_assignment_t* assignment
) {
    if (!bridge || !assignment) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_get_assignment: required parameter is NULL (bridge, assignment)");
        return false;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    bool result = bridge->has_assignment;
    if (result) {
        *assignment = bridge->current_assignment;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return result;
}

bool swarm_dragonfly_bridge_request_assignment(
    swarm_dragonfly_bridge_t* bridge,
    uint32_t target_id,
    float urgency
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_request_assignment: bridge is NULL");
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Find target */
    target_record_t* record = find_target(bridge, target_id);
    if (!record) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_request_assignment: record is NULL");
        return false;
    }

    /* Check if within pursuit distance */
    if (bridge->local_position_set) {
        float dist = vec3_distance(bridge->local_position, record->target.position);
        if (dist > bridge->config.max_pursuit_distance_m) {
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_dragonfly_bridge_request_assignment: validation failed");
            return false;
        }
    }

    /* Create assignment */
    bridge->current_assignment.target_id = target_id;
    bridge->current_assignment.target = record->target;
    bridge->current_assignment.role = PURSUIT_ROLE_LEAD;
    bridge->current_assignment.formation = PURSUIT_FORMATION_NONE;
    bridge->current_assignment.urgency = urgency;
    bridge->current_assignment.is_primary = true;
    bridge->current_assignment.assigned_time_us = get_time_us();
    bridge->has_assignment = true;

    /* Update target status */
    record->target.status = TARGET_STATUS_ASSIGNED;
    record->target.assigned_pursuers[0] = bridge->config.local_drone_id;
    record->target.num_pursuers = 1;

    bridge->stats.targets_assigned++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return true;
}

int swarm_dragonfly_bridge_release_assignment(
    swarm_dragonfly_bridge_t* bridge,
    swarm_target_status_t reason
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_release_assignment: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->has_assignment) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    uint32_t target_id = bridge->current_assignment.target_id;
    bridge->has_assignment = false;

    /* Update target record */
    target_record_t* record = find_target(bridge, target_id);
    if (record) {
        record->target.status = reason;
        /* Remove this drone from pursuers */
        for (int i = 0; i < record->target.num_pursuers; i++) {
            if (record->target.assigned_pursuers[i] == bridge->config.local_drone_id) {
                for (int j = i; j < record->target.num_pursuers - 1; j++) {
                    record->target.assigned_pursuers[j] =
                        record->target.assigned_pursuers[j + 1];
                }
                record->target.num_pursuers--;
                break;
            }
        }
    }

    /* Leave formation if in one */
    if (bridge->in_formation) {
        bridge->in_formation = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int swarm_dragonfly_bridge_report_intercept(
    swarm_dragonfly_bridge_t* bridge,
    uint32_t target_id,
    bool success
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_report_intercept: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    target_record_t* record = find_target(bridge, target_id);
    if (record) {
        record->target.status = success ? TARGET_STATUS_INTERCEPTED :
                                          TARGET_STATUS_LOST;
    }

    if (success) {
        bridge->stats.targets_intercepted++;
    } else {
        bridge->stats.lost_targets++;
    }

    /* Clear assignment if for this target */
    if (bridge->has_assignment &&
        bridge->current_assignment.target_id == target_id) {
        bridge->has_assignment = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Formation Functions
//=============================================================================

uint32_t swarm_dragonfly_bridge_create_formation(
    swarm_dragonfly_bridge_t* bridge,
    uint32_t target_id,
    pursuit_formation_t formation,
    const uint16_t* drone_ids,
    uint8_t num_drones
) {
    if (!bridge || formation == PURSUIT_FORMATION_NONE) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    target_record_t* record = find_target(bridge, target_id);
    if (!record) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Set up formation */
    formation_state_t* fs = &bridge->current_formation;
    memset(fs, 0, sizeof(*fs));

    fs->type = formation;
    fs->target_id = target_id;
    fs->lead_drone = bridge->config.local_drone_id;
    fs->spread = bridge->config.formation_spread_m;
    fs->start_time_us = get_time_us();
    fs->is_active = true;

    /* Add drones */
    if (drone_ids && num_drones > 0) {
        fs->num_drones = num_drones < SWARM_DRAGONFLY_MAX_PURSUERS ?
                         num_drones : SWARM_DRAGONFLY_MAX_PURSUERS;
        memcpy(fs->drone_ids, drone_ids, sizeof(uint16_t) * fs->num_drones);
    } else {
        fs->drone_ids[0] = bridge->config.local_drone_id;
        fs->num_drones = 1;
    }

    /* Assign roles */
    fs->roles[0] = PURSUIT_ROLE_LEAD;
    for (int i = 1; i < fs->num_drones; i++) {
        switch (i) {
            case 1: fs->roles[i] = PURSUIT_ROLE_FLANK_LEFT; break;
            case 2: fs->roles[i] = PURSUIT_ROLE_FLANK_RIGHT; break;
            default: fs->roles[i] = PURSUIT_ROLE_BACKUP; break;
        }
    }

    bridge->in_formation = true;
    bridge->stats.formations_joined++;

    /* Update target */
    record->target.formation = formation;
    record->target.num_pursuers = fs->num_drones;
    memcpy(record->target.assigned_pursuers, fs->drone_ids,
           sizeof(uint16_t) * fs->num_drones);

    nimcp_mutex_unlock(bridge->base.mutex);

    return target_id;  /* Use target ID as formation ID */
}

pursuit_role_t swarm_dragonfly_bridge_join_formation(
    swarm_dragonfly_bridge_t* bridge,
    uint32_t formation_id,
    pursuit_role_t preferred_role
) {
    if (!bridge) return PURSUIT_ROLE_NONE;

    nimcp_mutex_lock(bridge->base.mutex);

    /* For now, simple join logic */
    target_record_t* record = find_target(bridge, formation_id);
    if (!record || record->target.formation == PURSUIT_FORMATION_NONE) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return PURSUIT_ROLE_NONE;
    }

    /* Assign role based on preference and availability */
    pursuit_role_t assigned = preferred_role != PURSUIT_ROLE_NONE ?
                              preferred_role : PURSUIT_ROLE_BACKUP;

    /* Add to formation */
    if (record->target.num_pursuers < SWARM_DRAGONFLY_MAX_PURSUERS) {
        record->target.assigned_pursuers[record->target.num_pursuers++] =
            bridge->config.local_drone_id;
    }

    bridge->current_formation.type = record->target.formation;
    bridge->current_formation.target_id = formation_id;
    bridge->in_formation = true;
    bridge->stats.formations_joined++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return assigned;
}

int swarm_dragonfly_bridge_leave_formation(
    swarm_dragonfly_bridge_t* bridge,
    uint32_t formation_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_leave_formation: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->in_formation ||
        bridge->current_formation.target_id != formation_id) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    bridge->in_formation = false;

    /* Remove from target's pursuer list */
    target_record_t* record = find_target(bridge, formation_id);
    if (record) {
        for (int i = 0; i < record->target.num_pursuers; i++) {
            if (record->target.assigned_pursuers[i] == bridge->config.local_drone_id) {
                for (int j = i; j < record->target.num_pursuers - 1; j++) {
                    record->target.assigned_pursuers[j] =
                        record->target.assigned_pursuers[j + 1];
                }
                record->target.num_pursuers--;
                break;
            }
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool swarm_dragonfly_bridge_get_formation(
    const swarm_dragonfly_bridge_t* bridge,
    formation_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_get_formation: required parameter is NULL (bridge, state)");
        return false;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    bool result = bridge->in_formation;
    if (result) {
        *state = bridge->current_formation;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return result;
}

int swarm_dragonfly_bridge_update_formation_position(
    swarm_dragonfly_bridge_t* bridge,
    const float position[3],
    const float velocity[3]
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_update_formation_position: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    vec3_copy(bridge->local_position, position);
    vec3_copy(bridge->local_velocity, velocity);
    bridge->local_position_set = true;

    /* Update formation state if we're in one */
    if (bridge->in_formation) {
        /* TODO: Compute formation coherence based on all drone positions */
        bridge->current_formation.coherence = 0.9f;  /* Placeholder */
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Handoff Functions
//=============================================================================

int swarm_dragonfly_bridge_initiate_handoff(
    swarm_dragonfly_bridge_t* bridge,
    uint32_t target_id,
    uint16_t receiving_drone
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_initiate_handoff: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    target_record_t* record = find_target(bridge, target_id);
    if (!record) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_initiate_handoff: record is NULL");
        return -1;
    }

    /* For now, just update status */
    record->target.status = TARGET_STATUS_ASSIGNED;
    if (receiving_drone != 0 && record->target.num_pursuers < SWARM_DRAGONFLY_MAX_PURSUERS) {
        record->target.assigned_pursuers[record->target.num_pursuers++] = receiving_drone;
    }

    bridge->stats.handoffs_sent++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool swarm_dragonfly_bridge_accept_handoff(
    swarm_dragonfly_bridge_t* bridge,
    uint32_t target_id,
    uint16_t sending_drone
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_accept_handoff: bridge is NULL");
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    target_record_t* record = find_target(bridge, target_id);
    if (!record) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_accept_handoff: record is NULL");
        return false;
    }

    /* Accept if within range */
    bool accept = true;
    if (bridge->local_position_set) {
        float dist = vec3_distance(bridge->local_position, record->target.position);
        if (dist > bridge->config.max_pursuit_distance_m) {
            accept = false;
        }
    }

    if (accept) {
        /* Add ourselves as pursuer */
        if (record->target.num_pursuers < SWARM_DRAGONFLY_MAX_PURSUERS) {
            record->target.assigned_pursuers[record->target.num_pursuers++] =
                bridge->config.local_drone_id;
        }
        bridge->stats.handoffs_received++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return accept;
}

int swarm_dragonfly_bridge_reject_handoff(
    swarm_dragonfly_bridge_t* bridge,
    uint32_t target_id,
    uint16_t sending_drone
) {
    /* Nothing to do - just decline */
    (void)bridge;
    (void)target_id;
    (void)sending_drone;
    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

int swarm_dragonfly_bridge_get_stats(
    const swarm_dragonfly_bridge_t* bridge,
    swarm_dragonfly_bridge_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

int swarm_dragonfly_bridge_reset_stats(swarm_dragonfly_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_reset_stats: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

uint32_t swarm_dragonfly_bridge_target_count(
    const swarm_dragonfly_bridge_t* bridge
) {
    if (!bridge) return 0;

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    uint32_t count = bridge->num_targets;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return count;
}

bool swarm_dragonfly_bridge_in_formation(
    const swarm_dragonfly_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_bridge_in_formation: bridge is NULL");
        return false;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    bool result = bridge->in_formation;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return result;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* pursuit_formation_name(pursuit_formation_t formation) {
    switch (formation) {
        case PURSUIT_FORMATION_NONE:     return "NONE";
        case PURSUIT_FORMATION_WEDGE:    return "WEDGE";
        case PURSUIT_FORMATION_PINCER:   return "PINCER";
        case PURSUIT_FORMATION_ENCIRCLE: return "ENCIRCLE";
        case PURSUIT_FORMATION_RELAY:    return "RELAY";
        case PURSUIT_FORMATION_SHADOW:   return "SHADOW";
        default:                         return "UNKNOWN";
    }
}

const char* pursuit_role_name(pursuit_role_t role) {
    switch (role) {
        case PURSUIT_ROLE_NONE:        return "NONE";
        case PURSUIT_ROLE_LEAD:        return "LEAD";
        case PURSUIT_ROLE_FLANK_LEFT:  return "FLANK_LEFT";
        case PURSUIT_ROLE_FLANK_RIGHT: return "FLANK_RIGHT";
        case PURSUIT_ROLE_BACKUP:      return "BACKUP";
        case PURSUIT_ROLE_BLOCKER:     return "BLOCKER";
        case PURSUIT_ROLE_OBSERVER:    return "OBSERVER";
        default:                       return "UNKNOWN";
    }
}

const char* swarm_target_status_name(swarm_target_status_t status) {
    switch (status) {
        case TARGET_STATUS_DETECTED:    return "DETECTED";
        case TARGET_STATUS_SHARED:      return "SHARED";
        case TARGET_STATUS_ASSIGNED:    return "ASSIGNED";
        case TARGET_STATUS_PURSUING:    return "PURSUING";
        case TARGET_STATUS_INTERCEPTED: return "INTERCEPTED";
        case TARGET_STATUS_LOST:        return "LOST";
        case TARGET_STATUS_ABANDONED:   return "ABANDONED";
        default:                        return "UNKNOWN";
    }
}

pursuit_formation_t swarm_dragonfly_compute_formation(
    const shared_target_t* target,
    uint8_t num_drones
) {
    if (!target || num_drones < 2) {
        return PURSUIT_FORMATION_NONE;
    }

    /* Simple heuristics for formation selection */
    if (target->intercept_difficulty > 0.7f) {
        /* Hard target - use pincer to cut off escape */
        return PURSUIT_FORMATION_PINCER;
    }

    if (target->swarm_position == POSITION_CENTER ||
        target->swarm_position == POSITION_INTERIOR) {
        /* Target in swarm - use relay to avoid collision */
        return PURSUIT_FORMATION_RELAY;
    }

    if (num_drones >= 4) {
        /* Many drones - encircle */
        return PURSUIT_FORMATION_ENCIRCLE;
    }

    /* Default to wedge */
    return PURSUIT_FORMATION_WEDGE;
}

int swarm_dragonfly_compute_formation_positions(
    pursuit_formation_t formation,
    const float target_pos[3],
    const float target_vel[3],
    uint8_t num_drones,
    float offsets[][3]
) {
    if (!target_pos || !offsets || num_drones == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_dragonfly_compute_formation_positions: required parameter is NULL (target_pos, offsets)");
        return -1;
    }

    /* Compute approach direction (opposite to target velocity) */
    float approach[3] = {0.0f, 1.0f, 0.0f};  /* Default forward */
    if (target_vel && vec3_length(target_vel) > 0.1f) {
        vec3_copy(approach, target_vel);
        vec3_normalize(approach);
        vec3_scale(approach, -1.0f);  /* Opposite direction */
    }

    /* Compute perpendicular */
    float perp[3] = {-approach[1], approach[0], 0.0f};
    vec3_normalize(perp);

    float spread = 5.0f;  /* Default spread */

    switch (formation) {
        case PURSUIT_FORMATION_WEDGE:
            /* V-formation behind target */
            for (int i = 0; i < num_drones; i++) {
                float offset = (i % 2 == 0) ? (i / 2 + 1) : -(i / 2 + 1);
                if (i == 0) offset = 0;  /* Lead in center */

                offsets[i][0] = approach[0] * spread * (1 + abs((int)offset)) +
                                perp[0] * offset * spread;
                offsets[i][1] = approach[1] * spread * (1 + abs((int)offset)) +
                                perp[1] * offset * spread;
                offsets[i][2] = 0.0f;
            }
            break;

        case PURSUIT_FORMATION_PINCER:
            /* Two flanks */
            for (int i = 0; i < num_drones; i++) {
                float side = (i % 2 == 0) ? 1.0f : -1.0f;
                float depth = (float)(i / 2) * spread;

                offsets[i][0] = perp[0] * side * spread * 2.0f +
                                approach[0] * depth;
                offsets[i][1] = perp[1] * side * spread * 2.0f +
                                approach[1] * depth;
                offsets[i][2] = 0.0f;
            }
            break;

        case PURSUIT_FORMATION_ENCIRCLE:
            /* Circle around target */
            for (int i = 0; i < num_drones; i++) {
                float angle = (NIMCP_TWO_PI_F * i) / num_drones;
                offsets[i][0] = cosf(angle) * spread * 2.0f;
                offsets[i][1] = sinf(angle) * spread * 2.0f;
                offsets[i][2] = 0.0f;
            }
            break;

        case PURSUIT_FORMATION_RELAY:
            /* Line behind target */
            for (int i = 0; i < num_drones; i++) {
                offsets[i][0] = approach[0] * spread * (i + 1);
                offsets[i][1] = approach[1] * spread * (i + 1);
                offsets[i][2] = 0.0f;
            }
            break;

        case PURSUIT_FORMATION_SHADOW:
            /* Lead follows, others trail */
            offsets[0][0] = approach[0] * spread;
            offsets[0][1] = approach[1] * spread;
            offsets[0][2] = 0.0f;
            for (int i = 1; i < num_drones; i++) {
                offsets[i][0] = approach[0] * spread * 3.0f;
                offsets[i][1] = approach[1] * spread * 3.0f +
                                perp[1] * (i - 1) * spread;
                offsets[i][2] = 0.0f;
            }
            break;

        default:
            /* No formation - all at same offset */
            for (int i = 0; i < num_drones; i++) {
                offsets[i][0] = approach[0] * spread;
                offsets[i][1] = approach[1] * spread;
                offsets[i][2] = 0.0f;
            }
            break;
    }

    return 0;
}
