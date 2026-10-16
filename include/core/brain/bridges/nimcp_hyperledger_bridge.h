//=============================================================================
// nimcp_hyperledger_bridge.h - Hyperledger-Inspired Training/Inference Integration
//=============================================================================
/**
 * @file nimcp_hyperledger_bridge.h
 * @brief Execute-Order-Validate training, consensus-gated inference, auditable weights
 *
 * WHAT: Hyperledger Fabric-inspired patterns for distributed training security
 * WHY:  Protect training from Byzantine gradients, enable multi-brain consensus,
 *       provide tamper-evident audit trail of weight modifications
 * HOW:  Three integration points wired into existing subsystems:
 *
 * 1. EOV Training Pipeline:
 *    Execute: brain_learn_vector() computes gradients locally
 *    Order:   Gradient manager sequences and accumulates updates
 *    Validate: Distributed security bridge validates before weight application
 *
 * 2. Consensus-Gated Inference:
 *    When collective cognition is active, brain_decide() outcomes are voted on
 *    using BFT swarm consensus before being returned to the caller
 *
 * 3. Auditable Weight Ledger:
 *    Significant weight changes are logged to a tamper-evident ring buffer
 *    with cryptographic hash chaining, using knowledge COW snapshots for rollback
 *
 * @version 1.0.0
 * @date 2026-03-09
 */

#ifndef NIMCP_HYPERLEDGER_BRIDGE_H
#define NIMCP_HYPERLEDGER_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define HYPERLEDGER_BRIDGE_MAGIC        0x484C4247  /* "HLBG" */
#define HYPERLEDGER_AUDIT_RING_SIZE     4096        /* Audit log entries */
#define HYPERLEDGER_HASH_SIZE           32          /* SHA-256 hash bytes */
#define HYPERLEDGER_MAX_ROLLBACK_DEPTH  16          /* Max COW snapshots */

//=============================================================================
// Forward Declarations
//=============================================================================

struct brain_struct;
struct security_distributed_training_bridge;
struct nimcp_gradient_manager_ctx;
struct swarm_consensus_context;
struct collective_cognition;
struct knowledge_cow_base_struct;
struct knowledge_cow_snapshot_struct;

//=============================================================================
// Enumerations
//=============================================================================

/** EOV transaction phase */
typedef enum {
    EOV_PHASE_IDLE = 0,
    EOV_PHASE_EXECUTE,      /* Gradient computation in progress */
    EOV_PHASE_ORDER,        /* Gradient accumulation/sequencing */
    EOV_PHASE_VALIDATE,     /* Security validation before commit */
    EOV_PHASE_COMMITTED,    /* Weights updated successfully */
    EOV_PHASE_REJECTED      /* Validation failed, weights NOT updated */
} eov_phase_t;

/** Audit entry type */
typedef enum {
    AUDIT_WEIGHT_UPDATE = 0,    /* Normal weight update */
    AUDIT_GRADIENT_REJECTED,    /* Gradient rejected by validation */
    AUDIT_BYZANTINE_DETECTED,   /* Byzantine behavior detected */
    AUDIT_CONSENSUS_REACHED,    /* Multi-brain consensus decision */
    AUDIT_CONSENSUS_FAILED,     /* Multi-brain consensus failed */
    AUDIT_ROLLBACK,             /* Weight rollback via snapshot */
    AUDIT_CHECKPOINT,           /* Checkpoint created */
    AUDIT_TRUST_CHANGE          /* Worker trust level changed */
} audit_entry_type_t;

/** Consensus gate decision */
typedef enum {
    CONSENSUS_GATE_PASS = 0,    /* Decision approved by consensus */
    CONSENSUS_GATE_REJECT,      /* Decision rejected */
    CONSENSUS_GATE_TIMEOUT,     /* Consensus timed out, use local */
    CONSENSUS_GATE_SKIP         /* No collective active, pass through */
} consensus_gate_result_t;

//=============================================================================
// Structures
//=============================================================================

