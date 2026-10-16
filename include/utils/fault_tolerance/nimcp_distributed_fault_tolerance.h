/**
 * @file nimcp_distributed_fault_tolerance.h
 * @brief Distributed Fault Tolerance for Swarm Systems
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Cross-drone failure coordination with consensus-based recovery
 * WHY:  Enable resilient swarm operation even when individual nodes fail
 * HOW:  Distributed checkpoints, failure detection, coordinated recovery
 *
 * BIOLOGICAL BASIS:
 * - Distributed immune system (each node monitors neighbors like lymph nodes)
 * - Quorum sensing (bacteria coordinate responses via chemical signals)
 * - Neural redundancy (brain compensates for damaged regions via plasticity)
 *
 * INTEGRATION POINTS:
 * 1. Swarm Module (src/swarm/)
 *    - Integrate with swarm consensus for failure voting
 *    - Use swarm signal for checkpoint distribution
 * 2. Bio-Async (src/async/nimcp_bio_async.c)
 *    - Async checkpoint replication via neuromodulator channels
 * 3. Security (src/security/)
 *    - BBB validation for checkpoint integrity
 *    - Encrypted checkpoint transmission
 * 4. Logging (utils/logging/)
 *    - Audit log for all failure events
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_DISTRIBUTED_FAULT_TOLERANCE_H
#define NIMCP_DISTRIBUTED_FAULT_TOLERANCE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define DFT_MAX_PEERS 64                    /**< Maximum swarm peers */
#define DFT_MAX_CHECKPOINTS 16              /**< Max checkpoint replicas */
#define DFT_CHECKPOINT_MAGIC 0x44465443     /**< 'DFTC' checkpoint magic */
#define DFT_HEARTBEAT_INTERVAL_MS 100       /**< Heartbeat interval */
#define DFT_FAILURE_TIMEOUT_MS 500          /**< Failure detection timeout */
#define DFT_QUORUM_THRESHOLD 0.67f          /**< 2/3 majority for consensus */
#define DFT_MAX_RECOVERY_ATTEMPTS 3         /**< Max recovery retries */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Distributed node health states
 */
typedef enum {
    DFT_NODE_HEALTHY = 0,       /**< Node operating normally */
    DFT_NODE_SUSPECTED,         /**< Missed heartbeats, under investigation */
    DFT_NODE_FAILED,            /**< Confirmed failure */
    DFT_NODE_RECOVERING,        /**< Recovery in progress */
    DFT_NODE_QUARANTINED,       /**< Byzantine behavior detected */
    DFT_NODE_OFFLINE            /**< Gracefully shutdown */
} dft_node_state_t;

/**
 * @brief Failure detection methods
 */
typedef enum {
    DFT_DETECT_HEARTBEAT = 0,   /**< Heartbeat timeout */
    DFT_DETECT_PING,            /**< Request-response probe */
    DFT_DETECT_ACCRUAL,         /**< Phi accrual failure detector */
    DFT_DETECT_CONSENSUS        /**< Multi-node agreement */
} dft_detection_method_t;

/**
 * @brief Recovery coordination modes
 */
typedef enum {
    DFT_RECOVERY_LOCAL = 0,     /**< Node recovers itself */
    DFT_RECOVERY_PEER,          /**< Neighbor assists recovery */
    DFT_RECOVERY_LEADER,        /**< Leader coordinates recovery */
    DFT_RECOVERY_CONSENSUS      /**< Quorum-based recovery decision */
} dft_recovery_mode_t;

/**
 * @brief Checkpoint replication strategy
 */
typedef enum {
    DFT_REPLICATE_NONE = 0,     /**< No replication (local only) */
    DFT_REPLICATE_SYNC,         /**< Synchronous to N peers */
    DFT_REPLICATE_ASYNC,        /**< Asynchronous replication */
    DFT_REPLICATE_CHAIN         /**< Chain replication for ordering */
} dft_replication_strategy_t;

/**
 * @brief Distributed event types
 */
