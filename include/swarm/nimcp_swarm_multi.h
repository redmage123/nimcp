/**
 * @file nimcp_swarm_multi.h
 * @brief Multi-Swarm Coordination System for NIMCP
 *
 * Biological Inspiration: Inter-colony cooperation in social insects
 *
 * Implements hierarchical swarm coordination, territory negotiation,
 * resource sharing, and joint mission execution across multiple swarms.
 *
 * Features:
 * - Swarm identity and capability profiles
 * - Swarm-of-swarms hierarchical coordination
 * - Dynamic territory negotiation and conflict resolution
 * - Cross-swarm resource sharing and lending
 * - Joint multi-swarm mission coordination
 * - Communication bridges and message routing
 * - Bio-async integration for inter-swarm messaging
 *
 * @author NIMCP Development Team
 * @version 1.0
 * @date 2025
 */

#ifndef NIMCP_SWARM_MULTI_H
#define NIMCP_SWARM_MULTI_H

#include "utils/validation/nimcp_common.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/nimcp_brain.h"
#include "async/nimcp_bio_messages.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/containers/nimcp_vector.h"
#include "utils/containers/nimcp_hash_table.h"
#include <stdint.h>
#include <stdbool.h>

#include "async/nimcp_bio_router.h"
#include "utils/thread/nimcp_thread.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum number of swarms in a super-swarm
 */
#define NIMCP_MAX_SWARMS_PER_SUPER 64

/**
 * @brief Maximum number of capabilities per swarm
 */
#define NIMCP_MAX_SWARM_CAPABILITIES 32

/**
 * @brief Maximum number of active missions per swarm
 */
#define NIMCP_MAX_SWARM_MISSIONS 16

/**
 * @brief Maximum number of communication bridges
 */
#define NIMCP_MAX_COMM_BRIDGES 8

/**
 * @brief Maximum length of swarm name
 */
#define NIMCP_SWARM_NAME_MAX 64

/**
 * @brief Territory boundary precision (meters)
 */
#define NIMCP_TERRITORY_PRECISION 0.1

/**
 * @brief Swarm capability types
 */
typedef enum {
    NIMCP_SWARM_CAP_SURVEILLANCE,      /**< Surveillance and monitoring */
    NIMCP_SWARM_CAP_TRANSPORT,         /**< Cargo transport */
    NIMCP_SWARM_CAP_COMBAT,            /**< Combat operations */
    NIMCP_SWARM_CAP_RESCUE,            /**< Search and rescue */
    NIMCP_SWARM_CAP_CONSTRUCTION,      /**< Construction tasks */
    NIMCP_SWARM_CAP_MEDICAL,           /**< Medical support */
    NIMCP_SWARM_CAP_COMMUNICATION,     /**< Communication relay */
    NIMCP_SWARM_CAP_RECONNAISSANCE,    /**< Advanced reconnaissance */
    NIMCP_SWARM_CAP_DEFENSE,           /**< Defensive operations */
    NIMCP_SWARM_CAP_LOGISTICS,         /**< Logistics support */
    NIMCP_SWARM_CAP_COUNT
} nimcp_swarm_capability_type_t;

/**
 * @brief Swarm health status
 */
typedef enum {
    NIMCP_SWARM_HEALTH_EXCELLENT,      /**< > 90% operational */
    NIMCP_SWARM_HEALTH_GOOD,           /**< 70-90% operational */
    NIMCP_SWARM_HEALTH_FAIR,           /**< 50-70% operational */
    NIMCP_SWARM_HEALTH_POOR,           /**< 30-50% operational */
    NIMCP_SWARM_HEALTH_CRITICAL        /**< < 30% operational */
} nimcp_swarm_health_t;

/**
 * @brief Mission priority levels
 */
typedef enum {
    NIMCP_MISSION_PRIORITY_CRITICAL,   /**< Life-threatening emergency */
    NIMCP_MISSION_PRIORITY_HIGH,       /**< Urgent mission */
    NIMCP_MISSION_PRIORITY_MEDIUM,     /**< Normal mission */
    NIMCP_MISSION_PRIORITY_LOW,        /**< Background task */
    NIMCP_MISSION_PRIORITY_IDLE        /**< Maintenance activity */
} nimcp_mission_priority_t;

/**
 * @brief Mission status
 */
