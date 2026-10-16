/**
 * @file nimcp_byzantine_fault_tolerance.h
 * @brief Byzantine Fault Tolerance for Swarm Systems
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Detect and handle malicious or faulty nodes in distributed system
 * WHY:  Protect swarm from compromised nodes, ensure consensus integrity
 * HOW:  PBFT-style voting, cryptographic signatures, quorum validation
 *
 * BIOLOGICAL BASIS:
 * - Immune system T-cells (detect and destroy compromised cells)
 * - Blood-brain barrier (protect core systems from malicious agents)
 * - Quorum sensing (bacteria verify signals from multiple sources)
 * - Social insects (verify food sources through dance waggle consensus)
 *
 * THREAT MODEL:
 * - Byzantine nodes may lie, delay, or corrupt messages
 * - Up to f < n/3 nodes can be Byzantine (PBFT guarantee)
 * - Compromised nodes may collude
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BYZANTINE_FAULT_TOLERANCE_H
#define NIMCP_BYZANTINE_FAULT_TOLERANCE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define BFT_MAX_NODES 128               /**< Maximum nodes in BFT cluster */
#define BFT_MAX_MESSAGES 256            /**< Max pending messages */
#define BFT_SIGNATURE_SIZE 64           /**< Ed25519 signature size */
#define BFT_HASH_SIZE 32                /**< SHA-256 hash size */
#define BFT_PUBLIC_KEY_SIZE 32          /**< Ed25519 public key size */
#define BFT_PRIVATE_KEY_SIZE 64         /**< Ed25519 private key size */
#define BFT_MAX_EVIDENCE 32             /**< Max evidence items */
#define BFT_CHALLENGE_SIZE 32           /**< Challenge nonce size */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Byzantine behavior types
 */
typedef enum {
    BFT_BEHAV_NONE = 0,         /**< No Byzantine behavior */
    BFT_BEHAV_SILENT,           /**< Node not responding */
    BFT_BEHAV_EQUIVOCATION,     /**< Sending conflicting messages */
    BFT_BEHAV_INVALID_SIG,      /**< Invalid signatures */
    BFT_BEHAV_REPLAY,           /**< Replaying old messages */
    BFT_BEHAV_FABRICATION,      /**< Fabricating data */
    BFT_BEHAV_TIMING,           /**< Timing attacks */
    BFT_BEHAV_COLLUSION         /**< Coordinated attack */
} bft_behavior_t;

/**
 * @brief BFT message types (PBFT phases)
 */
typedef enum {
    BFT_MSG_REQUEST = 0,        /**< Client request */
    BFT_MSG_PRE_PREPARE,        /**< Pre-prepare (leader) */
    BFT_MSG_PREPARE,            /**< Prepare (replica) */
    BFT_MSG_COMMIT,             /**< Commit (replica) */
    BFT_MSG_REPLY,              /**< Reply to client */
    BFT_MSG_CHECKPOINT,         /**< Checkpoint */
    BFT_MSG_VIEW_CHANGE,        /**< View change request */
    BFT_MSG_NEW_VIEW,           /**< New view announcement */
    BFT_MSG_ACCUSATION,         /**< Byzantine accusation */
    BFT_MSG_DEFENSE             /**< Defense against accusation */
} bft_msg_type_t;

/**
 * @brief BFT node status
 */
typedef enum {
    BFT_STATUS_TRUSTED = 0,     /**< Fully trusted */
    BFT_STATUS_SUSPECTED,       /**< Under suspicion */
    BFT_STATUS_BYZANTINE,       /**< Confirmed Byzantine */
    BFT_STATUS_QUARANTINED,     /**< Isolated from network */
    BFT_STATUS_PROBATION        /**< On probation after recovery */
} bft_node_status_t;

/**
 * @brief Evidence types for accusation
 */
