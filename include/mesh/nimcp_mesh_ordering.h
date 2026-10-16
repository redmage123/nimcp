/**
 * @file nimcp_mesh_ordering.h
 * @brief Mesh Network Ordering Service - Raft-based Transaction Sequencing
 *
 * WHAT: Ordering service for total transaction ordering across channels
 * WHY:  Consistent ordering ensures deterministic state across all peers
 * HOW:  Raft consensus among orderer nodes with batching for throughput
 *
 * ORDERING SERVICE ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                       ORDERING SERVICE                                   │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐                 │
 * │  │   LEADER    │───►│  FOLLOWER   │───►│  FOLLOWER   │                 │
 * │  │  (sequencing)│    │ (replicating)│    │ (replicating)│                │
 * │  └─────────────┘    └─────────────┘    └─────────────┘                 │
 * │         │                                                               │
 * │         ▼                                                               │
 * │  ┌─────────────────────────────────────────────────────────────┐       │
 * │  │                    TRANSACTION QUEUE                        │       │
 * │  │  [TX1] [TX2] [TX3] ... ─────► BATCH ─────► SEQUENCE         │       │
 * │  └─────────────────────────────────────────────────────────────┘       │
 * │         │                                                               │
 * │         ▼                                                               │
 * │  ┌─────────────────────────────────────────────────────────────┐       │
 * │  │                    ORDERED BLOCKS                            │       │
 * │  │  [Block N-1] [Block N] [Block N+1] ...                       │       │
 * │  │  seq: 100-149  150-199  200-249                              │       │
 * │  └─────────────────────────────────────────────────────────────┘       │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * RAFT CONSENSUS:
 * - Leader election on timeout
 * - Log replication to followers
 * - Committed when majority acknowledges
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_ORDERING_H
#define NIMCP_MESH_ORDERING_H

#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_coordinator_pool.h"
#include "mesh/nimcp_mesh_transaction.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct mesh_ordering_service mesh_ordering_service_t;
typedef struct mesh_ordered_block mesh_ordered_block_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum transactions per block */
#define MESH_MAX_TXS_PER_BLOCK              64

/** @brief Maximum pending transactions in queue */
#define MESH_MAX_ORDERING_QUEUE             4096

/** @brief Default batch size */
#define MESH_DEFAULT_BATCH_SIZE             32

/** @brief Default batch timeout (ms) */
#define MESH_DEFAULT_ORDERING_BATCH_TIMEOUT 50

/** @brief Default Raft heartbeat interval (ms) */
#define MESH_RAFT_HEARTBEAT_INTERVAL_MS     50

/** @brief Default Raft election timeout (ms) */
#define MESH_RAFT_ELECTION_TIMEOUT_MS       150

/** @brief Maximum log entries to keep */
#define MESH_MAX_LOG_ENTRIES                10000

/* ============================================================================
 * Raft State
 * ============================================================================ */

/**
 * @brief Raft node role
 */
typedef enum raft_role {
    RAFT_ROLE_FOLLOWER = 0,     /**< Follower - receives log entries */
    RAFT_ROLE_CANDIDATE,        /**< Candidate - seeking election */
    RAFT_ROLE_LEADER            /**< Leader - accepts requests, replicates */
} raft_role_t;

/**
 * @brief Raft log entry type
 */
typedef enum raft_entry_type {
    RAFT_ENTRY_TX_BATCH = 0,    /**< Transaction batch */
    RAFT_ENTRY_CONFIG,          /**< Configuration change */
    RAFT_ENTRY_NOOP             /**< No-op (leader election marker) */
} raft_entry_type_t;

/**
 * @brief Raft log entry
 */
typedef struct raft_log_entry {
    uint64_t term;              /**< Term when entry created */
    uint64_t index;             /**< Log index */
    raft_entry_type_t type;     /**< Entry type */

    /* Payload */
    mesh_tx_id_t* tx_ids;       /**< Transaction IDs in batch */
    size_t tx_count;            /**< Number of transactions */

    /* Metadata */
    uint64_t timestamp_ns;      /**< When created */
    bool committed;             /**< Whether committed */
} raft_log_entry_t;

