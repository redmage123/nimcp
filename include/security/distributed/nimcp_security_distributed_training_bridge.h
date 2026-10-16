/**
 * @file nimcp_security_distributed_training_bridge.h
 * @brief Security-Distributed Training Integration Bridge for NIMCP
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Bidirectional integration between security systems and distributed/federated training
 * WHY:  Protect distributed ML training from Byzantine workers, gradient poisoning, and model corruption
 * HOW:  Security validates workers, detects Byzantine behavior, scores trust, secures checkpoints;
 *       Distributed training reports worker anomalies, gradient statistics, and synchronization issues
 *
 * THREAT MODEL:
 * =============
 * ```
 * ATTACK VECTOR                      DEFENSE MECHANISM
 * --------------------------------------------------------------------------------------------------------
 * Byzantine workers (malicious)      -> Behavior analysis + voting protocols + isolation
 * Gradient poisoning (coordinated)   -> Statistical anomaly detection on aggregated gradients
 * Model parameter manipulation       -> Cryptographic verification + hash chains across nodes
 * Sybil attacks (fake workers)       -> Worker identity verification + trust scoring
 * Free-rider attacks (lazy workers)  -> Contribution verification + work validation
 * Data leakage attacks               -> Differential privacy + secure aggregation
 * Man-in-the-middle attacks          -> Authenticated channels + message signing
 * ```
 *
 * ARCHITECTURE:
 * =============
 * ```
 * +-----------------------------------------------------------------------------+
 * |              SECURITY-DISTRIBUTED TRAINING BRIDGE                           |
 * +-----------------------------------------------------------------------------+
 * |                                                                             |
 * |  SECURITY -> DISTRIBUTED TRAINING (Protective Measures)                     |
 * |  +-------------------+       +------------------------------------+         |
 * |  | Byzantine         |       |     Distributed Training           |         |
 * |  | Detection         |------>|  - Worker filtering (trust levels) |         |
 * |  | Trust Scoring     |       |  - Gradient aggregation rules      |         |
 * |  | Worker Registry   |       |  - Checkpoint verification         |         |
 * |  +-------------------+       +------------------------------------+         |
 * |                                                                             |
 * |  DISTRIBUTED TRAINING -> SECURITY (Threat Signals)                          |
 * |  +------------------------------------+       +-------------------+         |
 * |  |     Distributed Training           |       |  Security         |         |
 * |  |  - Worker gradient anomalies       |------>|  - Alert BBB      |         |
 * |  |  - Synchronization failures        |       |  - Quarantine     |         |
 * |  |  - Contribution inconsistencies    |       |  - Trust update   |         |
 * |  +------------------------------------+       +-------------------+         |
 * |                                                                             |
 * +-----------------------------------------------------------------------------+
 * ```
 *
 * BYZANTINE FAULT TOLERANCE:
 * ==========================
 * ```
 * Trust Level    | Voting Weight | Allowed Operations        | Monitoring
 * ---------------|---------------|---------------------------|-------------------
 * QUARANTINED    | 0             | None (isolated)           | Full surveillance
 * UNTRUSTED      | 0.1           | Submit gradients only     | High monitoring
 * PROBATION      | 0.3           | Participate with limits   | Medium monitoring
 * VERIFIED       | 0.7           | Full participation        | Standard monitoring
 * TRUSTED        | 1.0           | Full + checkpoint rights  | Minimal monitoring
 * ```
 *
 * GRADIENT AGGREGATION SECURITY:
 * ==============================
 * ```
 * Method              | Byzantine Tolerance | Privacy  | Overhead
 * --------------------|---------------------|----------|------------
 * Simple Average      | 0%                  | None     | O(n)
 * Median              | <50%                | None     | O(n log n)
 * Trimmed Mean        | <50%                | None     | O(n log n)
 * Krum                | <n/2 - 1            | None     | O(n^2)
 * Bulyan              | <n/4 - 1            | None     | O(n^2)
 * Secure Aggregation  | Varies              | High     | O(n^2)
 * ```
 *
 * USAGE EXAMPLE:
 * ==============
 * ```c
 * // Create security-distributed training bridge
 * security_distributed_training_config_t config = security_distributed_training_default_config();
 * config.enable_byzantine_detection = true;
 * config.min_worker_trust = SECURITY_WORKER_TRUST_PROBATION;
 * config.aggregation_method = SECURITY_GRAD_AGG_KRUM;
 * security_distributed_training_bridge_t* bridge = security_distributed_training_bridge_create(&config);
 *
 * // Connect to systems
 * security_distributed_training_connect_distributed_coordinator(bridge, dist_ctx);
 * security_distributed_training_connect_bbb(bridge, bbb_system);
 *
 * // Register workers
 * security_distributed_training_register_worker(bridge, "worker_0", worker_key);
 * security_distributed_training_register_worker(bridge, "worker_1", worker_key);
 *
 * // Training loop
 * for (int round = 0; round < max_rounds; round++) {
 *     // Collect gradients from workers
 *     for (int w = 0; w < num_workers; w++) {
 *         // Validate gradient before aggregation
 *         security_gradient_validation_result_t result;
 *         if (security_distributed_training_validate_gradients(bridge, worker_id, grads, &result)) {
 *             // Gradient is suspicious
 *             if (result.quarantine_recommended) {
 *                 security_distributed_training_quarantine_worker(bridge, worker_id);
 *             }
 *         }
 *     }
 *
 *     // Detect Byzantine workers
 *     security_byzantine_result_t byz_result;
 *     security_distributed_training_detect_byzantine(bridge, &byz_result);
 *
 *     // Secure checkpoint across nodes
 *     if (round % checkpoint_interval == 0) {
 *         security_distributed_training_secure_checkpoint(bridge, "checkpoint_X", model, round);
 *     }
 * }
 * ```
 *
 * GOTCHAS:
 * ========
 * - Worker trust is persistent across training rounds
 * - Byzantine detection requires minimum 3f+1 workers for f Byzantine tolerance
 * - Secure checkpointing requires consensus across majority of trusted workers
 * - Gradient validation can modify gradients in-place for sanitization
 * - Quarantined workers cannot rejoin without manual approval
 *
 * NIMCP STANDARDS:
 * ================
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SECURITY_DISTRIBUTED_TRAINING_BRIDGE_H
#define NIMCP_SECURITY_DISTRIBUTED_TRAINING_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/validation/nimcp_common.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SECURITY_DISTRIBUTED_MODULE_NAME      "security_distributed_training"
#define SECURITY_DISTRIBUTED_MODULE_VERSION   "1.0.0"

/* Worker management limits */
#define SECURITY_DISTRIBUTED_MAX_WORKERS             1024
#define SECURITY_DISTRIBUTED_WORKER_ID_MAX           128
#define SECURITY_DISTRIBUTED_WORKER_KEY_SIZE         64