typedef enum {
    NIMCP_MISSION_STATUS_PENDING,      /**< Awaiting assignment */
    NIMCP_MISSION_STATUS_ASSIGNED,     /**< Assigned to swarm(s) */
    NIMCP_MISSION_STATUS_ACTIVE,       /**< Currently executing */
    NIMCP_MISSION_STATUS_PAUSED,       /**< Temporarily paused */
    NIMCP_MISSION_STATUS_COMPLETED,    /**< Successfully completed */
    NIMCP_MISSION_STATUS_FAILED,       /**< Mission failed */
    NIMCP_MISSION_STATUS_ABORTED       /**< Mission aborted */
} nimcp_mission_status_t;

/**
 * @brief Conflict types
 */
typedef enum {
    NIMCP_CONFLICT_TYPE_NONE = 0,
    NIMCP_CONFLICT_TYPE_RESOURCE,          /**< Multiple swarms want same resource */
    NIMCP_CONFLICT_TYPE_TERRITORY,         /**< Overlapping territories */
    NIMCP_CONFLICT_TYPE_GOAL,              /**< Incompatible goals */
    NIMCP_CONFLICT_TYPE_PRIORITY,          /**< Priority ordering disputes */
    NIMCP_CONFLICT_TYPE_COMMUNICATION      /**< Message routing conflicts */
} nimcp_swarm_conflict_type_t;

/**
 * @brief Conflict resolution strategy
 */
typedef enum {
    NIMCP_CONFLICT_PRIORITY,           /**< Higher priority wins */
    NIMCP_CONFLICT_NEGOTIATION,        /**< Negotiate solution */
    NIMCP_CONFLICT_TIME_SHARING,       /**< Share resource over time */
    NIMCP_CONFLICT_SPATIAL_SHARING,    /**< Divide spatial region */
    NIMCP_CONFLICT_COOPERATION,        /**< Cooperate on objective */
    NIMCP_CONFLICT_ESCALATION,         /**< Escalate to super-swarm */
    NIMCP_CONFLICT_RESOLVE_ARBITRATE,  /**< Central arbitrator decides */
    NIMCP_CONFLICT_RESOLVE_MERGE,      /**< Merge conflicting swarms */
    NIMCP_CONFLICT_RESOLVE_PARTITION,  /**< Partition resources/territory */
    NIMCP_CONFLICT_RESOLVE_DEFER       /**< Defer to later resolution */
} nimcp_conflict_resolution_t;

/**
 * @brief Resource request type
 */
typedef enum {
    NIMCP_RESOURCE_REQ_DRONES,         /**< Request drones */
    NIMCP_RESOURCE_REQ_ENERGY,         /**< Request energy/charging */
    NIMCP_RESOURCE_REQ_INFORMATION,    /**< Request information */
    NIMCP_RESOURCE_REQ_CAPABILITY,     /**< Request capability access */
    NIMCP_RESOURCE_REQ_COORDINATION,   /**< Request coordination support */
    NIMCP_RESOURCE_REQ_TERRITORY       /**< Request territory access */
} nimcp_resource_request_type_t;

/**
 * @brief 3D coordinate for territory boundaries
 */
typedef struct {
    double x;                          /**< X coordinate */
    double y;                          /**< Y coordinate */
    double z;                          /**< Z coordinate */
} nimcp_coord3d_t;

/**
 * @brief Territory boundary definition
 */
typedef struct {
    nimcp_coord3d_t min;               /**< Minimum boundary */
    nimcp_coord3d_t max;               /**< Maximum boundary */
    uint64_t timestamp;                /**< Last update time */
    bool is_dynamic;                   /**< Can boundary adjust? */
    float priority;                    /**< Territory priority */
} nimcp_territory_bounds_t;

/**
 * @brief Swarm capability profile entry
 */
typedef struct {
    nimcp_swarm_capability_type_t type; /**< Capability type */
    float proficiency;                  /**< Proficiency level (0-1) */
    uint32_t capacity;                  /**< Resource capacity */
    uint32_t available;                 /**< Currently available */
    bool is_lendable;                   /**< Can be shared? */
} nimcp_swarm_capability_t;

/**
 * @brief Swarm identity and profile
 */
typedef struct {
    uint64_t swarm_id;                 /**< Unique swarm identifier */
    char name[NIMCP_SWARM_NAME_MAX];   /**< Human-readable name */
    uint32_t agent_count;              /**< Number of agents */
    uint32_t active_agents;            /**< Currently active agents */
    nimcp_swarm_health_t health;       /**< Overall health status */
    float health_percentage;           /**< Numerical health (0-1) */

    nimcp_swarm_capability_t capabilities[NIMCP_MAX_SWARM_CAPABILITIES];
    uint32_t capability_count;         /**< Number of capabilities */

    nimcp_territory_bounds_t territory; /**< Operational territory */
    uint64_t formation_time;           /**< When swarm was formed */
    uint64_t last_contact;             /**< Last communication time */

    void* user_data;                   /**< User-defined data */
} nimcp_swarm_identity_t;