/** Audit log entry with hash chain */
typedef struct {
    uint64_t sequence_id;               /* Monotonic sequence number */
    uint64_t timestamp_ms;              /* Event timestamp */
    audit_entry_type_t type;            /* Entry type */
    uint8_t prev_hash[HYPERLEDGER_HASH_SIZE]; /* Hash of previous entry */
    uint8_t entry_hash[HYPERLEDGER_HASH_SIZE]; /* Hash of this entry */

    /* Entry-specific data */
    union {
        struct {
            float weight_delta_norm;    /* L2 norm of weight change */
            float loss_before;          /* Loss before update */
            float loss_after;           /* Loss after update */
            uint32_t weights_modified;  /* Number of weights changed */
        } weight_update;

        struct {
            float anomaly_score;        /* Why it was rejected */
            float gradient_norm;        /* Gradient norm */
            uint32_t worker_idx;        /* Which worker (distributed) */
        } gradient_rejected;

        struct {
            uint32_t byzantine_count;   /* Number of Byzantine workers */
            float confidence;           /* Detection confidence */
            float threat_level;         /* Overall threat */
        } byzantine;

        struct {
            float agreement;            /* Consensus agreement level */
            uint32_t agree_count;       /* Votes for */
            uint32_t disagree_count;    /* Votes against */
            float decision_confidence;  /* Decision confidence */
        } consensus;

        struct {
            uint32_t snapshot_idx;      /* Which snapshot restored */
            float loss_at_rollback;     /* Loss that triggered rollback */
        } rollback;
    } data;
} hyperledger_audit_entry_t;

/** EOV transaction state */
typedef struct {
    eov_phase_t phase;
    uint64_t tx_id;                     /* Transaction ID */
    uint64_t start_time_ms;             /* When execute began */
    float gradient_norm;                /* Computed gradient norm */
    float loss;                         /* Loss from forward pass */
    bool validated;                     /* Passed security validation */
    float anomaly_score;                /* From validation (0 = clean) */
    uint32_t validation_time_us;        /* Validation latency */
} eov_transaction_t;

/** Bridge statistics */
typedef struct {
    /* EOV pipeline */
    uint64_t total_transactions;        /* Total EOV transactions */
    uint64_t committed_transactions;    /* Successfully committed */
    uint64_t rejected_transactions;     /* Rejected by validation */
    uint64_t total_validation_time_us;  /* Cumulative validation time */
    float avg_validation_time_us;       /* Average validation latency */
    float rejection_rate;               /* Fraction rejected */

    /* Consensus */
    uint64_t consensus_proposals;       /* Total proposals */
    uint64_t consensus_passed;          /* Passed consensus */
    uint64_t consensus_failed;          /* Failed consensus */
    uint64_t consensus_timeouts;        /* Timed out */
    float avg_agreement;                /* Average agreement level */

    /* Audit */
    uint64_t audit_entries;             /* Total audit log entries */
    uint64_t hash_chain_verified;       /* Hash chain verifications */
    uint32_t snapshots_created;         /* COW snapshots taken */
    uint32_t rollbacks_performed;       /* Rollbacks executed */

    /* Byzantine */
    uint64_t byzantine_detections;      /* Total detections */
    uint64_t workers_quarantined;       /* Workers quarantined */
} hyperledger_bridge_stats_t;

/** Configuration */
typedef struct {
    /* EOV pipeline */
    bool enable_eov_pipeline;           /* Gate training through EOV */
    float anomaly_reject_threshold;     /* Reject if anomaly score > this */
    bool validate_every_step;           /* Validate every step (vs interval) */
    uint32_t validation_interval;       /* Steps between validations */

    /* Consensus */
    bool enable_consensus_gate;         /* Gate inference through consensus */
    float consensus_threshold;          /* Min agreement to pass (default 2/3) */
    uint32_t consensus_quorum;          /* Min voters (0 = all) */
    uint32_t consensus_timeout_ms;      /* Max wait for consensus */

    /* Audit */
    bool enable_audit_log;              /* Enable audit logging */
    bool enable_hash_chain;             /* Cryptographic hash chaining */
    float audit_weight_threshold;       /* Min delta norm to log */
    bool enable_snapshots;              /* COW snapshots for rollback */
    uint32_t snapshot_interval;         /* Steps between snapshots */
    uint32_t max_snapshots;             /* Max snapshots to keep */

    /* Auto-rollback */
    bool enable_auto_rollback;          /* Auto-rollback on loss spike */
    float rollback_loss_threshold;      /* Loss increase to trigger rollback */
} hyperledger_bridge_config_t;

