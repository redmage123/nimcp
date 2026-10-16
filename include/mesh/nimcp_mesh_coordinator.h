/**
 * @file nimcp_mesh_coordinator.h
 * @brief Mesh Network Coordinator - Individual Coordinator Node
 *
 * WHAT: Single coordinator in a coordinator pool
 * WHY:  Coordinators orchestrate participants, handle endorsement, and manage load
 * HOW:  Each coordinator can be leader, worker, or standby in a pool
 *
 * COORDINATOR ROLES:
 * - LEADER: Orchestrates pool, delegates to workers, handles elections
 * - WORKER: Handles subset of participants, processes transactions
 * - STANDBY: Ready for promotion on failure
 * - FOLLOWER: Raft follower for ordering service
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                           MESH COORDINATOR                              │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │  Coordinator: coord_001                                                 │
 * │  ├── Role: LEADER                                                       │
 * │  ├── State: ACTIVE                                                      │
 * │  ├── Pool: hemisphere_left_pool                                         │
 * │  ├── Channel: LEFT_HEMISPHERE                                           │
 * │  ├── Assigned Participants: [p1, p2, p3...]                            │
 * │  ├── Load: 0.65                                                         │
 * │  ├── Election Term: 42                                                  │
 * │  └── Health: 0.95                                                       │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_COORDINATOR_H
#define NIMCP_MESH_COORDINATOR_H

#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_channel.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct mesh_coordinator mesh_coordinator_t;
typedef struct mesh_coordinator_pool mesh_coordinator_pool_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum participants per coordinator */
#define MESH_MAX_PARTICIPANTS_PER_COORDINATOR   64

/** @brief Default heartbeat interval (ms) */
#define MESH_DEFAULT_HEARTBEAT_INTERVAL_MS      100

/** @brief Default election timeout (ms) */
#define MESH_DEFAULT_ELECTION_TIMEOUT_MS        500

/** @brief Default sync timeout (ms) */
#define MESH_DEFAULT_SYNC_TIMEOUT_MS            1000

/** @brief Load threshold for rebalancing */
#define MESH_COORDINATOR_LOAD_THRESHOLD         0.8f

/** @brief Maximum consecutive failures before marked unhealthy */
#define MESH_MAX_CONSECUTIVE_FAILURES           3

/* ============================================================================
 * Hierarchy Level
 * ============================================================================ */

/**
 * @brief Coordinator hierarchy level
 *
 * WHAT: Level in the coordinator hierarchy
 * WHY:  Different levels have different timing and responsibilities
 */
typedef enum coordinator_level {
    COORD_LEVEL_SYSTEM = 0,         /**< System-wide coordination (100ms base) */
    COORD_LEVEL_HEMISPHERE,         /**< Hemisphere coordination (50ms base) */
    COORD_LEVEL_LAYER,              /**< Layer coordination (10ms base) */
    COORD_LEVEL_ORDERING            /**< Ordering service (5ms base) */
} coordinator_level_t;

/* ============================================================================
 * Coordinator Configuration
 * ============================================================================ */

/**
 * @brief Coordinator configuration
 */
typedef struct mesh_coordinator_config {
    const char* name;                   /**< Coordinator name */
    coordinator_level_t level;          /**< Hierarchy level */
    mesh_channel_id_t channel;          /**< Home channel */
    mesh_pool_id_t pool_id;             /**< Pool ID (0 if not in pool) */

    /* Timing */
    float heartbeat_interval_ms;        /**< Heartbeat interval */
    float election_timeout_ms;          /**< Election timeout */
    float sync_timeout_ms;              /**< State sync timeout */

    /* Capacity */
    size_t max_participants;            /**< Max assigned participants */

    /* Logging */
    bool enable_logging;                /**< Enable coordinator logging */
} mesh_coordinator_config_t;

/**
 * @brief Coordinator statistics
 */
typedef struct mesh_coordinator_stats {
    mesh_participant_id_t id;           /**< Coordinator participant ID */
    coordinator_role_t role;            /**< Current role */
    coordinator_state_t state;          /**< Current state */
    size_t assigned_participants;       /**< Assigned participant count */
    float current_load;                 /**< Current load [0,1] */
    uint64_t transactions_coordinated;  /**< Total transactions */
    uint64_t endorsements_collected;    /**< Total endorsements */
    uint64_t heartbeats_sent;           /**< Total heartbeats */
    uint64_t elections_participated;    /**< Elections participated in */
    uint64_t elections_won;             /**< Elections won (became leader) */
    uint64_t failures_recovered;        /**< Failures recovered from */
    uint64_t consecutive_failures;      /**< Current failure streak */
    float health_score;                 /**< Health [0,1] */
    uint64_t uptime_ms;                 /**< Total uptime */
} mesh_coordinator_stats_t;