/**
 * @brief Mission assignment
 */
typedef struct {
    uint64_t mission_id;               /**< Unique mission ID */
    char description[256];             /**< Mission description */
    nimcp_mission_priority_t priority; /**< Mission priority */
    nimcp_mission_status_t status;     /**< Current status */

    uint64_t assigned_swarms[NIMCP_MAX_SWARMS_PER_SUPER];
    uint32_t swarm_count;              /**< Number of assigned swarms */

    nimcp_territory_bounds_t operation_area; /**< Mission area */
    uint64_t start_time;               /**< Mission start time */
    uint64_t deadline;                 /**< Mission deadline */
    float progress;                    /**< Progress percentage (0-1) */

    void* mission_data;                /**< Mission-specific data */
} nimcp_mission_assignment_t;

/**
 * @brief Communication bridge between swarms
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    uint64_t bridge_id;                /**< Bridge identifier */
    uint64_t swarm_a;                  /**< First swarm ID */
    uint64_t swarm_b;                  /**< Second swarm ID */
    uint32_t relay_agents[4];          /**< Relay agent IDs */
    uint32_t relay_count;              /**< Active relay count */
    float link_quality;                /**< Link quality (0-1) */
    uint64_t last_message_time;        /**< Last message timestamp */
    bool is_active;                    /**< Bridge active? */
} nimcp_comm_bridge_t;

/**
 * @brief Resource sharing request
 */
typedef struct {
    uint64_t request_id;               /**< Request identifier */
    uint64_t requesting_swarm;         /**< Requesting swarm ID */
    uint64_t target_swarm;             /**< Target swarm ID */
    nimcp_resource_request_type_t type; /**< Request type */
    uint32_t quantity;                 /**< Requested quantity */
    nimcp_mission_priority_t priority; /**< Request priority */
    uint64_t expiry_time;              /**< Request expiry */
    bool is_approved;                  /**< Approval status */
    float cost;                        /**< Resource cost */
} nimcp_resource_request_t;

/**
 * @brief Negotiation round information
 */
typedef struct {
    uint32_t round;                    /**< Negotiation round number */
    uint64_t proposer_swarm_id;        /**< Swarm making proposal */
    float* proposal;                   /**< Proposed division/solution */
    uint32_t proposal_size;            /**< Size of proposal array */
    float acceptance_score;            /**< How acceptable (0-1) */
} nimcp_negotiation_round_t;

/**
 * @brief Conflict between swarms (enhanced)
 */
typedef struct {
    uint64_t conflict_id;              /**< Conflict identifier */
    nimcp_swarm_conflict_type_t type;  /**< Type of conflict */
    uint64_t swarm_ids[NIMCP_MAX_SWARMS_PER_SUPER];
    uint32_t swarm_count;              /**< Involved swarms */

    nimcp_conflict_resolution_t strategy; /**< Resolution strategy */
    nimcp_territory_bounds_t contested_area; /**< Contested region */

    float severity;                    /**< Severity 0.0-1.0 */
    uint64_t detection_time;           /**< When detected */
    uint64_t resolution_time;          /**< When resolved */
    bool is_resolved;                  /**< Resolution status */

    void* conflict_context;            /**< Type-specific data */
    uint32_t context_size;             /**< Context data size */

    nimcp_negotiation_round_t* negotiation; /**< Active negotiation */
    uint32_t negotiation_round_count;  /**< Number of rounds */

    char description[256];             /**< Conflict description */
} nimcp_swarm_conflict_t;

/**
 * @brief Conflict resolution result
 */
typedef struct {
    uint32_t conflict_id;              /**< Conflict ID */
    nimcp_swarm_conflict_type_t type;  /**< Conflict type */
    nimcp_conflict_resolution_t strategy_used; /**< Strategy used */
    bool resolved;                     /**< Was it resolved? */
    float resolution_time_ms;          /**< Time to resolve */
    uint32_t negotiation_rounds;       /**< Rounds of negotiation */
    char outcome_description[256];     /**< Outcome description */
} nimcp_swarm_resolution_result_t;

/**
 * @brief Conflict resolution statistics
 */