/** Opaque bridge type */
typedef struct hyperledger_bridge hyperledger_bridge_t;

//=============================================================================
// Configuration
//=============================================================================

/** Get default configuration */
hyperledger_bridge_config_t hyperledger_bridge_default_config(void);

//=============================================================================
// Lifecycle
//=============================================================================

/** Create bridge */
hyperledger_bridge_t* hyperledger_bridge_create(
    const hyperledger_bridge_config_t* config);

/** Destroy bridge */
void hyperledger_bridge_destroy(hyperledger_bridge_t* bridge);

//=============================================================================
// Connection (wire to existing subsystems)
//=============================================================================

/** Connect to distributed training security bridge */
int hyperledger_bridge_connect_security(
    hyperledger_bridge_t* bridge,
    struct security_distributed_training_bridge* sec_bridge);

/** Connect to gradient manager */
int hyperledger_bridge_connect_gradient_manager(
    hyperledger_bridge_t* bridge,
    struct nimcp_gradient_manager_ctx* grad_mgr);

/** Connect to swarm consensus for multi-brain voting */
int hyperledger_bridge_connect_consensus(
    hyperledger_bridge_t* bridge,
    struct swarm_consensus_context* consensus);

/** Connect to collective cognition for multi-brain decisions */
int hyperledger_bridge_connect_collective(
    hyperledger_bridge_t* bridge,
    struct collective_cognition* collective);

//=============================================================================
// EOV Training Pipeline (Integration Point #1)
//=============================================================================

/**
 * @brief Begin EOV transaction for a training step
 *
 * Called at the start of brain_learn_vector() to enter EXECUTE phase.
 * Returns a transaction handle used to track through ORDER → VALIDATE → COMMIT.
 *
 * @param bridge Hyperledger bridge
 * @param loss Current loss value from forward pass
 * @return Transaction ID (0 on error)
 */
uint64_t hyperledger_eov_begin(hyperledger_bridge_t* bridge, float loss);

/**
 * @brief Order phase: record gradient statistics after computation
 *
 * Called after gradient computation, before weight update.
 * Records gradient norm and statistics for validation.
 *
 * @param bridge Hyperledger bridge
 * @param tx_id Transaction ID from eov_begin
 * @param gradient_norm L2 norm of computed gradients
 * @param num_gradients Number of gradient values
 * @return 0 on success
 */
int hyperledger_eov_order(hyperledger_bridge_t* bridge,
                           uint64_t tx_id,
                           float gradient_norm,
                           uint32_t num_gradients);

/**
 * @brief Validate phase: security check before weight commit
 *
 * Calls the distributed training security bridge to validate gradients.
 * If validation fails, the transaction is REJECTED and weights must NOT be updated.
 *
 * @param bridge Hyperledger bridge
 * @param tx_id Transaction ID
 * @param gradients Gradient array to validate
 * @param num_gradients Number of gradient values
 * @return true if validation passes (safe to commit), false if rejected
 */
bool hyperledger_eov_validate(hyperledger_bridge_t* bridge,
                               uint64_t tx_id,
                               const float* gradients,
                               uint32_t num_gradients);

/**
 * @brief Commit phase: record successful weight update
 *
 * Called after weights are updated. Logs to audit trail.
 *
 * @param bridge Hyperledger bridge
 * @param tx_id Transaction ID
 * @param weight_delta_norm L2 norm of actual weight change
 * @param loss_after Loss after weight update
 * @return 0 on success
 */