typedef enum {
    BFT_EVIDENCE_CONFLICTING_MSG = 0,  /**< Conflicting messages */
    BFT_EVIDENCE_INVALID_SIG,          /**< Invalid signature */
    BFT_EVIDENCE_INVALID_DATA,         /**< Invalid data */
    BFT_EVIDENCE_TIMING_VIOLATION,     /**< Timing violation */
    BFT_EVIDENCE_PROTOCOL_VIOLATION,   /**< Protocol violation */
    BFT_EVIDENCE_WITNESS              /**< Witness testimony */
} bft_evidence_type_t;

/**
 * @brief View change reasons
 */
typedef enum {
    BFT_VIEW_LEADER_TIMEOUT = 0,    /**< Leader not responding */
    BFT_VIEW_LEADER_BYZANTINE,      /**< Leader is Byzantine */
    BFT_VIEW_STUCK_CONSENSUS,       /**< Consensus not progressing */
    BFT_VIEW_MANUAL                 /**< Manual trigger */
} bft_view_reason_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Cryptographic identity
 */
typedef struct {
    uint32_t node_id;                       /**< Node identifier */
    uint8_t public_key[BFT_PUBLIC_KEY_SIZE]; /**< Ed25519 public key */
    uint8_t private_key[BFT_PRIVATE_KEY_SIZE]; /**< Ed25519 private key */
    bool has_private_key;                    /**< Has private key access */
} bft_identity_t;

/**
 * @brief BFT message header
 */
typedef struct {
    bft_msg_type_t type;            /**< Message type */
    uint32_t sender_id;             /**< Sender node ID */
    uint64_t view_number;           /**< Current view */
    uint64_t sequence_number;       /**< Sequence in view */
    uint64_t timestamp_ms;          /**< Message timestamp */
    uint8_t digest[BFT_HASH_SIZE];  /**< Content digest */
    uint8_t signature[BFT_SIGNATURE_SIZE]; /**< Ed25519 signature */
} bft_msg_header_t;

/**
 * @brief Evidence item for Byzantine accusation
 */
typedef struct {
    bft_evidence_type_t type;       /**< Evidence type */
    uint32_t accused_node_id;       /**< Node being accused */
    uint64_t timestamp_ms;          /**< Evidence timestamp */
    uint8_t data[256];              /**< Evidence data */
    size_t data_size;               /**< Data size */
    uint32_t witness_ids[8];        /**< Witness nodes */
    uint32_t witness_count;         /**< Number of witnesses */
} bft_evidence_t;

/**
 * @brief Byzantine accusation
 */
typedef struct {
    uint32_t accuser_id;            /**< Accusing node */
    uint32_t accused_id;            /**< Accused node */
    bft_behavior_t behavior;        /**< Observed behavior */
    bft_evidence_t evidence[BFT_MAX_EVIDENCE]; /**< Evidence items */
    uint32_t evidence_count;        /**< Evidence count */
    uint64_t accusation_time_ms;    /**< Accusation timestamp */
    uint8_t signature[BFT_SIGNATURE_SIZE]; /**< Accuser signature */
    uint32_t supporting_nodes[BFT_MAX_NODES]; /**< Supporting votes */
    uint32_t support_count;         /**< Support count */
} bft_accusation_t;

/**
 * @brief Node trust score
 */
typedef struct {
    uint32_t node_id;               /**< Node identifier */
    bft_node_status_t status;       /**< Current status */
    float trust_score;              /**< Trust score (0-100) */
    uint32_t total_votes;           /**< Total voting participation */
    uint32_t correct_votes;         /**< Votes matching consensus */
    uint32_t accusations_received;  /**< Accusations against node */
    uint32_t accusations_made;      /**< Accusations made by node */
    uint32_t false_accusations;     /**< False accusations made */
    uint64_t last_activity_ms;      /**< Last activity timestamp */
    uint64_t quarantine_until_ms;   /**< Quarantine end time */
} bft_trust_info_t;

/**
 * @brief Immune state snapshot for checkpoints
 *
 * WHAT: Immune system state at checkpoint time
 * WHY:  Include immune health in fault tolerance checkpoints
 * HOW:  Snapshot of key immune metrics
 */