typedef enum {
    DFT_EVENT_NODE_JOINED = 0x5000,
    DFT_EVENT_NODE_LEFT,
    DFT_EVENT_NODE_SUSPECTED,
    DFT_EVENT_NODE_FAILED,
    DFT_EVENT_NODE_RECOVERED,
    DFT_EVENT_CHECKPOINT_CREATED,
    DFT_EVENT_CHECKPOINT_REPLICATED,
    DFT_EVENT_RECOVERY_STARTED,
    DFT_EVENT_RECOVERY_COMPLETE,
    DFT_EVENT_QUORUM_LOST,
    DFT_EVENT_QUORUM_RESTORED,
    DFT_EVENT_LEADER_ELECTED,
    DFT_EVENT_BYZANTINE_DETECTED
} dft_event_type_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Peer node information
 */
typedef struct {
    uint32_t node_id;               /**< Unique node identifier */
    dft_node_state_t state;         /**< Current node state */
    uint64_t last_heartbeat_ms;     /**< Last heartbeat timestamp */
    uint64_t last_checkpoint_id;    /**< Latest checkpoint version */
    float health_score;             /**< Node health (0-100) */
    uint32_t failure_count;         /**< Historical failure count */
    uint32_t suspected_by_count;    /**< Nodes suspecting this node */
    bool is_leader;                 /**< Is this node the leader */
    void* user_data;                /**< User-defined data */
} dft_peer_info_t;

/**
 * @brief Distributed checkpoint metadata
 */
typedef struct {
    uint32_t magic;                 /**< Magic number for validation */
    uint64_t checkpoint_id;         /**< Unique checkpoint identifier */
    uint32_t source_node_id;        /**< Node that created checkpoint */
    uint32_t replica_count;         /**< Number of replicas */
    uint32_t replica_nodes[DFT_MAX_CHECKPOINTS]; /**< Nodes holding replicas */
    uint64_t created_at_ms;         /**< Creation timestamp */
    uint64_t expires_at_ms;         /**< Expiration timestamp */
    size_t data_size;               /**< Checkpoint data size */
    uint32_t crc32;                 /**< Data integrity checksum */
    uint8_t signature[64];          /**< Cryptographic signature */
    bool is_valid;                  /**< Validation status */
} dft_checkpoint_meta_t;

/**
 * @brief Recovery vote from a peer
 */
typedef struct {
    uint32_t voter_node_id;         /**< Node casting vote */
    uint32_t target_node_id;        /**< Node being voted on */
    bool vote_for_recovery;         /**< true = recover, false = wait */
    uint64_t checkpoint_id;         /**< Suggested checkpoint to restore */
    uint64_t vote_timestamp_ms;     /**< Vote timestamp */
    uint8_t signature[64];          /**< Vote signature */
} dft_recovery_vote_t;

/**
 * @brief Failure detection result
 */
typedef struct {
    uint32_t node_id;               /**< Detected failed node */
    dft_detection_method_t method;  /**< Detection method used */
    float confidence;               /**< Confidence in failure (0-1) */
    uint64_t detected_at_ms;        /**< Detection timestamp */
    uint32_t witnesses[DFT_MAX_PEERS]; /**< Nodes that witnessed failure */
    uint32_t witness_count;         /**< Number of witnesses */
    char reason[256];               /**< Failure reason */
} dft_failure_detection_t;

/**
 * @brief Configuration for distributed fault tolerance
 */
typedef struct {
    uint32_t node_id;               /**< This node's ID */
    uint32_t heartbeat_interval_ms; /**< Heartbeat interval */
    uint32_t failure_timeout_ms;    /**< Failure detection timeout */
    float quorum_threshold;         /**< Quorum percentage (0-1) */
    dft_detection_method_t detection_method; /**< Failure detection method */
    dft_recovery_mode_t recovery_mode;  /**< Recovery coordination mode */
    dft_replication_strategy_t replication; /**< Checkpoint replication */
    uint32_t min_replicas;          /**< Minimum checkpoint replicas */
    uint32_t max_recovery_attempts; /**< Max recovery retries */
    bool enable_byzantine_detection; /**< Enable Byzantine detection */
    bool enable_auto_recovery;      /**< Auto-recover on failure */
    bool enable_bio_async;          /**< Use bio-async for messaging */
    bool enable_encryption;         /**< Encrypt checkpoints */
} dft_config_t;

