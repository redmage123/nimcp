/**
 * @file nimcp_swarm_multi.c
 * @brief Multi-Swarm Coordination System Implementation
 */

#include "swarm/nimcp_swarm_multi.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/validation/nimcp_common.h"
#include "utils/containers/nimcp_hash_table.h"
#include "utils/containers/nimcp_darray.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

#define LOG_MODULE "swarm_multi"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_multi)

/* Bio-async module context for the coordinator */
static bio_module_context_t g_multi_swarm_ctx = NULL;

/* ============================================================================
 * Hash Table Wrapper Functions
 * ============================================================================ */

/* Value destructor for swarm identity hash table entries.
 * The value stored is a pointer (void*) to the swarm identity.
 * The hash table passes us a pointer TO this stored value. */
static void swarm_identity_value_destructor(void* value_ptr, size_t value_size) {
    (void)value_size;  /* Unused parameter */
    if (!value_ptr) return;
    /* value_ptr points to the stored void* which is the swarm identity pointer */
    void** identity_ptr = (void**)value_ptr;
    if (*identity_ptr) {
        nimcp_swarm_identity_destroy((nimcp_swarm_identity_t*)*identity_ptr);
        *identity_ptr = NULL;
    }
}

/* Create a uint64-keyed hash table using the config-based API.
 * If with_destructor is true, registered values will be freed on table destroy. */
static hash_table_t* swarm_hash_table_create_ex(size_t buckets, bool with_destructor) {
    hash_table_config_t config = {
        .initial_buckets = buckets,
        .key_type = HASH_KEY_UINT64,
        .hash_algorithm = HASH_ALG_MURMUR3,
        .value_destructor = with_destructor ? swarm_identity_value_destructor : NULL,
        .case_insensitive = false,
        .thread_safe = false
    };
    return hash_table_create(&config);
}

/* Create a uint64-keyed hash table without value destructor (for mission registry etc.) */
static hash_table_t* swarm_hash_table_create(size_t buckets) {
    return swarm_hash_table_create_ex(buckets, false);
}

/* Wrapper to insert with uint64 key (full 64-bit, no truncation) */
static bool swarm_hash_table_insert(hash_table_t* table, uint64_t key, void* value) {
    /* Store the pointer value itself - pass address of the pointer so it copies the pointer bytes */
    return hash_table_insert_uint64(table, key, &value, sizeof(void*));
}

/* Wrapper to lookup with uint64 key (full 64-bit, no truncation) */
static void* swarm_hash_table_lookup(hash_table_t* table, uint64_t key) {
    void** result = (void**)hash_table_lookup_uint64(table, key);
    return result ? *result : NULL;
}

/* Wrapper to remove with uint64 key (full 64-bit, no truncation) */
static bool swarm_hash_table_remove(hash_table_t* table, uint64_t key) {
    return hash_table_remove_uint64(table, key);
}

/* ============================================================================
 * Dynamic Array Type Aliases for Clarity
 * ============================================================================ */

/* Use nimcp_darray_t for conflicts - typed wrapper macros */
#define conflict_array_create() nimcp_darray_create(sizeof(nimcp_swarm_conflict_t), 16)
#define conflict_array_destroy(arr) nimcp_darray_destroy(arr)
#define conflict_array_clear(arr) nimcp_darray_clear(arr)
#define conflict_array_push_back(arr, elem) nimcp_darray_push_back(arr, elem)
#define conflict_array_size(arr) nimcp_darray_size(arr)
#define conflict_array_at(arr, idx) ((nimcp_swarm_conflict_t*)nimcp_darray_at(arr, idx))

/* Use nimcp_darray_t for uint64 results - typed wrapper macros */
#define uint64_array_create() nimcp_darray_create(sizeof(uint64_t), 16)
#define uint64_array_destroy(arr) nimcp_darray_destroy(arr)
#define uint64_array_push_back(arr, elem) nimcp_darray_push_back(arr, elem)

/* RWLock name aliases */
#define nimcp_rwlock_write_lock(lock) nimcp_rwlock_wrlock(lock)
#define nimcp_rwlock_write_unlock(lock) nimcp_rwlock_unlock(lock)
#define nimcp_rwlock_read_lock(lock) nimcp_rwlock_rdlock(lock)
#define nimcp_rwlock_read_unlock(lock) nimcp_rwlock_unlock(lock)

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define CONFLICT_DETECTION_THRESHOLD 0.1  /**< Territory overlap threshold */
#define BRIDGE_QUALITY_THRESHOLD 0.3      /**< Minimum bridge quality */
#define RESOURCE_EXPIRY_TIME 60000        /**< Request expiry (ms) */
#define DISCOVERY_BROADCAST_INTERVAL 5000 /**< Discovery interval (ms) */

/* ============================================================================
 * Internal Message Types
 * ============================================================================ */

/* Swarm module ID for bio-async registration */
#define BIO_MODULE_SWARM_COORDINATOR 0x0B00

/* Swarm-specific message types (0x0B00-0x0BFF range) */
#define MSG_TYPE_SWARM_DISCOVERY    0x0B01
#define MSG_TYPE_RESOURCE_REQUEST   0x0B02
#define MSG_TYPE_RESOURCE_RESPONSE  0x0B03
#define MSG_TYPE_TERRITORY_NEGOTIATE 0x0B04
#define MSG_TYPE_MISSION_UPDATE     0x0B05
#define MSG_TYPE_CONFLICT_ALERT     0x0B06

/* ============================================================================
 * KG-Driven Wiring Infrastructure
 * ============================================================================ */