typedef struct {
    uint32_t active_antigens;           /**< Active threats */
    uint32_t active_antibodies;         /**< Active responses */
    uint32_t memory_cells;              /**< Immune memory */
    uint32_t inflammation_sites;        /**< Active inflammation */
    float system_health;                /**< Overall health (0-1) */
    uint8_t immune_phase;               /**< Current immune phase */
} bft_immune_state_t;

/**
 * @brief Checkpoint for BFT state
 */
typedef struct {
    uint64_t sequence_number;       /**< Last committed sequence */
    uint64_t view_number;           /**< Current view */
    uint8_t state_hash[BFT_HASH_SIZE]; /**< State hash */
    uint32_t signatures_count;      /**< Number of signatures */
    uint8_t signatures[BFT_MAX_NODES][BFT_SIGNATURE_SIZE]; /**< Node sigs */
    uint32_t signers[BFT_MAX_NODES]; /**< Signing nodes */
    bft_immune_state_t immune_state; /**< Immune system state */
} bft_checkpoint_t;

/**
 * @brief Configuration for BFT
 */
typedef struct {
    uint32_t node_id;               /**< This node's ID */
    uint32_t total_nodes;           /**< Total nodes (n) */
    uint32_t max_byzantine;         /**< Max Byzantine nodes (f) */
    uint64_t view_timeout_ms;       /**< View change timeout */
    uint64_t message_timeout_ms;    /**< Message timeout */
    uint32_t checkpoint_interval;   /**< Sequences per checkpoint */
    float initial_trust;            /**< Initial trust score */
    float trust_decay;              /**< Trust decay per violation */
    float trust_recovery;           /**< Trust recovery per correct action */
    float quarantine_threshold;     /**< Trust below this = quarantine */
    uint64_t quarantine_duration_ms; /**< Quarantine duration */
    bool enable_signatures;         /**< Enable cryptographic signatures */
    bool enable_trust_scoring;      /**< Enable trust scoring */
} bft_config_t;

/**
 * @brief Statistics for BFT
 */
typedef struct {
    uint64_t total_messages;
    uint64_t total_consensus_rounds;
    uint64_t successful_consensus;
    uint64_t failed_consensus;
    uint64_t view_changes;
    uint64_t byzantine_detected;
    uint64_t accusations_processed;
    uint64_t false_accusations;
    uint64_t nodes_quarantined;
    uint64_t avg_consensus_time_ms;
    float cluster_trust_score;
} bft_stats_t;

/**
 * @brief Consensus callback
 */
typedef void (*bft_consensus_callback_t)(
    uint64_t sequence,
    const void* data,
    size_t data_size,
    void* user_data
);

/**
 * @brief Byzantine detection callback
 */
typedef void (*bft_byzantine_callback_t)(
    uint32_t node_id,
    bft_behavior_t behavior,
    const bft_evidence_t* evidence,
    void* user_data
);

/**
 * @brief Accusation event callback
 *
 * WHAT: Callback when Byzantine accusation is raised
 * WHY:  Enable immune system to react to accusations
 * HOW:  Called before accusation is processed
 */
typedef void (*bft_accusation_callback_t)(
    uint32_t accuser_id,
    uint32_t accused_id,
    bft_behavior_t behavior,
    const bft_evidence_t* evidence,
    uint32_t evidence_count,
    void* user_data
);

/**
 * @brief Quarantine action callback
 *
 * WHAT: Callback when node is quarantined
 * WHY:  Enable immune system to coordinate quarantine
 * HOW:  Called when BFT quarantines a node
 */
typedef void (*bft_quarantine_callback_t)(
    uint32_t node_id,
    uint64_t duration_ms,
    float trust_score,
    void* user_data
);

/**
 * @brief Trust recovery callback
 *
 * WHAT: Callback when node trust is restored
 * WHY:  Enable immune memory formation
 * HOW:  Called when node exits probation
 */
typedef void (*bft_trust_recovery_callback_t)(
    uint32_t node_id,
    float old_trust,
    float new_trust,
    void* user_data
);