typedef struct {
    uint32_t total_conflicts;          /**< Total conflicts detected */
    uint32_t conflicts_resolved;       /**< Successfully resolved */
    uint32_t conflicts_pending;        /**< Still pending */
    float avg_resolution_time_ms;      /**< Average resolution time */
    uint32_t escalations;              /**< Number of escalations */
    uint32_t merges_performed;         /**< Swarms merged */
} nimcp_conflict_resolution_stats_t;

/**
 * @brief Conflict resolution configuration
 */
typedef struct {
    nimcp_conflict_resolution_t default_strategy;  /**< Default strategy */
    float negotiation_timeout_ms;      /**< Negotiation timeout */
    uint32_t max_negotiation_rounds;   /**< Max negotiation rounds */
    bool allow_escalation;             /**< Allow escalation? */
    float merge_threshold;             /**< When to merge (0-1) */
} nimcp_conflict_resolution_config_t;

/**
 * @brief Super-swarm (swarm-of-swarms) coordinator
 */
typedef struct {
    uint64_t super_swarm_id;           /**< Super-swarm identifier */
    char name[NIMCP_SWARM_NAME_MAX];   /**< Super-swarm name */

    nimcp_swarm_identity_t* swarms[NIMCP_MAX_SWARMS_PER_SUPER];
    uint32_t swarm_count;              /**< Member swarm count */

    nimcp_mission_assignment_t missions[NIMCP_MAX_SWARM_MISSIONS];
    uint32_t active_mission_count;     /**< Active missions */

    nimcp_comm_bridge_t bridges[NIMCP_MAX_COMM_BRIDGES];
    uint32_t bridge_count;             /**< Active bridges */

    void* resource_requests; /**< Pending requests */
    void* conflicts;         /**< Active conflicts */

    nimcp_territory_bounds_t overall_territory; /**< Total area */

    nimcp_rwlock_t swarm_lock;         /**< Swarm list lock */
    nimcp_rwlock_t mission_lock;       /**< Mission list lock */
    nimcp_rwlock_t bridge_lock;        /**< Bridge list lock */

    void* user_data;                   /**< User-defined data */
} nimcp_super_swarm_t;

/**
 * @brief Multi-swarm coordinator
 */
typedef struct {
    nimcp_super_swarm_t* super_swarms[NIMCP_MAX_SWARMS_PER_SUPER];
    uint32_t super_swarm_count;        /**< Number of super-swarms */

    void* swarm_registry; /**< All known swarms */
    void* mission_registry; /**< All missions */

    void* brain;              /**< Associated brain (optional) */
    bio_router_t* router;        /**< Bio-async router */

    uint64_t next_swarm_id;            /**< Next swarm ID */
    uint64_t next_mission_id;          /**< Next mission ID */
    uint64_t next_conflict_id;         /**< Next conflict ID */

    nimcp_rwlock_t coordinator_lock;   /**< Global coordinator lock */

    bool enable_auto_negotiation;      /**< Auto-negotiate conflicts */
    bool enable_resource_sharing;      /**< Allow resource sharing */
    bool enable_bridge_formation;      /**< Auto-create bridges */

    nimcp_conflict_resolution_config_t conflict_config; /**< Conflict config */
    nimcp_conflict_resolution_stats_t conflict_stats;   /**< Conflict stats */

    void* user_data;                   /**< User-defined data */
} nimcp_multi_swarm_coordinator_t;

/**
 * @brief Callback for conflict resolution
 */
typedef bool (*nimcp_conflict_resolver_fn)(
    nimcp_multi_swarm_coordinator_t* coordinator,
    nimcp_swarm_conflict_t* conflict,
    void* user_data
);

/**
 * @brief Callback for resource allocation decisions
 */
typedef bool (*nimcp_resource_allocator_fn)(
    nimcp_multi_swarm_coordinator_t* coordinator,
    nimcp_resource_request_t* request,
    void* user_data
);

/**
 * @brief Callback for mission assignment decisions
 */
typedef void (*nimcp_mission_assigner_fn)(
    nimcp_multi_swarm_coordinator_t* coordinator,
    nimcp_mission_assignment_t* mission,
    void* user_data
);

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Create a new multi-swarm coordinator
 *
 * @param brain Optional associated brain (can be NULL)
 * @param router Bio-async router for messaging (can be NULL)
 * @return Newly created coordinator, or NULL on failure
 */
nimcp_multi_swarm_coordinator_t* nimcp_multi_swarm_create(
    void* brain,
    bio_router_t* router
);

/**
 * @brief Destroy multi-swarm coordinator
 *
 * @param coordinator Coordinator to destroy
 */