/**
 * @brief Raft state for a single orderer node
 */
typedef struct raft_state {
    /* Persistent state */
    uint64_t current_term;              /**< Current term */
    mesh_participant_id_t voted_for;    /**< Candidate voted for in current term */
    raft_log_entry_t* log;              /**< Log entries */
    size_t log_size;                    /**< Number of log entries */
    size_t log_capacity;                /**< Log capacity */

    /* Volatile state */
    uint64_t commit_index;              /**< Highest log index known committed */
    uint64_t last_applied;              /**< Highest log index applied */

    /* Leader state (reinitialized after election) */
    uint64_t* next_index;               /**< Next log index to send to each follower */
    uint64_t* match_index;              /**< Highest log index replicated on each follower */
    size_t follower_count;              /**< Number of followers */

    /* Role */
    raft_role_t role;                   /**< Current role */
    mesh_participant_id_t leader_id;    /**< Current leader (0 if none) */

    /* Timing */
    uint64_t last_heartbeat_ns;         /**< Last heartbeat received */
    uint64_t election_timeout_ns;       /**< Randomized election timeout */
} raft_state_t;

/* ============================================================================
 * Ordered Block
 * ============================================================================ */

/**
 * @brief Block of ordered transactions
 */
struct mesh_ordered_block {
    uint64_t block_number;              /**< Block sequence number */
    uint64_t first_sequence;            /**< First transaction sequence */
    uint64_t last_sequence;             /**< Last transaction sequence */

    mesh_tx_id_t* tx_ids;               /**< Transaction IDs */
    size_t tx_count;                    /**< Number of transactions */

    mesh_channel_id_t channel;          /**< Primary channel */
    uint64_t timestamp_ns;              /**< Block creation time */

    uint8_t block_hash[32];             /**< Hash of block contents */
    uint8_t prev_block_hash[32];        /**< Previous block hash */

    mesh_participant_id_t orderer_id;   /**< Orderer that created block */
    uint8_t signature[MESH_SIGNATURE_SIZE]; /**< Orderer signature */
};

/* ============================================================================
 * Ordering Service Configuration
 * ============================================================================ */

/**
 * @brief Ordering service configuration
 */
typedef struct mesh_ordering_config {
    const char* service_name;           /**< Service name */

    /* Batching */
    size_t batch_size;                  /**< Max transactions per batch */
    float batch_timeout_ms;             /**< Max time to wait for batch */

    /* Queue */
    size_t max_pending;                 /**< Max pending transactions */

    /* Raft */
    float heartbeat_interval_ms;        /**< Raft heartbeat interval */
    float election_timeout_ms;          /**< Raft election timeout */
    size_t max_log_entries;             /**< Max log entries to keep */

    /* Channels */
    mesh_channel_id_t* channels;        /**< Channels this service orders */
    size_t channel_count;               /**< Number of channels */

    /* Logging */
    bool enable_logging;                /**< Enable ordering service logging */
} mesh_ordering_config_t;

/**
 * @brief Ordering service statistics
 */
typedef struct mesh_ordering_stats {
    /* Transactions */
    uint64_t transactions_submitted;    /**< Total submitted */
    uint64_t transactions_ordered;      /**< Total ordered */
    uint64_t transactions_batched;      /**< Total batched */

    /* Blocks */
    uint64_t blocks_created;            /**< Blocks created */
    uint64_t current_block;             /**< Current block number */
    uint64_t current_sequence;          /**< Current sequence number */

    /* Raft */
    uint64_t current_term;              /**< Current Raft term */
    raft_role_t role;                   /**< Current role */
    mesh_participant_id_t leader_id;    /**< Current leader */
    uint64_t elections_started;         /**< Elections initiated */
    uint64_t elections_won;             /**< Elections won */
    uint64_t log_entries;               /**< Current log entries */
    uint64_t commit_index;              /**< Current commit index */

    /* Queue */
    size_t pending_count;               /**< Pending transactions */
    size_t batch_count;                 /**< Pending batches */
    size_t max_pending;                 /**< Max pending capacity */
    float queue_utilization;            /**< Queue utilization (0-1) */
    uint64_t backpressure_events;       /**< Times backpressure threshold hit */
    uint64_t queue_full_rejections;     /**< Times transactions rejected due to full queue */

    /* Performance */
    float avg_batch_size;               /**< Average batch size */
    float avg_ordering_latency_ms;      /**< Average ordering latency */
    float throughput_tps;               /**< Transactions per second */
} mesh_ordering_stats_t;