/* ============================================================================
 * Coordinator Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default coordinator configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_default_config(mesh_coordinator_config_t* config);

/**
 * @brief Create a coordinator
 *
 * WHAT: Allocate and initialize single coordinator
 * WHY:  Coordinators manage participants and transactions
 *
 * @param config Configuration
 * @param registry Participant registry
 * @param channel Associated channel
 * @return Coordinator handle or NULL on failure
 */
mesh_coordinator_t* mesh_coordinator_create(
    const mesh_coordinator_config_t* config,
    mesh_participant_registry_t* registry,
    mesh_channel_t* channel
);

/**
 * @brief Destroy a coordinator
 *
 * @param coordinator Coordinator to destroy (NULL-safe)
 */
void mesh_coordinator_destroy(mesh_coordinator_t* coordinator);

/**
 * @brief Get coordinator participant ID
 *
 * @param coordinator Coordinator handle
 * @return Coordinator's participant ID
 */
mesh_participant_id_t mesh_coordinator_get_id(const mesh_coordinator_t* coordinator);

/**
 * @brief Get coordinator name
 *
 * @param coordinator Coordinator handle
 * @return Coordinator name or NULL
 */
const char* mesh_coordinator_get_name(const mesh_coordinator_t* coordinator);

/* ============================================================================
 * Role and State API
 * ============================================================================ */

/**
 * @brief Get current role
 *
 * @param coordinator Coordinator handle
 * @return Current role
 */
coordinator_role_t mesh_coordinator_get_role(const mesh_coordinator_t* coordinator);

/**
 * @brief Set role (internal, usually set by pool)
 *
 * @param coordinator Coordinator handle
 * @param role New role
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_set_role(
    mesh_coordinator_t* coordinator,
    coordinator_role_t role
);

/**
 * @brief Get current state
 *
 * @param coordinator Coordinator handle
 * @return Current state
 */
coordinator_state_t mesh_coordinator_get_state(const mesh_coordinator_t* coordinator);

/**
 * @brief Set state
 *
 * @param coordinator Coordinator handle
 * @param state New state
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_set_state(
    mesh_coordinator_t* coordinator,
    coordinator_state_t state
);

/**
 * @brief Get hierarchy level
 *
 * @param coordinator Coordinator handle
 * @return Hierarchy level
 */
coordinator_level_t mesh_coordinator_get_level(const mesh_coordinator_t* coordinator);

/* ============================================================================
 * Participant Assignment API
 * ============================================================================ */

/**
 * @brief Assign participant to coordinator
 *
 * WHAT: Add participant to coordinator's responsibility
 * WHY:  Load balancing across coordinator pool
 *
 * @param coordinator Coordinator handle
 * @param participant_id Participant to assign
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_assign_participant(
    mesh_coordinator_t* coordinator,
    mesh_participant_id_t participant_id
);

/**
 * @brief Unassign participant from coordinator
 *
 * @param coordinator Coordinator handle
 * @param participant_id Participant to unassign
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_unassign_participant(
    mesh_coordinator_t* coordinator,
    mesh_participant_id_t participant_id
);

/**
 * @brief Check if participant is assigned
 *
 * @param coordinator Coordinator handle
 * @param participant_id Participant to check
 * @return true if assigned
 */
bool mesh_coordinator_has_participant(
    const mesh_coordinator_t* coordinator,
    mesh_participant_id_t participant_id
);

/**
 * @brief Get assigned participant count
 *
 * @param coordinator Coordinator handle
 * @return Assigned participant count
 */
size_t mesh_coordinator_get_participant_count(const mesh_coordinator_t* coordinator);

/**
 * @brief Get all assigned participants
 *
 * @param coordinator Coordinator handle
 * @param ids_out Output array (caller allocates)
 * @param max_ids Maximum IDs
 * @param count_out Output: actual count
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_get_participants(
    const mesh_coordinator_t* coordinator,
    mesh_participant_id_t* ids_out,
    size_t max_ids,
    size_t* count_out
);

/* ============================================================================
 * Load and Health API
 * ============================================================================ */