/**
 * @brief Opaque BFT context handle
 */
typedef struct bft_context bft_context_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create BFT context
 *
 * WHAT: Initialize Byzantine fault tolerance
 * WHY:  Required before any BFT operations
 * HOW:  Generate keys, initialize state machine
 *
 * @param config Configuration
 * @return BFT context or NULL on failure
 */
bft_context_t* bft_create(const bft_config_t* config);

/**
 * @brief Destroy BFT context
 *
 * @param ctx BFT context
 */
void bft_destroy(bft_context_t* ctx);

/**
 * @brief Get default configuration
 *
 * @return Default configuration
 */
bft_config_t bft_default_config(void);

/**
 * @brief Start BFT protocol
 *
 * @param ctx BFT context
 * @return true on success
 */
bool bft_start(bft_context_t* ctx);

/**
 * @brief Stop BFT protocol
 *
 * @param ctx BFT context
 * @return true on success
 */
bool bft_stop(bft_context_t* ctx);

//=============================================================================
// Key Management
//=============================================================================

/**
 * @brief Generate new keypair
 *
 * @param identity Output identity with keys
 * @return true on success
 */
bool bft_generate_keys(bft_identity_t* identity);

/**
 * @brief Set this node's identity
 *
 * @param ctx BFT context
 * @param identity Node identity
 * @return true on success
 */
bool bft_set_identity(bft_context_t* ctx, const bft_identity_t* identity);

/**
 * @brief Register peer's public key
 *
 * @param ctx BFT context
 * @param node_id Peer node ID
 * @param public_key Peer's public key
 * @return true on success
 */
bool bft_register_peer_key(
    bft_context_t* ctx,
    uint32_t node_id,
    const uint8_t* public_key
);

//=============================================================================
// Consensus Protocol
//=============================================================================

/**
 * @brief Submit request for consensus
 *
 * WHAT: Submit data for BFT consensus
 * WHY:  Achieve agreement across nodes
 * HOW:  Initiate PBFT protocol
 *
 * @param ctx BFT context
 * @param data Request data
 * @param data_size Data size
 * @param sequence Output: assigned sequence number
 * @return true if request accepted
 */
bool bft_submit_request(
    bft_context_t* ctx,
    const void* data,
    size_t data_size,
    uint64_t* sequence
);

/**
 * @brief Process received BFT message
 *
 * @param ctx BFT context
 * @param header Message header
 * @param payload Message payload
 * @param payload_size Payload size
 * @return true on success
 */
bool bft_process_message(
    bft_context_t* ctx,
    const bft_msg_header_t* header,
    const void* payload,
    size_t payload_size
);

/**
 * @brief Check if consensus reached
 *
 * @param ctx BFT context
 * @param sequence Sequence to check
 * @return true if consensus reached
 */
bool bft_is_consensus_reached(bft_context_t* ctx, uint64_t sequence);

/**
 * @brief Get consensus result
 *
 * @param ctx BFT context
 * @param sequence Sequence number
 * @param data_buffer Output buffer
 * @param buffer_size Buffer capacity
 * @param actual_size Actual data size
 * @return true if consensus data available
 */
bool bft_get_consensus_result(
    bft_context_t* ctx,
    uint64_t sequence,
    void* data_buffer,
    size_t buffer_size,
    size_t* actual_size
);

//=============================================================================
// Byzantine Detection
//=============================================================================

/**
 * @brief Verify message signature
 *
 * @param ctx BFT context
 * @param header Message header
 * @param payload Message payload
 * @param payload_size Payload size
 * @return true if signature valid
 */
bool bft_verify_signature(
    bft_context_t* ctx,
    const bft_msg_header_t* header,
    const void* payload,
    size_t payload_size
);

/**
 * @brief Report Byzantine behavior
 *
 * @param ctx BFT context
 * @param accused_id Node being accused
 * @param behavior Observed behavior
 * @param evidence Evidence array
 * @param evidence_count Number of evidence items
 * @return true if accusation submitted
 */