/**
 * @brief Statistics for distributed fault tolerance
 */
typedef struct {
    uint64_t total_heartbeats_sent;
    uint64_t total_heartbeats_received;
    uint64_t total_failures_detected;
    uint64_t total_recoveries_completed;
    uint64_t total_checkpoints_created;
    uint64_t total_checkpoints_replicated;
    uint64_t total_byzantine_detected;
    uint64_t total_quorum_losses;
    uint64_t avg_detection_time_ms;
    uint64_t avg_recovery_time_ms;
    float current_availability;     /**< System availability (0-1) */
} dft_stats_t;

/**
 * @brief Callback for distributed events
 */
typedef void (*dft_event_callback_t)(
    dft_event_type_t event,
    const void* event_data,
    void* user_data
);

/**
 * @brief Opaque distributed fault tolerance handle
 */
typedef struct dft_context dft_context_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create distributed fault tolerance context
 *
 * WHAT: Initialize DFT system with configuration
 * WHY:  Required before any distributed operations
 * HOW:  Allocate context, initialize peers, start heartbeat
 *
 * @param config Configuration (non-NULL)
 * @return DFT context or NULL on failure
 */
dft_context_t* dft_create(const dft_config_t* config);

/**
 * @brief Destroy distributed fault tolerance context
 *
 * WHAT: Cleanup and free all resources
 * WHY:  Prevent memory leaks on shutdown
 * HOW:  Stop heartbeat, notify peers, free memory
 *
 * @param ctx DFT context (NULL safe)
 */
void dft_destroy(dft_context_t* ctx);

/**
 * @brief Get default configuration
 *
 * @return Default configuration struct
 */
dft_config_t dft_default_config(void);

/**
 * @brief Start distributed fault tolerance
 *
 * WHAT: Begin failure detection and recovery
 * WHY:  Activate DFT after configuration
 * HOW:  Start heartbeat thread, register with peers
 *
 * @param ctx DFT context
 * @return true on success
 */
bool dft_start(dft_context_t* ctx);

/**
 * @brief Stop distributed fault tolerance
 *
 * WHAT: Gracefully shutdown DFT
 * WHY:  Clean shutdown before destroy
 * HOW:  Stop heartbeat, notify peers of leaving
 *
 * @param ctx DFT context
 * @return true on success
 */
bool dft_stop(dft_context_t* ctx);

//=============================================================================
// Peer Management
//=============================================================================

/**
 * @brief Add peer to the DFT cluster
 *
 * WHAT: Register a new peer node
 * WHY:  Expand fault tolerance group
 * HOW:  Add to peer list, initiate heartbeat
 *
 * @param ctx DFT context
 * @param node_id Peer's node ID
 * @param user_data Optional user data
 * @return true on success
 */
bool dft_add_peer(dft_context_t* ctx, uint32_t node_id, void* user_data);

/**
 * @brief Remove peer from the DFT cluster
 *
 * WHAT: Unregister a peer node
 * WHY:  Handle graceful peer departure
 * HOW:  Remove from peer list, redistribute checkpoints
 *
 * @param ctx DFT context
 * @param node_id Peer's node ID
 * @return true on success
 */
bool dft_remove_peer(dft_context_t* ctx, uint32_t node_id);

/**
 * @brief Get peer information
 *
 * @param ctx DFT context
 * @param node_id Peer's node ID
 * @param info Output peer info
 * @return true if found
 */
bool dft_get_peer_info(dft_context_t* ctx, uint32_t node_id, dft_peer_info_t* info);

/**
 * @brief Get all active peers
 *
 * @param ctx DFT context
 * @param peers Output array (caller provides)
 * @param max_peers Array capacity
 * @return Number of peers copied
 */
