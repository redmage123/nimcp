/**
 * @file nimcp_mesh_coordinator_pool.h
 * @brief Mesh Network Coordinator Pool - BFT Leader Election and Load Balancing
 *
 * WHAT: Pool of coordinators with leader election, failover, and load balancing
 * WHY:  Fault tolerance and scalability through coordinator redundancy
 * HOW:  BFT consensus for leader election, consistent hashing for assignment
 *
 * POOL STRUCTURE:
 * - One LEADER orchestrates the pool
 * - Multiple WORKERs handle participants
 * - STANDBY coordinators ready for promotion
 *
 * LEADER ELECTION:
 * Uses Byzantine Fault Tolerant voting (tolerates 1/3 faulty coordinators):
 * 1. Heartbeat timeout triggers election
 * 2. Candidates request votes
 * 3. Majority (>2/3) wins election
 * 4. New leader announces to pool
 *
 * LOAD BALANCING:
 * - Round-robin assignment to workers
 * - Rebalancing when load exceeds threshold
 * - Automatic migration on coordinator failure
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                        COORDINATOR POOL                                 │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │  Pool: left_hemisphere_pool                                             │
 * │  ├── Leader: [coord_1] ◄── Orchestrates pool                           │
 * │  ├── Workers: [coord_2, coord_3] ◄── Handle participants               │
 * │  ├── Standby: [coord_4] ◄── Ready for promotion                        │
 * │  ├── Election Term: 42                                                  │
 * │  ├── Total Participants: 150                                            │
 * │  └── Load Distribution: [0.5, 0.6, 0.4, 0.0]                           │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_COORDINATOR_POOL_H
#define NIMCP_MESH_COORDINATOR_POOL_H

#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_coordinator.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct mesh_coordinator_pool mesh_coordinator_pool_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Minimum coordinators for BFT (3f+1 where f=1) */
#define MESH_MIN_POOL_SIZE_BFT              4

/** @brief Default pool size */
#define MESH_DEFAULT_POOL_SIZE              4

/** @brief BFT threshold (1/3 faulty tolerance) */
#define MESH_BFT_THRESHOLD                  0.333333f

/** @brief Vote quorum (2/3 + 1) */
#define MESH_VOTE_QUORUM                    0.666667f

/** @brief Default rebalance interval (ms) */
#define MESH_DEFAULT_REBALANCE_INTERVAL_MS  5000

/** @brief Maximum election rounds before timeout */
#define MESH_MAX_ELECTION_ROUNDS            5

/* ============================================================================
 * Pool Configuration
 * ============================================================================ */

/**
 * @brief Coordinator pool configuration
 */
typedef struct mesh_coordinator_pool_config {
    const char* pool_name;              /**< Pool name */
    mesh_pool_id_t pool_id;             /**< Pool ID */
    mesh_channel_id_t channel;          /**< Associated channel */
    coordinator_level_t level;          /**< Hierarchy level for all coords */

    /* Pool size */
    size_t initial_size;                /**< Initial coordinator count */
    size_t min_size;                    /**< Minimum size (for BFT) */
    size_t max_size;                    /**< Maximum size */

    /* Election parameters */
    float election_timeout_ms;          /**< Election timeout */
    size_t max_election_rounds;         /**< Max rounds before failure */

    /* Load balancing */
    float load_threshold;               /**< Rebalance threshold */
    float rebalance_interval_ms;        /**< Rebalance interval */

    /* Health */
    size_t max_consecutive_failures;    /**< Failures before demotion */

    /* Logging */
    bool enable_logging;                /**< Enable pool logging */
} mesh_coordinator_pool_config_t;

/**
 * @brief Pool statistics
 */