/* Byzantine detection defaults */
#define SECURITY_DISTRIBUTED_DEFAULT_BYZANTINE_THRESHOLD     0.7f
#define SECURITY_DISTRIBUTED_DEFAULT_ANOMALY_THRESHOLD       0.8f
#define SECURITY_DISTRIBUTED_DEFAULT_CONSISTENCY_THRESHOLD   0.6f

/* Gradient validation defaults */
#define SECURITY_DISTRIBUTED_DEFAULT_GRAD_NORM_THRESHOLD     10.0f
#define SECURITY_DISTRIBUTED_DEFAULT_GRAD_VARIANCE_THRESHOLD 5.0f
#define SECURITY_DISTRIBUTED_DEFAULT_GRAD_OUTLIER_THRESHOLD  3.0f

/* Trust scoring defaults */
#define SECURITY_DISTRIBUTED_DEFAULT_TRUST_DECAY_RATE        0.01f
#define SECURITY_DISTRIBUTED_DEFAULT_TRUST_RECOVERY_RATE     0.05f
#define SECURITY_DISTRIBUTED_DEFAULT_TRUST_VIOLATION_PENALTY 0.2f

/* Checkpoint management */
#define SECURITY_DISTRIBUTED_CHECKPOINT_NAME_MAX      256
#define SECURITY_DISTRIBUTED_MAX_CHECKPOINTS          64
#define SECURITY_DISTRIBUTED_HASH_SIZE                32

/* Bio-async */
#define SECURITY_DISTRIBUTED_BIO_INBOX_CAPACITY       64

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct security_distributed_training_bridge security_distributed_training_bridge_t;
typedef struct bbb_system_struct* bbb_system_t;
typedef struct distributed_coordinator_struct* distributed_coordinator_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Worker trust levels
 *
 * WHAT: Trust levels for distributed training workers
 * WHY:  Different trust levels determine participation rights and monitoring
 * HOW:  Workers progress through levels based on behavior
 */
typedef enum {
    SECURITY_WORKER_TRUST_QUARANTINED = 0,  /**< Isolated due to malicious behavior */
    SECURITY_WORKER_TRUST_UNTRUSTED,        /**< New or suspicious worker */
    SECURITY_WORKER_TRUST_PROBATION,        /**< Under observation */
    SECURITY_WORKER_TRUST_VERIFIED,         /**< Verified worker */
    SECURITY_WORKER_TRUST_TRUSTED           /**< Fully trusted worker */
} security_worker_trust_t;

/**
 * @brief Byzantine attack types
 *
 * WHAT: Categories of Byzantine behavior in distributed training
 * WHY:  Different attacks require different detection methods
 * HOW:  Each type has specialized detection logic
 */
typedef enum {
    SECURITY_BYZANTINE_NONE = 0,            /**< No Byzantine behavior detected */
    SECURITY_BYZANTINE_GRADIENT_ATTACK,     /**< Malicious gradient submission */
    SECURITY_BYZANTINE_FREE_RIDER,          /**< Not contributing valid work */
    SECURITY_BYZANTINE_SYBIL,               /**< Fake worker identities */
    SECURITY_BYZANTINE_COLLUSION,           /**< Coordinated attack */
    SECURITY_BYZANTINE_MODEL_POISONING,     /**< Model parameter manipulation */
    SECURITY_BYZANTINE_DATA_LEAKAGE,        /**< Information extraction attempt */
    SECURITY_BYZANTINE_COUNT
} security_byzantine_type_t;

/**
 * @brief Gradient aggregation security methods
 *
 * WHAT: Methods for secure gradient aggregation
 * WHY:  Different methods provide different Byzantine tolerance
 * HOW:  Applied during gradient aggregation step
 */
