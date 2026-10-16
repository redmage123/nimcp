/**
 * @file nimcp_mesh_transaction.h
 * @brief Mesh Network Transaction Lifecycle
 *
 * WHAT: Transaction structure and Execute-Order-Validate flow
 * WHY:  Hyperledger-inspired transaction processing for distributed consensus
 * HOW:  Proposal -> Endorsement -> Ordering -> Validation -> Commit
 *
 * TRANSACTION FLOW:
 * ```
 * ┌─────────┐     ┌─────────────┐     ┌─────────────┐     ┌──────────┐     ┌────────┐
 * │ PROPOSE │────►│   ENDORSE   │────►│    ORDER    │────►│ VALIDATE │────►│ COMMIT │
 * └─────────┘     └─────────────┘     └─────────────┘     └──────────┘     └────────┘
 *      │                 │                   │                  │               │
 *      ▼                 ▼                   ▼                  ▼               ▼
 *   Module          Endorsing           Ordering            All peers      World State
 *   submits         peers per           service             validate       updated
 *   proposal        policy              sequences           signatures     (CRDT + KG)
 *                   simulate            (Raft)              & policy
 *                   & sign
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_TRANSACTION_H
#define NIMCP_MESH_TRANSACTION_H

#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_participant.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct mesh_tx_manager mesh_tx_manager_t;

/* ============================================================================
 * Transaction Structure
 * ============================================================================ */

/**
 * @brief Complete mesh transaction
 *
 * WHAT: Full transaction with all phases tracked
 * WHY:  Enable Execute-Order-Validate flow
 */
typedef struct mesh_transaction {
    /* ---- Identity ---- */
    mesh_tx_id_t id;                        /**< Unique transaction ID */
    mesh_tx_type_t type;                    /**< Transaction type */
    mesh_tx_status_t status;                /**< Current status */

    /* ---- Proposer ---- */
    mesh_participant_id_t proposer_id;      /**< Who proposed */
    mesh_channel_id_t source_channel;       /**< Source channel */
    mesh_channel_id_t target_channel;       /**< Target channel (may differ) */

    /* ---- Payload ---- */
    uint8_t* payload;                       /**< Transaction payload */
    size_t payload_size;                    /**< Payload size */
    uint8_t payload_hash[32];               /**< SHA256 of payload */

    /* ---- Endorsement ---- */
    endorsement_set_t endorsements;         /**< Collected endorsements */
    const char* endorsement_policy;         /**< Policy name */
    uint64_t endorsement_deadline_ns;       /**< Endorsement timeout */

    /* ---- Ordering ---- */
    uint64_t sequence_number;               /**< Assigned by ordering service */
    uint64_t ordering_timestamp_ns;         /**< When ordered */
    uint8_t ordering_signature[MESH_SIGNATURE_SIZE]; /**< Orderer signature */

    /* ---- Validation ---- */
    uint32_t validation_count;              /**< Validations received */
    uint32_t validation_required;           /**< Validations needed */
    bool validation_passed;                 /**< Validation result */

    /* ---- Commit ---- */
    uint64_t commit_timestamp_ns;           /**< When committed */
    uint8_t result_hash[32];                /**< Hash of execution result */

    /* ---- Timing ---- */
    uint64_t created_ns;                    /**< Creation timestamp */
    uint64_t timeout_ns;                    /**< Overall timeout */

    /* ---- Flags ---- */
    bool is_cross_channel;                  /**< Cross-channel transaction */
    bool is_emergency;                      /**< Emergency override */
    bool requires_gpu;                      /**< Requires GPU processing */

    /* ---- Callback ---- */
    mesh_tx_callback_t callback;            /**< Completion callback */
    void* callback_ctx;                     /**< Callback context */

} mesh_transaction_t;

/* ============================================================================
 * Transaction Manager Configuration
 * ============================================================================ */

/**
 * @brief Transaction manager configuration
 */