void nimcp_multi_swarm_destroy(nimcp_multi_swarm_coordinator_t* coordinator);

/* ============================================================================
 * Swarm Identity Management
 * ============================================================================ */

/**
 * @brief Create a new swarm identity
 *
 * @param coordinator Multi-swarm coordinator
 * @param name Human-readable swarm name
 * @param agent_count Number of agents in swarm
 * @return Newly created swarm identity, or NULL on failure
 */
nimcp_swarm_identity_t* nimcp_swarm_identity_create(
    nimcp_multi_swarm_coordinator_t* coordinator,
    const char* name,
    uint32_t agent_count
);

/**
 * @brief Register a swarm with the coordinator
 *
 * @param coordinator Multi-swarm coordinator
 * @param identity Swarm identity to register
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_register(
    nimcp_multi_swarm_coordinator_t* coordinator,
    nimcp_swarm_identity_t* identity
);

/**
 * @brief Unregister a swarm from the coordinator
 *
 * @param coordinator Multi-swarm coordinator
 * @param swarm_id Swarm ID to unregister
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_unregister(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t swarm_id
);

/**
 * @brief Add a capability to a swarm
 *
 * @param identity Swarm identity
 * @param type Capability type
 * @param proficiency Proficiency level (0-1)
 * @param capacity Total capacity
 * @param is_lendable Can be shared with other swarms?
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_add_capability(
    nimcp_swarm_identity_t* identity,
    nimcp_swarm_capability_type_t type,
    float proficiency,
    uint32_t capacity,
    bool is_lendable
);

/**
 * @brief Update swarm health metrics
 *
 * @param identity Swarm identity
 * @param active_agents Currently active agents
 */
void nimcp_swarm_update_health(
    nimcp_swarm_identity_t* identity,
    uint32_t active_agents
);

/**
 * @brief Destroy swarm identity
 *
 * @param identity Swarm identity to destroy
 */
void nimcp_swarm_identity_destroy(nimcp_swarm_identity_t* identity);

/* ============================================================================
 * Super-Swarm Management
 * ============================================================================ */

/**
 * @brief Create a super-swarm (swarm-of-swarms)
 *
 * @param coordinator Multi-swarm coordinator
 * @param name Super-swarm name
 * @return Newly created super-swarm, or NULL on failure
 */
nimcp_super_swarm_t* nimcp_super_swarm_create(
    nimcp_multi_swarm_coordinator_t* coordinator,
    const char* name
);

/**
 * @brief Add a swarm to a super-swarm
 *
 * @param super_swarm Super-swarm
 * @param identity Swarm identity to add
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_super_swarm_add_swarm(
    nimcp_super_swarm_t* super_swarm,
    nimcp_swarm_identity_t* identity
);

/**
 * @brief Remove a swarm from a super-swarm
 *
 * @param super_swarm Super-swarm
 * @param swarm_id Swarm ID to remove
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_super_swarm_remove_swarm(
    nimcp_super_swarm_t* super_swarm,
    uint64_t swarm_id
);

/**
 * @brief Destroy super-swarm
 *
 * @param super_swarm Super-swarm to destroy
 */
void nimcp_super_swarm_destroy(nimcp_super_swarm_t* super_swarm);

/* ============================================================================
 * Territory Management
 * ============================================================================ */

/**
 * @brief Set swarm territory boundaries
 *
 * @param identity Swarm identity
 * @param min Minimum boundary coordinates
 * @param max Maximum boundary coordinates
 * @param is_dynamic Can boundary adjust dynamically?
 * @param priority Territory priority
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_swarm_set_territory(
    nimcp_swarm_identity_t* identity,
    nimcp_coord3d_t min,
    nimcp_coord3d_t max,
    bool is_dynamic,
    float priority
);

/**
 * @brief Check if territories overlap
 *
 * @param bounds_a First territory
 * @param bounds_b Second territory
 * @return true if territories overlap, false otherwise
 */
bool nimcp_territory_overlaps(
    const nimcp_territory_bounds_t* bounds_a,
    const nimcp_territory_bounds_t* bounds_b
);

/**
 * @brief Negotiate territory boundaries between swarms
 *
 * @param coordinator Multi-swarm coordinator
 * @param swarm_a First swarm ID
 * @param swarm_b Second swarm ID
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_territory_negotiate(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t swarm_a,
    uint64_t swarm_b
);

/**
 * @brief Detect territory conflicts
 *
 * @param coordinator Multi-swarm coordinator
 * @param conflicts Output vector for detected conflicts
 * @return Number of conflicts detected
 */