typedef struct mesh_coordinator_pool_stats {
    mesh_pool_id_t pool_id;             /**< Pool ID */
    size_t coordinator_count;           /**< Total coordinators */
    size_t leader_count;                /**< Leaders (should be 1) */
    size_t worker_count;                /**< Workers */
    size_t standby_count;               /**< Standbys */
    size_t active_count;                /**< Active (not failed) */
    size_t failed_count;                /**< Failed coordinators */

    uint64_t current_term;              /**< Current election term */
    mesh_participant_id_t leader_id;    /**< Current leader */

    size_t total_participants;          /**< Total assigned participants */
    float avg_load;                     /**< Average coordinator load */
    float max_load;                     /**< Maximum coordinator load */

    uint64_t elections_held;            /**< Total elections */
    uint64_t rebalances;                /**< Total rebalances */
    uint64_t failovers;                 /**< Total failovers */
    uint64_t leader_changes;            /**< Leader changes */

    float uptime_ratio;                 /**< Pool uptime ratio */
} mesh_coordinator_pool_stats_t;

/**
 * @brief Election result
 */
typedef struct mesh_election_result {
    mesh_pool_id_t pool_id;             /**< Pool that held election */
    uint64_t term;                      /**< Election term */
    mesh_participant_id_t winner;       /**< Election winner */
    size_t votes_received;              /**< Votes winner received */
    size_t total_voters;                /**< Total voters */
    bool success;                       /**< Election succeeded */
    uint64_t duration_ms;               /**< Election duration */
} mesh_election_result_t;

/* ============================================================================
 * Pool Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default pool configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_pool_default_config(
    mesh_coordinator_pool_config_t* config
);

/**
 * @brief Create coordinator pool
 *
 * WHAT: Allocate and initialize pool with coordinators
 * WHY:  Coordinator pools provide fault tolerance and load balancing
 *
 * @param config Pool configuration
 * @param registry Participant registry
 * @param channel Associated channel
 * @return Pool handle or NULL on failure
 */
mesh_coordinator_pool_t* mesh_coordinator_pool_create(
    const mesh_coordinator_pool_config_t* config,
    mesh_participant_registry_t* registry,
    mesh_channel_t* channel
);

/**
 * @brief Destroy coordinator pool
 *
 * @param pool Pool to destroy (NULL-safe)
 */
void mesh_coordinator_pool_destroy(mesh_coordinator_pool_t* pool);

/**
 * @brief Get pool ID
 *
 * @param pool Pool handle
 * @return Pool ID
 */
mesh_pool_id_t mesh_coordinator_pool_get_id(const mesh_coordinator_pool_t* pool);

/**
 * @brief Get pool name
 *
 * @param pool Pool handle
 * @return Pool name or NULL
 */
const char* mesh_coordinator_pool_get_name(const mesh_coordinator_pool_t* pool);

/* ============================================================================
 * Coordinator Management API
 * ============================================================================ */

/**
 * @brief Add coordinator to pool
 *
 * @param pool Pool handle
 * @param coordinator Coordinator to add
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_pool_add(
    mesh_coordinator_pool_t* pool,
    mesh_coordinator_t* coordinator
);

/**
 * @brief Remove coordinator from pool
 *
 * @param pool Pool handle
 * @param coordinator_id Coordinator to remove
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_pool_remove(
    mesh_coordinator_pool_t* pool,
    mesh_participant_id_t coordinator_id
);

/**
 * @brief Get coordinator by ID
 *
 * @param pool Pool handle
 * @param coordinator_id Coordinator ID
 * @return Coordinator or NULL
 */
mesh_coordinator_t* mesh_coordinator_pool_get(
    mesh_coordinator_pool_t* pool,
    mesh_participant_id_t coordinator_id
);

/**
 * @brief Get coordinator by index
 *
 * @param pool Pool handle
 * @param index Index in pool
 * @return Coordinator or NULL
 */
mesh_coordinator_t* mesh_coordinator_pool_get_by_index(
    mesh_coordinator_pool_t* pool,
    size_t index
);

/**
 * @brief Get coordinator count
 *
 * @param pool Pool handle
 * @return Coordinator count
 */
size_t mesh_coordinator_pool_get_size(const mesh_coordinator_pool_t* pool);