typedef enum {
    SECURITY_GRAD_AGG_SIMPLE_AVERAGE = 0,   /**< No Byzantine tolerance */
    SECURITY_GRAD_AGG_MEDIAN,               /**< Coordinate-wise median */
    SECURITY_GRAD_AGG_TRIMMED_MEAN,         /**< Remove outliers, then average */
    SECURITY_GRAD_AGG_KRUM,                 /**< Krum algorithm */
    SECURITY_GRAD_AGG_BULYAN,               /**< Bulyan algorithm */
    SECURITY_GRAD_AGG_SECURE,               /**< Secure aggregation protocol */
    SECURITY_GRAD_AGG_MULTI_KRUM            /**< Multi-Krum selection */
} security_grad_aggregation_t;

/**
 * @brief Bridge operational phase
 *
 * WHAT: Current operational phase of the bridge
 * WHY:  Different phases have different behaviors
 * HOW:  Phases transition based on threat detection
 */
typedef enum {
    SECURITY_DISTRIBUTED_PHASE_INACTIVE = 0,  /**< Bridge not active */
    SECURITY_DISTRIBUTED_PHASE_MONITORING,    /**< Passive monitoring */
    SECURITY_DISTRIBUTED_PHASE_PROTECTING,    /**< Active protection */
    SECURITY_DISTRIBUTED_PHASE_RESPONDING,    /**< Responding to threat */
    SECURITY_DISTRIBUTED_PHASE_RECOVERY       /**< Recovering from incident */
} security_distributed_phase_t;

/**
 * @brief Checkpoint verification result
 *
 * WHAT: Result of distributed checkpoint verification
 * WHY:  Determine checkpoint integrity across nodes
 * HOW:  Consensus-based verification
 */
typedef enum {
    SECURITY_CHECKPOINT_OK = 0,             /**< Checkpoint verified */
    SECURITY_CHECKPOINT_HASH_MISMATCH,      /**< Hash inconsistency */
    SECURITY_CHECKPOINT_NO_CONSENSUS,       /**< Failed to reach consensus */
    SECURITY_CHECKPOINT_INSUFFICIENT_NODES, /**< Not enough trusted nodes */
    SECURITY_CHECKPOINT_TAMPERED            /**< Definite tampering */
} security_checkpoint_result_t;

/* ============================================================================
 * Core Structures
 * ============================================================================ */

/**
 * @brief Configuration for security-distributed training bridge
 */
typedef struct {
    /* Feature enables */
    bool enable_byzantine_detection;        /**< Enable Byzantine worker detection */
    bool enable_gradient_validation;        /**< Enable gradient validation */
    bool enable_worker_trust_scoring;       /**< Enable worker trust scoring */
    bool enable_secure_checkpointing;       /**< Enable secure checkpoints */
    bool enable_secure_aggregation;         /**< Enable secure aggregation */

    /* Byzantine detection */
    float byzantine_threshold;              /**< Byzantine detection threshold */
    float anomaly_threshold;                /**< Anomaly score threshold */
    float consistency_threshold;            /**< Gradient consistency threshold */
    uint32_t min_workers_for_detection;     /**< Minimum workers for detection */

    /* Gradient validation */
    float gradient_norm_threshold;          /**< Max gradient norm per worker */
    float gradient_variance_threshold;      /**< Max gradient variance */
    float gradient_outlier_threshold;       /**< Outlier detection threshold (std devs) */
    security_grad_aggregation_t aggregation_method; /**< Aggregation method */

    /* Trust scoring */
    security_worker_trust_t min_worker_trust;  /**< Minimum trust for participation */
    float trust_decay_rate;                 /**< Trust decay per round */
    float trust_recovery_rate;              /**< Trust recovery rate */
    float trust_violation_penalty;          /**< Penalty for violations */
    bool auto_quarantine_byzantine;         /**< Auto-quarantine detected Byzantine */

    /* Secure aggregation */
    bool enable_differential_privacy;       /**< Enable DP noise */
    float dp_epsilon;                       /**< Differential privacy epsilon */
    float dp_delta;                         /**< Differential privacy delta */

    /* Checkpointing */
    char checkpoint_directory[256];         /**< Directory for checkpoints */
    uint32_t checkpoint_consensus_quorum;   /**< Quorum for checkpoint consensus */
    bool sign_checkpoints;                  /**< Sign checkpoints */

    /* Bio-async */
    bool enable_bio_async;                  /**< Enable bio-async messaging */
    uint32_t bio_inbox_capacity;            /**< Message queue size */

    /* Logging */
    bool enable_logging;                    /**< Enable detailed logging */
    bool log_all_validations;               /**< Log every validation */
} security_distributed_training_config_t;

/**
 * @brief Worker information
 *
 * WHAT: Information about a registered distributed training worker
 * WHY:  Track worker state, trust, and behavior history
 * HOW:  Maintained per-worker in registry
 */
typedef struct {
    char worker_id[SECURITY_DISTRIBUTED_WORKER_ID_MAX];
    uint8_t worker_key[SECURITY_DISTRIBUTED_WORKER_KEY_SIZE];
    security_worker_trust_t trust_level;
    float trust_score;                      /**< Continuous trust score [0-1] */

    /* Behavior tracking */
    uint64_t rounds_participated;
    uint64_t gradients_submitted;
    uint64_t gradients_rejected;
    uint64_t violations_count;
    uint64_t byzantine_detections;

    /* Statistics */
    float avg_gradient_norm;
    float gradient_norm_variance;
    float contribution_score;               /**< Work contribution score [0-1] */

    /* Timestamps */
    uint64_t registration_time_ms;
    uint64_t last_activity_ms;
    uint64_t last_violation_ms;
    uint64_t quarantine_time_ms;            /**< If quarantined */

    /* State flags */
    bool is_active;
    bool is_quarantined;
    bool pending_removal;
} security_worker_info_t;