typedef struct mesh_tx_manager_config {
    size_t max_pending;                     /**< Max pending transactions */
    size_t max_batch_size;                  /**< Max batch size for ordering */
    uint64_t default_timeout_ms;            /**< Default transaction timeout */
    uint64_t endorsement_timeout_ms;        /**< Endorsement collection timeout */
    uint64_t batch_timeout_ms;              /**< Batch timeout */
    bool enable_logging;                    /**< Enable transaction logging */
} mesh_tx_manager_config_t;

/**
 * @brief Transaction manager statistics
 */
typedef struct mesh_tx_manager_stats {
    uint64_t transactions_proposed;         /**< Total proposed */
    uint64_t transactions_endorsed;         /**< Successfully endorsed */
    uint64_t transactions_ordered;          /**< Successfully ordered */
    uint64_t transactions_committed;        /**< Successfully committed */
    uint64_t transactions_failed;           /**< Failed */
    uint64_t transactions_expired;          /**< Timed out */
    uint64_t transactions_rejected;         /**< Rejected by endorsement */
    size_t pending_count;                   /**< Currently pending */
    float avg_latency_ms;                   /**< Average commit latency */
    float endorsement_success_rate;         /**< Endorsement success rate */
} mesh_tx_manager_stats_t;

/* ============================================================================
 * Transaction Manager Lifecycle
 * ============================================================================ */

/**
 * @brief Get default transaction manager configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_tx_manager_default_config(mesh_tx_manager_config_t* config);

/**
 * @brief Create transaction manager
 *
 * WHAT: Allocate and initialize transaction manager
 * WHY:  Central manager for transaction lifecycle
 *
 * @param config Configuration (NULL for defaults)
 * @param registry Participant registry for callbacks
 * @return Manager handle or NULL on failure
 */
mesh_tx_manager_t* mesh_tx_manager_create(
    const mesh_tx_manager_config_t* config,
    mesh_participant_registry_t* registry
);

/**
 * @brief Destroy transaction manager
 *
 * @param manager Manager to destroy (NULL-safe)
 */
void mesh_tx_manager_destroy(mesh_tx_manager_t* manager);

/* ============================================================================
 * Transaction Creation API
 * ============================================================================ */

/**
 * @brief Create a new transaction
 *
 * WHAT: Allocate and initialize transaction structure
 * WHY:  Create transaction for proposal
 *
 * @param type Transaction type
 * @param proposer Proposing participant
 * @param channel Target channel
 * @return New transaction or NULL on failure
 */
mesh_transaction_t* mesh_transaction_create(
    mesh_tx_type_t type,
    mesh_participant_id_t proposer,
    mesh_channel_id_t channel
);

/**
 * @brief Destroy a transaction
 *
 * @param tx Transaction to destroy (NULL-safe)
 */
void mesh_transaction_destroy(mesh_transaction_t* tx);

/**
 * @brief Set transaction payload
 *
 * @param tx Transaction
 * @param payload Payload data (copied)
 * @param size Payload size
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_transaction_set_payload(
    mesh_transaction_t* tx,
    const void* payload,
    size_t size
);

/**
 * @brief Set endorsement policy for transaction
 *
 * @param tx Transaction
 * @param policy_name Policy name
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_transaction_set_policy(
    mesh_transaction_t* tx,
    const char* policy_name
);

/**
 * @brief Set transaction callback
 *
 * @param tx Transaction
 * @param callback Completion callback
 * @param ctx Callback context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_transaction_set_callback(
    mesh_transaction_t* tx,
    mesh_tx_callback_t callback,
    void* ctx
);

/**
 * @brief Set transaction timeout
 *
 * @param tx Transaction
 * @param timeout_ms Timeout in milliseconds
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_transaction_set_timeout(
    mesh_transaction_t* tx,
    uint64_t timeout_ms
);

/* ============================================================================
 * Transaction Lifecycle API (Execute-Order-Validate)
 * ============================================================================ */