/* ============================================================================
 * Ordering Service Lifecycle
 * ============================================================================ */

/**
 * @brief Get default ordering service configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_ordering_default_config(mesh_ordering_config_t* config);

/**
 * @brief Create ordering service
 *
 * WHAT: Allocate and initialize ordering service
 * WHY:  Central service for transaction sequencing
 *
 * @param config Configuration (NULL for defaults)
 * @param orderer_pool Pool of orderer coordinators
 * @return Service handle or NULL on failure
 */
mesh_ordering_service_t* mesh_ordering_create(
    const mesh_ordering_config_t* config,
    mesh_coordinator_pool_t* orderer_pool
);

/**
 * @brief Destroy ordering service
 *
 * @param service Service to destroy (NULL-safe)
 */
void mesh_ordering_destroy(mesh_ordering_service_t* service);

/**
 * @brief Get service name
 *
 * @param service Service handle
 * @return Service name or NULL
 */
const char* mesh_ordering_get_name(const mesh_ordering_service_t* service);

/* ============================================================================
 * Transaction Submission API
 * ============================================================================ */

/**
 * @brief Submit transaction for ordering
 *
 * WHAT: Add transaction to ordering queue
 * WHY:  Transactions must be ordered before validation/commit
 *
 * @param service Ordering service
 * @param tx Transaction to order
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_ordering_submit(
    mesh_ordering_service_t* service,
    mesh_transaction_t* tx
);

/**
 * @brief Submit batch of transactions
 *
 * @param service Ordering service
 * @param batch Transaction batch
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_ordering_submit_batch(
    mesh_ordering_service_t* service,
    mesh_tx_batch_t* batch
);

/**
 * @brief Check if transaction is pending
 *
 * @param service Ordering service
 * @param tx_id Transaction ID
 * @return true if pending ordering
 */
bool mesh_ordering_is_pending(
    const mesh_ordering_service_t* service,
    const mesh_tx_id_t* tx_id
);

/**
 * @brief Get pending transaction count
 *
 * @param service Ordering service
 * @return Number of pending transactions
 */
size_t mesh_ordering_get_pending_count(const mesh_ordering_service_t* service);

/* ============================================================================
 * Ordering Operations API
 * ============================================================================ */

/**
 * @brief Create batch from pending transactions
 *
 * WHAT: Group pending transactions into batch
 * WHY:  Batching improves throughput
 *
 * @param service Ordering service
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_ordering_create_batch(mesh_ordering_service_t* service);

/**
 * @brief Assign sequence numbers to batch
 *
 * WHAT: Assign sequential numbers to batched transactions
 * WHY:  Total ordering for deterministic state
 *
 * @param service Ordering service
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_ordering_sequence_batch(mesh_ordering_service_t* service);

/**
 * @brief Create ordered block from sequenced batch
 *
 * WHAT: Package sequenced transactions into block
 * WHY:  Blocks are the unit of distribution to peers
 *
 * @param service Ordering service
 * @return New block or NULL on failure
 */
mesh_ordered_block_t* mesh_ordering_create_block(
    mesh_ordering_service_t* service
);

/**
 * @brief Get block by number
 *
 * @param service Ordering service
 * @param block_number Block number
 * @return Block or NULL if not found
 */
const mesh_ordered_block_t* mesh_ordering_get_block(
    const mesh_ordering_service_t* service,
    uint64_t block_number
);

/**
 * @brief Get latest block number
 *
 * @param service Ordering service
 * @return Latest block number
 */