/* ============================================================================
 * Leader and Role API
 * ============================================================================ */

/**
 * @brief Get current leader
 *
 * @param pool Pool handle
 * @return Leader coordinator or NULL if no leader
 */
mesh_coordinator_t* mesh_coordinator_pool_get_leader(
    mesh_coordinator_pool_t* pool
);

/**
 * @brief Get leader ID
 *
 * @param pool Pool handle
 * @return Leader participant ID or 0 if no leader
 */
mesh_participant_id_t mesh_coordinator_pool_get_leader_id(
    const mesh_coordinator_pool_t* pool
);

/**
 * @brief Check if pool has leader
 *
 * @param pool Pool handle
 * @return true if pool has active leader
 */
bool mesh_coordinator_pool_has_leader(const mesh_coordinator_pool_t* pool);

/**
 * @brief Get coordinators by role
 *
 * @param pool Pool handle
 * @param role Role to filter
 * @param coords_out Output array (caller allocates)
 * @param max_coords Maximum coordinators
 * @param count_out Output: actual count
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_pool_get_by_role(
    mesh_coordinator_pool_t* pool,
    coordinator_role_t role,
    mesh_coordinator_t** coords_out,
    size_t max_coords,
    size_t* count_out
);

/* ============================================================================
 * Leader Election API
 * ============================================================================ */

/**
 * @brief Trigger leader election
 *
 * WHAT: Initiate BFT leader election
 * WHY:  No leader or leader failed
 *
 * @param pool Pool handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_pool_elect_leader(
    mesh_coordinator_pool_t* pool
);

/**
 * @brief Process election vote
 *
 * @param pool Pool handle
 * @param voter_id Voting coordinator
 * @param candidate_id Voted for candidate
 * @param term Election term
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_pool_process_vote(
    mesh_coordinator_pool_t* pool,
    mesh_participant_id_t voter_id,
    mesh_participant_id_t candidate_id,
    uint64_t term
);

/**
 * @brief Get current election term
 *
 * @param pool Pool handle
 * @return Current term
 */
uint64_t mesh_coordinator_pool_get_term(const mesh_coordinator_pool_t* pool);

/**
 * @brief Get last election result
 *
 * @param pool Pool handle
 * @param result Output: election result
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_pool_get_last_election(
    const mesh_coordinator_pool_t* pool,
    mesh_election_result_t* result
);

/**
 * @brief Check if election is in progress
 *
 * @param pool Pool handle
 * @return true if election ongoing
 */
bool mesh_coordinator_pool_election_in_progress(
    const mesh_coordinator_pool_t* pool
);

/**
 * @brief Set election callback
 *
 * @param pool Pool handle
 * @param callback Callback function
 * @param ctx Callback context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_pool_set_election_callback(
    mesh_coordinator_pool_t* pool,
    mesh_election_callback_t callback,
    void* ctx
);

/* ============================================================================
 * Load Balancing API
 * ============================================================================ */

/**
 * @brief Assign participant to pool
 *
 * WHAT: Assign participant to appropriate coordinator
 * WHY:  Distribute load across pool
 *
 * @param pool Pool handle
 * @param participant_id Participant to assign
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_pool_assign_participant(
    mesh_coordinator_pool_t* pool,
    mesh_participant_id_t participant_id
);

/**
 * @brief Assign participant to leader
 *
 * WHAT: Assign directly to leader (for high-priority)
 * WHY:  Some participants need faster processing
 *
 * @param pool Pool handle
 * @param participant_id Participant to assign
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_pool_assign_to_leader(
    mesh_coordinator_pool_t* pool,
    mesh_participant_id_t participant_id
);

/**
 * @brief Rebalance participant assignments
 *
 * WHAT: Redistribute participants for even load
 * WHY:  Prevent coordinator overload
 *
 * @param pool Pool handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_pool_rebalance(
    mesh_coordinator_pool_t* pool
);

/**
 * @brief Get assignment for participant
 *
 * @param pool Pool handle
 * @param participant_id Participant ID
 * @return Assigned coordinator or NULL
 */