/**
 * @brief Submit transaction for processing (PROPOSE)
 *
 * WHAT: Submit transaction to begin E-O-V flow
 * WHY:  Entry point for transaction processing
 *
 * @param manager Transaction manager
 * @param tx Transaction to submit
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_tx_propose(
    mesh_tx_manager_t* manager,
    mesh_transaction_t* tx
);

/**
 * @brief Collect endorsements for transaction (ENDORSE)
 *
 * WHAT: Request endorsements from policy-specified endorsers
 * WHY:  Endorsement required before ordering
 *
 * @param manager Transaction manager
 * @param tx_id Transaction ID
 * @return NIMCP_SUCCESS when policy satisfied
 */
nimcp_error_t mesh_tx_collect_endorsements(
    mesh_tx_manager_t* manager,
    const mesh_tx_id_t* tx_id
);

/**
 * @brief Add endorsement to transaction
 *
 * @param manager Transaction manager
 * @param tx_id Transaction ID
 * @param endorsement Endorsement to add
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_tx_add_endorsement(
    mesh_tx_manager_t* manager,
    const mesh_tx_id_t* tx_id,
    const mesh_endorsement_t* endorsement
);

/**
 * @brief Submit to ordering service (ORDER)
 *
 * WHAT: Submit endorsed transaction for sequencing
 * WHY:  Ordering establishes total order
 *
 * @param manager Transaction manager
 * @param tx_id Transaction ID
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_tx_submit_for_ordering(
    mesh_tx_manager_t* manager,
    const mesh_tx_id_t* tx_id
);

/**
 * @brief Mark transaction as ordered
 *
 * @param manager Transaction manager
 * @param tx_id Transaction ID
 * @param sequence Assigned sequence number
 * @param signature Orderer signature
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_tx_mark_ordered(
    mesh_tx_manager_t* manager,
    const mesh_tx_id_t* tx_id,
    uint64_t sequence,
    const uint8_t* signature
);

/**
 * @brief Validate transaction (VALIDATE)
 *
 * WHAT: Validate endorsements and check for conflicts
 * WHY:  All peers validate before commit
 *
 * @param manager Transaction manager
 * @param tx_id Transaction ID
 * @param validator Validating participant
 * @param valid Validation result
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_tx_validate(
    mesh_tx_manager_t* manager,
    const mesh_tx_id_t* tx_id,
    mesh_participant_id_t validator,
    bool valid
);

/**
 * @brief Commit transaction (COMMIT)
 *
 * WHAT: Commit transaction to world state
 * WHY:  Final phase - apply changes
 *
 * @param manager Transaction manager
 * @param tx_id Transaction ID
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_tx_commit(
    mesh_tx_manager_t* manager,
    const mesh_tx_id_t* tx_id
);

/**
 * @brief Fail a transaction
 *
 * @param manager Transaction manager
 * @param tx_id Transaction ID
 * @param error Error code
 * @param message Error message
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_tx_fail(
    mesh_tx_manager_t* manager,
    const mesh_tx_id_t* tx_id,
    nimcp_error_t error,
    const char* message
);

/* ============================================================================
 * Transaction Query API
 * ============================================================================ */

/**
 * @brief Get transaction by ID
 *
 * @param manager Transaction manager
 * @param tx_id Transaction ID
 * @return Transaction or NULL if not found
 */
const mesh_transaction_t* mesh_tx_get(
    mesh_tx_manager_t* manager,
    const mesh_tx_id_t* tx_id
);

/**
 * @brief Get transaction status
 *
 * @param manager Transaction manager
 * @param tx_id Transaction ID
 * @return Transaction status
 */
mesh_tx_status_t mesh_tx_get_status(
    mesh_tx_manager_t* manager,
    const mesh_tx_id_t* tx_id
);

/**
 * @brief Get transaction result
 *
 * @param manager Transaction manager
 * @param tx_id Transaction ID
 * @param result Output: result structure
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_tx_get_result(
    mesh_tx_manager_t* manager,
    const mesh_tx_id_t* tx_id,
    mesh_result_t* result
);

/**
 * @brief Check if transaction is complete
 *
 * @param manager Transaction manager
 * @param tx_id Transaction ID
 * @return true if committed or failed
 */