uint64_t mesh_ordering_get_latest_block(const mesh_ordering_service_t* service);

/**
 * @brief Get current sequence number
 *
 * @param service Ordering service
 * @return Current sequence number
 */
uint64_t mesh_ordering_get_sequence(const mesh_ordering_service_t* service);

/* ============================================================================
 * Raft Consensus API
 * ============================================================================ */

/**
 * @brief Start Raft election
 *
 * WHAT: Initiate leader election
 * WHY:  Called on election timeout or leader failure
 *
 * @param service Ordering service
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_ordering_start_election(mesh_ordering_service_t* service);

/**
 * @brief Process vote request
 *
 * @param service Ordering service
 * @param candidate_id Requesting candidate
 * @param term Election term
 * @param last_log_index Candidate's last log index
 * @param last_log_term Candidate's last log term
 * @param vote_granted Output: whether vote granted
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_ordering_handle_vote_request(
    mesh_ordering_service_t* service,
    mesh_participant_id_t candidate_id,
    uint64_t term,
    uint64_t last_log_index,
    uint64_t last_log_term,
    bool* vote_granted
);

/**
 * @brief Process vote response
 *
 * @param service Ordering service
 * @param voter_id Responding voter
 * @param term Response term
 * @param vote_granted Whether vote was granted
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_ordering_handle_vote_response(
    mesh_ordering_service_t* service,
    mesh_participant_id_t voter_id,
    uint64_t term,
    bool vote_granted
);

/**
 * @brief Process append entries (leader to follower)
 *
 * @param service Ordering service
 * @param leader_id Leader ID
 * @param term Leader's term
 * @param prev_log_index Index of log entry immediately preceding new ones
 * @param prev_log_term Term of prev_log_index entry
 * @param entries Log entries to append
 * @param entry_count Number of entries
 * @param leader_commit Leader's commit index
 * @param success Output: whether follower accepted
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_ordering_handle_append_entries(
    mesh_ordering_service_t* service,
    mesh_participant_id_t leader_id,
    uint64_t term,
    uint64_t prev_log_index,
    uint64_t prev_log_term,
    const raft_log_entry_t* entries,
    size_t entry_count,
    uint64_t leader_commit,
    bool* success
);

/**
 * @brief Send heartbeat (leader only)
 *
 * @param service Ordering service
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_ordering_send_heartbeat(mesh_ordering_service_t* service);

/**
 * @brief Get current Raft role
 *
 * @param service Ordering service
 * @return Current Raft role
 */
raft_role_t mesh_ordering_get_role(const mesh_ordering_service_t* service);

/**
 * @brief Get current Raft term
 *
 * @param service Ordering service
 * @return Current term
 */
uint64_t mesh_ordering_get_term(const mesh_ordering_service_t* service);

/**
 * @brief Get current leader ID
 *
 * @param service Ordering service
 * @return Leader participant ID (0 if no leader)
 */
mesh_participant_id_t mesh_ordering_get_leader(
    const mesh_ordering_service_t* service
);

/**
 * @brief Check if this node is leader
 *
 * @param service Ordering service
 * @return true if leader
 */
bool mesh_ordering_is_leader(const mesh_ordering_service_t* service);

/**
 * @brief Check if Raft has quorum
 *
 * @param service Ordering service
 * @return true if quorum available
 */
bool mesh_ordering_has_quorum(const mesh_ordering_service_t* service);

/* ============================================================================
 * Log Management API
 * ============================================================================ */

/**
 * @brief Append entry to log
 *
 * @param service Ordering service
 * @param entry Entry to append
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_ordering_log_append(
    mesh_ordering_service_t* service,
    const raft_log_entry_t* entry
);

/**
 * @brief Get log entry by index
 *
 * @param service Ordering service
 * @param index Log index
 * @return Log entry or NULL if not found
 */
const raft_log_entry_t* mesh_ordering_log_get(
    const mesh_ordering_service_t* service,
    uint64_t index
);

/**
 * @brief Get last log index
 *
 * @param service Ordering service
 * @return Last log index
 */