uint32_t dft_get_all_peers(dft_context_t* ctx, dft_peer_info_t* peers, uint32_t max_peers);

/**
 * @brief Get current peer count
 *
 * @param ctx DFT context
 * @return Number of active peers
 */
uint32_t dft_get_peer_count(dft_context_t* ctx);

//=============================================================================
// Heartbeat and Failure Detection
//=============================================================================

/**
 * @brief Send heartbeat to peers
 *
 * WHAT: Broadcast heartbeat to all peers
 * WHY:  Signal liveness
 * HOW:  Send via bio-async or direct messaging
 *
 * @param ctx DFT context
 * @return Number of peers reached
 */
uint32_t dft_send_heartbeat(dft_context_t* ctx);

/**
 * @brief Process received heartbeat
 *
 * WHAT: Handle heartbeat from peer
 * WHY:  Update peer liveness
 * HOW:  Update last_heartbeat timestamp
 *
 * @param ctx DFT context
 * @param node_id Sending node
 * @param health_score Peer's reported health
 * @return true on success
 */
bool dft_receive_heartbeat(dft_context_t* ctx, uint32_t node_id, float health_score);

/**
 * @brief Check for failed nodes
 *
 * WHAT: Detect nodes that missed heartbeat deadline
 * WHY:  Identify potential failures
 * HOW:  Compare last heartbeat to timeout threshold
 *
 * @param ctx DFT context
 * @param failures Output array for detected failures
 * @param max_failures Array capacity
 * @return Number of failures detected
 */
uint32_t dft_detect_failures(
    dft_context_t* ctx,
    dft_failure_detection_t* failures,
    uint32_t max_failures
);

/**
 * @brief Report suspected failure
 *
 * WHAT: Report a node suspected of failure
 * WHY:  Contribute to consensus failure detection
 * HOW:  Record suspicion, broadcast to peers
 *
 * @param ctx DFT context
 * @param node_id Suspected node
 * @param reason Suspicion reason
 * @return true on success
 */
bool dft_report_suspected_failure(
    dft_context_t* ctx,
    uint32_t node_id,
    const char* reason
);

//=============================================================================
// Checkpoint Management
//=============================================================================

/**
 * @brief Create distributed checkpoint
 *
 * WHAT: Create checkpoint and replicate to peers
 * WHY:  Enable recovery from any peer
 * HOW:  Serialize state, sign, replicate
 *
 * @param ctx DFT context
 * @param data Checkpoint data
 * @param data_size Data size
 * @param meta Output checkpoint metadata
 * @return true on success
 */
bool dft_create_checkpoint(
    dft_context_t* ctx,
    const void* data,
    size_t data_size,
    dft_checkpoint_meta_t* meta
);

/**
 * @brief Retrieve checkpoint from cluster
 *
 * WHAT: Fetch checkpoint data from peers
 * WHY:  Recover state after failure
 * HOW:  Query peers, select best checkpoint
 *
 * @param ctx DFT context
 * @param checkpoint_id Checkpoint to retrieve
 * @param data_buffer Output buffer
 * @param buffer_size Buffer capacity
 * @param actual_size Actual data size
 * @return true on success
 */
bool dft_retrieve_checkpoint(
    dft_context_t* ctx,
    uint64_t checkpoint_id,
    void* data_buffer,
    size_t buffer_size,
    size_t* actual_size
);

/**
 * @brief Get latest checkpoint ID
 *
 * @param ctx DFT context
 * @return Latest checkpoint ID, 0 if none
 */
uint64_t dft_get_latest_checkpoint_id(dft_context_t* ctx);

/**
 * @brief List available checkpoints
 *
 * @param ctx DFT context
 * @param metas Output array
 * @param max_count Array capacity
 * @return Number of checkpoints
 */
uint32_t dft_list_checkpoints(
    dft_context_t* ctx,
    dft_checkpoint_meta_t* metas,
    uint32_t max_count
);