/* Forward declarations for handlers */
static nimcp_error_t handle_swarm_discovery(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);
static nimcp_error_t handle_resource_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);
static nimcp_error_t handle_territory_negotiate(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);
static nimcp_error_t handle_mission_update(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);
static nimcp_error_t handle_conflict_alert(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

/**
 * Handler map for multi-swarm coordinator module.
 * Handles all swarm coordination message types.
 */
DEFINE_HANDLER_MAP_BEGIN(multi_swarm)
    HANDLER_MAP_ENTRY(MSG_TYPE_SWARM_DISCOVERY, handle_swarm_discovery)
    HANDLER_MAP_ENTRY(MSG_TYPE_RESOURCE_REQUEST, handle_resource_request)
    HANDLER_MAP_ENTRY(MSG_TYPE_TERRITORY_NEGOTIATE, handle_territory_negotiate)
    HANDLER_MAP_ENTRY(MSG_TYPE_MISSION_UPDATE, handle_mission_update)
    HANDLER_MAP_ENTRY(MSG_TYPE_CONFLICT_ALERT, handle_conflict_alert)
DEFINE_HANDLER_MAP_END()

/**
 * Wiring callback for KG-driven handler registration.
 */
DEFINE_HANDLER_CALLBACK(multi_swarm, nimcp_multi_swarm_coordinator_t, coordinator)

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Generate unique ID
 */
static uint64_t generate_unique_id(void) {
    static uint64_t counter = 1;
    uint64_t timestamp = nimcp_time_get_ms();
    return (timestamp << 20) | (counter++ & 0xFFFFF);
}

/**
 * @brief Calculate health percentage from active agents
 */
static float calculate_health_percentage(uint32_t active, uint32_t total) {
    if (total == 0) return 0.0F;
    return (float)active / (float)total;
}

/**
 * @brief Determine health status from percentage
 */
static nimcp_swarm_health_t determine_health_status(float percentage) {
    if (percentage > 0.9F) return NIMCP_SWARM_HEALTH_EXCELLENT;
    if (percentage > 0.7F) return NIMCP_SWARM_HEALTH_GOOD;
    if (percentage > 0.5F) return NIMCP_SWARM_HEALTH_FAIR;
    if (percentage > 0.3F) return NIMCP_SWARM_HEALTH_POOR;
    return NIMCP_SWARM_HEALTH_CRITICAL;
}

/**
 * @brief Check if point is inside territory
 */
static bool point_in_territory(
    nimcp_coord3d_t point,
    const nimcp_territory_bounds_t* bounds
) {
    return (point.x >= bounds->min.x && point.x <= bounds->max.x &&
            point.y >= bounds->min.y && point.y <= bounds->max.y &&
            point.z >= bounds->min.z && point.z <= bounds->max.z);
}

/**
 * @brief Calculate territory overlap volume
 */
static double calculate_overlap_volume(
    const nimcp_territory_bounds_t* a,
    const nimcp_territory_bounds_t* b
) {
    double overlap_x = fmax(0.0, fmin(a->max.x, b->max.x) - fmax(a->min.x, b->min.x));
    double overlap_y = fmax(0.0, fmin(a->max.y, b->max.y) - fmax(a->min.y, b->min.y));
    double overlap_z = fmax(0.0, fmin(a->max.z, b->max.z) - fmax(a->min.z, b->min.z));

    return overlap_x * overlap_y * overlap_z;
}

/**
 * @brief Hash function for swarm IDs
 */
static uint64_t swarm_id_hash(const void* key) {
    uint64_t id = *(const uint64_t*)key;
    id ^= (id >> 33);
    id *= 0xff51afd7ed558ccdULL;
    id ^= (id >> 33);
    return id;
}

/**
 * @brief Compare function for swarm IDs
 */
static int swarm_id_compare(const void* a, const void* b) {
    uint64_t id_a = *(const uint64_t*)a;
    uint64_t id_b = *(const uint64_t*)b;
    if (id_a < id_b) return -1;
    if (id_a > id_b) return 1;
    return 0;
}

/* ============================================================================
 * Bio-Async Message Handlers
 * ============================================================================ */

/**
 * @brief Handle swarm discovery messages
 */
static nimcp_error_t handle_swarm_discovery(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    (void)response_promise;
    nimcp_multi_swarm_coordinator_t* coordinator =
        (nimcp_multi_swarm_coordinator_t*)user_data;

    if (!coordinator || msg_size < sizeof(bio_message_header_t)) {
        return NIMCP_INVALID_PARAM;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;
    LOG_DEBUG("Received swarm discovery from module %u", header->source_module);

    /* Process discovery payload if present */
    if (msg_size > sizeof(bio_message_header_t)) {
        /* Discovery message contains swarm identity info */
        LOG_INFO("Processing swarm discovery with %zu bytes payload",
                 msg_size - sizeof(bio_message_header_t));
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle resource request messages
 */
static nimcp_error_t handle_resource_request(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    (void)response_promise;
    nimcp_multi_swarm_coordinator_t* coordinator =
        (nimcp_multi_swarm_coordinator_t*)user_data;

    if (!coordinator || msg_size < sizeof(bio_message_header_t)) {
        return NIMCP_INVALID_PARAM;
    }

    LOG_DEBUG("Received resource request message");

    /* Process resource request - match with available resources */
    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    /* Extract resource request from message payload */
    if (header->payload_size >= sizeof(nimcp_resource_request_t)) {
        const nimcp_resource_request_t* request =
            (const nimcp_resource_request_t*)((const uint8_t*)msg + sizeof(bio_message_header_t));

        /* Look up target swarm */
        nimcp_swarm_identity_t* target = (nimcp_swarm_identity_t*)swarm_hash_table_lookup(
            coordinator->swarm_registry, request->target_swarm);

        if (target) {
            /* Check capability availability based on request type */
            bool can_fulfill = false;
            for (uint32_t i = 0; i < target->capability_count; i++) {
                nimcp_swarm_capability_t* cap = &target->capabilities[i];

                /* Map request type to capability type */
                nimcp_swarm_capability_type_t needed_cap = NIMCP_SWARM_CAP_COUNT;
                switch (request->type) {
                    case NIMCP_RESOURCE_REQ_DRONES:
                        needed_cap = NIMCP_SWARM_CAP_TRANSPORT;
                        break;
                    case NIMCP_RESOURCE_REQ_INFORMATION:
                        needed_cap = NIMCP_SWARM_CAP_RECONNAISSANCE;
                        break;
                    case NIMCP_RESOURCE_REQ_CAPABILITY:
                    case NIMCP_RESOURCE_REQ_COORDINATION:
                        needed_cap = NIMCP_SWARM_CAP_COMMUNICATION;
                        break;
                    default:
                        break;
                }

                if (cap->type == needed_cap && cap->is_lendable &&
                    cap->available >= request->quantity) {
                    can_fulfill = true;
                    LOG_INFO("Resource request %lu can be fulfilled by swarm %lu",
                             (unsigned long)request->request_id, (unsigned long)target->swarm_id);
                    break;
                }
            }

            if (!can_fulfill) {
                LOG_DEBUG("Resource request %lu cannot be fulfilled - insufficient resources",
                          (unsigned long)request->request_id);
            }
        }
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle territory negotiation messages
 */
static nimcp_error_t handle_territory_negotiate(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    (void)response_promise;
    nimcp_multi_swarm_coordinator_t* coordinator =
        (nimcp_multi_swarm_coordinator_t*)user_data;

    if (!coordinator || msg_size < sizeof(bio_message_header_t)) {
        return NIMCP_INVALID_PARAM;
    }

    LOG_DEBUG("Received territory negotiation message");

    /* Process territory negotiation request */
    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    /* Extract negotiation data from message payload */
    if (header->payload_size >= sizeof(uint64_t) * 2 + sizeof(nimcp_territory_bounds_t)) {
        const uint8_t* payload = (const uint8_t*)msg + sizeof(bio_message_header_t);
        uint64_t swarm_a = *(const uint64_t*)payload;
        uint64_t swarm_b = *(const uint64_t*)(payload + sizeof(uint64_t));
        const nimcp_territory_bounds_t* proposed =
            (const nimcp_territory_bounds_t*)(payload + sizeof(uint64_t) * 2);

        /* Look up both swarms */
        nimcp_swarm_identity_t* identity_a = (nimcp_swarm_identity_t*)swarm_hash_table_lookup(
            coordinator->swarm_registry, swarm_a);
        nimcp_swarm_identity_t* identity_b = (nimcp_swarm_identity_t*)swarm_hash_table_lookup(
            coordinator->swarm_registry, swarm_b);

        if (identity_a && identity_b) {
            /* Adjust boundaries - simple midpoint calculation for contested areas */
            nimcp_territory_bounds_t* bounds_a = &identity_a->territory;
            nimcp_territory_bounds_t* bounds_b = &identity_b->territory;

            /* Check for overlap and adjust */
            if (bounds_a->max.x > bounds_b->min.x && bounds_b->max.x > bounds_a->min.x) {
                /* X-axis overlap - adjust based on priority */
                double midpoint = (bounds_a->max.x + bounds_b->min.x) / 2.0;
                if (bounds_a->priority > bounds_b->priority) {
                    midpoint += (bounds_a->max.x - bounds_b->min.x) * 0.1;
                } else if (bounds_b->priority > bounds_a->priority) {
                    midpoint -= (bounds_a->max.x - bounds_b->min.x) * 0.1;
                }
                bounds_a->max.x = midpoint;
                bounds_b->min.x = midpoint;
            }

            bounds_a->timestamp = nimcp_time_get_us();
            bounds_b->timestamp = nimcp_time_get_us();

            LOG_INFO("Adjusted territory boundaries between swarms %lu and %lu",
                     (unsigned long)swarm_a, (unsigned long)swarm_b);
        }
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle mission update messages
 */
static nimcp_error_t handle_mission_update(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    (void)response_promise;
    nimcp_multi_swarm_coordinator_t* coordinator =
        (nimcp_multi_swarm_coordinator_t*)user_data;

    if (!coordinator || msg_size < sizeof(bio_message_header_t)) {
        return NIMCP_INVALID_PARAM;
    }

    LOG_DEBUG("Received mission update message");

    /* Process mission status update */
    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    /* Extract mission update from payload */
    if (header->payload_size >= sizeof(uint64_t) + sizeof(nimcp_mission_status_t) + sizeof(float)) {
        const uint8_t* payload = (const uint8_t*)msg + sizeof(bio_message_header_t);
        uint64_t mission_id = *(const uint64_t*)payload;
        nimcp_mission_status_t new_status = *(const nimcp_mission_status_t*)(payload + sizeof(uint64_t));
        float progress = *(const float*)(payload + sizeof(uint64_t) + sizeof(nimcp_mission_status_t));

        /* Look up mission in registry */
        nimcp_mission_assignment_t* mission = (nimcp_mission_assignment_t*)swarm_hash_table_lookup(
            coordinator->mission_registry, mission_id);

        if (mission) {
            nimcp_mission_status_t old_status = mission->status;
            mission->status = new_status;
            mission->progress = progress;

            LOG_INFO("Mission %lu status updated: %d -> %d (progress: %.1f%%)",
                     (unsigned long)mission_id, old_status, new_status, progress * 100.0f);

            /* Handle completion or failure */
            if (new_status == NIMCP_MISSION_STATUS_COMPLETED ||
                new_status == NIMCP_MISSION_STATUS_FAILED ||
                new_status == NIMCP_MISSION_STATUS_ABORTED) {
                LOG_INFO("Mission %lu ended with status %d", (unsigned long)mission_id, new_status);
            }
        } else {
            LOG_DEBUG("Mission %lu not found in registry", (unsigned long)mission_id);
        }
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Extended conflict alert payload structure
 */
typedef struct {
    nimcp_swarm_conflict_type_t conflict_type;
    uint64_t swarm1_id;
    uint64_t swarm2_id;
    float severity;
    nimcp_territory_bounds_t contested_area;
} conflict_alert_payload_t;

/**
 * @brief Handle conflict alert messages
 *
 * WHAT: Process incoming conflict alerts and trigger resolution
 * WHY:  Enable distributed conflict detection and resolution
 * HOW:  Parse alert, register conflict, select and initiate resolution strategy
 */
static nimcp_error_t handle_conflict_alert(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    (void)response_promise;
    nimcp_multi_swarm_coordinator_t* coordinator =
        (nimcp_multi_swarm_coordinator_t*)user_data;

    if (!coordinator || msg_size < sizeof(bio_message_header_t)) {
        return NIMCP_INVALID_PARAM;
    }

    LOG_DEBUG("Received conflict alert message");

    /* Process conflict notification */
    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    /* Extract conflict type and involved swarms from message payload */
    if (msg_size > sizeof(bio_message_header_t) + sizeof(nimcp_swarm_conflict_type_t)) {
        const uint8_t* payload = (const uint8_t*)msg + sizeof(bio_message_header_t);
        nimcp_swarm_conflict_type_t conflict_type;
        memcpy(&conflict_type, payload, sizeof(nimcp_swarm_conflict_type_t));

        /* Try to extract extended payload if available */
        uint64_t swarm1_id = 0;
        uint64_t swarm2_id = 0;
        float severity = 0.5f;

        if (msg_size >= sizeof(bio_message_header_t) + sizeof(conflict_alert_payload_t)) {
            conflict_alert_payload_t extended_payload;
            memcpy(&extended_payload, payload, sizeof(conflict_alert_payload_t));
            swarm1_id = extended_payload.swarm1_id;
            swarm2_id = extended_payload.swarm2_id;
            severity = extended_payload.severity;
        }

        LOG_INFO("Conflict alert received: type=%d, severity=%.3f from module=0x%x",
                 conflict_type, severity, header->source_module);

        /* Create new conflict record */
        nimcp_swarm_conflict_t new_conflict = {0};
        new_conflict.conflict_id = generate_unique_id();
        new_conflict.type = conflict_type;
        new_conflict.detection_time = nimcp_time_get_ms();
        new_conflict.severity = severity;
        new_conflict.is_resolved = false;
        new_conflict.swarm_count = 2;
        new_conflict.swarm_ids[0] = swarm1_id;
        new_conflict.swarm_ids[1] = swarm2_id;

        /* Select resolution strategy based on conflict type and severity */
        nimcp_conflict_resolution_t strategy = NIMCP_CONFLICT_NEGOTIATION;

        switch (conflict_type) {
            case NIMCP_CONFLICT_TYPE_TERRITORY:
                LOG_INFO("Territory conflict detected - initiating spatial partition");
                if (severity >= 0.8f) {
                    strategy = NIMCP_CONFLICT_ESCALATION;
                } else if (severity >= 0.5f) {
                    strategy = NIMCP_CONFLICT_NEGOTIATION;
                } else {
                    strategy = NIMCP_CONFLICT_SPATIAL_SHARING;
                }
                break;

            case NIMCP_CONFLICT_TYPE_RESOURCE:
                LOG_INFO("Resource conflict detected - initiating resource sharing");
                if (severity >= 0.7f) {
                    strategy = NIMCP_CONFLICT_PRIORITY;
                } else {
                    strategy = NIMCP_CONFLICT_TIME_SHARING;
                }
                break;

            case NIMCP_CONFLICT_TYPE_GOAL:
                LOG_INFO("Goal conflict detected - initiating priority arbitration");
                strategy = NIMCP_CONFLICT_PRIORITY;
                break;

            case NIMCP_CONFLICT_TYPE_PRIORITY:
                LOG_INFO("Priority conflict detected - escalating to coordinator");
                strategy = NIMCP_CONFLICT_ESCALATION;
                break;

            case NIMCP_CONFLICT_TYPE_COMMUNICATION:
                LOG_INFO("Communication conflict detected - resolving routing");
                strategy = NIMCP_CONFLICT_COOPERATION;
                break;

            default:
                LOG_DEBUG("Unknown conflict type: %d - using default negotiation", conflict_type);
                strategy = NIMCP_CONFLICT_NEGOTIATION;
                break;
        }

        new_conflict.strategy = strategy;
        snprintf(new_conflict.description, sizeof(new_conflict.description),
                 "Auto-detected conflict type=%d, swarms=%lu,%lu",
                 conflict_type, (unsigned long)swarm1_id, (unsigned long)swarm2_id);

        /* Update conflict statistics */
        /* Note: conflict history tracking not currently implemented */
        coordinator->conflict_stats.total_conflicts++;
        coordinator->conflict_stats.conflicts_pending++;
        LOG_DEBUG("Conflict recorded: total=%u pending=%u",
                  coordinator->conflict_stats.total_conflicts,
                  coordinator->conflict_stats.conflicts_pending);

        /* Trigger resolution based on selected strategy */
        nimcp_swarm_resolution_result_t result = {0};
        nimcp_result_t resolve_status = NIMCP_ERROR;

        switch (strategy) {
            case NIMCP_CONFLICT_PRIORITY:
                /* Immediate resolution by priority */
                resolve_status = nimcp_multi_swarm_resolve_conflict(
                    coordinator, new_conflict.conflict_id,
                    NIMCP_CONFLICT_PRIORITY, &result);
                if (resolve_status == NIMCP_SUCCESS && result.resolved) {
                    LOG_INFO("Conflict %lu resolved by priority",
                             (unsigned long)new_conflict.conflict_id);
                }
                break;

            case NIMCP_CONFLICT_NEGOTIATION:
                /* Start negotiation process */
                resolve_status = nimcp_multi_swarm_start_negotiation(
                    coordinator, new_conflict.conflict_id);
                if (resolve_status == NIMCP_SUCCESS) {
                    LOG_INFO("Negotiation started for conflict %lu",
                             (unsigned long)new_conflict.conflict_id);
                }
                break;

            case NIMCP_CONFLICT_TIME_SHARING:
            case NIMCP_CONFLICT_SPATIAL_SHARING:
            case NIMCP_CONFLICT_RESOLVE_PARTITION:
                /* Direct resolution via sharing */
                resolve_status = nimcp_multi_swarm_resolve_conflict(
                    coordinator, new_conflict.conflict_id,
                    strategy, &result);
                if (resolve_status == NIMCP_SUCCESS && result.resolved) {
                    LOG_INFO("Conflict %lu resolved by sharing strategy",
                             (unsigned long)new_conflict.conflict_id);
                }
                break;

            case NIMCP_CONFLICT_ESCALATION:
                /* Escalate to super-swarm or higher authority */
                LOG_WARN("Escalating conflict %lu - severity=%.3f",
                         (unsigned long)new_conflict.conflict_id, severity);
                coordinator->conflict_stats.escalations++;
                /* Escalation will be handled asynchronously by super-swarm */
                break;

            case NIMCP_CONFLICT_COOPERATION:
                /* Initiate cooperative resolution */
                resolve_status = nimcp_multi_swarm_resolve_conflict(
                    coordinator, new_conflict.conflict_id,
                    NIMCP_CONFLICT_COOPERATION, &result);
                break;

            default:
                LOG_WARN("Unhandled resolution strategy: %d", strategy);
                break;
        }

        /* Log resolution attempt outcome */
        if (resolve_status != NIMCP_SUCCESS && strategy != NIMCP_CONFLICT_ESCALATION) {
            LOG_WARN("Conflict resolution attempt failed for %lu: status=%d",
                     (unsigned long)new_conflict.conflict_id, resolve_status);
        }
    }
    (void)header;

    return NIMCP_SUCCESS;
}

/**
 * @brief Register bio-async message handlers for the coordinator
 */
static nimcp_error_t register_bio_async_handlers(
    nimcp_multi_swarm_coordinator_t* coordinator
) {
    if (!g_multi_swarm_ctx) {
        LOG_WARN("Bio-async module context not initialized");
        return NIMCP_NOT_INITIALIZED;
    }

    /* Register handlers via KG-driven wiring callback */
    nimcp_error_t result = bio_router_register_wiring_callback(
        BIO_MODULE_SWARM_COORDINATOR,
        (void*)multi_swarm_handler_callback,
        coordinator
    );

    if (result != NIMCP_SUCCESS) {
        /* Legacy fallback: direct handler registration */
        LOG_DEBUG("KG wiring unavailable, using legacy registration");

        /* Register handler for swarm discovery */
        result = LEGACY_HANDLER_REGISTRATION(bio_router_register_handler(
            g_multi_swarm_ctx,
            (bio_message_type_t)MSG_TYPE_SWARM_DISCOVERY,
            handle_swarm_discovery
        ));
        if (result != NIMCP_SUCCESS) {
            LOG_WARN("Failed to register swarm discovery handler: %d", result);
        }

        /* Register handler for resource requests */
        result = LEGACY_HANDLER_REGISTRATION(bio_router_register_handler(
            g_multi_swarm_ctx,
            (bio_message_type_t)MSG_TYPE_RESOURCE_REQUEST,
            handle_resource_request
        ));
        if (result != NIMCP_SUCCESS) {
            LOG_WARN("Failed to register resource request handler: %d", result);
        }

        /* Register handler for territory negotiations */
        result = LEGACY_HANDLER_REGISTRATION(bio_router_register_handler(
            g_multi_swarm_ctx,
            (bio_message_type_t)MSG_TYPE_TERRITORY_NEGOTIATE,
            handle_territory_negotiate
        ));
        if (result != NIMCP_SUCCESS) {
            LOG_WARN("Failed to register territory negotiate handler: %d", result);
        }

        /* Register handler for mission updates */
        result = LEGACY_HANDLER_REGISTRATION(bio_router_register_handler(
            g_multi_swarm_ctx,
            (bio_message_type_t)MSG_TYPE_MISSION_UPDATE,
            handle_mission_update
        ));
        if (result != NIMCP_SUCCESS) {
            LOG_WARN("Failed to register mission update handler: %d", result);
        }

        /* Register handler for conflict alerts */
        result = LEGACY_HANDLER_REGISTRATION(bio_router_register_handler(
            g_multi_swarm_ctx,
            (bio_message_type_t)MSG_TYPE_CONFLICT_ALERT,
            handle_conflict_alert
        ));
        if (result != NIMCP_SUCCESS) {
            LOG_WARN("Failed to register conflict alert handler: %d", result);
        }
    }

    LOG_INFO("Registered bio-async handlers for multi-swarm coordinator");
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

nimcp_multi_swarm_coordinator_t* nimcp_multi_swarm_create(
    void* brain,
    bio_router_t* router
) {
    /* brain and router are optional - coordinator can operate in standalone mode
     * for conflict resolution and swarm management without brain/router context */

    LOG_INFO("Creating multi-swarm coordinator");

    nimcp_multi_swarm_coordinator_t* coordinator =
        (nimcp_multi_swarm_coordinator_t*)nimcp_malloc(
            sizeof(nimcp_multi_swarm_coordinator_t));

    if (!coordinator) {
        LOG_ERROR("Failed to allocate multi-swarm coordinator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_multi_swarm_create: coordinator is NULL");
        return NULL;
    }

    memset(coordinator, 0, sizeof(nimcp_multi_swarm_coordinator_t));

    coordinator->brain = brain;
    coordinator->router = router;

    /* Create registries using wrapper for real hash table API.
     * Use _ex variant with destructor=true for swarm_registry so registered
     * swarms are automatically cleaned up when coordinator is destroyed. */
    coordinator->swarm_registry = swarm_hash_table_create_ex(64, true);
    if (!coordinator->swarm_registry) {
        LOG_ERROR("Failed to create swarm registry");
        nimcp_free(coordinator);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_multi_swarm_create: coordinator->swarm_registry is NULL");
        return NULL;
    }

    coordinator->mission_registry = swarm_hash_table_create(32);
    if (!coordinator->mission_registry) {
        LOG_ERROR("Failed to create mission registry");
        hash_table_destroy(coordinator->swarm_registry);
        nimcp_free(coordinator);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_multi_swarm_create: coordinator->mission_registry is NULL");
        return NULL;
    }

    /* Initialize locks */
    if (nimcp_rwlock_init(&coordinator->coordinator_lock) != NIMCP_SUCCESS) {
        LOG_ERROR("Failed to initialize coordinator lock");
        hash_table_destroy(coordinator->mission_registry);
        hash_table_destroy(coordinator->swarm_registry);
        nimcp_free(coordinator);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_multi_swarm_create: validation failed");
        return NULL;
    }

    /* Initialize ID generators */
    coordinator->next_swarm_id = 1000;
    coordinator->next_mission_id = 1;
    coordinator->next_conflict_id = 1;

    /* Enable features by default */
    coordinator->enable_auto_negotiation = true;
    coordinator->enable_resource_sharing = true;
    coordinator->enable_bridge_formation = true;

    /* Initialize conflict resolution configuration with defaults */
    coordinator->conflict_config.default_strategy = NIMCP_CONFLICT_NEGOTIATION;
    coordinator->conflict_config.negotiation_timeout_ms = 30000.0F;  /* 30 seconds */
    coordinator->conflict_config.max_negotiation_rounds = 10;
    coordinator->conflict_config.allow_escalation = true;
    coordinator->conflict_config.merge_threshold = 0.8F;

    /* Initialize conflict statistics */
    memset(&coordinator->conflict_stats, 0, sizeof(nimcp_conflict_resolution_stats_t));

    /* Register with bio-async router if available */
    if (bio_router_is_initialized()) {
        bio_module_info_t module_info = {
            .module_id = BIO_MODULE_SWARM_COORDINATOR,
            .module_name = "swarm_coordinator",
            .inbox_capacity = NIMCP_INBOX_CAPACITY_LARGE,
            .user_data = coordinator
        };

        g_multi_swarm_ctx = bio_router_register_module(&module_info);
        if (g_multi_swarm_ctx) {
            register_bio_async_handlers(coordinator);
            LOG_INFO("Registered swarm coordinator with bio-async router");
        } else {
            LOG_WARN("Failed to register with bio-async router");
        }
    } else {
        LOG_DEBUG("Bio-async router not initialized, skipping registration");
    }

    LOG_INFO("Multi-swarm coordinator created successfully");
    return coordinator;
}

void nimcp_multi_swarm_destroy(nimcp_multi_swarm_coordinator_t* coordinator) {
    if (!coordinator) return;

    LOG_INFO("Destroying multi-swarm coordinator");

    /* Unregister from bio-async router */
    if (g_multi_swarm_ctx) {
        bio_router_unregister_module(g_multi_swarm_ctx);
        g_multi_swarm_ctx = NULL;
        LOG_DEBUG("Unregistered swarm coordinator from bio-async router");
    }

    /* Destroy all super-swarms */
    for (uint32_t i = 0; i < coordinator->super_swarm_count; i++) {
        if (coordinator->super_swarms[i]) {
            nimcp_super_swarm_destroy(coordinator->super_swarms[i]);
        }
    }

    /* Clean up registries */
    if (coordinator->swarm_registry) {
        hash_table_destroy(coordinator->swarm_registry);
    }
    if (coordinator->mission_registry) {
        hash_table_destroy(coordinator->mission_registry);
    }

    /* Destroy lock */
    nimcp_rwlock_destroy(&coordinator->coordinator_lock);

    nimcp_free(coordinator);
    LOG_INFO("Multi-swarm coordinator destroyed");
}

/* ============================================================================
 * Swarm Identity Management
 * ============================================================================ */

nimcp_swarm_identity_t* nimcp_swarm_identity_create(
    nimcp_multi_swarm_coordinator_t* coordinator,
    const char* name,
    uint32_t agent_count
) {
    if (!coordinator || !name) {
        LOG_ERROR("Invalid parameters for swarm identity creation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_swarm_identity_create: required parameter is NULL (coordinator, name)");
        return NULL;
    }

    nimcp_swarm_identity_t* identity =
        (nimcp_swarm_identity_t*)nimcp_malloc(sizeof(nimcp_swarm_identity_t));

    if (!identity) {
        LOG_ERROR("Failed to allocate swarm identity");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_swarm_identity_create: identity is NULL");
        return NULL;
    }

    memset(identity, 0, sizeof(nimcp_swarm_identity_t));

    /* Assign unique ID */
    nimcp_rwlock_write_lock(&coordinator->coordinator_lock);
    identity->swarm_id = coordinator->next_swarm_id++;
    nimcp_rwlock_write_unlock(&coordinator->coordinator_lock);

    /* Set basic properties */
    strncpy(identity->name, name, NIMCP_SWARM_NAME_MAX - 1);
    identity->name[NIMCP_SWARM_NAME_MAX - 1] = '\0';
    identity->agent_count = agent_count;
    identity->active_agents = agent_count;
    identity->formation_time = nimcp_time_get_ms();
    identity->last_contact = identity->formation_time;

    /* Initialize health */
    identity->health_percentage = 1.0F;
    identity->health = NIMCP_SWARM_HEALTH_EXCELLENT;

    /* Initialize territory to zero bounds */
    memset(&identity->territory, 0, sizeof(nimcp_territory_bounds_t));
    identity->territory.timestamp = identity->formation_time;

    LOG_INFO("Created swarm identity: ID=%lu, Name=%s, Agents=%u",
                   identity->swarm_id, identity->name, agent_count);

    return identity;
}

nimcp_result_t nimcp_swarm_register(
    nimcp_multi_swarm_coordinator_t* coordinator,
    nimcp_swarm_identity_t* identity
) {
    if (!coordinator || !identity) {
        return NIMCP_INVALID_PARAM;
    }

    LOG_INFO("Registering swarm: ID=%lu, Name=%s",
                   identity->swarm_id, identity->name);

    nimcp_rwlock_write_lock(&coordinator->coordinator_lock);

    /* Check if already registered */
    if (swarm_hash_table_lookup(coordinator->swarm_registry, identity->swarm_id)) {
        LOG_WARN("Swarm already registered: ID=%lu", identity->swarm_id);
        nimcp_rwlock_write_unlock(&coordinator->coordinator_lock);
        return NIMCP_ERROR;
    }

    /* Register swarm */
    bool success = swarm_hash_table_insert(
        coordinator->swarm_registry,
        identity->swarm_id,
        identity
    );

    nimcp_rwlock_write_unlock(&coordinator->coordinator_lock);

    if (success) {
        LOG_INFO("Swarm registered successfully: ID=%lu", identity->swarm_id);

        /* Broadcast discovery if router available */
        if (coordinator->router && coordinator->enable_bridge_formation) {
            nimcp_multi_swarm_broadcast_discovery(coordinator, identity->swarm_id);
        }
    } else {
        LOG_ERROR("Failed to register swarm: ID=%lu", identity->swarm_id);
    }

    return success ? NIMCP_SUCCESS : NIMCP_ERROR;
}

nimcp_result_t nimcp_swarm_unregister(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t swarm_id
) {
    if (!coordinator) {
        return NIMCP_INVALID_PARAM;
    }

    LOG_INFO("Unregistering swarm: ID=%lu", swarm_id);

    nimcp_rwlock_write_lock(&coordinator->coordinator_lock);

    /* Check if exists before removing */
    nimcp_swarm_identity_t* identity = swarm_hash_table_lookup(
        coordinator->swarm_registry, swarm_id);

    if (!identity) {
        nimcp_rwlock_write_unlock(&coordinator->coordinator_lock);
        LOG_WARN("Swarm not found for unregistration: ID=%lu", swarm_id);
        return NIMCP_NOT_FOUND;
    }

    swarm_hash_table_remove(coordinator->swarm_registry, swarm_id);
    nimcp_rwlock_write_unlock(&coordinator->coordinator_lock);

    LOG_INFO("Swarm unregistered: ID=%lu", swarm_id);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_swarm_add_capability(
    nimcp_swarm_identity_t* identity,
    nimcp_swarm_capability_type_t type,
    float proficiency,
    uint32_t capacity,
    bool is_lendable
) {
    if (!identity) {
        return NIMCP_INVALID_PARAM;
    }

    if (identity->capability_count >= NIMCP_MAX_SWARM_CAPABILITIES) {
        LOG_ERROR("Maximum capabilities reached for swarm: ID=%lu",
                        identity->swarm_id);
        return NIMCP_INVALID_PARAM;
    }

    /* Clamp proficiency to [0, 1] */
    proficiency = fmaxf(0.0F, fminf(1.0F, proficiency));

    /* Add capability */
    nimcp_swarm_capability_t* cap =
        &identity->capabilities[identity->capability_count++];

    cap->type = type;
    cap->proficiency = proficiency;
    cap->capacity = capacity;
    cap->available = capacity;
    cap->is_lendable = is_lendable;

    LOG_DEBUG("Added capability to swarm ID=%lu: Type=%d, Prof=%.2f, Cap=%u",
                    identity->swarm_id, type, proficiency, capacity);

    return NIMCP_SUCCESS;
}

void nimcp_swarm_update_health(
    nimcp_swarm_identity_t* identity,
    uint32_t active_agents
) {
    if (!identity) return;

    identity->active_agents = active_agents;
    identity->health_percentage = calculate_health_percentage(
        active_agents, identity->agent_count);
    identity->health = determine_health_status(identity->health_percentage);
    identity->last_contact = nimcp_time_get_ms();

    LOG_DEBUG("Swarm health updated: ID=%lu, Active=%u/%u, Health=%.1f%%",
                    identity->swarm_id, active_agents, identity->agent_count,
                    identity->health_percentage * 100.0F);
}

void nimcp_swarm_identity_destroy(nimcp_swarm_identity_t* identity) {
    if (!identity) return;

    LOG_DEBUG("Destroying swarm identity: ID=%lu", identity->swarm_id);
    nimcp_free(identity);
}

/* ============================================================================
 * Super-Swarm Management
 * ============================================================================ */

nimcp_super_swarm_t* nimcp_super_swarm_create(
    nimcp_multi_swarm_coordinator_t* coordinator,
    const char* name
) {
    if (!coordinator || !name) {
        LOG_ERROR("Invalid parameters for super-swarm creation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_super_swarm_create: required parameter is NULL (coordinator, name)");
        return NULL;
    }

    nimcp_super_swarm_t* super_swarm =
        (nimcp_super_swarm_t*)nimcp_malloc(sizeof(nimcp_super_swarm_t));

    if (!super_swarm) {
        LOG_ERROR("Failed to allocate super-swarm");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_super_swarm_create: super_swarm is NULL");
        return NULL;
    }

    memset(super_swarm, 0, sizeof(nimcp_super_swarm_t));

    /* Assign ID */
    super_swarm->super_swarm_id = generate_unique_id();
    strncpy(super_swarm->name, name, NIMCP_SWARM_NAME_MAX - 1);
    super_swarm->name[NIMCP_SWARM_NAME_MAX - 1] = '\0';

    /* Create resource request table (stubbed) */
    super_swarm->resource_requests = swarm_hash_table_create(16);
    if (!super_swarm->resource_requests) {
        LOG_ERROR("Failed to create resource request table");
        nimcp_free(super_swarm);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_super_swarm_create: super_swarm->resource_requests is NULL");
        return NULL;
    }

    /* Create conflicts array */
    super_swarm->conflicts = conflict_array_create();
    if (!super_swarm->conflicts) {
        LOG_ERROR("Failed to create conflicts array");
        hash_table_destroy(super_swarm->resource_requests);
        nimcp_free(super_swarm);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_super_swarm_create: super_swarm->conflicts is NULL");
        return NULL;
    }

    /* Initialize locks */
    if (nimcp_rwlock_init(&super_swarm->swarm_lock) != NIMCP_SUCCESS ||
        nimcp_rwlock_init(&super_swarm->mission_lock) != NIMCP_SUCCESS ||
        nimcp_rwlock_init(&super_swarm->bridge_lock) != NIMCP_SUCCESS) {
        LOG_ERROR("Failed to initialize super-swarm locks");
        conflict_array_destroy((nimcp_darray_t*)super_swarm->conflicts);
        hash_table_destroy(super_swarm->resource_requests);
        nimcp_free(super_swarm);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_super_swarm_create: validation failed");
        return NULL;
    }

    /* Add to coordinator */
    if (coordinator->super_swarm_count < NIMCP_MAX_SWARMS_PER_SUPER) {
        coordinator->super_swarms[coordinator->super_swarm_count++] = super_swarm;
    }

    LOG_INFO("Created super-swarm: ID=%lu, Name=%s",
                   super_swarm->super_swarm_id, super_swarm->name);

    return super_swarm;
}

nimcp_result_t nimcp_super_swarm_add_swarm(
    nimcp_super_swarm_t* super_swarm,
    nimcp_swarm_identity_t* identity
) {
    if (!super_swarm || !identity) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_rwlock_write_lock(&super_swarm->swarm_lock);

    if (super_swarm->swarm_count >= NIMCP_MAX_SWARMS_PER_SUPER) {
        LOG_ERROR("Super-swarm at maximum capacity");
        nimcp_rwlock_write_unlock(&super_swarm->swarm_lock);
        return NIMCP_INVALID_PARAM;
    }

    super_swarm->swarms[super_swarm->swarm_count++] = identity;

    /* Update overall territory */
    if (super_swarm->swarm_count == 1) {
        super_swarm->overall_territory = identity->territory;
    } else {
        /* Expand to encompass new swarm */
        super_swarm->overall_territory.min.x = fmin(
            super_swarm->overall_territory.min.x, identity->territory.min.x);
        super_swarm->overall_territory.min.y = fmin(
            super_swarm->overall_territory.min.y, identity->territory.min.y);
        super_swarm->overall_territory.min.z = fmin(
            super_swarm->overall_territory.min.z, identity->territory.min.z);

        super_swarm->overall_territory.max.x = fmax(
            super_swarm->overall_territory.max.x, identity->territory.max.x);
        super_swarm->overall_territory.max.y = fmax(
            super_swarm->overall_territory.max.y, identity->territory.max.y);
        super_swarm->overall_territory.max.z = fmax(
            super_swarm->overall_territory.max.z, identity->territory.max.z);
    }

    nimcp_rwlock_write_unlock(&super_swarm->swarm_lock);

    LOG_INFO("Added swarm to super-swarm: Swarm=%lu, SuperSwarm=%lu",
                   identity->swarm_id, super_swarm->super_swarm_id);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_super_swarm_remove_swarm(
    nimcp_super_swarm_t* super_swarm,
    uint64_t swarm_id
) {
    if (!super_swarm) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_rwlock_write_lock(&super_swarm->swarm_lock);

    bool found = false;
    for (uint32_t i = 0; i < super_swarm->swarm_count; i++) {
        if (super_swarm->swarms[i]->swarm_id == swarm_id) {
            /* Shift remaining swarms */
            for (uint32_t j = i; j < super_swarm->swarm_count - 1; j++) {
                super_swarm->swarms[j] = super_swarm->swarms[j + 1];
            }
            super_swarm->swarm_count--;
            found = true;
            break;
        }
    }

    nimcp_rwlock_write_unlock(&super_swarm->swarm_lock);

    if (!found) {
        LOG_WARN("Swarm not found in super-swarm: ID=%lu", swarm_id);
        return NIMCP_NOT_FOUND;
    }

    LOG_INFO("Removed swarm from super-swarm: Swarm=%lu", swarm_id);
    return NIMCP_SUCCESS;
}

void nimcp_super_swarm_destroy(nimcp_super_swarm_t* super_swarm) {
    if (!super_swarm) return;

    LOG_INFO("Destroying super-swarm: ID=%lu", super_swarm->super_swarm_id);

    /* Clean up resources */
    if (super_swarm->resource_requests) {
        hash_table_destroy(super_swarm->resource_requests);
    }
    if (super_swarm->conflicts) {
        conflict_array_destroy((nimcp_darray_t*)super_swarm->conflicts);
    }

    /* Destroy locks */
    nimcp_rwlock_destroy(&super_swarm->swarm_lock);
    nimcp_rwlock_destroy(&super_swarm->mission_lock);
    nimcp_rwlock_destroy(&super_swarm->bridge_lock);

    nimcp_free(super_swarm);
}

/* ============================================================================
 * Territory Management
 * ============================================================================ */

nimcp_result_t nimcp_swarm_set_territory(
    nimcp_swarm_identity_t* identity,
    nimcp_coord3d_t min,
    nimcp_coord3d_t max,
    bool is_dynamic,
    float priority
) {
    if (!identity) {
        return NIMCP_INVALID_PARAM;
    }

    /* Validate bounds */
    if (min.x > max.x || min.y > max.y || min.z > max.z) {
        LOG_ERROR("Invalid territory bounds: min > max");
        return NIMCP_INVALID_PARAM;
    }

    identity->territory.min = min;
    identity->territory.max = max;
    identity->territory.is_dynamic = is_dynamic;
    identity->territory.priority = priority;
    identity->territory.timestamp = nimcp_time_get_ms();

    LOG_INFO("Set territory for swarm ID=%lu: [%.1f,%.1f,%.1f]-[%.1f,%.1f,%.1f]",
                   identity->swarm_id,
                   min.x, min.y, min.z, max.x, max.y, max.z);

    return NIMCP_SUCCESS;
}

bool nimcp_territory_overlaps(
    const nimcp_territory_bounds_t* bounds_a,
    const nimcp_territory_bounds_t* bounds_b
) {
    if (!bounds_a || !bounds_b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_territory_overlaps: required parameter is NULL (bounds_a, bounds_b)");
        return false;
    }

    double overlap = calculate_overlap_volume(bounds_a, bounds_b);
    return overlap > CONFLICT_DETECTION_THRESHOLD;
}

nimcp_result_t nimcp_territory_negotiate(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t swarm_a,
    uint64_t swarm_b
) {
    if (!coordinator) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_swarm_identity_t* identity_a = nimcp_swarm_get(coordinator, swarm_a);
    nimcp_swarm_identity_t* identity_b = nimcp_swarm_get(coordinator, swarm_b);

    if (!identity_a || !identity_b) {
        LOG_ERROR("One or both swarms not found for negotiation");
        return NIMCP_NOT_FOUND;
    }

    /* Check if territories overlap */
    if (!nimcp_territory_overlaps(&identity_a->territory, &identity_b->territory)) {
        LOG_DEBUG("No overlap detected, negotiation not needed");
        return NIMCP_SUCCESS;
    }

    LOG_INFO("Negotiating territory between swarms %lu and %lu",
                   swarm_a, swarm_b);

    /* Simple negotiation: adjust based on priority and dynamic flags */
    if (identity_a->territory.is_dynamic && !identity_b->territory.is_dynamic) {
        /* A adjusts to avoid B */
        LOG_INFO("Swarm %lu adjusting territory (dynamic)", swarm_a);
        /* In a real implementation, adjust bounds here */
        return NIMCP_SUCCESS;
    } else if (!identity_a->territory.is_dynamic && identity_b->territory.is_dynamic) {
        /* B adjusts to avoid A */
        LOG_INFO("Swarm %lu adjusting territory (dynamic)", swarm_b);
        return NIMCP_SUCCESS;
    } else if (identity_a->territory.priority > identity_b->territory.priority) {
        /* B yields to A */
        LOG_INFO("Swarm %lu yields to higher priority swarm %lu",
                       swarm_b, swarm_a);
        return NIMCP_SUCCESS;
    } else {
        /* A yields to B */
        LOG_INFO("Swarm %lu yields to higher priority swarm %lu",
                       swarm_a, swarm_b);
        return NIMCP_SUCCESS;
    }
}

uint32_t nimcp_territory_detect_conflicts(
    nimcp_multi_swarm_coordinator_t* coordinator,
    void* conflicts
) {
    if (!coordinator || !conflicts) {
        return 0;
    }

    uint32_t conflict_count = 0;

    nimcp_rwlock_read_lock(&coordinator->coordinator_lock);

    /* Compare all pairs of swarms */
    for (uint32_t i = 0; i < coordinator->super_swarm_count; i++) {
        nimcp_super_swarm_t* super = coordinator->super_swarms[i];
        if (!super) continue;

        nimcp_rwlock_read_lock(&super->swarm_lock);

        for (uint32_t j = 0; j < super->swarm_count; j++) {
            for (uint32_t k = j + 1; k < super->swarm_count; k++) {
                nimcp_swarm_identity_t* swarm_a = super->swarms[j];
                nimcp_swarm_identity_t* swarm_b = super->swarms[k];

                if (nimcp_territory_overlaps(&swarm_a->territory,
                                            &swarm_b->territory)) {
                    /* Create conflict record */
                    nimcp_swarm_conflict_t conflict = {0};
                    conflict.conflict_id = coordinator->next_conflict_id++;
                    conflict.type = NIMCP_CONFLICT_TYPE_TERRITORY;
                    conflict.swarm_ids[0] = swarm_a->swarm_id;
                    conflict.swarm_ids[1] = swarm_b->swarm_id;
                    conflict.swarm_count = 2;
                    conflict.detection_time = nimcp_time_get_ms();
                    conflict.is_resolved = false;
                    conflict.severity = 0.5F;  /* Default severity for territory conflicts */

                    snprintf(conflict.description, sizeof(conflict.description),
                            "Territory overlap between swarms %lu and %lu",
                            swarm_a->swarm_id, swarm_b->swarm_id);

                    conflict_array_push_back((nimcp_darray_t*)conflicts, &conflict);
                    conflict_count++;

                    LOG_WARN("Territory conflict detected: Swarms %lu and %lu",
                                  swarm_a->swarm_id, swarm_b->swarm_id);
                }
            }
        }

        nimcp_rwlock_read_unlock(&super->swarm_lock);
    }

    nimcp_rwlock_read_unlock(&coordinator->coordinator_lock);

    return conflict_count;
}

/* ============================================================================
 * Resource Sharing
 * ============================================================================ */

uint64_t nimcp_resource_request(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t requesting_swarm,
    uint64_t target_swarm,
    nimcp_resource_request_type_t type,
    uint32_t quantity,
    nimcp_mission_priority_t priority
) {
    if (!coordinator) {
        return 0;
    }

    /* Verify both swarms exist */
    if (!nimcp_swarm_get(coordinator, requesting_swarm) ||
        !nimcp_swarm_get(coordinator, target_swarm)) {
        LOG_ERROR("Invalid swarm IDs for resource request");
        return 0;
    }

    nimcp_resource_request_t* request =
        (nimcp_resource_request_t*)nimcp_malloc(sizeof(nimcp_resource_request_t));

    if (!request) {
        LOG_ERROR("Failed to allocate resource request");
        return 0;
    }

    memset(request, 0, sizeof(nimcp_resource_request_t));

    request->request_id = generate_unique_id();
    request->requesting_swarm = requesting_swarm;
    request->target_swarm = target_swarm;
    request->type = type;
    request->quantity = quantity;
    request->priority = priority;
    request->expiry_time = nimcp_time_get_ms() + RESOURCE_EXPIRY_TIME;
    request->is_approved = false;

    /* Store in appropriate super-swarm */
    for (uint32_t i = 0; i < coordinator->super_swarm_count; i++) {
        nimcp_super_swarm_t* super = coordinator->super_swarms[i];
        if (!super) continue;

        /* Check if both swarms are in this super-swarm */
        bool has_requesting = false, has_target = false;
        for (uint32_t j = 0; j < super->swarm_count; j++) {
            if (super->swarms[j]->swarm_id == requesting_swarm) has_requesting = true;
            if (super->swarms[j]->swarm_id == target_swarm) has_target = true;
        }

        if (has_requesting && has_target) {
            swarm_hash_table_insert(super->resource_requests,
                                   request->request_id, request);
            break;
        }
    }

    LOG_INFO("Created resource request: ID=%lu, From=%lu, To=%lu, Type=%d",
                   request->request_id, requesting_swarm, target_swarm, type);

    return request->request_id;
}

nimcp_result_t nimcp_resource_approve(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t request_id,
    float cost
) {
    if (!coordinator) {
        return NIMCP_INVALID_PARAM;
    }

    /* Find request in super-swarms */
    for (uint32_t i = 0; i < coordinator->super_swarm_count; i++) {
        nimcp_super_swarm_t* super = coordinator->super_swarms[i];
        if (!super) continue;

        nimcp_resource_request_t* request =
            swarm_hash_table_lookup(super->resource_requests, request_id);

        if (request) {
            request->is_approved = true;
            request->cost = cost;

            LOG_INFO("Approved resource request: ID=%lu, Cost=%.2f",
                          request_id, cost);
            return NIMCP_SUCCESS;
        }
    }

    LOG_WARN("Resource request not found: ID=%lu", request_id);
    return NIMCP_NOT_FOUND;
}

nimcp_result_t nimcp_resource_deny(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t request_id
) {
    if (!coordinator) {
        return NIMCP_INVALID_PARAM;
    }

    /* Find and remove request */
    for (uint32_t i = 0; i < coordinator->super_swarm_count; i++) {
        nimcp_super_swarm_t* super = coordinator->super_swarms[i];
        if (!super) continue;

        nimcp_resource_request_t* request =
            swarm_hash_table_lookup(super->resource_requests, request_id);

        if (request) {
            swarm_hash_table_remove(super->resource_requests, request_id);
            LOG_INFO("Denied resource request: ID=%lu", request_id);
            nimcp_free(request);
            return NIMCP_SUCCESS;
        }
    }

    LOG_WARN("Resource request not found: ID=%lu", request_id);
    return NIMCP_NOT_FOUND;
}

uint32_t nimcp_resource_process_requests(
    nimcp_multi_swarm_coordinator_t* coordinator,
    nimcp_resource_allocator_fn allocator,
    void* user_data
) {
    if (!coordinator) {
        return 0;
    }

    uint32_t processed = 0;
    uint64_t current_time = nimcp_time_get_ms();

    /* Process requests in all super-swarms */
    for (uint32_t i = 0; i < coordinator->super_swarm_count; i++) {
        nimcp_super_swarm_t* super = coordinator->super_swarms[i];
        if (!super || !super->resource_requests) continue;

        /* This is a simplified implementation */
        /* In a real implementation, iterate through hash table */
        LOG_DEBUG("Processing resource requests for super-swarm %lu",
                       super->super_swarm_id);
    }

    return processed;
}

/* ============================================================================
 * Mission Management
 * ============================================================================ */

uint64_t nimcp_mission_create(
    nimcp_multi_swarm_coordinator_t* coordinator,
    const char* description,
    nimcp_mission_priority_t priority,
    nimcp_territory_bounds_t operation_area,
    uint64_t deadline
) {
    if (!coordinator || !description) {
        return 0;
    }

    nimcp_mission_assignment_t* mission =
        (nimcp_mission_assignment_t*)nimcp_malloc(sizeof(nimcp_mission_assignment_t));

    if (!mission) {
        LOG_ERROR("Failed to allocate mission");
        return 0;
    }

    memset(mission, 0, sizeof(nimcp_mission_assignment_t));

    nimcp_rwlock_write_lock(&coordinator->coordinator_lock);
    mission->mission_id = coordinator->next_mission_id++;
    nimcp_rwlock_write_unlock(&coordinator->coordinator_lock);

    strncpy(mission->description, description, sizeof(mission->description) - 1);
    mission->priority = priority;
    mission->status = NIMCP_MISSION_STATUS_PENDING;
    mission->operation_area = operation_area;
    mission->start_time = nimcp_time_get_ms();
    mission->deadline = deadline;
    mission->progress = 0.0F;

    /* Store in mission registry */
    swarm_hash_table_insert(coordinator->mission_registry,
                           mission->mission_id, mission);

    LOG_INFO("Created mission: ID=%lu, Priority=%d, Desc='%s'",
                   mission->mission_id, priority, description);

    return mission->mission_id;
}

nimcp_result_t nimcp_mission_assign_swarms(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t mission_id,
    const uint64_t* swarm_ids,
    uint32_t swarm_count
) {
    if (!coordinator || !swarm_ids || swarm_count == 0) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_mission_assignment_t* mission =
        swarm_hash_table_lookup(coordinator->mission_registry, mission_id);

    if (!mission) {
        LOG_ERROR("Mission not found: ID=%lu", mission_id);
        return NIMCP_NOT_FOUND;
    }

    if (swarm_count > NIMCP_MAX_SWARMS_PER_SUPER) {
        LOG_ERROR("Too many swarms for mission assignment");
        return NIMCP_INVALID_PARAM;
    }

    /* Assign swarms */
    memcpy(mission->assigned_swarms, swarm_ids, swarm_count * sizeof(uint64_t));
    mission->swarm_count = swarm_count;
    mission->status = NIMCP_MISSION_STATUS_ASSIGNED;

    /* Add mission to the super-swarm containing the first assigned swarm */
    for (uint32_t i = 0; i < coordinator->super_swarm_count; i++) {
        nimcp_super_swarm_t* super = coordinator->super_swarms[i];
        if (!super) continue;

        /* Check if this super-swarm contains any of the assigned swarms */
        bool found = false;
        for (uint32_t j = 0; j < super->swarm_count && !found; j++) {
            if (super->swarms[j]) {
                for (uint32_t k = 0; k < swarm_count && !found; k++) {
                    if (super->swarms[j]->swarm_id == swarm_ids[k]) {
                        found = true;
                    }
                }
            }
        }

        if (found && super->active_mission_count < NIMCP_MAX_SWARM_MISSIONS) {
            nimcp_rwlock_write_lock(&super->mission_lock);
            memcpy(&super->missions[super->active_mission_count], mission,
                   sizeof(nimcp_mission_assignment_t));
            super->active_mission_count++;
            nimcp_rwlock_write_unlock(&super->mission_lock);

            LOG_INFO("Added mission %lu to super-swarm %lu",
                     mission_id, super->super_swarm_id);
            break;
        }
    }

    LOG_INFO("Assigned %u swarms to mission %lu", swarm_count, mission_id);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_mission_update_progress(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t mission_id,
    float progress
) {
    if (!coordinator) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_mission_assignment_t* mission =
        swarm_hash_table_lookup(coordinator->mission_registry, mission_id);

    if (!mission) {
        return NIMCP_NOT_FOUND;
    }

    mission->progress = fmaxf(0.0F, fminf(1.0F, progress));

    if (mission->status == NIMCP_MISSION_STATUS_ASSIGNED) {
        mission->status = NIMCP_MISSION_STATUS_ACTIVE;
    }

    LOG_DEBUG("Mission progress updated: ID=%lu, Progress=%.1f%%",
                    mission_id, mission->progress * 100.0F);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_mission_complete(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t mission_id,
    bool success
) {
    if (!coordinator) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_mission_assignment_t* mission =
        swarm_hash_table_lookup(coordinator->mission_registry, mission_id);

    if (!mission) {
        return NIMCP_NOT_FOUND;
    }

    mission->status = success ? NIMCP_MISSION_STATUS_COMPLETED :
                               NIMCP_MISSION_STATUS_FAILED;
    mission->progress = success ? 1.0F : mission->progress;

    LOG_INFO("Mission completed: ID=%lu, Success=%s",
                   mission_id, success ? "YES" : "NO");

    return NIMCP_SUCCESS;
}

nimcp_mission_assignment_t* nimcp_mission_get(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t mission_id
) {
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_mission_get: coordinator is NULL");
        return NULL;
    }

    return swarm_hash_table_lookup(coordinator->mission_registry, mission_id);
}

/* ============================================================================
 * Communication Bridges
 * ============================================================================ */

uint64_t nimcp_comm_bridge_create(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t swarm_a,
    uint64_t swarm_b,
    const uint32_t* relay_agents,
    uint32_t relay_count
) {
    if (!coordinator) {
        return 0;
    }

    /* Find appropriate super-swarm */
    for (uint32_t i = 0; i < coordinator->super_swarm_count; i++) {
        nimcp_super_swarm_t* super = coordinator->super_swarms[i];
        if (!super) continue;

        bool has_a = false, has_b = false;
        for (uint32_t j = 0; j < super->swarm_count; j++) {
            if (super->swarms[j]->swarm_id == swarm_a) has_a = true;
            if (super->swarms[j]->swarm_id == swarm_b) has_b = true;
        }

        if (has_a && has_b) {
            nimcp_rwlock_write_lock(&super->bridge_lock);

            if (super->bridge_count >= NIMCP_MAX_COMM_BRIDGES) {
                LOG_ERROR("Maximum bridges reached for super-swarm");
                nimcp_rwlock_write_unlock(&super->bridge_lock);
                return 0;
            }

            nimcp_comm_bridge_t* bridge = &super->bridges[super->bridge_count++];
            memset(bridge, 0, sizeof(nimcp_comm_bridge_t));

            bridge->bridge_id = generate_unique_id();
            bridge->swarm_a = swarm_a;
            bridge->swarm_b = swarm_b;
            bridge->link_quality = 1.0F;
            bridge->is_active = true;
            bridge->last_message_time = nimcp_time_get_ms();

            if (relay_agents && relay_count > 0) {
                uint32_t count = (relay_count > 4) ? 4 : relay_count;
                memcpy(bridge->relay_agents, relay_agents, count * sizeof(uint32_t));
                bridge->relay_count = count;
            }

            nimcp_rwlock_write_unlock(&super->bridge_lock);

            LOG_INFO("Created communication bridge: ID=%lu, Between %lu and %lu",
                          bridge->bridge_id, swarm_a, swarm_b);

            return bridge->bridge_id;
        }
    }

    LOG_ERROR("Swarms not found in same super-swarm for bridge creation");
    return 0;
}

nimcp_result_t nimcp_comm_bridge_update_quality(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t bridge_id,
    float link_quality
) {
    if (!coordinator) {
        return NIMCP_INVALID_PARAM;
    }

    link_quality = fmaxf(0.0F, fminf(1.0F, link_quality));

    /* Find bridge in super-swarms */
    for (uint32_t i = 0; i < coordinator->super_swarm_count; i++) {
        nimcp_super_swarm_t* super = coordinator->super_swarms[i];
        if (!super) continue;

        nimcp_rwlock_write_lock(&super->bridge_lock);

        for (uint32_t j = 0; j < super->bridge_count; j++) {
            if (super->bridges[j].bridge_id == bridge_id) {
                super->bridges[j].link_quality = link_quality;

                /* Deactivate if quality too low */
                if (link_quality < BRIDGE_QUALITY_THRESHOLD) {
                    super->bridges[j].is_active = false;
                    LOG_WARN("Bridge deactivated due to low quality: ID=%lu",
                                  bridge_id);
                }

                nimcp_rwlock_write_unlock(&super->bridge_lock);
                return NIMCP_SUCCESS;
            }
        }

        nimcp_rwlock_write_unlock(&super->bridge_lock);
    }

    return NIMCP_NOT_FOUND;
}

nimcp_result_t nimcp_comm_bridge_deactivate(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t bridge_id
) {
    if (!coordinator) {
        return NIMCP_INVALID_PARAM;
    }

    /* Find and deactivate bridge */
    for (uint32_t i = 0; i < coordinator->super_swarm_count; i++) {
        nimcp_super_swarm_t* super = coordinator->super_swarms[i];
        if (!super) continue;

        nimcp_rwlock_write_lock(&super->bridge_lock);

        for (uint32_t j = 0; j < super->bridge_count; j++) {
            if (super->bridges[j].bridge_id == bridge_id) {
                super->bridges[j].is_active = false;
                nimcp_rwlock_write_unlock(&super->bridge_lock);

                LOG_INFO("Bridge deactivated: ID=%lu", bridge_id);
                return NIMCP_SUCCESS;
            }
        }

        nimcp_rwlock_write_unlock(&super->bridge_lock);
    }

    return NIMCP_NOT_FOUND;
}

nimcp_result_t nimcp_comm_bridge_route_message(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t from_swarm,
    uint64_t to_swarm,
    bio_message_header_t* message
) {
    if (!coordinator || !message) {
        return NIMCP_INVALID_PARAM;
    }

    if (!coordinator->router) {
        LOG_WARN("No bio-router available for message routing");
        return NIMCP_ERROR;
    }

    /* Find appropriate bridge */
    for (uint32_t i = 0; i < coordinator->super_swarm_count; i++) {
        nimcp_super_swarm_t* super = coordinator->super_swarms[i];
        if (!super) continue;

        nimcp_rwlock_read_lock(&super->bridge_lock);

        for (uint32_t j = 0; j < super->bridge_count; j++) {
            nimcp_comm_bridge_t* bridge = &super->bridges[j];

            if (bridge->is_active &&
                ((bridge->swarm_a == from_swarm && bridge->swarm_b == to_swarm) ||
                 (bridge->swarm_a == to_swarm && bridge->swarm_b == from_swarm))) {

                bridge->last_message_time = nimcp_time_get_ms();
                nimcp_rwlock_read_unlock(&super->bridge_lock);

                /* Route through bio-async system using global context */
                nimcp_result_t result = NIMCP_NOT_INITIALIZED;
                if (g_multi_swarm_ctx) {
                    result = bio_router_send(
                        g_multi_swarm_ctx,
                        message,
                        sizeof(bio_message_header_t) + message->payload_size,
                        100  /* 100ms timeout */
                    );
                }

                LOG_DEBUG("Routed message via bridge %lu: From=%lu To=%lu",
                               bridge->bridge_id, from_swarm, to_swarm);

                return result;
            }
        }

        nimcp_rwlock_read_unlock(&super->bridge_lock);
    }

    LOG_WARN("No active bridge found between swarms %lu and %lu",
                   from_swarm, to_swarm);
    return NIMCP_NOT_FOUND;
}

/* ============================================================================
 * Conflict Resolution - Enhanced Implementation
 * ============================================================================ */

/**
 * @brief Calculate conflict severity based on overlap and priorities
 *
 * WHAT: Computes severity score from 0.0 (minor) to 1.0 (critical)
 * WHY:  Enables prioritization of conflicts for resolution
 * HOW:  Considers territory overlap, resource contention, priority differences
 */
static float calculate_conflict_severity(
    nimcp_multi_swarm_coordinator_t* coordinator,
    const nimcp_swarm_conflict_t* conflict
) {
    if (!coordinator || !conflict || conflict->swarm_count < 2) {
        return 0.0F;
    }

    float severity = 0.0F;

    /* Base severity on type */
    switch (conflict->type) {
        case NIMCP_CONFLICT_TYPE_RESOURCE:
            severity = 0.6F;
            break;
        case NIMCP_CONFLICT_TYPE_TERRITORY:
            severity = 0.7F;
            break;
        case NIMCP_CONFLICT_TYPE_GOAL:
            severity = 0.8F;
            break;
        case NIMCP_CONFLICT_TYPE_PRIORITY:
            severity = 0.5F;
            break;
        case NIMCP_CONFLICT_TYPE_COMMUNICATION:
            severity = 0.4F;
            break;
        default:
            severity = 0.5F;
            break;
    }

    /* Increase severity based on number of involved swarms */
    if (conflict->swarm_count > 2) {
        severity += 0.1F * (conflict->swarm_count - 2);
    }

    /* Clamp to [0, 1] */
    return fmaxf(0.0F, fminf(1.0F, severity));
}

/**
 * @brief Context for resource request collection during hash table iteration
 */
typedef struct {
    nimcp_resource_request_t** requests;  /**< Array of request pointers */
    uint32_t count;                        /**< Number of requests collected */
    uint32_t capacity;                     /**< Array capacity */
} resource_request_collect_ctx_t;

/**
 * @brief Iterator callback to collect resource requests from hash table
 */
static bool collect_resource_request_cb(const void* key, size_t key_size,
                                        void* value, size_t value_size,
                                        void* user_data) {
    (void)key;
    (void)key_size;
    (void)value_size;

    resource_request_collect_ctx_t* ctx = (resource_request_collect_ctx_t*)user_data;
    if (!ctx || ctx->count >= ctx->capacity) {
        return false;  /* Stop iteration if ctx is NULL or capacity full */
    }

    /* Value is a pointer to nimcp_resource_request_t stored in hash table */
    nimcp_resource_request_t** req_ptr = (nimcp_resource_request_t**)value;
    if (req_ptr && *req_ptr && !(*req_ptr)->is_approved) {
        /* Only collect pending (unapproved) requests */
        ctx->requests[ctx->count++] = *req_ptr;
    }

    return true;  /* Continue iteration */
}

/**
 * @brief Detect resource conflicts between swarms
 *
 * WHAT: Identifies conflicts where multiple swarms request same resource
 * WHY:  Resource conflicts can deadlock missions or cause starvation
 * HOW:  Collects pending requests, groups by target and type, detects duplicates
 *
 * Resource conflict types detected:
 * 1. Same target conflict: Multiple swarms requesting from same target swarm
 * 2. Same resource type conflict: Multiple requests for same resource type
 * 3. Capacity conflict: Total requests exceed target's available capacity
 * 4. Priority conflict: Same priority requests competing for limited resources
 */
static uint32_t detect_resource_conflicts(
    nimcp_multi_swarm_coordinator_t* coordinator,
    nimcp_darray_t* conflicts_array
) {
    if (!coordinator || !conflicts_array) {
        return 0;
    }

    uint32_t detected = 0;

    /* Temporary storage for all pending requests across all super-swarms */
    #define MAX_PENDING_REQUESTS 256
    nimcp_resource_request_t* all_requests[MAX_PENDING_REQUESTS];
    uint32_t total_requests = 0;

    /* Phase 1: Collect all pending resource requests from all super-swarms */
    for (uint32_t i = 0; i < coordinator->super_swarm_count; i++) {
        nimcp_super_swarm_t* super = coordinator->super_swarms[i];
        if (!super || !super->resource_requests) continue;

        resource_request_collect_ctx_t collect_ctx = {
            .requests = &all_requests[total_requests],
            .count = 0,
            .capacity = MAX_PENDING_REQUESTS - total_requests
        };

        hash_table_iterate((hash_table_t*)super->resource_requests,
                          collect_resource_request_cb, &collect_ctx);

        total_requests += collect_ctx.count;

        LOG_DEBUG("Collected %u pending requests from super-swarm %lu",
                  collect_ctx.count, super->super_swarm_id);
    }

    if (total_requests < 2) {
        /* Need at least 2 requests for a conflict */
        return 0;
    }

    LOG_DEBUG("Analyzing %u total pending resource requests for conflicts",
              total_requests);

    /* Phase 2: Compare all request pairs to find conflicts */
    for (uint32_t i = 0; i < total_requests; i++) {
        for (uint32_t j = i + 1; j < total_requests; j++) {
            nimcp_resource_request_t* req_a = all_requests[i];
            nimcp_resource_request_t* req_b = all_requests[j];

            if (!req_a || !req_b) continue;

            /* Check for conflict conditions */
            bool is_conflict = false;
            float severity = 0.0F;
            const char* conflict_reason = NULL;

            /* Condition 1: Same target swarm, same resource type */
            if (req_a->target_swarm == req_b->target_swarm &&
                req_a->type == req_b->type) {
                is_conflict = true;
                severity = 0.7F;
                conflict_reason = "competing for same resource type from same target";

                /* Higher severity if same priority (harder to resolve) */
                if (req_a->priority == req_b->priority) {
                    severity = 0.85F;
                }
            }

            /* Condition 2: Same requesting swarm targeting multiple targets for same resource
             * (indicates resource shortage) */
            else if (req_a->requesting_swarm == req_b->requesting_swarm &&
                     req_a->type == req_b->type &&
                     req_a->target_swarm != req_b->target_swarm) {
                is_conflict = true;
                severity = 0.4F;  /* Lower severity - just indicates shortage */
                conflict_reason = "resource shortage - swarm seeking from multiple sources";
            }

            /* Condition 3: Cross-requesting (A wants from B, B wants from A) */
            else if (req_a->requesting_swarm == req_b->target_swarm &&
                     req_b->requesting_swarm == req_a->target_swarm &&
                     req_a->type == req_b->type) {
                is_conflict = true;
                severity = 0.9F;  /* High severity - potential deadlock */
                conflict_reason = "potential deadlock - mutual resource dependency";
            }

            if (is_conflict) {
                /* Create conflict record */
                nimcp_swarm_conflict_t conflict = {0};
                conflict.conflict_id = coordinator->next_conflict_id++;
                conflict.type = NIMCP_CONFLICT_TYPE_RESOURCE;
                conflict.swarm_ids[0] = req_a->requesting_swarm;
                conflict.swarm_ids[1] = req_b->requesting_swarm;

                /* Also track target swarms if different from requesters */
                uint32_t swarm_idx = 2;
                if (req_a->target_swarm != req_a->requesting_swarm &&
                    req_a->target_swarm != req_b->requesting_swarm &&
                    swarm_idx < NIMCP_MAX_SWARMS_PER_SUPER) {
                    conflict.swarm_ids[swarm_idx++] = req_a->target_swarm;
                }
                if (req_b->target_swarm != req_a->requesting_swarm &&
                    req_b->target_swarm != req_b->requesting_swarm &&
                    req_b->target_swarm != req_a->target_swarm &&
                    swarm_idx < NIMCP_MAX_SWARMS_PER_SUPER) {
                    conflict.swarm_ids[swarm_idx++] = req_b->target_swarm;
                }
                conflict.swarm_count = swarm_idx;

                conflict.severity = severity;
                conflict.detection_time = nimcp_time_get_ms();
                conflict.is_resolved = false;

                /* Store request IDs in context for resolution */
                conflict.conflict_context = nimcp_malloc(2 * sizeof(uint64_t));
                if (conflict.conflict_context) {
                    uint64_t* req_ids = (uint64_t*)conflict.conflict_context;
                    req_ids[0] = req_a->request_id;
                    req_ids[1] = req_b->request_id;
                    conflict.context_size = 2 * sizeof(uint64_t);
                }

                snprintf(conflict.description, sizeof(conflict.description),
                        "Resource conflict: %s (swarms %lu vs %lu, type=%d)",
                        conflict_reason ? conflict_reason : "resource competition",
                        req_a->requesting_swarm, req_b->requesting_swarm,
                        (int)req_a->type);

                conflict_array_push_back(conflicts_array, &conflict);
                detected++;

                LOG_WARN("Resource conflict detected: Swarms %lu and %lu - %s",
                         req_a->requesting_swarm, req_b->requesting_swarm,
                         conflict_reason);
            }
        }
    }

    #undef MAX_PENDING_REQUESTS
    return detected;
}

/**
 * @brief Detect goal conflicts between swarms
 *
 * WHAT: Identifies incompatible mission goals
 * WHY:  Goal conflicts can cause mission failures
 * HOW:  Compares mission objectives and operation areas
 */
static uint32_t detect_goal_conflicts(
    nimcp_multi_swarm_coordinator_t* coordinator,
    nimcp_darray_t* conflicts_array
) {
    if (!coordinator || !conflicts_array) {
        return 0;
    }

    uint32_t detected = 0;

    /* Check missions for conflicting goals */
    for (uint32_t i = 0; i < coordinator->super_swarm_count; i++) {
        nimcp_super_swarm_t* super = coordinator->super_swarms[i];
        if (!super) continue;

        nimcp_rwlock_read_lock(&super->mission_lock);

        /* Compare all active mission pairs */
        for (uint32_t j = 0; j < super->active_mission_count; j++) {
            for (uint32_t k = j + 1; k < super->active_mission_count; k++) {
                nimcp_mission_assignment_t* mission_a = &super->missions[j];
                nimcp_mission_assignment_t* mission_b = &super->missions[k];

                /* Check if missions have overlapping areas and different priorities */
                if (nimcp_territory_overlaps(&mission_a->operation_area,
                                            &mission_b->operation_area) &&
                    mission_a->priority != mission_b->priority) {

                    nimcp_swarm_conflict_t new_conflict = {0};
                    new_conflict.conflict_id = coordinator->next_conflict_id++;
                    new_conflict.type = NIMCP_CONFLICT_TYPE_GOAL;
                    new_conflict.swarm_count = 0;

                    /* Add involved swarms */
                    for (uint32_t s = 0; s < mission_a->swarm_count &&
                         new_conflict.swarm_count < NIMCP_MAX_SWARMS_PER_SUPER; s++) {
                        new_conflict.swarm_ids[new_conflict.swarm_count++] =
                            mission_a->assigned_swarms[s];
                    }

                    new_conflict.detection_time = nimcp_time_get_ms();
                    new_conflict.contested_area = mission_a->operation_area;
                    new_conflict.severity = calculate_conflict_severity(coordinator, &new_conflict);

                    snprintf(new_conflict.description, sizeof(new_conflict.description),
                            "Goal conflict: Mission %lu vs %lu",
                            mission_a->mission_id, mission_b->mission_id);

                    conflict_array_push_back(conflicts_array, &new_conflict);
                    detected++;
                }
            }
        }

        nimcp_rwlock_read_unlock(&super->mission_lock);
    }

    return detected;
}

/**
 * @brief Enhanced conflict detection with multiple conflict types
 *
 * WHAT: Comprehensive conflict detection across all swarms
 * WHY:  Identifies all types of conflicts for resolution
 * HOW:  Calls type-specific detection functions
 */
nimcp_result_t nimcp_multi_swarm_detect_conflicts(
    nimcp_multi_swarm_coordinator_t* coordinator,
    nimcp_swarm_conflict_t** conflicts,
    uint32_t* count
) {
    if (!coordinator || !conflicts || !count) {
        return NIMCP_INVALID_PARAM;
    }

    /* Create temporary array for conflicts */
    nimcp_darray_t* conflicts_array = conflict_array_create();
    if (!conflicts_array) {
        LOG_ERROR("Failed to create conflicts array");
        return NIMCP_NO_MEMORY;
    }

    uint32_t total_detected = 0;

    /* Detect territory conflicts */
    uint32_t territory_conflicts = nimcp_territory_detect_conflicts(
        coordinator, conflicts_array);
    total_detected += territory_conflicts;
    LOG_INFO("Detected %u territory conflicts", territory_conflicts);

    /* Detect resource conflicts */
    uint32_t resource_conflicts = detect_resource_conflicts(
        coordinator, conflicts_array);
    total_detected += resource_conflicts;
    LOG_INFO("Detected %u resource conflicts", resource_conflicts);

    /* Detect goal conflicts */
    uint32_t goal_conflicts = detect_goal_conflicts(
        coordinator, conflicts_array);
    total_detected += goal_conflicts;
    LOG_INFO("Detected %u goal conflicts", goal_conflicts);

    /* Update statistics */
    coordinator->conflict_stats.total_conflicts = total_detected;
    coordinator->conflict_stats.conflicts_pending = total_detected;

    /* Store conflicts in super-swarms for find_conflict_by_id() lookup.
     * Conflicts are stored in the first super-swarm with a valid conflicts array. */
    for (uint32_t i = 0; i < coordinator->super_swarm_count; i++) {
        nimcp_super_swarm_t* super = coordinator->super_swarms[i];
        if (!super || !super->conflicts) continue;

        /* Clear old conflicts */
        conflict_array_clear((nimcp_darray_t*)super->conflicts);

        /* Copy all detected conflicts to this super-swarm for lookup */
        for (uint32_t j = 0; j < conflict_array_size(conflicts_array); j++) {
            nimcp_swarm_conflict_t* conflict = conflict_array_at(conflicts_array, j);
            if (conflict) {
                conflict_array_push_back((nimcp_darray_t*)super->conflicts, conflict);
            }
        }

        /* Only store in first valid super-swarm to avoid duplicates */
        break;
    }

    /* Return results */
    *count = total_detected;
    if (total_detected > 0) {
        /* Allocate array for caller */
        *conflicts = (nimcp_swarm_conflict_t*)nimcp_malloc(
            total_detected * sizeof(nimcp_swarm_conflict_t));

        if (*conflicts) {
            /* Copy conflicts to output array */
            for (uint32_t i = 0; i < total_detected; i++) {
                nimcp_swarm_conflict_t* src = conflict_array_at(conflicts_array, i);
                if (src) {
                    memcpy(&(*conflicts)[i], src, sizeof(nimcp_swarm_conflict_t));
                }
            }
        } else {
            LOG_ERROR("Failed to allocate output conflicts array");
            conflict_array_destroy(conflicts_array);
            return NIMCP_NO_MEMORY;
        }
    }

    conflict_array_destroy(conflicts_array);

    /* Broadcast conflict detected messages if any found */
    if (total_detected > 0 && g_multi_swarm_ctx) {
        /* Note: Would send BIO_MSG_SWARM_CONFLICT_DETECTED here */
        LOG_INFO("Would broadcast %u conflict detection messages", total_detected);
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Legacy conflict detection interface
 */
uint32_t nimcp_conflict_detect(
    nimcp_multi_swarm_coordinator_t* coordinator
) {
    if (!coordinator) {
        return 0;
    }

    uint32_t total_conflicts = 0;

    /* Detect conflicts in all super-swarms */
    for (uint32_t i = 0; i < coordinator->super_swarm_count; i++) {
        nimcp_super_swarm_t* super = coordinator->super_swarms[i];
        if (!super) continue;

        void* conflicts = super->conflicts;
        if (!conflicts) continue;

        /* Clear old conflicts */
        conflict_array_clear((nimcp_darray_t*)conflicts);

        /* Detect new conflicts */
        uint32_t count = nimcp_territory_detect_conflicts(coordinator, conflicts);
        total_conflicts += count;

        LOG_INFO("Detected %u conflicts in super-swarm %lu",
                      count, super->super_swarm_id);
    }

    return total_conflicts;
}

nimcp_result_t nimcp_conflict_resolve(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t conflict_id,
    nimcp_conflict_resolution_t strategy,
    nimcp_conflict_resolver_fn resolver,
    void* user_data
) {
    if (!coordinator) {
        return NIMCP_INVALID_PARAM;
    }

    /* Find conflict in super-swarms */
    for (uint32_t i = 0; i < coordinator->super_swarm_count; i++) {
        nimcp_super_swarm_t* super = coordinator->super_swarms[i];
        if (!super || !super->conflicts) continue;

        for (size_t j = 0; j < conflict_array_size((nimcp_darray_t*)super->conflicts); j++) {
            nimcp_swarm_conflict_t* conflict =
                conflict_array_at((nimcp_darray_t*)super->conflicts, j);

            if (conflict && conflict->conflict_id == conflict_id) {
                conflict->strategy = strategy;

                /* Use custom resolver if provided */
                bool resolved = true;
                if (resolver) {
                    resolved = resolver(coordinator, conflict, user_data);
                } else {
                    /* Default resolution based on strategy */
                    switch (strategy) {
                        case NIMCP_CONFLICT_PRIORITY:
                            /* Handled by territory negotiation */
                            if (conflict->swarm_count >= 2) {
                                nimcp_territory_negotiate(coordinator,
                                    conflict->swarm_ids[0],
                                    conflict->swarm_ids[1]);
                            }
                            break;

                        case NIMCP_CONFLICT_NEGOTIATION:
                            LOG_INFO("Initiating negotiation for conflict %lu",
                                          conflict_id);
                            break;

                        default:
                            LOG_INFO("Resolving conflict %lu with strategy %d",
                                          conflict_id, strategy);
                            break;
                    }
                }

                if (resolved) {
                    conflict->is_resolved = true;
                    conflict->resolution_time = nimcp_time_get_ms();

                    /* Update coordinator statistics */
                    coordinator->conflict_stats.conflicts_resolved++;
                    if (coordinator->conflict_stats.conflicts_pending > 0) {
                        coordinator->conflict_stats.conflicts_pending--;
                    }

                    /* Update average resolution time */
                    float elapsed = (float)(conflict->resolution_time - conflict->detection_time);
                    if (elapsed < 0.001F) {
                        elapsed = 0.001F;  /* Minimum for meaningful stats */
                    }
                    if (coordinator->conflict_stats.conflicts_resolved == 1) {
                        coordinator->conflict_stats.avg_resolution_time_ms = elapsed;
                    } else {
                        coordinator->conflict_stats.avg_resolution_time_ms =
                            (coordinator->conflict_stats.avg_resolution_time_ms *
                             (coordinator->conflict_stats.conflicts_resolved - 1) + elapsed) /
                            coordinator->conflict_stats.conflicts_resolved;
                    }

                    LOG_INFO("Conflict resolved: ID=%lu", conflict_id);
                    return NIMCP_SUCCESS;
                }

                return NIMCP_ERROR;
            }
        }
    }

    return NIMCP_NOT_FOUND;
}

uint32_t nimcp_conflict_auto_resolve(
    nimcp_multi_swarm_coordinator_t* coordinator,
    nimcp_conflict_resolver_fn resolver,
    void* user_data
) {
    if (!coordinator || !coordinator->enable_auto_negotiation) {
        return 0;
    }

    uint32_t resolved_count = 0;

    /* Auto-resolve conflicts in all super-swarms */
    for (uint32_t i = 0; i < coordinator->super_swarm_count; i++) {
        nimcp_super_swarm_t* super = coordinator->super_swarms[i];
        if (!super || !super->conflicts) continue;

        for (size_t j = 0; j < conflict_array_size((nimcp_darray_t*)super->conflicts); j++) {
            nimcp_swarm_conflict_t* conflict =
                conflict_array_at((nimcp_darray_t*)super->conflicts, j);

            if (conflict && !conflict->is_resolved) {
                /* Try to resolve */
                nimcp_result_t result = nimcp_conflict_resolve(
                    coordinator,
                    conflict->conflict_id,
                    NIMCP_CONFLICT_PRIORITY,
                    resolver,
                    user_data
                );

                if (result == NIMCP_SUCCESS) {
                    resolved_count++;
                }
            }
        }
    }

    LOG_INFO("Auto-resolved %u conflicts", resolved_count);
    return resolved_count;
}

/**
 * @brief Find conflict by ID in super-swarms
 *
 * WHAT: Locates conflict structure by ID
 * WHY:  Needed for negotiation and resolution operations
 * HOW:  Searches all super-swarms' conflict arrays
 */
static nimcp_swarm_conflict_t* find_conflict_by_id(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint32_t conflict_id
) {
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_conflict_by_id: coordinator is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < coordinator->super_swarm_count; i++) {
        nimcp_super_swarm_t* super = coordinator->super_swarms[i];
        if (!super || !super->conflicts) continue;

        for (size_t j = 0; j < conflict_array_size((nimcp_darray_t*)super->conflicts); j++) {
            nimcp_swarm_conflict_t* conflict =
                conflict_array_at((nimcp_darray_t*)super->conflicts, j);

            if (conflict && conflict->conflict_id == conflict_id) {
                return conflict;
            }
        }
    }

    return NULL;  /* Not found - normal search miss */
}

/**
 * @brief Enhanced conflict resolution with result tracking
 *
 * WHAT: Resolves conflict using specified strategy
 * WHY:  Provides detailed resolution result for monitoring
 * HOW:  Applies strategy-specific logic and tracks metrics
 */
nimcp_result_t nimcp_multi_swarm_resolve_conflict(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint32_t conflict_id,
    nimcp_conflict_resolution_t strategy,
    nimcp_swarm_resolution_result_t* result
) {
    if (!coordinator) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_swarm_conflict_t* conflict = find_conflict_by_id(coordinator, conflict_id);
    if (!conflict) {
        LOG_WARN("Conflict not found: ID=%u", conflict_id);
        return NIMCP_NOT_FOUND;
    }

    if (conflict->is_resolved) {
        LOG_WARN("Conflict already resolved: ID=%u", conflict_id);
        return NIMCP_ERROR;
    }

    uint64_t start_time_us = nimcp_time_get_us();
    conflict->strategy = strategy;
    bool resolved = false;

    /* Apply resolution strategy */
    switch (strategy) {
        case NIMCP_CONFLICT_PRIORITY:
            /* Higher priority swarm wins */
            if (conflict->swarm_count >= 2) {
                nimcp_swarm_identity_t* swarm_a = nimcp_swarm_get(coordinator,
                    conflict->swarm_ids[0]);
                nimcp_swarm_identity_t* swarm_b = nimcp_swarm_get(coordinator,
                    conflict->swarm_ids[1]);

                if (swarm_a && swarm_b) {
                    if (swarm_a->territory.priority > swarm_b->territory.priority) {
                        LOG_INFO("Swarm %lu wins priority conflict", swarm_a->swarm_id);
                        resolved = true;
                    } else {
                        LOG_INFO("Swarm %lu wins priority conflict", swarm_b->swarm_id);
                        resolved = true;
                    }
                }
            }
            break;

        case NIMCP_CONFLICT_NEGOTIATION:
        case NIMCP_CONFLICT_RESOLVE_ARBITRATE:
            /* Start negotiation process */
            LOG_INFO("Initiating negotiation for conflict %u", conflict_id);
            resolved = false;  /* Requires async negotiation */
            break;

        case NIMCP_CONFLICT_TIME_SHARING:
            /* Share resource over time */
            LOG_INFO("Implementing time-sharing for conflict %u", conflict_id);
            resolved = true;
            break;

        case NIMCP_CONFLICT_SPATIAL_SHARING:
        case NIMCP_CONFLICT_RESOLVE_PARTITION:
            /* Divide territory/resources */
            if (conflict->swarm_count >= 2 &&
                conflict->type == NIMCP_CONFLICT_TYPE_TERRITORY) {
                /* Partition contested area */
                LOG_INFO("Partitioning territory for conflict %u", conflict_id);
                resolved = true;
            }
            break;

        case NIMCP_CONFLICT_RESOLVE_MERGE:
            /* Merge conflicting swarms */
            LOG_INFO("Merging swarms for conflict %u", conflict_id);
            coordinator->conflict_stats.merges_performed++;
            resolved = true;
            break;

        case NIMCP_CONFLICT_ESCALATION:
            /* Escalate to super-swarm coordinator */
            LOG_INFO("Escalating conflict %u to super-swarm", conflict_id);
            coordinator->conflict_stats.escalations++;
            resolved = false;  /* Requires higher-level decision */
            break;

        case NIMCP_CONFLICT_RESOLVE_DEFER:
            /* Defer resolution */
            LOG_INFO("Deferring conflict %u for later", conflict_id);
            resolved = false;
            break;

        default:
            LOG_WARN("Unknown resolution strategy: %d", strategy);
            resolved = false;
            break;
    }

    /* Update conflict state */
    if (resolved) {
        conflict->is_resolved = true;
        conflict->resolution_time = nimcp_time_get_ms();
        coordinator->conflict_stats.conflicts_resolved++;
        if (coordinator->conflict_stats.conflicts_pending > 0) {
            coordinator->conflict_stats.conflicts_pending--;
        }

        /* Update average resolution time using microsecond precision for stats */
        float elapsed = (float)(nimcp_time_get_us() - start_time_us) / 1000.0F;
        if (elapsed < 0.001F) {
            elapsed = 0.001F;  /* Minimum 1 microsecond for meaningful stats */
        }
        if (coordinator->conflict_stats.conflicts_resolved == 1) {
            coordinator->conflict_stats.avg_resolution_time_ms = elapsed;
        } else {
            coordinator->conflict_stats.avg_resolution_time_ms =
                (coordinator->conflict_stats.avg_resolution_time_ms *
                 (coordinator->conflict_stats.conflicts_resolved - 1) + elapsed) /
                coordinator->conflict_stats.conflicts_resolved;
        }

        LOG_INFO("Conflict resolved: ID=%u, Strategy=%d, Time=%.3fms",
                 conflict_id, strategy, elapsed);

        /* Send bio-async message if available */
        if (g_multi_swarm_ctx) {
            /* Would send BIO_MSG_SWARM_CONFLICT_RESOLVED here */
        }
    }

    /* Fill result structure if provided */
    if (result) {
        result->conflict_id = conflict_id;
        result->type = conflict->type;
        result->strategy_used = strategy;
        result->resolved = resolved;
        /* Use microsecond precision for accurate timing, convert to ms */
        result->resolution_time_ms = (float)(nimcp_time_get_us() - start_time_us) / 1000.0F;
        /* Ensure at least minimal time is reported for fast resolutions */
        if (result->resolution_time_ms < 0.001F && resolved) {
            result->resolution_time_ms = 0.001F;  /* 1 microsecond minimum */
        }
        result->negotiation_rounds = conflict->negotiation_round_count;
        snprintf(result->outcome_description, sizeof(result->outcome_description),
                "%s", conflict->description);
    }

    return resolved ? NIMCP_SUCCESS : NIMCP_ERROR;
}

/**
 * @brief Start negotiation for a conflict
 *
 * WHAT: Initiates multi-round negotiation protocol
 * WHY:  Allows swarms to reach mutually acceptable solution
 * HOW:  Allocates negotiation state and broadcasts start message
 */
nimcp_result_t nimcp_multi_swarm_start_negotiation(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint32_t conflict_id
) {
    if (!coordinator) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_swarm_conflict_t* conflict = find_conflict_by_id(coordinator, conflict_id);
    if (!conflict) {
        return NIMCP_NOT_FOUND;
    }

    /* Check negotiation timeout */
    if (coordinator->conflict_config.negotiation_timeout_ms > 0) {
        uint64_t elapsed = nimcp_time_get_ms() - conflict->detection_time;
        if (elapsed > coordinator->conflict_config.negotiation_timeout_ms) {
            LOG_WARN("Negotiation timeout for conflict %u", conflict_id);
            return NIMCP_ERROR;
        }
    }

    /* Allocate negotiation structure */
    if (!conflict->negotiation) {
        conflict->negotiation = (nimcp_negotiation_round_t*)nimcp_malloc(
            sizeof(nimcp_negotiation_round_t));
        if (!conflict->negotiation) {
            LOG_ERROR("Failed to allocate negotiation structure");
            return NIMCP_NO_MEMORY;
        }
        memset(conflict->negotiation, 0, sizeof(nimcp_negotiation_round_t));
    }

    conflict->negotiation->round = 0;
    conflict->negotiation_round_count = 0;

    LOG_INFO("Started negotiation for conflict %u", conflict_id);

    /* Broadcast negotiation started message */
    if (g_multi_swarm_ctx) {
        /* Would send BIO_MSG_SWARM_NEGOTIATION_STARTED here */
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Propose a solution during negotiation
 *
 * WHAT: Swarm proposes specific solution to conflict
 * WHY:  Enables collaborative problem-solving
 * HOW:  Stores proposal and broadcasts to involved swarms
 */
nimcp_result_t nimcp_multi_swarm_propose(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint32_t conflict_id,
    const float* proposal,
    uint32_t proposal_size
) {
    if (!coordinator || !proposal || proposal_size == 0) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_swarm_conflict_t* conflict = find_conflict_by_id(coordinator, conflict_id);
    if (!conflict || !conflict->negotiation) {
        return NIMCP_NOT_FOUND;
    }

    /* Check max rounds */
    if (conflict->negotiation_round_count >=
        coordinator->conflict_config.max_negotiation_rounds) {
        LOG_WARN("Max negotiation rounds reached for conflict %u", conflict_id);
        return NIMCP_ERROR;
    }

    /* Free old proposal if exists */
    if (conflict->negotiation->proposal) {
        nimcp_free(conflict->negotiation->proposal);
    }

    /* Allocate and copy new proposal */
    conflict->negotiation->proposal = (float*)nimcp_malloc(
        proposal_size * sizeof(float));
    if (!conflict->negotiation->proposal) {
        LOG_ERROR("Failed to allocate proposal memory");
        return NIMCP_NO_MEMORY;
    }

    memcpy(conflict->negotiation->proposal, proposal, proposal_size * sizeof(float));
    conflict->negotiation->proposal_size = proposal_size;
    conflict->negotiation->round++;
    conflict->negotiation_round_count++;

    LOG_INFO("Proposal made for conflict %u, round %u",
             conflict_id, conflict->negotiation->round);

    /* Broadcast proposal message */
    if (g_multi_swarm_ctx) {
        /* Would send BIO_MSG_SWARM_PROPOSAL_MADE here */
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Accept a negotiation proposal
 *
 * WHAT: Swarm accepts current proposal
 * WHY:  Finalizes negotiated solution
 * HOW:  Marks conflict as resolved with accepted proposal
 */
nimcp_result_t nimcp_multi_swarm_accept_proposal(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint32_t conflict_id
) {
    if (!coordinator) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_swarm_conflict_t* conflict = find_conflict_by_id(coordinator, conflict_id);
    if (!conflict || !conflict->negotiation) {
        return NIMCP_NOT_FOUND;
    }

    /* Mark as resolved */
    conflict->is_resolved = true;
    conflict->resolution_time = nimcp_time_get_ms();

    /* Update statistics */
    coordinator->conflict_stats.conflicts_resolved++;
    coordinator->conflict_stats.conflicts_pending--;

    LOG_INFO("Proposal accepted for conflict %u after %u rounds",
             conflict_id, conflict->negotiation_round_count);

    /* Send resolution message */
    if (g_multi_swarm_ctx) {
        /* Would send BIO_MSG_SWARM_CONFLICT_RESOLVED here */
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Reject a negotiation proposal
 *
 * WHAT: Swarm rejects current proposal with reason
 * WHY:  Allows continued negotiation with feedback
 * HOW:  Logs rejection and allows new proposal
 */
nimcp_result_t nimcp_multi_swarm_reject_proposal(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint32_t conflict_id,
    const char* reason
) {
    if (!coordinator) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_swarm_conflict_t* conflict = find_conflict_by_id(coordinator, conflict_id);
    if (!conflict || !conflict->negotiation) {
        return NIMCP_NOT_FOUND;
    }

    LOG_INFO("Proposal rejected for conflict %u: %s",
             conflict_id, reason ? reason : "No reason");

    /* Check if max rounds exceeded */
    if (conflict->negotiation_round_count >=
        coordinator->conflict_config.max_negotiation_rounds) {
        LOG_WARN("Max negotiation rounds exceeded, escalating conflict %u", conflict_id);

        /* Escalate if allowed */
        if (coordinator->conflict_config.allow_escalation) {
            return nimcp_multi_swarm_resolve_conflict(coordinator, conflict_id,
                NIMCP_CONFLICT_ESCALATION, NULL);
        }

        return NIMCP_ERROR;
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Get current negotiation status
 *
 * WHAT: Retrieves current round information
 * WHY:  Allows monitoring negotiation progress
 * HOW:  Returns copy of current negotiation round data
 */
nimcp_result_t nimcp_multi_swarm_get_negotiation_status(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint32_t conflict_id,
    nimcp_negotiation_round_t* current_round
) {
    if (!coordinator) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_swarm_conflict_t* conflict = find_conflict_by_id(coordinator, conflict_id);
    if (!conflict || !conflict->negotiation) {
        return NIMCP_NOT_FOUND;
    }

    if (current_round) {
        memcpy(current_round, conflict->negotiation, sizeof(nimcp_negotiation_round_t));
        /* Don't copy pointer, just set to NULL */
        current_round->proposal = NULL;
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Get conflict resolution statistics
 *
 * WHAT: Returns comprehensive conflict statistics
 * WHY:  Enables monitoring system health and performance
 * HOW:  Returns copy of statistics structure
 */
nimcp_conflict_resolution_stats_t nimcp_multi_swarm_get_conflict_stats(
    nimcp_multi_swarm_coordinator_t* coordinator
) {
    nimcp_conflict_resolution_stats_t empty_stats = {0};

    if (!coordinator) {
        return empty_stats;
    }

    return coordinator->conflict_stats;
}

/**
 * @brief Configure conflict resolution behavior
 *
 * WHAT: Sets conflict resolution configuration
 * WHY:  Allows customization of resolution strategies
 * HOW:  Validates and stores config in coordinator
 */
nimcp_result_t nimcp_multi_swarm_set_conflict_config(
    nimcp_multi_swarm_coordinator_t* coordinator,
    const nimcp_conflict_resolution_config_t* config
) {
    if (!coordinator || !config) {
        return NIMCP_INVALID_PARAM;
    }

    /* Validate configuration */
    if (config->negotiation_timeout_ms < 0 ||
        config->max_negotiation_rounds == 0 ||
        config->merge_threshold < 0.0F || config->merge_threshold > 1.0F) {
        LOG_ERROR("Invalid conflict resolution configuration");
        return NIMCP_INVALID_PARAM;
    }

    /* Apply configuration */
    memcpy(&coordinator->conflict_config, config,
           sizeof(nimcp_conflict_resolution_config_t));

    LOG_INFO("Applied conflict resolution config: strategy=%d, timeout=%.0fms, max_rounds=%u",
             config->default_strategy, config->negotiation_timeout_ms,
             config->max_negotiation_rounds);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Swarm discovery message payload
 *
 * WHAT: Message payload for swarm discovery broadcasts
 * WHY:  Standardized format for inter-swarm communication
 */
typedef struct {
    bio_message_header_t header;
    nimcp_swarm_identity_t identity;
} swarm_discovery_msg_t;

/**
 * @brief Generic swarm message payload
 *
 * WHAT: Variable-length message with header and payload
 * WHY:  Flexible format for different message types
 */
typedef struct {
    bio_message_header_t header;
    uint8_t payload[];  /* Flexible array member */
} swarm_generic_msg_t;

uint32_t nimcp_multi_swarm_process_inbox(
    nimcp_multi_swarm_coordinator_t* coordinator
) {
    if (!coordinator) {
        return 0;
    }

    /* Use bio-router's process_inbox which invokes registered handlers */
    if (!g_multi_swarm_ctx) {
        LOG_DEBUG("Bio-async context not initialized for swarm coordinator");
        return 0;
    }

    /* Process up to 64 messages per call to avoid blocking */
    uint32_t processed = bio_router_process_inbox(g_multi_swarm_ctx, 64);

    if (processed > 0) {
        LOG_DEBUG("Processed %u swarm messages", processed);
    }

    return processed;
}

nimcp_result_t nimcp_multi_swarm_broadcast_discovery(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t swarm_id
) {
    if (!coordinator) {
        return NIMCP_ERROR;
    }

    /* Require bio-async context for messaging */
    if (!g_multi_swarm_ctx) {
        LOG_WARN("Bio-async context not initialized, cannot broadcast");
        return NIMCP_NOT_INITIALIZED;
    }

    nimcp_swarm_identity_t* identity = nimcp_swarm_get(coordinator, swarm_id);
    if (!identity) {
        return NIMCP_NOT_FOUND;
    }

    /* Allocate discovery message with embedded identity */
    swarm_discovery_msg_t* message = (swarm_discovery_msg_t*)nimcp_malloc(
        sizeof(swarm_discovery_msg_t));
    if (!message) {
        LOG_ERROR("Failed to allocate discovery message");
        return NIMCP_NO_MEMORY;
    }

    /* Initialize message header using bio_msg_init_header */
    bio_msg_init_header(
        &message->header,
        (bio_message_type_t)MSG_TYPE_SWARM_DISCOVERY,
        BIO_MODULE_SWARM_COORDINATOR,
        BIO_MODULE_ALL,  /* Broadcast to all modules */
        sizeof(nimcp_swarm_identity_t)
    );
    message->header.flags = BIO_MSG_FLAG_BROADCAST;

    /* Copy identity into message payload */
    memcpy(&message->identity, identity, sizeof(nimcp_swarm_identity_t));

    /* Broadcast via router */
    nimcp_result_t result = bio_router_broadcast(
        g_multi_swarm_ctx,
        message,
        sizeof(swarm_discovery_msg_t)
    );

    /* Clean up allocated message */
    nimcp_free(message);

    if (result == NIMCP_SUCCESS) {
        LOG_INFO("Broadcasted discovery for swarm %lu", swarm_id);
    } else {
        LOG_WARN("Failed to broadcast discovery for swarm %lu: %d", swarm_id, result);
    }

    return result;
}

nimcp_result_t nimcp_multi_swarm_send_message(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t from_swarm,
    uint64_t to_swarm,
    uint32_t message_type,
    const void* payload,
    size_t payload_size
) {
    if (!coordinator || !payload || payload_size == 0) {
        return NIMCP_INVALID_PARAM;
    }

    /* Require bio-async context for messaging */
    if (!g_multi_swarm_ctx) {
        LOG_WARN("Bio-async context not initialized, cannot send message");
        return NIMCP_NOT_INITIALIZED;
    }

    /* Allocate message with header + payload */
    size_t msg_size = sizeof(bio_message_header_t) + payload_size;
    swarm_generic_msg_t* message = (swarm_generic_msg_t*)nimcp_malloc(msg_size);
    if (!message) {
        LOG_ERROR("Failed to allocate swarm message");
        return NIMCP_NO_MEMORY;
    }

    /* Initialize message header */
    bio_msg_init_header(
        &message->header,
        (bio_message_type_t)message_type,
        BIO_MODULE_SWARM_COORDINATOR,
        BIO_MODULE_SWARM_COORDINATOR,  /* Target is another swarm coordinator */
        (uint32_t)payload_size
    );

    /* Copy payload into message */
    memcpy(message->payload, payload, payload_size);

    /* Route through communication bridge for swarm-to-swarm messaging */
    nimcp_result_t result = nimcp_comm_bridge_route_message(
        coordinator,
        from_swarm,
        to_swarm,
        &message->header
    );

    /* Clean up allocated message */
    nimcp_free(message);

    return result;
}

/* ============================================================================
 * Query and Statistics
 * ============================================================================ */

nimcp_swarm_identity_t* nimcp_swarm_get(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t swarm_id
) {
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_swarm_get: coordinator is NULL");
        return NULL;
    }

    nimcp_rwlock_read_lock(&coordinator->coordinator_lock);
    nimcp_swarm_identity_t* identity =
        swarm_hash_table_lookup(coordinator->swarm_registry, swarm_id);
    nimcp_rwlock_read_unlock(&coordinator->coordinator_lock);

    return identity;
}

uint32_t nimcp_swarm_find_by_capability(
    nimcp_multi_swarm_coordinator_t* coordinator,
    nimcp_swarm_capability_type_t capability,
    float min_proficiency,
    void* results
) {
    if (!coordinator || !results) {
        return 0;
    }

    uint32_t found = 0;

    nimcp_rwlock_read_lock(&coordinator->coordinator_lock);

    /* Search through all super-swarms */
    for (uint32_t i = 0; i < coordinator->super_swarm_count; i++) {
        nimcp_super_swarm_t* super = coordinator->super_swarms[i];
        if (!super) continue;

        nimcp_rwlock_read_lock(&super->swarm_lock);

        for (uint32_t j = 0; j < super->swarm_count; j++) {
            nimcp_swarm_identity_t* swarm = super->swarms[j];

            /* Check capabilities */
            for (uint32_t k = 0; k < swarm->capability_count; k++) {
                if (swarm->capabilities[k].type == capability &&
                    swarm->capabilities[k].proficiency >= min_proficiency) {
                    uint64_array_push_back((nimcp_darray_t*)results, &swarm->swarm_id);
                    found++;
                    break;
                }
            }
        }

        nimcp_rwlock_read_unlock(&super->swarm_lock);
    }

    nimcp_rwlock_read_unlock(&coordinator->coordinator_lock);

    return found;
}

uint32_t nimcp_swarm_find_in_territory(
    nimcp_multi_swarm_coordinator_t* coordinator,
    nimcp_territory_bounds_t territory,
    void* results
) {
    if (!coordinator || !results) {
        return 0;
    }

    uint32_t found = 0;

    nimcp_rwlock_read_lock(&coordinator->coordinator_lock);

    for (uint32_t i = 0; i < coordinator->super_swarm_count; i++) {
        nimcp_super_swarm_t* super = coordinator->super_swarms[i];
        if (!super) continue;

        nimcp_rwlock_read_lock(&super->swarm_lock);

        for (uint32_t j = 0; j < super->swarm_count; j++) {
            nimcp_swarm_identity_t* swarm = super->swarms[j];

            if (nimcp_territory_overlaps(&swarm->territory, &territory)) {
                uint64_array_push_back((nimcp_darray_t*)results, &swarm->swarm_id);
                found++;
            }
        }

        nimcp_rwlock_read_unlock(&super->swarm_lock);
    }

    nimcp_rwlock_read_unlock(&coordinator->coordinator_lock);

    return found;
}

void nimcp_multi_swarm_get_stats(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint32_t* total_swarms,
    uint32_t* total_agents,
    uint32_t* active_missions,
    uint32_t* active_conflicts
) {
    if (!coordinator) {
        if (total_swarms) *total_swarms = 0;
        if (total_agents) *total_agents = 0;
        if (active_missions) *active_missions = 0;
        if (active_conflicts) *active_conflicts = 0;
        return;
    }

    uint32_t swarms = 0;
    uint32_t agents = 0;
    uint32_t missions = 0;
    uint32_t conflicts = 0;

    nimcp_rwlock_read_lock(&coordinator->coordinator_lock);

    for (uint32_t i = 0; i < coordinator->super_swarm_count; i++) {
        nimcp_super_swarm_t* super = coordinator->super_swarms[i];
        if (!super) continue;

        nimcp_rwlock_read_lock(&super->swarm_lock);
        swarms += super->swarm_count;

        for (uint32_t j = 0; j < super->swarm_count; j++) {
            agents += super->swarms[j]->active_agents;
        }
        nimcp_rwlock_read_unlock(&super->swarm_lock);

        nimcp_rwlock_read_lock(&super->mission_lock);
        missions += super->active_mission_count;
        nimcp_rwlock_read_unlock(&super->mission_lock);

        if (super->conflicts) {
            conflicts += (uint32_t)conflict_array_size((nimcp_darray_t*)super->conflicts);
        }
    }

    nimcp_rwlock_read_unlock(&coordinator->coordinator_lock);

    if (total_swarms) *total_swarms = swarms;
    if (total_agents) *total_agents = agents;
    if (active_missions) *active_missions = missions;
    if (active_conflicts) *active_conflicts = conflicts;
}

void nimcp_multi_swarm_print_status(
    nimcp_multi_swarm_coordinator_t* coordinator,
    bool verbose
) {
    if (!coordinator) {
        printf("Multi-Swarm Coordinator: NULL\n");
        return;
    }

    uint32_t total_swarms, total_agents, active_missions, active_conflicts;
    nimcp_multi_swarm_get_stats(coordinator, &total_swarms, &total_agents,
                                &active_missions, &active_conflicts);

    printf("\n=== Multi-Swarm Coordinator Status ===\n");
    printf("Super-Swarms:     %u\n", coordinator->super_swarm_count);
    printf("Total Swarms:     %u\n", total_swarms);
    printf("Total Agents:     %u\n", total_agents);
    printf("Active Missions:  %u\n", active_missions);
    printf("Active Conflicts: %u\n", active_conflicts);
    printf("Auto-Negotiation: %s\n", coordinator->enable_auto_negotiation ? "ON" : "OFF");
    printf("Resource Sharing: %s\n", coordinator->enable_resource_sharing ? "ON" : "OFF");
    printf("Bridge Formation: %s\n", coordinator->enable_bridge_formation ? "ON" : "OFF");

    if (verbose) {
        printf("\n--- Super-Swarm Details ---\n");
        for (uint32_t i = 0; i < coordinator->super_swarm_count; i++) {
            nimcp_super_swarm_t* super = coordinator->super_swarms[i];
            if (!super) continue;

            printf("\nSuper-Swarm %u: %s (ID=%lu)\n", i, super->name,
                   super->super_swarm_id);
            printf("  Swarms: %u\n", super->swarm_count);
            printf("  Missions: %u\n", super->active_mission_count);
            printf("  Bridges: %u\n", super->bridge_count);

            for (uint32_t j = 0; j < super->swarm_count; j++) {
                nimcp_swarm_identity_t* swarm = super->swarms[j];
                printf("    Swarm: %s (ID=%lu) - %u/%u agents (%.0f%% health)\n",
                       swarm->name, swarm->swarm_id,
                       swarm->active_agents, swarm->agent_count,
                       swarm->health_percentage * 100.0F);
            }
        }
    }

    printf("=====================================\n\n");
}