int hyperledger_eov_commit(hyperledger_bridge_t* bridge,
                            uint64_t tx_id,
                            float weight_delta_norm,
                            float loss_after);

/**
 * @brief Get current EOV transaction state
 */
int hyperledger_eov_get_state(const hyperledger_bridge_t* bridge,
                               eov_transaction_t* state);

//=============================================================================
// Consensus-Gated Inference (Integration Point #2)
//=============================================================================

/**
 * @brief Propose a decision for multi-brain consensus
 *
 * When collective cognition is active, submits the local brain's decision
 * for BFT voting. Other brains in the collective vote to agree/disagree.
 *
 * @param bridge Hyperledger bridge
 * @param decision_vector Decision output vector
 * @param decision_dim Dimension of decision vector
 * @param local_confidence Local brain's confidence in this decision
 * @param result Output: consensus gate result
 * @return 0 on success
 */
int hyperledger_consensus_gate(hyperledger_bridge_t* bridge,
                                const float* decision_vector,
                                uint32_t decision_dim,
                                float local_confidence,
                                consensus_gate_result_t* result);

/**
 * @brief Vote on another brain's proposed decision
 *
 * @param bridge Hyperledger bridge
 * @param proposal_id Proposal to vote on
 * @param agreement Agreement level [0=disagree, 1=agree]
 * @param confidence Vote confidence
 * @return 0 on success
 */
int hyperledger_consensus_vote(hyperledger_bridge_t* bridge,
                                uint32_t proposal_id,
                                float agreement,
                                float confidence);

//=============================================================================
// Auditable Weight Ledger (Integration Point #3)
//=============================================================================

/**
 * @brief Log an audit entry
 *
 * Creates a hash-chained audit entry. Each entry contains the hash of the
 * previous entry, forming a tamper-evident chain.
 *
 * @param bridge Hyperledger bridge
 * @param entry Audit entry to log
 * @return Sequence ID assigned to entry (0 on error)
 */
uint64_t hyperledger_audit_log(hyperledger_bridge_t* bridge,
                                hyperledger_audit_entry_t* entry);

/**
 * @brief Verify audit log hash chain integrity
 *
 * Walks the audit ring buffer and verifies each entry's prev_hash
 * matches the actual hash of the previous entry.
 *
 * @param bridge Hyperledger bridge
 * @param num_verified Output: number of entries verified
 * @return true if chain is intact, false if tampered
 */
bool hyperledger_audit_verify_chain(const hyperledger_bridge_t* bridge,
                                     uint64_t* num_verified);

/**
 * @brief Get audit log entry by sequence ID
 *
 * @param bridge Hyperledger bridge
 * @param sequence_id Entry sequence ID
 * @param entry Output: audit entry
 * @return 0 on success, -1 if not found
 */
int hyperledger_audit_get_entry(const hyperledger_bridge_t* bridge,
                                 uint64_t sequence_id,
                                 hyperledger_audit_entry_t* entry);

/**
 * @brief Get number of entries in audit log
 */
uint64_t hyperledger_audit_count(const hyperledger_bridge_t* bridge);

/**
 * @brief Create COW snapshot for potential rollback
 *
 * @param bridge Hyperledger bridge
 * @return Snapshot index (0 on error or snapshots disabled)
 */
uint32_t hyperledger_snapshot_create(hyperledger_bridge_t* bridge);

/**
 * @brief Rollback to a previous snapshot
 *
 * @param bridge Hyperledger bridge
 * @param snapshot_idx Snapshot index from snapshot_create
 * @return 0 on success, -1 on failure
 */
int hyperledger_snapshot_rollback(hyperledger_bridge_t* bridge,
                                   uint32_t snapshot_idx);

//=============================================================================
// Statistics & Introspection
//=============================================================================

/** Get bridge statistics */
int hyperledger_bridge_get_stats(const hyperledger_bridge_t* bridge,
                                  hyperledger_bridge_stats_t* stats);

/** Reset statistics */
void hyperledger_bridge_reset_stats(hyperledger_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPERLEDGER_BRIDGE_H */