uint64_t mesh_ordering_log_last_index(const mesh_ordering_service_t* service);

/**
 * @brief Get last log term
 *
 * @param service Ordering service
 * @return Last log term
 */
uint64_t mesh_ordering_log_last_term(const mesh_ordering_service_t* service);

/**
 * @brief Get commit index
 *
 * @param service Ordering service
 * @return Commit index
 */
uint64_t mesh_ordering_get_commit_index(const mesh_ordering_service_t* service);

/**
 * @brief Truncate log after index (for conflict resolution)
 *
 * @param service Ordering service
 * @param index Index after which to truncate
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_ordering_log_truncate(
    mesh_ordering_service_t* service,
    uint64_t index
);

/**
 * @brief Compact log (remove applied entries)
 *
 * @param service Ordering service
 * @param up_to_index Remove entries up to this index
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_ordering_log_compact(
    mesh_ordering_service_t* service,
    uint64_t up_to_index
);

/* ============================================================================
 * Channel Management API
 * ============================================================================ */

/**
 * @brief Add channel to ordering service
 *
 * @param service Ordering service
 * @param channel Channel ID
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_ordering_add_channel(
    mesh_ordering_service_t* service,
    mesh_channel_id_t channel
);

/**
 * @brief Remove channel from ordering service
 *
 * @param service Ordering service
 * @param channel Channel ID
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_ordering_remove_channel(
    mesh_ordering_service_t* service,
    mesh_channel_id_t channel
);

/**
 * @brief Check if channel is being ordered
 *
 * @param service Ordering service
 * @param channel Channel ID
 * @return true if channel is ordered by this service
 */
bool mesh_ordering_has_channel(
    const mesh_ordering_service_t* service,
    mesh_channel_id_t channel
);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update ordering service
 *
 * WHAT: Perform periodic update
 * WHY:  Process batches, check timeouts, send heartbeats
 *
 * @param service Ordering service
 * @param delta_ms Time since last update
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_ordering_update(
    mesh_ordering_service_t* service,
    uint64_t delta_ms
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get ordering service statistics
 *
 * @param service Ordering service
 * @param stats Output: statistics
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_ordering_get_stats(
    const mesh_ordering_service_t* service,
    mesh_ordering_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param service Ordering service
 */
void mesh_ordering_reset_stats(mesh_ordering_service_t* service);

/**
 * @brief Get queue utilization (for backpressure monitoring)
 *
 * @param service Ordering service
 * @return Utilization ratio (0.0 - 1.0), or 0.0 on error
 */
float mesh_ordering_get_utilization(const mesh_ordering_service_t* service);

/**
 * @brief Check if backpressure is active (queue > 80% full)
 *
 * @param service Ordering service
 * @return true if backpressure threshold exceeded
 */
bool mesh_ordering_is_backpressure_active(const mesh_ordering_service_t* service);

/* ============================================================================
 * Block Management
 * ============================================================================ */

/**
 * @brief Destroy ordered block
 *
 * @param block Block to destroy (NULL-safe)
 */
void mesh_ordered_block_destroy(mesh_ordered_block_t* block);

/**
 * @brief Compute block hash
 *
 * @param block Block to hash
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_ordered_block_compute_hash(mesh_ordered_block_t* block);

/**
 * @brief Verify block hash
 *
 * @param block Block to verify
 * @return true if hash is valid
 */
bool mesh_ordered_block_verify_hash(const mesh_ordered_block_t* block);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get Raft role name
 *
 * @param role Raft role
 * @return Role name string
 */
const char* mesh_raft_role_to_string(raft_role_t role);

/**
 * @brief Print ordering service status
 *
 * @param service Ordering service
 */
void mesh_ordering_print_status(const mesh_ordering_service_t* service);

/**
 * @brief Print Raft state
 *
 * @param service Ordering service
 */
void mesh_ordering_print_raft_state(const mesh_ordering_service_t* service);

/**
 * @brief Print ordered block
 *
 * @param block Block to print
 */
void mesh_ordered_block_print(const mesh_ordered_block_t* block);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_ORDERING_H */