//=============================================================================
// Recovery Coordination
//=============================================================================

/**
 * @brief Initiate coordinated recovery
 *
 * WHAT: Start recovery for a failed node
 * WHY:  Restore system to healthy state
 * HOW:  Coordinate with peers, select checkpoint, restore
 *
 * @param ctx DFT context
 * @param node_id Failed node ID
 * @return true if recovery initiated
 */
bool dft_initiate_recovery(dft_context_t* ctx, uint32_t node_id);

/**
 * @brief Vote on recovery decision
 *
 * WHAT: Cast vote for recovery action
 * WHY:  Participate in consensus
 * HOW:  Sign vote, broadcast to peers
 *
 * @param ctx DFT context
 * @param vote Recovery vote
 * @return true on success
 */
bool dft_vote_recovery(dft_context_t* ctx, const dft_recovery_vote_t* vote);

/**
 * @brief Check recovery consensus
 *
 * WHAT: Check if quorum reached for recovery
 * WHY:  Decide if recovery should proceed
 * HOW:  Count votes, compare to threshold
 *
 * @param ctx DFT context
 * @param node_id Node under recovery consideration
 * @param quorum_reached Output: true if quorum reached
 * @param checkpoint_id Output: consensus checkpoint
 * @return true on success
 */
bool dft_check_recovery_consensus(
    dft_context_t* ctx,
    uint32_t node_id,
    bool* quorum_reached,
    uint64_t* checkpoint_id
);

/**
 * @brief Complete recovery for a node
 *
 * WHAT: Finalize recovery process
 * WHY:  Mark node as recovered
 * HOW:  Update state, notify peers
 *
 * @param ctx DFT context
 * @param node_id Recovered node
 * @param success Recovery success status
 * @return true on success
 */
bool dft_complete_recovery(dft_context_t* ctx, uint32_t node_id, bool success);

//=============================================================================
// Quorum and Leader Election
//=============================================================================

/**
 * @brief Check if quorum is available
 *
 * @param ctx DFT context
 * @return true if quorum available
 */
bool dft_has_quorum(dft_context_t* ctx);

/**
 * @brief Get current leader node
 *
 * @param ctx DFT context
 * @return Leader node ID, 0 if no leader
 */
uint32_t dft_get_leader(dft_context_t* ctx);

/**
 * @brief Trigger leader election
 *
 * @param ctx DFT context
 * @return true if election started
 */
bool dft_trigger_election(dft_context_t* ctx);

//=============================================================================
// Events and Callbacks
//=============================================================================

/**
 * @brief Register event callback
 *
 * @param ctx DFT context
 * @param callback Callback function
 * @param user_data User data for callback
 * @return true on success
 */
bool dft_register_callback(
    dft_context_t* ctx,
    dft_event_callback_t callback,
    void* user_data
);

/**
 * @brief Unregister event callback
 *
 * @param ctx DFT context
 * @param callback Callback to remove
 * @return true on success
 */
bool dft_unregister_callback(dft_context_t* ctx, dft_event_callback_t callback);

//=============================================================================
// Statistics and Diagnostics
//=============================================================================

/**
 * @brief Get DFT statistics
 *
 * @param ctx DFT context
 * @param stats Output statistics
 * @return true on success
 */
bool dft_get_stats(dft_context_t* ctx, dft_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param ctx DFT context
 */
void dft_reset_stats(dft_context_t* ctx);

/**
 * @brief Get cluster health score
 *
 * @param ctx DFT context
 * @return Cluster health (0-100)
 */
float dft_get_cluster_health(dft_context_t* ctx);

//=============================================================================
// String Conversion Utilities
//=============================================================================

const char* dft_node_state_to_string(dft_node_state_t state);
const char* dft_detection_method_to_string(dft_detection_method_t method);
const char* dft_recovery_mode_to_string(dft_recovery_mode_t mode);
const char* dft_event_type_to_string(dft_event_type_t event);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_DISTRIBUTED_FAULT_TOLERANCE_H