bool bft_report_byzantine(
    bft_context_t* ctx,
    uint32_t accused_id,
    bft_behavior_t behavior,
    const bft_evidence_t* evidence,
    uint32_t evidence_count
);

/**
 * @brief Process accusation
 *
 * @param ctx BFT context
 * @param accusation Accusation to process
 * @return true if accusation valid
 */
bool bft_process_accusation(bft_context_t* ctx, const bft_accusation_t* accusation);

/**
 * @brief Vote on accusation
 *
 * @param ctx BFT context
 * @param accusation Accusation to vote on
 * @param support true to support accusation
 * @return true on success
 */
bool bft_vote_accusation(
    bft_context_t* ctx,
    const bft_accusation_t* accusation,
    bool support
);

/**
 * @brief Check for equivocation
 *
 * @param ctx BFT context
 * @param msg1 First message
 * @param msg2 Second message
 * @return true if equivocation detected
 */
bool bft_check_equivocation(
    bft_context_t* ctx,
    const bft_msg_header_t* msg1,
    const bft_msg_header_t* msg2
);

//=============================================================================
// Trust Management
//=============================================================================

/**
 * @brief Get node trust info
 *
 * @param ctx BFT context
 * @param node_id Node to query
 * @param trust Output trust info
 * @return true if found
 */
bool bft_get_trust_info(bft_context_t* ctx, uint32_t node_id, bft_trust_info_t* trust);

/**
 * @brief Update trust score
 *
 * @param ctx BFT context
 * @param node_id Node to update
 * @param correct_behavior true if behavior was correct
 * @return New trust score
 */
float bft_update_trust(bft_context_t* ctx, uint32_t node_id, bool correct_behavior);

/**
 * @brief Quarantine node
 *
 * @param ctx BFT context
 * @param node_id Node to quarantine
 * @param duration_ms Quarantine duration
 * @return true on success
 */
bool bft_quarantine_node(bft_context_t* ctx, uint32_t node_id, uint64_t duration_ms);

/**
 * @brief Check if node is quarantined
 *
 * @param ctx BFT context
 * @param node_id Node to check
 * @return true if quarantined
 */
bool bft_is_quarantined(bft_context_t* ctx, uint32_t node_id);

/**
 * @brief Release node from quarantine
 *
 * @param ctx BFT context
 * @param node_id Node to release
 * @return true on success
 */
bool bft_release_quarantine(bft_context_t* ctx, uint32_t node_id);

//=============================================================================
// View Change
//=============================================================================

/**
 * @brief Request view change
 *
 * @param ctx BFT context
 * @param reason Reason for view change
 * @return true if request submitted
 */
bool bft_request_view_change(bft_context_t* ctx, bft_view_reason_t reason);

/**
 * @brief Get current view number
 *
 * @param ctx BFT context
 * @return Current view number
 */
uint64_t bft_get_view(bft_context_t* ctx);

/**
 * @brief Get current leader ID
 *
 * @param ctx BFT context
 * @return Leader node ID
 */
uint32_t bft_get_leader(bft_context_t* ctx);

/**
 * @brief Check if this node is leader
 *
 * @param ctx BFT context
 * @return true if this node is leader
 */
bool bft_is_leader(bft_context_t* ctx);

//=============================================================================
// Checkpoints
//=============================================================================

/**
 * @brief Create checkpoint
 *
 * @param ctx BFT context
 * @param state_hash Hash of current state
 * @return true if checkpoint created
 */
bool bft_create_checkpoint(bft_context_t* ctx, const uint8_t* state_hash);

/**
 * @brief Get stable checkpoint
 *
 * @param ctx BFT context
 * @param checkpoint Output checkpoint
 * @return true if stable checkpoint exists
 */
bool bft_get_stable_checkpoint(bft_context_t* ctx, bft_checkpoint_t* checkpoint);

/**
 * @brief Verify checkpoint
 *
 * @param ctx BFT context
 * @param checkpoint Checkpoint to verify
 * @return true if checkpoint valid
 */