/**
 * @brief Security effects on distributed training (Security -> Training)
 *
 * WHAT: Security-derived constraints on distributed training
 * WHY:  Communicate protection measures to training coordinator
 * HOW:  Updated by security modules, consumed by training
 */
typedef struct {
    /* Worker filtering */
    security_worker_trust_t min_trust_level; /**< Minimum trust for participation */
    uint32_t quarantined_worker_count;      /**< Number of quarantined workers */
    uint32_t active_worker_count;           /**< Number of active trusted workers */
    bool worker_verification_required;      /**< Require worker verification */

    /* Gradient bounds */
    float gradient_norm_limit;              /**< Max gradient norm allowed */
    float gradient_scale_factor;            /**< Scale factor for gradients (0-1) */
    bool gradient_sanitization_active;      /**< Sanitization enabled */

    /* Aggregation constraints */
    security_grad_aggregation_t required_aggregation; /**< Required aggregation method */
    uint32_t min_gradients_for_aggregation; /**< Minimum gradients needed */
    float outlier_trim_ratio;               /**< Ratio to trim for trimmed mean */

    /* Model protection */
    bool model_locked;                      /**< Model updates blocked */
    bool checkpoint_required;               /**< Checkpoint before continuing */
    uint64_t last_safe_checkpoint_round;    /**< Last verified checkpoint round */

    /* Threat level */
    float threat_level;                     /**< Overall threat level [0-1] */
    float byzantine_ratio;                  /**< Estimated Byzantine worker ratio */
    bool under_attack;                      /**< Active attack detected */

    /* Metadata */
    uint64_t last_update_ms;                /**< When effects were updated */
    bool valid;                             /**< Whether effects are current */
} security_distributed_effects_t;

/**
 * @brief Training effects on security (Training -> Security)
 *
 * WHAT: Training-derived signals for security monitoring
 * WHY:  Alert security to potential threats
 * HOW:  Updated by training coordinator, consumed by security
 */
typedef struct {
    /* Worker anomalies */
    uint32_t anomalous_worker_count;        /**< Workers with anomalies */
    uint32_t* anomalous_worker_indices;     /**< Indices of anomalous workers */
    float* worker_anomaly_scores;           /**< Per-worker anomaly scores */

    /* Gradient statistics */
    float aggregated_gradient_norm;         /**< Norm after aggregation */
    float gradient_diversity;               /**< Diversity across workers */
    float gradient_agreement;               /**< Agreement score [0-1] */
    bool gradient_anomaly_detected;         /**< Anomaly in aggregated gradient */

    /* Synchronization status */
    uint32_t sync_failures_this_round;      /**< Sync failures in current round */
    uint32_t stale_gradient_count;          /**< Workers with stale gradients */
    float avg_worker_latency_ms;            /**< Average worker response time */

    /* Training state */
    uint64_t current_round;                 /**< Current training round */
    float current_loss;                     /**< Current loss value */
    uint32_t active_workers;                /**< Active workers this round */
    uint32_t total_workers;                 /**< Total registered workers */

    /* Metadata */
    uint64_t timestamp_ms;                  /**< When effects were captured */
    bool valid;                             /**< Whether effects are current */
} distributed_security_effects_t;

/**
 * @brief Byzantine detection result
 *
 * WHAT: Result of Byzantine worker detection
 * WHY:  Provide detailed information about detected Byzantine behavior
 * HOW:  Returned by detect_byzantine function
 */
typedef struct {
    bool byzantine_detected;                /**< Whether Byzantine behavior detected */
    security_byzantine_type_t type;         /**< Type of Byzantine behavior */
    float confidence;                       /**< Detection confidence [0-1] */

    /* Affected workers */
    uint32_t num_byzantine;                 /**< Number of Byzantine workers */
    uint32_t* byzantine_indices;            /**< Indices of Byzantine workers */
    float* worker_scores;                   /**< Per-worker Byzantine scores */

    /* Detection details */
    char description[256];                  /**< Human-readable description */
    uint64_t detection_time_us;             /**< Time taken for detection */

    /* Recommended action */
    bool quarantine_recommended;            /**< Should quarantine workers */
    bool halt_training_recommended;         /**< Should halt training */
    bool rollback_recommended;              /**< Should rollback model */
} security_byzantine_result_t;

/**
 * @brief Gradient validation result
 *
 * WHAT: Result of gradient validation for a worker
 * WHY:  Provide detailed validation outcome
 * HOW:  Returned by validate_gradients function
 */
typedef struct {
    bool is_valid;                          /**< Whether gradient is valid */
    bool is_suspicious;                     /**< Whether gradient is suspicious */
    float anomaly_score;                    /**< Anomaly score [0-1] */

    /* Validation details */
    float gradient_norm;                    /**< Computed gradient norm */
    float norm_deviation;                   /**< Deviation from expected norm */
    float consistency_score;                /**< Consistency with other workers */

    /* Flags */
    bool norm_exceeded;                     /**< Norm threshold exceeded */
    bool variance_exceeded;                 /**< Variance threshold exceeded */
    bool outlier_detected;                  /**< Statistical outlier detected */
    bool nan_inf_detected;                  /**< NaN or Inf values detected */

    /* Recommended action */
    bool reject_recommended;                /**< Should reject gradient */
    bool quarantine_recommended;            /**< Should quarantine worker */
    bool trust_penalty_recommended;         /**< Should penalize trust score */
} security_gradient_validation_result_t;