uint32_t nimcp_territory_detect_conflicts(
    nimcp_multi_swarm_coordinator_t* coordinator,
    void* conflicts
);

/* ============================================================================
 * Resource Sharing
 * ============================================================================ */

/**
 * @brief Request resources from another swarm
 *
 * @param coordinator Multi-swarm coordinator
 * @param requesting_swarm Requesting swarm ID
 * @param target_swarm Target swarm ID
 * @param type Resource type
 * @param quantity Requested quantity
 * @param priority Request priority
 * @return Request ID on success, 0 on failure
 */
uint64_t nimcp_resource_request(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t requesting_swarm,
    uint64_t target_swarm,
    nimcp_resource_request_type_t type,
    uint32_t quantity,
    nimcp_mission_priority_t priority
);

/**
 * @brief Approve a resource request
 *
 * @param coordinator Multi-swarm coordinator
 * @param request_id Request ID to approve
 * @param cost Cost/conditions for approval
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_resource_approve(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t request_id,
    float cost
);

/**
 * @brief Deny a resource request
 *
 * @param coordinator Multi-swarm coordinator
 * @param request_id Request ID to deny
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_resource_deny(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t request_id
);

/**
 * @brief Process pending resource requests
 *
 * @param coordinator Multi-swarm coordinator
 * @param allocator Optional custom allocator callback
 * @param user_data User data for callback
 * @return Number of requests processed
 */
uint32_t nimcp_resource_process_requests(
    nimcp_multi_swarm_coordinator_t* coordinator,
    nimcp_resource_allocator_fn allocator,
    void* user_data
);

/* ============================================================================
 * Mission Management
 * ============================================================================ */

/**
 * @brief Create a new mission assignment
 *
 * @param coordinator Multi-swarm coordinator
 * @param description Mission description
 * @param priority Mission priority
 * @param operation_area Mission operational area
 * @param deadline Mission deadline (0 for no deadline)
 * @return Mission ID on success, 0 on failure
 */
uint64_t nimcp_mission_create(
    nimcp_multi_swarm_coordinator_t* coordinator,
    const char* description,
    nimcp_mission_priority_t priority,
    nimcp_territory_bounds_t operation_area,
    uint64_t deadline
);

/**
 * @brief Assign swarms to a mission
 *
 * @param coordinator Multi-swarm coordinator
 * @param mission_id Mission ID
 * @param swarm_ids Array of swarm IDs to assign
 * @param swarm_count Number of swarms
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_mission_assign_swarms(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t mission_id,
    const uint64_t* swarm_ids,
    uint32_t swarm_count
);

/**
 * @brief Update mission progress
 *
 * @param coordinator Multi-swarm coordinator
 * @param mission_id Mission ID
 * @param progress Progress percentage (0-1)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_mission_update_progress(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t mission_id,
    float progress
);

/**
 * @brief Complete a mission
 *
 * @param coordinator Multi-swarm coordinator
 * @param mission_id Mission ID
 * @param success true if successful, false if failed
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_mission_complete(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t mission_id,
    bool success
);

/**
 * @brief Get mission by ID
 *
 * @param coordinator Multi-swarm coordinator
 * @param mission_id Mission ID
 * @return Mission assignment, or NULL if not found
 */
nimcp_mission_assignment_t* nimcp_mission_get(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t mission_id
);

/* ============================================================================
 * Communication Bridges
 * ============================================================================ */

/**
 * @brief Create a communication bridge between two swarms
 *
 * @param coordinator Multi-swarm coordinator
 * @param swarm_a First swarm ID
 * @param swarm_b Second swarm ID
 * @param relay_agents Array of relay agent IDs (can be NULL)
 * @param relay_count Number of relay agents
 * @return Bridge ID on success, 0 on failure
 */
uint64_t nimcp_comm_bridge_create(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t swarm_a,
    uint64_t swarm_b,
    const uint32_t* relay_agents,
    uint32_t relay_count
);

/**
 * @brief Update bridge link quality
 *
 * @param coordinator Multi-swarm coordinator
 * @param bridge_id Bridge ID
 * @param link_quality Link quality (0-1)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_comm_bridge_update_quality(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t bridge_id,
    float link_quality
);

/**
 * @brief Deactivate a communication bridge
 *
 * @param coordinator Multi-swarm coordinator
 * @param bridge_id Bridge ID
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_comm_bridge_deactivate(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t bridge_id
);

/**
 * @brief Route message through appropriate bridge
 *
 * @param coordinator Multi-swarm coordinator
 * @param from_swarm Source swarm ID
 * @param to_swarm Destination swarm ID
 * @param message Message to route
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_comm_bridge_route_message(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t from_swarm,
    uint64_t to_swarm,
    bio_message_header_t* message
);

/* ============================================================================
 * Conflict Resolution
 * ============================================================================ */