bool bft_verify_checkpoint(bft_context_t* ctx, const bft_checkpoint_t* checkpoint);

//=============================================================================
// Callbacks
//=============================================================================

/**
 * @brief Register consensus callback
 *
 * @param ctx BFT context
 * @param callback Consensus callback
 * @param user_data User data
 * @return true on success
 */
bool bft_register_consensus_callback(
    bft_context_t* ctx,
    bft_consensus_callback_t callback,
    void* user_data
);

/**
 * @brief Register Byzantine detection callback
 *
 * @param ctx BFT context
 * @param callback Detection callback
 * @param user_data User data
 * @return true on success
 */
bool bft_register_byzantine_callback(
    bft_context_t* ctx,
    bft_byzantine_callback_t callback,
    void* user_data
);

/**
 * @brief Register accusation callback
 *
 * WHAT: Register callback for accusation events
 * WHY:  Enable immune system antigen presentation
 * HOW:  Store callback, invoke on accusations
 *
 * @param ctx BFT context
 * @param callback Accusation callback
 * @param user_data User data
 * @return true on success
 */
bool bft_register_accusation_callback(
    bft_context_t* ctx,
    bft_accusation_callback_t callback,
    void* user_data
);

/**
 * @brief Register quarantine callback
 *
 * WHAT: Register callback for quarantine actions
 * WHY:  Enable immune system killer T cell coordination
 * HOW:  Store callback, invoke on quarantine
 *
 * @param ctx BFT context
 * @param callback Quarantine callback
 * @param user_data User data
 * @return true on success
 */
bool bft_register_quarantine_callback(
    bft_context_t* ctx,
    bft_quarantine_callback_t callback,
    void* user_data
);

/**
 * @brief Register trust recovery callback
 *
 * WHAT: Register callback for trust restoration
 * WHY:  Enable immune memory formation
 * HOW:  Store callback, invoke on trust recovery
 *
 * @param ctx BFT context
 * @param callback Trust recovery callback
 * @param user_data User data
 * @return true on success
 */
bool bft_register_trust_recovery_callback(
    bft_context_t* ctx,
    bft_trust_recovery_callback_t callback,
    void* user_data
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get BFT statistics
 *
 * @param ctx BFT context
 * @param stats Output statistics
 * @return true on success
 */
bool bft_get_stats(bft_context_t* ctx, bft_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param ctx BFT context
 */
void bft_reset_stats(bft_context_t* ctx);

/**
 * @brief Check cluster health
 *
 * @param ctx BFT context
 * @return true if cluster is healthy
 */
bool bft_is_cluster_healthy(bft_context_t* ctx);

//=============================================================================
// Cryptographic Utilities
//=============================================================================

/**
 * @brief Sign data
 *
 * @param ctx BFT context
 * @param data Data to sign
 * @param data_size Data size
 * @param signature Output signature
 * @return true on success
 */
bool bft_sign(
    bft_context_t* ctx,
    const void* data,
    size_t data_size,
    uint8_t* signature
);

/**
 * @brief Verify signature with public key
 *
 * @param public_key Public key
 * @param data Signed data
 * @param data_size Data size
 * @param signature Signature to verify
 * @return true if valid
 */
bool bft_verify(
    const uint8_t* public_key,
    const void* data,
    size_t data_size,
    const uint8_t* signature
);

/**
 * @brief Compute hash
 *
 * @param data Data to hash
 * @param data_size Data size
 * @param hash Output hash
 */
void bft_hash(const void* data, size_t data_size, uint8_t* hash);

//=============================================================================
// String Conversion
//=============================================================================

const char* bft_behavior_to_string(bft_behavior_t behavior);
const char* bft_msg_type_to_string(bft_msg_type_t type);
const char* bft_status_to_string(bft_node_status_t status);
const char* bft_evidence_type_to_string(bft_evidence_type_t type);
const char* bft_view_reason_to_string(bft_view_reason_t reason);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BYZANTINE_FAULT_TOLERANCE_H