/**
 * @brief Distributed checkpoint information
 *
 * WHAT: Metadata for secure distributed checkpoint
 * WHY:  Track checkpoint integrity across nodes
 * HOW:  Stored alongside checkpoint data
 */
typedef struct {
    char name[SECURITY_DISTRIBUTED_CHECKPOINT_NAME_MAX];
    uint64_t round;                         /**< Training round */
    uint64_t timestamp_ms;                  /**< Creation timestamp */
    uint8_t model_hash[SECURITY_DISTRIBUTED_HASH_SIZE]; /**< SHA-256 of model */
    uint8_t consensus_hash[SECURITY_DISTRIBUTED_HASH_SIZE]; /**< Consensus hash */

    /* Consensus info */
    uint32_t participating_workers;         /**< Workers in consensus */
    uint32_t agreeing_workers;              /**< Workers agreeing on hash */
    float consensus_ratio;                  /**< Agreement ratio */
    bool consensus_reached;                 /**< Whether consensus reached */

    /* Verification */
    bool is_verified;                       /**< Whether verified */
    security_checkpoint_result_t status;    /**< Verification status */
    float loss_at_checkpoint;               /**< Loss at checkpoint time */
} security_distributed_checkpoint_t;

/**
 * @brief Security-distributed training bridge state
 */
struct security_distributed_training_bridge {
    bridge_base_t base;                     /**< MUST be first: base bridge */

    /* Connected systems */
    distributed_coordinator_t coordinator;  /**< Distributed coordinator */
    bbb_system_t bbb;                       /**< Blood-Brain Barrier system */

    /* Configuration */
    security_distributed_training_config_t config;

    /* Current state */
    security_distributed_phase_t phase;

    /* Effects */
    security_distributed_effects_t security_effects;   /**< Security -> Training */
    distributed_security_effects_t training_effects;   /**< Training -> Security */

    /* Worker registry */
    uint32_t num_workers;
    security_worker_info_t workers[SECURITY_DISTRIBUTED_MAX_WORKERS];

    /* Checkpoint management */
    uint32_t num_checkpoints;
    security_distributed_checkpoint_t checkpoints[SECURITY_DISTRIBUTED_MAX_CHECKPOINTS];
    uint32_t current_checkpoint_idx;

    /* Training round tracking */
    uint64_t current_round;
    float* round_gradient_norms;            /**< Per-worker norms this round */
    uint32_t round_gradient_count;

    /* Connection state */
    bool coordinator_connected;
    bool bbb_connected;

    /* Timestamps */
    uint64_t creation_time_ms;
    uint64_t last_update_ms;
    uint64_t last_byzantine_check_ms;
};

/**
 * @brief Security-distributed training bridge statistics
 */
typedef struct {
    /* Worker stats */
    uint64_t total_workers_registered;
    uint64_t workers_quarantined;
    uint64_t workers_removed;
    uint32_t current_active_workers;
    uint32_t current_trusted_workers;
    uint32_t current_quarantined_workers;

    /* Byzantine detection */
    uint64_t byzantine_checks;
    uint64_t byzantine_detections;
    uint64_t byzantine_by_type[SECURITY_BYZANTINE_COUNT];

    /* Gradient validation */
    uint64_t gradients_validated;
    uint64_t gradients_rejected;
    uint64_t gradients_suspicious;
    float avg_gradient_norm;
    float max_gradient_norm;

    /* Trust scoring */
    uint64_t trust_updates;
    uint64_t trust_penalties;
    uint64_t trust_recoveries;
    float avg_trust_score;

    /* Checkpointing */
    uint64_t checkpoints_created;
    uint64_t checkpoints_verified;
    uint64_t checkpoints_failed;
    uint64_t consensus_rounds;

    /* Connection status */
    bool coordinator_connected;
    bool bbb_connected;
    bool bio_async_connected;

    /* Current state */
    security_distributed_phase_t current_phase;
    float current_threat_level;
    float current_byzantine_ratio;

    /* Timing */
    uint64_t total_rounds;
    uint64_t total_updates;
    float avg_byzantine_check_time_us;
    float avg_validation_time_us;
    uint64_t uptime_ms;
} security_distributed_training_stats_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with secure defaults
 * HOW:  Return struct with all features enabled
 *
 * @param config Output configuration
 * @return 0 on success, error code on failure
 */
int security_distributed_training_default_config(
    security_distributed_training_config_t* config
);

/**
 * @brief Create security-distributed training bridge
 *
 * WHAT: Initialize security-distributed training integration
 * WHY:  Set up bidirectional security-training coordination
 * HOW:  Allocate state, initialize mutex, register bio-async
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
security_distributed_training_bridge_t* security_distributed_training_bridge_create(
    const security_distributed_training_config_t* config
);

/**
 * @brief Destroy security-distributed training bridge
 *
 * WHAT: Clean up resources
 * WHY:  Proper resource deallocation
 * HOW:  Free mutex, unregister bio-async, free structure
 *
 * @param bridge Bridge to destroy
 */
void security_distributed_training_bridge_destroy(
    security_distributed_training_bridge_t* bridge
);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect to distributed training coordinator
 *
 * WHAT: Link to distributed training coordinator
 * WHY:  Enable coordination with distributed training
 * HOW:  Store opaque handle
 *
 * @param bridge Security-distributed training bridge
 * @param coordinator Distributed coordinator handle
 * @return 0 on success, error code on failure
 */