/**
 * @brief Detect conflicts between swarms
 *
 * WHAT: Scans all swarms for conflicts and populates conflict array
 * WHY:  Proactive conflict detection enables early resolution
 * HOW:  Compares territories, resources, and goals pairwise
 *
 * @param coordinator Multi-swarm coordinator
 * @param conflicts Output array for detected conflicts
 * @param count Output for number of conflicts detected
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_multi_swarm_detect_conflicts(
    nimcp_multi_swarm_coordinator_t* coordinator,
    nimcp_swarm_conflict_t** conflicts,
    uint32_t* count
);

/**
 * @brief Detect conflicts (legacy interface)
 *
 * @param coordinator Multi-swarm coordinator
 * @return Number of conflicts detected
 */
uint32_t nimcp_conflict_detect(
    nimcp_multi_swarm_coordinator_t* coordinator
);

/**
 * @brief Resolve a specific conflict
 *
 * WHAT: Applies resolution strategy to resolve conflict
 * WHY:  Enables conflict-free multi-swarm coordination
 * HOW:  Uses strategy-specific logic (priority, negotiation, etc.)
 *
 * @param coordinator Multi-swarm coordinator
 * @param conflict_id Conflict ID
 * @param strategy Resolution strategy
 * @param result Output for resolution result (can be NULL)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_multi_swarm_resolve_conflict(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint32_t conflict_id,
    nimcp_conflict_resolution_t strategy,
    nimcp_swarm_resolution_result_t* result
);

/**
 * @brief Resolve a specific conflict (legacy interface)
 *
 * @param coordinator Multi-swarm coordinator
 * @param conflict_id Conflict ID
 * @param strategy Resolution strategy
 * @param resolver Optional custom resolver callback
 * @param user_data User data for callback
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_conflict_resolve(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t conflict_id,
    nimcp_conflict_resolution_t strategy,
    nimcp_conflict_resolver_fn resolver,
    void* user_data
);

/**
 * @brief Automatically resolve all conflicts
 *
 * @param coordinator Multi-swarm coordinator
 * @param resolver Optional custom resolver callback
 * @param user_data User data for callback
 * @return Number of conflicts resolved
 */
uint32_t nimcp_conflict_auto_resolve(
    nimcp_multi_swarm_coordinator_t* coordinator,
    nimcp_conflict_resolver_fn resolver,
    void* user_data
);

/**
 * @brief Start negotiation for a conflict
 *
 * WHAT: Initiates multi-round negotiation protocol
 * WHY:  Allows swarms to reach mutually acceptable solution
 * HOW:  Sets up negotiation context and sends initial messages
 *
 * @param coordinator Multi-swarm coordinator
 * @param conflict_id Conflict ID to negotiate
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_multi_swarm_start_negotiation(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint32_t conflict_id
);

/**
 * @brief Propose a solution during negotiation
 *
 * WHAT: Swarm proposes specific solution to conflict
 * WHY:  Enables collaborative problem-solving
 * HOW:  Stores proposal and broadcasts to involved swarms
 *
 * @param coordinator Multi-swarm coordinator
 * @param conflict_id Conflict ID
 * @param proposal Proposed solution array
 * @param proposal_size Size of proposal array
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_multi_swarm_propose(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint32_t conflict_id,
    const float* proposal,
    uint32_t proposal_size
);

/**
 * @brief Accept a negotiation proposal
 *
 * WHAT: Swarm accepts current proposal
 * WHY:  Finalizes negotiated solution
 * HOW:  Marks conflict as resolved with accepted proposal
 *
 * @param coordinator Multi-swarm coordinator
 * @param conflict_id Conflict ID
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_multi_swarm_accept_proposal(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint32_t conflict_id
);

/**
 * @brief Reject a negotiation proposal
 *
 * WHAT: Swarm rejects current proposal with reason
 * WHY:  Allows continued negotiation with feedback
 * HOW:  Logs rejection and increments negotiation round
 *
 * @param coordinator Multi-swarm coordinator
 * @param conflict_id Conflict ID
 * @param reason Rejection reason string
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_multi_swarm_reject_proposal(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint32_t conflict_id,
    const char* reason
);

/**
 * @brief Get current negotiation status
 *
 * WHAT: Retrieves current round information
 * WHY:  Allows monitoring negotiation progress
 * HOW:  Returns copy of current negotiation round data
 *
 * @param coordinator Multi-swarm coordinator
 * @param conflict_id Conflict ID
 * @param current_round Output for current round (can be NULL)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_multi_swarm_get_negotiation_status(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint32_t conflict_id,
    nimcp_negotiation_round_t* current_round
);

/**
 * @brief Get conflict resolution statistics
 *
 * WHAT: Returns comprehensive conflict statistics
 * WHY:  Enables monitoring system health and performance
 * HOW:  Aggregates stats from all super-swarms
 *
 * @param coordinator Multi-swarm coordinator
 * @return Conflict resolution statistics structure
 */