mesh_coordinator_t* mesh_coordinator_pool_get_assignment(
    mesh_coordinator_pool_t* pool,
    mesh_participant_id_t participant_id
);

/**
 * @brief Get total assigned participants
 *
 * @param pool Pool handle
 * @return Total assigned count
 */
size_t mesh_coordinator_pool_get_total_participants(
    const mesh_coordinator_pool_t* pool
);

/* ============================================================================
 * Failure Handling API
 * ============================================================================ */

/**
 * @brief Handle coordinator failure
 *
 * WHAT: Process coordinator failure, migrate participants
 * WHY:  Maintain service despite failures
 *
 * @param pool Pool handle
 * @param failed_coord_id Failed coordinator ID
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_pool_handle_failure(
    mesh_coordinator_pool_t* pool,
    mesh_participant_id_t failed_coord_id
);

/**
 * @brief Promote standby to worker
 *
 * @param pool Pool handle
 * @param standby_id Standby coordinator to promote
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_pool_promote_standby(
    mesh_coordinator_pool_t* pool,
    mesh_participant_id_t standby_id
);

/**
 * @brief Demote failed coordinator
 *
 * @param pool Pool handle
 * @param coord_id Coordinator to demote
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_pool_demote(
    mesh_coordinator_pool_t* pool,
    mesh_participant_id_t coord_id
);

/**
 * @brief Get failed coordinator count
 *
 * @param pool Pool handle
 * @return Failed coordinator count
 */
size_t mesh_coordinator_pool_get_failed_count(
    const mesh_coordinator_pool_t* pool
);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update pool
 *
 * WHAT: Perform periodic pool update
 * WHY:  Check health, trigger elections, rebalance
 *
 * @param pool Pool handle
 * @param delta_ms Time since last update
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_pool_update(
    mesh_coordinator_pool_t* pool,
    uint64_t delta_ms
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get pool statistics
 *
 * @param pool Pool handle
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_pool_get_stats(
    const mesh_coordinator_pool_t* pool,
    mesh_coordinator_pool_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param pool Pool handle
 */
void mesh_coordinator_pool_reset_stats(mesh_coordinator_pool_t* pool);

/* ============================================================================
 * Scaling API
 * ============================================================================ */

/**
 * @brief Scale pool up
 *
 * WHAT: Add coordinators to pool
 * WHY:  Handle increased load
 *
 * @param pool Pool handle
 * @param count Number of coordinators to add
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_pool_scale_up(
    mesh_coordinator_pool_t* pool,
    size_t count
);

/**
 * @brief Scale pool down
 *
 * WHAT: Remove coordinators from pool
 * WHY:  Reduce resources when load decreases
 *
 * @param pool Pool handle
 * @param count Number of coordinators to remove
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_coordinator_pool_scale_down(
    mesh_coordinator_pool_t* pool,
    size_t count
);

/**
 * @brief Get optimal pool size
 *
 * WHAT: Calculate optimal size for current load
 * WHY:  Auto-scaling recommendation
 *
 * @param pool Pool handle
 * @param total_participants Total participants to handle
 * @return Optimal pool size
 */
size_t mesh_coordinator_pool_optimal_size(
    const mesh_coordinator_pool_t* pool,
    size_t total_participants
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Print pool status for debugging
 *
 * @param pool Pool handle
 */
void mesh_coordinator_pool_print_status(const mesh_coordinator_pool_t* pool);

/**
 * @brief Validate pool BFT requirements
 *
 * WHAT: Check if pool meets BFT requirements
 * WHY:  Ensure fault tolerance
 *
 * @param pool Pool handle
 * @return true if BFT requirements met
 */
bool mesh_coordinator_pool_is_bft_valid(const mesh_coordinator_pool_t* pool);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_COORDINATOR_POOL_H */