bool mesh_tx_is_complete(
    mesh_tx_manager_t* manager,
    const mesh_tx_id_t* tx_id
);

/**
 * @brief Check if transaction endorsement policy is satisfied
 *
 * @param manager Transaction manager
 * @param tx_id Transaction ID
 * @return true if policy satisfied
 */
bool mesh_tx_is_endorsed(
    mesh_tx_manager_t* manager,
    const mesh_tx_id_t* tx_id
);

/* ============================================================================
 * Batch Operations
 * ============================================================================ */

/**
 * @brief Transaction batch for bulk operations
 */
typedef struct mesh_tx_batch {
    mesh_transaction_t** transactions;      /**< Array of transactions */
    size_t count;                           /**< Number of transactions */
    size_t capacity;                        /**< Array capacity */
    mesh_channel_id_t channel;              /**< Batch channel */
} mesh_tx_batch_t;

/**
 * @brief Create transaction batch
 *
 * @param channel Target channel
 * @param capacity Initial capacity
 * @return New batch or NULL on failure
 */
mesh_tx_batch_t* mesh_tx_batch_create(
    mesh_channel_id_t channel,
    size_t capacity
);

/**
 * @brief Destroy transaction batch
 *
 * @param batch Batch to destroy (NULL-safe)
 */
void mesh_tx_batch_destroy(mesh_tx_batch_t* batch);

/**
 * @brief Add transaction to batch
 *
 * @param batch Batch
 * @param tx Transaction to add
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_tx_batch_add(
    mesh_tx_batch_t* batch,
    mesh_transaction_t* tx
);

/**
 * @brief Clear batch
 *
 * @param batch Batch to clear
 */
void mesh_tx_batch_clear(mesh_tx_batch_t* batch);

/**
 * @brief Submit batch for processing
 *
 * @param manager Transaction manager
 * @param batch Batch to submit
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_tx_batch_submit(
    mesh_tx_manager_t* manager,
    mesh_tx_batch_t* batch
);

/* ============================================================================
 * Timeout and Cleanup
 * ============================================================================ */

/**
 * @brief Process expired transactions
 *
 * @param manager Transaction manager
 * @param current_time_ns Current time (nanoseconds)
 * @return Number of transactions expired
 */
size_t mesh_tx_cleanup_expired(
    mesh_tx_manager_t* manager,
    uint64_t current_time_ns
);

/**
 * @brief Update transaction manager (call periodically)
 *
 * @param manager Transaction manager
 * @param delta_ms Time since last update
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_tx_manager_update(
    mesh_tx_manager_t* manager,
    uint64_t delta_ms
);

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Get transaction manager statistics
 *
 * @param manager Transaction manager
 * @param stats Output: statistics
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_tx_manager_get_stats(
    mesh_tx_manager_t* manager,
    mesh_tx_manager_stats_t* stats
);

/**
 * @brief Reset transaction manager statistics
 *
 * @param manager Transaction manager
 */
void mesh_tx_manager_reset_stats(mesh_tx_manager_t* manager);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Generate transaction ID
 *
 * @param proposer Proposing participant
 * @param channel Channel
 * @param id_out Output: generated ID
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_tx_generate_id(
    mesh_participant_id_t proposer,
    mesh_channel_id_t channel,
    mesh_tx_id_t* id_out
);

/**
 * @brief Compute payload hash
 *
 * @param payload Payload data
 * @param size Payload size
 * @param hash_out Output: 32-byte hash
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_tx_compute_hash(
    const void* payload,
    size_t size,
    uint8_t* hash_out
);

/**
 * @brief Print transaction for debugging
 *
 * @param tx Transaction to print
 */
void mesh_transaction_print(const mesh_transaction_t* tx);

/**
 * @brief Print transaction manager status
 *
 * @param manager Transaction manager
 */
void mesh_tx_manager_print_status(const mesh_tx_manager_t* manager);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_TRANSACTION_H */