nimcp_conflict_resolution_stats_t nimcp_multi_swarm_get_conflict_stats(
    nimcp_multi_swarm_coordinator_t* coordinator
);

/**
 * @brief Configure conflict resolution behavior
 *
 * WHAT: Sets conflict resolution configuration
 * WHY:  Allows customization of resolution strategies
 * HOW:  Stores config in coordinator structure
 *
 * @param coordinator Multi-swarm coordinator
 * @param config Configuration to apply
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_multi_swarm_set_conflict_config(
    nimcp_multi_swarm_coordinator_t* coordinator,
    const nimcp_conflict_resolution_config_t* config
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Process bio-async inbox messages
 *
 * @param coordinator Multi-swarm coordinator
 * @return Number of messages processed
 */
uint32_t nimcp_multi_swarm_process_inbox(
    nimcp_multi_swarm_coordinator_t* coordinator
);

/**
 * @brief Broadcast swarm discovery message
 *
 * @param coordinator Multi-swarm coordinator
 * @param swarm_id Swarm ID to broadcast
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_multi_swarm_broadcast_discovery(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t swarm_id
);

/**
 * @brief Send inter-swarm coordination message
 *
 * @param coordinator Multi-swarm coordinator
 * @param from_swarm Source swarm ID
 * @param to_swarm Destination swarm ID
 * @param message_type Message type
 * @param payload Message payload
 * @param payload_size Payload size in bytes
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_multi_swarm_send_message(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t from_swarm,
    uint64_t to_swarm,
    uint32_t message_type,
    const void* payload,
    size_t payload_size
);

/* ============================================================================
 * Query and Statistics
 * ============================================================================ */

/**
 * @brief Get swarm by ID
 *
 * @param coordinator Multi-swarm coordinator
 * @param swarm_id Swarm ID
 * @return Swarm identity, or NULL if not found
 */
nimcp_swarm_identity_t* nimcp_swarm_get(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint64_t swarm_id
);

/**
 * @brief Get all swarms with a specific capability
 *
 * @param coordinator Multi-swarm coordinator
 * @param capability Capability type
 * @param min_proficiency Minimum proficiency level
 * @param results Output vector for results
 * @return Number of swarms found
 */
uint32_t nimcp_swarm_find_by_capability(
    nimcp_multi_swarm_coordinator_t* coordinator,
    nimcp_swarm_capability_type_t capability,
    float min_proficiency,
    void* results
);

/**
 * @brief Get swarms in a specific territory
 *
 * @param coordinator Multi-swarm coordinator
 * @param territory Territory bounds
 * @param results Output vector for results
 * @return Number of swarms found
 */
uint32_t nimcp_swarm_find_in_territory(
    nimcp_multi_swarm_coordinator_t* coordinator,
    nimcp_territory_bounds_t territory,
    void* results
);

/**
 * @brief Get overall multi-swarm statistics
 *
 * @param coordinator Multi-swarm coordinator
 * @param total_swarms Output for total swarms
 * @param total_agents Output for total agents
 * @param active_missions Output for active missions
 * @param active_conflicts Output for active conflicts
 */
void nimcp_multi_swarm_get_stats(
    nimcp_multi_swarm_coordinator_t* coordinator,
    uint32_t* total_swarms,
    uint32_t* total_agents,
    uint32_t* active_missions,
    uint32_t* active_conflicts
);

/**
 * @brief Print multi-swarm coordinator status
 *
 * @param coordinator Multi-swarm coordinator
 * @param verbose Include detailed information
 */
void nimcp_multi_swarm_print_status(
    nimcp_multi_swarm_coordinator_t* coordinator,
    bool verbose
);

#include "async/nimcp_bio_router.h"
#include "utils/thread/nimcp_thread.h"
#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_MULTI_H */