int security_distributed_training_connect_coordinator(
    security_distributed_training_bridge_t* bridge,
    distributed_coordinator_t coordinator
);

/**
 * @brief Connect to Blood-Brain Barrier
 *
 * WHAT: Link to BBB for security coordination
 * WHY:  Report threats, use signing keys
 * HOW:  Store handle for BBB operations
 *
 * @param bridge Security-distributed training bridge
 * @param bbb BBB system handle
 * @return 0 on success, error code on failure
 */
int security_distributed_training_connect_bbb(
    security_distributed_training_bridge_t* bridge,
    bbb_system_t bbb
);

/**
 * @brief Disconnect distributed training coordinator
 *
 * @param bridge Security-distributed training bridge
 * @return 0 on success
 */
int security_distributed_training_disconnect_coordinator(
    security_distributed_training_bridge_t* bridge
);

/**
 * @brief Disconnect BBB
 *
 * @param bridge Security-distributed training bridge
 * @return 0 on success
 */
int security_distributed_training_disconnect_bbb(
    security_distributed_training_bridge_t* bridge
);

/* ============================================================================
 * Worker Management API
 * ============================================================================ */

/**
 * @brief Register a worker
 *
 * WHAT: Register a new distributed training worker
 * WHY:  Track worker identity and behavior
 * HOW:  Add to worker registry with initial trust level
 *
 * @param bridge Security-distributed training bridge
 * @param worker_id Worker identifier
 * @param worker_key Worker's authentication key (can be NULL)
 * @return 0 on success, error code on failure
 */
int security_distributed_training_register_worker(
    security_distributed_training_bridge_t* bridge,
    const char* worker_id,
    const uint8_t* worker_key
);

/**
 * @brief Unregister a worker
 *
 * WHAT: Remove a worker from registry
 * WHY:  Clean up when worker leaves
 * HOW:  Mark worker as removed
 *
 * @param bridge Security-distributed training bridge
 * @param worker_id Worker identifier
 * @return 0 on success, error code on failure
 */
int security_distributed_training_unregister_worker(
    security_distributed_training_bridge_t* bridge,
    const char* worker_id
);

/**
 * @brief Get worker information
 *
 * WHAT: Query information about a worker
 * WHY:  Check worker status and history
 * HOW:  Look up in registry
 *
 * @param bridge Security-distributed training bridge
 * @param worker_id Worker identifier
 * @param info Output: worker information
 * @return 0 on success, error code on failure
 */
int security_distributed_training_get_worker_info(
    const security_distributed_training_bridge_t* bridge,
    const char* worker_id,
    security_worker_info_t* info
);

/**
 * @brief Get worker trust level
 *
 * WHAT: Query worker's current trust level
 * WHY:  Determine worker's participation rights
 * HOW:  Look up in registry
 *
 * @param bridge Security-distributed training bridge
 * @param worker_id Worker identifier
 * @return Trust level (QUARANTINED if not found)
 */
security_worker_trust_t security_distributed_training_get_worker_trust(
    const security_distributed_training_bridge_t* bridge,
    const char* worker_id
);

/**
 * @brief Set worker trust level
 *
 * WHAT: Update worker's trust level
 * WHY:  Adjust trust based on behavior
 * HOW:  Update registry entry
 *
 * @param bridge Security-distributed training bridge
 * @param worker_id Worker identifier
 * @param trust_level New trust level
 * @return 0 on success, error code on failure
 */
int security_distributed_training_set_worker_trust(
    security_distributed_training_bridge_t* bridge,
    const char* worker_id,
    security_worker_trust_t trust_level
);

/**
 * @brief Score worker trust
 *
 * WHAT: Calculate worker's continuous trust score
 * WHY:  Fine-grained trust assessment
 * HOW:  Analyze behavior history
 *
 * @param bridge Security-distributed training bridge
 * @param worker_id Worker identifier
 * @param score Output: trust score [0-1]
 * @return 0 on success, error code on failure
 */
int security_distributed_training_score_worker(
    security_distributed_training_bridge_t* bridge,
    const char* worker_id,
    float* score
);

/**
 * @brief Quarantine a worker
 *
 * WHAT: Isolate a malicious or suspicious worker
 * WHY:  Prevent further damage from Byzantine worker
 * HOW:  Set trust to QUARANTINED, remove from participation
 *
 * @param bridge Security-distributed training bridge
 * @param worker_id Worker identifier
 * @param reason Reason for quarantine
 * @return 0 on success, error code on failure
 */
int security_distributed_training_quarantine_worker(
    security_distributed_training_bridge_t* bridge,
    const char* worker_id,
    const char* reason
);

/**
 * @brief Release a worker from quarantine
 *
 * WHAT: Restore a quarantined worker
 * WHY:  Allow reintegration after investigation
 * HOW:  Set trust to PROBATION, allow participation
 *
 * @param bridge Security-distributed training bridge
 * @param worker_id Worker identifier
 * @return 0 on success, error code on failure
 */
int security_distributed_training_release_worker(
    security_distributed_training_bridge_t* bridge,
    const char* worker_id
);

/**
 * @brief List registered workers
 *
 * WHAT: Get list of all registered workers
 * WHY:  Enumerate workers for management
 * HOW:  Return worker info array
 *
 * @param bridge Security-distributed training bridge
 * @param workers Output: array of worker info
 * @param max_workers Size of output array
 * @return Number of workers, or negative on error
 */
int security_distributed_training_list_workers(
    const security_distributed_training_bridge_t* bridge,
    security_worker_info_t* workers,
    uint32_t max_workers
);