/**
 * @brief Get current load
 *
 * @param coordinator Coordinator handle
 * @return Load [0,1] where 1 = fully loaded
 */
float mesh_coordinator_get_load(const mesh_coordinator_t* coordinator);

/**
 * @brief Get health score
 *
 * @param coordinator Coordinator handle
 * @return Health [0,1] where 1 = fully healthy
 */
float mesh_coordinator_get_health(const mesh_coordinator_t* coordinator);

/**
 * @brief Check if coordinator is overloaded
 *
 * @param coordinator Coordinator handle
 * @return true if load > threshold
 */
bool mesh_coordinator_is_overloaded(const mesh_coordinator_t* coordinator);

/**
 * @brief Report failure
 *
 * @param coordinator Coordinator handle
 * @param error Error that occurred
 */
void mesh_coordinator_report_failure(
    mesh_coordinator_t* coordinator,
    nimcp_error_t error
);

/**
 * @brief Report recovery
 *
 * @param coordinator Coordinator handle
 */
void mesh_coordinator_report_recovery(mesh_coordinator_t* coordinator);

/* ============================================================================
 * Heartbeat and Election API
 * ============================================================================ */

/**
 * @brief Send heartbeat
 *
 * WHAT: Send heartbeat to pool/peers
 * WHY:  Failure detection and leader liveness
 *
 * @param coordinator Coordinator handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_send_heartbeat(mesh_coordinator_t* coordinator);

/**
 * @brief Receive heartbeat
 *
 * @param coordinator Coordinator handle
 * @param from_id Source coordinator ID
 * @param term Election term
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_receive_heartbeat(
    mesh_coordinator_t* coordinator,
    mesh_participant_id_t from_id,
    uint64_t term
);

/**
 * @brief Get last heartbeat time
 *
 * @param coordinator Coordinator handle
 * @return Last heartbeat timestamp (ns)
 */
uint64_t mesh_coordinator_get_last_heartbeat(const mesh_coordinator_t* coordinator);

/**
 * @brief Check if heartbeat timed out
 *
 * @param coordinator Coordinator handle
 * @return true if timed out
 */
bool mesh_coordinator_heartbeat_timed_out(const mesh_coordinator_t* coordinator);

/**
 * @brief Request vote for election
 *
 * @param coordinator Coordinator handle
 * @param term Election term
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_request_vote(
    mesh_coordinator_t* coordinator,
    uint64_t term
);

/**
 * @brief Cast vote in election
 *
 * @param coordinator Coordinator handle
 * @param candidate_id Candidate to vote for
 * @param term Election term
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_cast_vote(
    mesh_coordinator_t* coordinator,
    mesh_participant_id_t candidate_id,
    uint64_t term
);

/**
 * @brief Get current election term
 *
 * @param coordinator Coordinator handle
 * @return Current term
 */
uint64_t mesh_coordinator_get_term(const mesh_coordinator_t* coordinator);

/**
 * @brief Set election term
 *
 * @param coordinator Coordinator handle
 * @param term New term
 */
void mesh_coordinator_set_term(mesh_coordinator_t* coordinator, uint64_t term);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update coordinator
 *
 * WHAT: Perform periodic coordinator update
 * WHY:  Process pending operations, check health, send heartbeat
 *
 * @param coordinator Coordinator handle
 * @param delta_ms Time since last update
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_update(
    mesh_coordinator_t* coordinator,
    uint64_t delta_ms
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get coordinator statistics
 *
 * @param coordinator Coordinator handle
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_get_stats(
    const mesh_coordinator_t* coordinator,
    mesh_coordinator_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param coordinator Coordinator handle
 */
void mesh_coordinator_reset_stats(mesh_coordinator_t* coordinator);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get timing for hierarchy level
 *
 * @param level Hierarchy level
 * @param timing Output timing configuration
 */
void mesh_coordinator_get_level_timing(
    coordinator_level_t level,
    mesh_timing_t* timing
);

/**
 * @brief Get level name
 *
 * @param level Hierarchy level
 * @return Level name string
 */
const char* mesh_coordinator_level_to_string(coordinator_level_t level);

/**
 * @brief Print coordinator info for debugging
 *
 * @param coordinator Coordinator handle
 */
void mesh_coordinator_print_info(const mesh_coordinator_t* coordinator);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_COORDINATOR_H */