/* ============================================================================
 * Byzantine Detection API
 * ============================================================================ */

/**
 * @brief Detect Byzantine workers
 *
 * WHAT: Scan for Byzantine behavior across workers
 * WHY:  Identify malicious or faulty workers
 * HOW:  Statistical analysis, voting, consistency checks
 *
 * DETECTION METHODS:
 * - Gradient statistics: Identify outliers in norm/direction
 * - Consistency: Compare workers against each other
 * - Behavior: Track historical patterns
 * - Contribution: Verify actual work contribution
 *
 * @param bridge Security-distributed training bridge
 * @param result Output: detection result
 * @return 0 on success, error code on failure
 */
int security_distributed_training_detect_byzantine(
    security_distributed_training_bridge_t* bridge,
    security_byzantine_result_t* result
);

/**
 * @brief Report worker anomaly
 *
 * WHAT: Report anomalous behavior from a worker
 * WHY:  Aggregate signals for Byzantine detection
 * HOW:  Update worker behavior history
 *
 * @param bridge Security-distributed training bridge
 * @param worker_id Worker identifier
 * @param anomaly_score Anomaly score [0-1]
 * @param reason Description of anomaly
 * @return 0 on success, error code on failure
 */
int security_distributed_training_report_worker_anomaly(
    security_distributed_training_bridge_t* bridge,
    const char* worker_id,
    float anomaly_score,
    const char* reason
);

/* ============================================================================
 * Gradient Validation API
 * ============================================================================ */

/**
 * @brief Validate gradients from a worker
 *
 * WHAT: Validate gradient submission from a worker
 * WHY:  Detect poisoned or malicious gradients
 * HOW:  Statistical analysis, bounds checking, consistency
 *
 * VALIDATION CHECKS:
 * - Norm bounds: Gradient norm within expected range
 * - NaN/Inf: No invalid values
 * - Variance: Variance within expected range
 * - Consistency: Agreement with other workers
 * - Outlier: Statistical outlier detection
 *
 * @param bridge Security-distributed training bridge
 * @param worker_id Worker identifier
 * @param gradients Gradient array
 * @param num_params Number of parameters
 * @param result Output: validation result
 * @return 0 on success, error code on failure
 */
int security_distributed_training_validate_gradients(
    security_distributed_training_bridge_t* bridge,
    const char* worker_id,
    const float* gradients,
    uint32_t num_params,
    security_gradient_validation_result_t* result
);

/**
 * @brief Validate aggregated gradients
 *
 * WHAT: Validate the result of gradient aggregation
 * WHY:  Detect anomalies in aggregated gradient
 * HOW:  Statistical analysis, historical comparison
 *
 * @param bridge Security-distributed training bridge
 * @param aggregated_gradients Aggregated gradient array
 * @param num_params Number of parameters
 * @param anomaly_score Output: anomaly score [0-1]
 * @return true if anomaly detected, false otherwise
 */
bool security_distributed_training_validate_aggregated(
    security_distributed_training_bridge_t* bridge,
    const float* aggregated_gradients,
    uint32_t num_params,
    float* anomaly_score
);

/**
 * @brief Get recommended aggregation method
 *
 * WHAT: Get security-recommended aggregation method
 * WHY:  Adapt aggregation to threat level
 * HOW:  Based on Byzantine ratio and threat level
 *
 * @param bridge Security-distributed training bridge
 * @return Recommended aggregation method
 */
security_grad_aggregation_t security_distributed_training_get_aggregation_method(
    const security_distributed_training_bridge_t* bridge
);

/* ============================================================================
 * Secure Checkpointing API
 * ============================================================================ */

/**
 * @brief Create secure distributed checkpoint
 *
 * WHAT: Create checkpoint with consensus across nodes
 * WHY:  Enable verified rollback in distributed setting
 * HOW:  Compute hash, gather consensus, sign
 *
 * @param bridge Security-distributed training bridge
 * @param checkpoint_name Name for checkpoint
 * @param model_weights Model weight array
 * @param num_weights Number of weights
 * @param round Current training round
 * @return 0 on success, error code on failure
 */
int security_distributed_training_secure_checkpoint(
    security_distributed_training_bridge_t* bridge,
    const char* checkpoint_name,
    const float* model_weights,
    uint32_t num_weights,
    uint64_t round
);

/**
 * @brief Verify distributed checkpoint
 *
 * WHAT: Verify checkpoint integrity across nodes
 * WHY:  Ensure checkpoint is trustworthy
 * HOW:  Hash verification, consensus check
 *
 * @param bridge Security-distributed training bridge
 * @param checkpoint_name Checkpoint to verify
 * @param model_weights Model weights to verify
 * @param num_weights Number of weights
 * @return Verification result
 */
security_checkpoint_result_t security_distributed_training_verify_checkpoint(
    security_distributed_training_bridge_t* bridge,
    const char* checkpoint_name,
    const float* model_weights,
    uint32_t num_weights
);

/**
 * @brief Get checkpoint information
 *
 * WHAT: Query metadata for a checkpoint
 * WHY:  Determine checkpoint validity
 * HOW:  Look up in checkpoint registry
 *
 * @param bridge Security-distributed training bridge
 * @param checkpoint_name Checkpoint to query
 * @param info Output: checkpoint information
 * @return 0 on success, error code on failure
 */
int security_distributed_training_get_checkpoint_info(
    const security_distributed_training_bridge_t* bridge,
    const char* checkpoint_name,
    security_distributed_checkpoint_t* info
);

/**
 * @brief List checkpoints
 *
 * WHAT: Get list of all checkpoints
 * WHY:  Enumerate available rollback points
 * HOW:  Return checkpoint info array
 *
 * @param bridge Security-distributed training bridge
 * @param checkpoints Output: array of checkpoint info
 * @param max_checkpoints Size of output array
 * @return Number of checkpoints, or negative on error
 */
int security_distributed_training_list_checkpoints(
    const security_distributed_training_bridge_t* bridge,
    security_distributed_checkpoint_t* checkpoints,
    uint32_t max_checkpoints
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update security -> training effects
 *
 * WHAT: Compute security constraints for training
 * WHY:  Provide protection measures to training
 * HOW:  Aggregate threat signals, compute bounds
 *
 * @param bridge Security-distributed training bridge
 * @return 0 on success, error code on failure
 */
int security_distributed_training_update_security_effects(
    security_distributed_training_bridge_t* bridge
);

/**
 * @brief Update training -> security effects
 *
 * WHAT: Process training signals for security
 * WHY:  Detect threats from training behavior
 * HOW:  Analyze training metrics for anomalies
 *
 * @param bridge Security-distributed training bridge
 * @param round Current training round
 * @param loss Current loss value
 * @param active_workers Number of active workers
 * @return 0 on success, error code on failure
 */
int security_distributed_training_update_training_effects(
    security_distributed_training_bridge_t* bridge,
    uint64_t round,
    float loss,
    uint32_t active_workers
);

/**
 * @brief Get security effects
 *
 * WHAT: Query current security constraints
 * WHY:  Training needs protection parameters
 * HOW:  Copy internal effects to output
 *
 * @param bridge Security-distributed training bridge
 * @param effects Output: security effects
 * @return 0 on success, error code on failure
 */
int security_distributed_training_get_security_effects(
    const security_distributed_training_bridge_t* bridge,
    security_distributed_effects_t* effects
);

/**
 * @brief Get training effects
 *
 * WHAT: Query current training signals
 * WHY:  Security needs training state
 * HOW:  Copy internal effects to output
 *
 * @param bridge Security-distributed training bridge
 * @param effects Output: training effects
 * @return 0 on success, error code on failure
 */
int security_distributed_training_get_training_effects(
    const security_distributed_training_bridge_t* bridge,
    distributed_security_effects_t* effects
);

/* ============================================================================
 * Bio-async Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Security-distributed training bridge
 * @return 0 on success, error code on failure
 */
int security_distributed_training_connect_bio_async(
    security_distributed_training_bridge_t* bridge
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Security-distributed training bridge
 * @return 0 on success, error code on failure
 */
int security_distributed_training_disconnect_bio_async(
    security_distributed_training_bridge_t* bridge
);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Security-distributed training bridge
 * @return true if connected
 */
bool security_distributed_training_is_bio_async_connected(
    const security_distributed_training_bridge_t* bridge
);

/* ============================================================================
 * Query and Statistics API
 * ============================================================================ */

/**
 * @brief Get current phase
 *
 * @param bridge Security-distributed training bridge
 * @return Current phase
 */
security_distributed_phase_t security_distributed_training_get_phase(
    const security_distributed_training_bridge_t* bridge
);

/**
 * @brief Get current threat level
 *
 * @param bridge Security-distributed training bridge
 * @return Threat level [0-1]
 */
float security_distributed_training_get_threat_level(
    const security_distributed_training_bridge_t* bridge
);

/**
 * @brief Get estimated Byzantine ratio
 *
 * @param bridge Security-distributed training bridge
 * @return Estimated ratio of Byzantine workers [0-1]
 */
float security_distributed_training_get_byzantine_ratio(
    const security_distributed_training_bridge_t* bridge
);

/**
 * @brief Check if under active attack
 *
 * @param bridge Security-distributed training bridge
 * @return true if attack detected
 */
bool security_distributed_training_is_under_attack(
    const security_distributed_training_bridge_t* bridge
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Security-distributed training bridge
 * @param stats Output: statistics
 * @return 0 on success, error code on failure
 */
int security_distributed_training_get_stats(
    const security_distributed_training_bridge_t* bridge,
    security_distributed_training_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Security-distributed training bridge
 * @return 0 on success, error code on failure
 */
int security_distributed_training_reset_stats(
    security_distributed_training_bridge_t* bridge
);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

/**
 * @brief Convert worker trust level to string
 *
 * @param trust Trust level
 * @return Human-readable string
 */
const char* security_worker_trust_to_string(security_worker_trust_t trust);

/**
 * @brief Convert Byzantine type to string
 *
 * @param type Byzantine type
 * @return Human-readable string
 */
const char* security_byzantine_type_to_string(security_byzantine_type_t type);

/**
 * @brief Convert aggregation method to string
 *
 * @param method Aggregation method
 * @return Human-readable string
 */
const char* security_grad_aggregation_to_string(security_grad_aggregation_t method);

/**
 * @brief Convert phase to string
 *
 * @param phase Bridge phase
 * @return Human-readable string
 */
const char* security_distributed_phase_to_string(security_distributed_phase_t phase);

/**
 * @brief Convert checkpoint result to string
 *
 * @param result Checkpoint result
 * @return Human-readable string
 */
const char* security_checkpoint_result_to_string(security_checkpoint_result_t result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_DISTRIBUTED_TRAINING_BRIDGE_H */
