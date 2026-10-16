/**
 * @file nimcp_security_training_bridge.h
 * @brief Security-Training Integration Bridge for NIMCP
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Bidirectional integration between security systems and training pipeline
 * WHY:  Protect ML training from data poisoning, gradient manipulation, and model tampering
 * HOW:  Security validates data sources, detects poisoning, sanitizes gradients;
 *       Training reports suspicious samples, model drift, and anomalies
 *
 * THREAT MODEL:
 * =============
 * ```
 * ATTACK VECTOR                      DEFENSE MECHANISM
 * --------------------------------------------------------------------------------------------------------
 * Label flipping (mislabeled data)   → Statistical anomaly detection on label distributions
 * Backdoor triggers                  → Pattern matching + anomaly detection on inputs
 * Trojan insertion                   → Model integrity verification with hash chains
 * Gradient manipulation              → Gradient clipping + sanitization bounds
 * Model weight tampering             → Secure checkpointing with cryptographic signatures
 * Concept drift attacks              → Distribution shift monitoring
 * Data source compromise             → Trust level verification + provenance tracking
 * ```
 *
 * ARCHITECTURE:
 * =============
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────────┐
 * │                     SECURITY-TRAINING BRIDGE                                 │
 * ├─────────────────────────────────────────────────────────────────────────────┤
 * │                                                                              │
 * │  SECURITY → TRAINING (Protective Measures)                                   │
 * │  ┌─────────────────┐       ┌──────────────────────────────────┐             │
 * │  │  BBB/Anomaly    │       │     Training Pipeline            │             │
 * │  │  Detector       │──────→│  - Data filters (trust levels)   │             │
 * │  │                 │       │  - Gradient bounds (sanitization)│             │
 * │  └─────────────────┘       │  - Model verification (integrity)│             │
 * │                            └──────────────────────────────────┘             │
 * │                                                                              │
 * │  TRAINING → SECURITY (Threat Signals)                                        │
 * │  ┌──────────────────────────────────┐       ┌─────────────────┐             │
 * │  │     Training Pipeline            │       │  Security       │             │
 * │  │  - Suspicious samples detected   │──────→│  - Alert BBB    │             │
 * │  │  - Model drift observed          │       │  - Log anomaly  │             │
 * │  │  - Gradient anomalies            │       │  - Quarantine   │             │
 * │  └──────────────────────────────────┘       └─────────────────┘             │
 * │                                                                              │
 * └─────────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * DATA SOURCE TRUST MODEL:
 * ========================
 * ```
 * Trust Level    | Validation     | Allowed Operations
 * ---------------|----------------|---------------------------------------------
 * UNTRUSTED      | Full scrutiny  | Read-only, no training until verified
 * VERIFIED       | Spot checks    | Training with monitoring
 * CERTIFIED      | Signature only | Training with standard monitoring
 * INTERNAL       | Minimal        | Full training access
 * ```
 *
 * POISONING DETECTION:
 * ====================
 * ```
 * Attack Type          | Detection Method                     | Response
 * ---------------------|--------------------------------------|------------------
 * Label Flip           | Label distribution anomaly           | Quarantine sample
 * Backdoor             | Input pattern anomaly + trigger scan | Block + alert
 * Trojan               | Model behavior divergence            | Rollback model
 * Gradient Manipulation| Gradient norm/direction anomaly      | Sanitize gradient
 * ```
 *
 * USAGE EXAMPLE:
 * ==============
 * ```c
 * // Create security-training bridge
 * security_training_config_t config = security_training_default_config();
 * config.enable_poisoning_detection = true;
 * config.gradient_clip_norm = 1.0f;
 * security_training_bridge_t* bridge = security_training_bridge_create(&config);
 *
 * // Connect to systems
 * security_training_connect_training_pipeline(bridge, training_ctx);
 * security_training_connect_bbb(bridge, bbb_system);
 * security_training_connect_anomaly_detector(bridge, anomaly_detector);
 *
 * // Training loop
 * for (int step = 0; step < max_steps; step++) {
 *     // Validate data source before using
 *     if (!security_training_validate_data_source(bridge, data_source)) {
 *         continue;  // Skip untrusted data
 *     }
 *
 *     // Detect poisoning in batch
 *     security_poisoning_result_t result;
 *     if (security_training_detect_poisoning(bridge, batch, &result)) {
 *         // Handle poisoned samples
 *         security_training_quarantine_samples(bridge, result.suspicious_indices);
 *     }
 *
 *     // ... compute gradients ...
 *
 *     // Sanitize gradients before applying
 *     security_training_sanitize_gradients(bridge, gradients, num_params);
 *
 *     // Periodic model integrity check
 *     if (step % 1000 == 0) {
 *         security_training_verify_model_integrity(bridge);
 *     }
 * }
 * ```
 *
 * GOTCHAS:
 * ========
 * - Data source trust levels are persistent across sessions
 * - Gradient sanitization modifies gradients in-place
 * - Model checkpoints are signed with BBB keys
 * - Rollback clears all training state since checkpoint
 * - Concept drift detection requires baseline establishment
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

#ifndef NIMCP_SECURITY_TRAINING_BRIDGE_H
#define NIMCP_SECURITY_TRAINING_BRIDGE_H

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

#define SECURITY_TRAINING_MODULE_NAME      "security_training"
#define SECURITY_TRAINING_MODULE_VERSION   "1.0.0"

/* Gradient sanitization defaults */
#define SECURITY_TRAINING_DEFAULT_GRAD_CLIP_NORM       1.0f
#define SECURITY_TRAINING_DEFAULT_GRAD_CLIP_VALUE      10.0f
#define SECURITY_TRAINING_DEFAULT_GRAD_MIN_VALUE       -100.0f
#define SECURITY_TRAINING_DEFAULT_GRAD_MAX_VALUE       100.0f

/* Poisoning detection thresholds */
#define SECURITY_TRAINING_DEFAULT_LABEL_FLIP_THRESHOLD     0.1f
#define SECURITY_TRAINING_DEFAULT_BACKDOOR_THRESHOLD       0.8f
#define SECURITY_TRAINING_DEFAULT_TROJAN_THRESHOLD         0.7f
#define SECURITY_TRAINING_DEFAULT_GRADIENT_ANOMALY_THRESHOLD 0.9f

/* Model integrity */
#define SECURITY_TRAINING_HASH_SIZE                    32
#define SECURITY_TRAINING_MAX_CHECKPOINTS              64
#define SECURITY_TRAINING_CHECKPOINT_NAME_MAX          256

/* Concept drift */
#define SECURITY_TRAINING_DRIFT_WINDOW_SIZE            1000
#define SECURITY_TRAINING_DEFAULT_DRIFT_THRESHOLD      0.3f

/* Bio-async */
#define SECURITY_TRAINING_BIO_INBOX_CAPACITY           64

/* Limits */
#define SECURITY_TRAINING_MAX_SUSPICIOUS_SAMPLES       1024
#define SECURITY_TRAINING_MAX_DATA_SOURCES             256

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct security_training_bridge security_training_bridge_t;
typedef struct bbb_system_struct* bbb_system_t;
typedef struct nimcp_anomaly_detector_internal* nimcp_anomaly_detector_t;
typedef struct nimcp_optimizer_context nimcp_optimizer_context_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Data poisoning attack types
 *
 * WHAT: Categories of data poisoning attacks
 * WHY:  Different attacks require different detection methods
 * HOW:  Each type has specialized detection logic
 */
typedef enum {
    SECURITY_POISONING_NONE = 0,           /**< No poisoning detected */
    SECURITY_POISONING_LABEL_FLIP,         /**< Mislabeled samples */
    SECURITY_POISONING_BACKDOOR,           /**< Hidden trigger patterns */
    SECURITY_POISONING_TROJAN,             /**< Model behavior modification */
    SECURITY_POISONING_GRADIENT_MANIPULATION, /**< Adversarial gradient attacks */
    SECURITY_POISONING_DATA_INJECTION,     /**< Injected malicious samples */
    SECURITY_POISONING_FEATURE_COLLISION,  /**< Collision attacks on features */
    SECURITY_POISONING_COUNT
} security_poisoning_type_t;

/**
 * @brief Data source trust levels
 *
 * WHAT: Levels of trust for data sources
 * WHY:  Untrusted data requires more validation
 * HOW:  Higher trust → less validation overhead
 */
typedef enum {
    SECURITY_TRUST_UNTRUSTED = 0,          /**< Unknown/untrusted source */
    SECURITY_TRUST_VERIFIED,               /**< Verified but not certified */
    SECURITY_TRUST_CERTIFIED,              /**< Certified data source */
    SECURITY_TRUST_INTERNAL                /**< Internal trusted source */
} security_data_trust_t;

/**
 * @brief Security-training bridge phase
 *
 * WHAT: Current operational phase of the bridge
 * WHY:  Different phases have different behaviors
 * HOW:  Phases transition based on training state
 */
typedef enum {
    SECURITY_TRAINING_PHASE_INACTIVE = 0,  /**< Bridge not active */
    SECURITY_TRAINING_PHASE_MONITORING,    /**< Passive monitoring only */
    SECURITY_TRAINING_PHASE_PROTECTING,    /**< Active protection enabled */
    SECURITY_TRAINING_PHASE_RESPONDING,    /**< Responding to detected threat */
    SECURITY_TRAINING_PHASE_RECOVERY       /**< Recovering from incident */
} security_training_phase_t;

/**
 * @brief Gradient sanitization mode
 *
 * WHAT: Methods for sanitizing gradients
 * WHY:  Different modes trade off protection vs. training quality
 * HOW:  Applied during gradient update step
 */
typedef enum {
    SECURITY_GRAD_SANITIZE_NONE = 0,       /**< No sanitization */
    SECURITY_GRAD_SANITIZE_CLIP_NORM,      /**< Clip by global norm */
    SECURITY_GRAD_SANITIZE_CLIP_VALUE,     /**< Clip by element value */
    SECURITY_GRAD_SANITIZE_CLIP_BOTH,      /**< Clip norm and value */
    SECURITY_GRAD_SANITIZE_BOUND,          /**< Apply min/max bounds */
    SECURITY_GRAD_SANITIZE_DIFFERENTIAL    /**< Differential privacy noise */
} security_grad_sanitize_mode_t;

/**
 * @brief Model integrity verification result
 *
 * WHAT: Result of model integrity check
 * WHY:  Determine if model has been tampered with
 * HOW:  Hash comparison with signed checkpoints
 */
typedef enum {
    SECURITY_INTEGRITY_OK = 0,             /**< Model integrity verified */
    SECURITY_INTEGRITY_HASH_MISMATCH,      /**< Hash does not match */
    SECURITY_INTEGRITY_SIGNATURE_INVALID,  /**< Signature verification failed */
    SECURITY_INTEGRITY_CHECKPOINT_MISSING, /**< No checkpoint available */
    SECURITY_INTEGRITY_TAMPERED            /**< Definite tampering detected */
} security_integrity_result_t;

/* ============================================================================
 * Core Structures
 * ============================================================================ */

/**
 * @brief Configuration for security-training bridge
 */
typedef struct {
    /* Feature enables */
    bool enable_data_validation;           /**< Enable data source validation */
    bool enable_poisoning_detection;       /**< Enable poisoning detection */
    bool enable_gradient_sanitization;     /**< Enable gradient sanitization */
    bool enable_model_verification;        /**< Enable model integrity checks */
    bool enable_concept_drift_detection;   /**< Enable concept drift monitoring */
    bool enable_secure_checkpointing;      /**< Enable signed checkpoints */

    /* Gradient sanitization */
    security_grad_sanitize_mode_t grad_sanitize_mode;
    float gradient_clip_norm;              /**< Max gradient norm (default: 1.0) */
    float gradient_clip_value;             /**< Max gradient value (default: 10.0) */
    float gradient_min_bound;              /**< Min gradient bound (default: -100.0) */
    float gradient_max_bound;              /**< Max gradient bound (default: 100.0) */
    float differential_privacy_epsilon;    /**< DP epsilon (if mode is differential) */

    /* Poisoning detection thresholds */
    float label_flip_threshold;            /**< Label flip detection threshold */
    float backdoor_threshold;              /**< Backdoor detection threshold */
    float trojan_threshold;                /**< Trojan detection threshold */
    float gradient_anomaly_threshold;      /**< Gradient anomaly threshold */

    /* Model verification */
    uint32_t verification_interval_steps;  /**< Steps between verifications */
    bool verify_on_checkpoint;             /**< Verify before checkpoint */

    /* Concept drift */
    uint32_t drift_window_size;            /**< Window for drift detection */
    float drift_threshold;                 /**< Drift detection threshold */

    /* Data source defaults */
    security_data_trust_t default_trust_level;
    bool require_source_verification;      /**< Require verification for new sources */

    /* Checkpointing */
    char checkpoint_directory[256];        /**< Directory for secure checkpoints */
    bool sign_checkpoints;                 /**< Sign checkpoints with BBB key */

    /* Bio-async */
    bool enable_bio_async;                 /**< Enable bio-async messaging */
    uint32_t bio_inbox_capacity;           /**< Message queue size */

    /* Logging */
    bool enable_logging;                   /**< Enable detailed logging */
    bool log_all_validations;              /**< Log every validation (verbose) */
} security_training_config_t;

/**
 * @brief Security effects on training (Security -> Training)
 *
 * WHAT: Security-derived constraints on training
 * WHY:  Communicate protection measures to training pipeline
 * HOW:  Updated by security modules, consumed by training
 */
typedef struct {
    /* Data filtering */
    security_data_trust_t min_trust_level; /**< Minimum trust for data usage */
    uint32_t blocked_source_count;         /**< Number of blocked sources */
    bool data_quarantine_active;           /**< Data quarantine in effect */

    /* Gradient bounds */
    float gradient_clip_norm;              /**< Current gradient norm limit */
    float gradient_clip_value;             /**< Current gradient value limit */
    float gradient_scale_factor;           /**< Scale factor for gradients (0-1) */
    bool gradient_sanitization_active;     /**< Sanitization enabled */

    /* Model protection */
    bool model_locked;                     /**< Model updates blocked */
    bool checkpoint_required;              /**< Checkpoint before continuing */
    bool rollback_recommended;             /**< Rollback to checkpoint recommended */
    uint64_t last_safe_checkpoint_step;    /**< Last known-good checkpoint step */

    /* Threat level */
    float threat_level;                    /**< Overall threat level [0-1] */
    bool under_attack;                     /**< Active attack detected */

    /* Metadata */
    uint64_t last_update_ms;               /**< When effects were last updated */
    bool valid;                            /**< Whether effects are current */
} security_training_effects_t;

/**
 * @brief Training effects on security (Training -> Security)
 *
 * WHAT: Training-derived signals for security monitoring
 * WHY:  Alert security to potential threats
 * HOW:  Updated by training pipeline, consumed by security
 */
typedef struct {
    /* Suspicious samples */
    uint32_t suspicious_sample_count;      /**< Number of suspicious samples */
    uint32_t* suspicious_sample_indices;   /**< Indices of suspicious samples */
    float* suspicion_scores;               /**< Per-sample suspicion scores */

    /* Model drift indicators */
    float loss_drift;                      /**< Drift in loss distribution */
    float gradient_drift;                  /**< Drift in gradient statistics */
    float activation_drift;                /**< Drift in activation patterns */
    bool drift_detected;                   /**< Significant drift detected */

    /* Anomaly signals */
    float gradient_anomaly_score;          /**< Gradient anomaly score [0-1] */
    float label_distribution_anomaly;      /**< Label dist anomaly [0-1] */
    float input_distribution_anomaly;      /**< Input dist anomaly [0-1] */

    /* Training instabilities */
    bool loss_nan_detected;                /**< NaN in loss */
    bool loss_inf_detected;                /**< Inf in loss */
    bool gradient_explosion;               /**< Gradient explosion detected */
    bool gradient_vanishing;               /**< Gradient vanishing detected */

    /* Current training state */
    uint64_t current_step;                 /**< Current training step */
    float current_loss;                    /**< Current loss value */
    float current_gradient_norm;           /**< Current gradient norm */

    /* Metadata */
    uint64_t timestamp_ms;                 /**< When effects were captured */
    bool valid;                            /**< Whether effects are current */
} training_security_effects_t;

/**
 * @brief Poisoning detection result
 *
 * WHAT: Result of poisoning detection scan
 * WHY:  Provide detailed information about detected poisoning
 * HOW:  Returned by detect_poisoning function
 */
typedef struct {
    bool poisoning_detected;               /**< Whether poisoning was detected */
    security_poisoning_type_t type;        /**< Type of poisoning detected */
    float confidence;                      /**< Detection confidence [0-1] */

    /* Affected samples */
    uint32_t num_affected;                 /**< Number of affected samples */
    uint32_t* affected_indices;            /**< Indices of affected samples */
    float* sample_scores;                  /**< Per-sample anomaly scores */

    /* Detection details */
    char description[256];                 /**< Human-readable description */
    uint64_t detection_time_us;            /**< Time taken for detection */

    /* Recommended action */
    bool quarantine_recommended;           /**< Should quarantine samples */
    bool halt_training_recommended;        /**< Should halt training */
} security_poisoning_result_t;

/**
 * @brief Model checkpoint metadata
 *
 * WHAT: Metadata for secure model checkpoint
 * WHY:  Track checkpoint integrity and provenance
 * HOW:  Stored alongside checkpoint data
 */
typedef struct {
    char name[SECURITY_TRAINING_CHECKPOINT_NAME_MAX];
    uint64_t step;                         /**< Training step */
    uint64_t timestamp_ms;                 /**< Creation timestamp */
    uint8_t model_hash[SECURITY_TRAINING_HASH_SIZE]; /**< SHA-256 of model */
    uint8_t signature[64];                 /**< BBB signature (if signed) */
    bool is_signed;                        /**< Whether checkpoint is signed */
    bool is_verified;                      /**< Whether signature verified */
    float loss_at_checkpoint;              /**< Loss at checkpoint time */
} security_checkpoint_info_t;

/**
 * @brief Security-training bridge state
 */
struct security_training_bridge {
    bridge_base_t base;                    /**< MUST be first: base bridge */

    /* Connected systems */
    void* training_pipeline;               /**< Training pipeline (opaque) */
    nimcp_optimizer_context_t* optimizer;  /**< Optimizer context */
    bbb_system_t bbb;                      /**< Blood-Brain Barrier system */
    nimcp_anomaly_detector_t anomaly_detector; /**< Anomaly detector */

    /* Configuration */
    security_training_config_t config;

    /* Current state */
    security_training_phase_t phase;

    /* Effects */
    security_training_effects_t security_effects;  /**< Security -> Training */
    training_security_effects_t training_effects;  /**< Training -> Security */

    /* Data source registry */
    uint32_t num_data_sources;
    struct {
        char name[128];
        security_data_trust_t trust_level;
        uint64_t last_validated_ms;
        uint32_t samples_processed;
        uint32_t anomalies_detected;
        bool blocked;
    } data_sources[SECURITY_TRAINING_MAX_DATA_SOURCES];

    /* Checkpoint management */
    uint32_t num_checkpoints;
    security_checkpoint_info_t checkpoints[SECURITY_TRAINING_MAX_CHECKPOINTS];
    uint32_t current_checkpoint_idx;

    /* Concept drift tracking */
    float* drift_baseline;                 /**< Baseline feature statistics */
    uint32_t drift_baseline_samples;       /**< Samples in baseline */
    float drift_score;                     /**< Current drift score */

    /* Connection state */
    bool training_connected;
    bool optimizer_connected;
    bool bbb_connected;
    bool anomaly_connected;

    /* Timestamps */
    uint64_t creation_time_ms;
    uint64_t last_update_ms;
    uint64_t last_verification_ms;
};

/**
 * @brief Security-training bridge statistics
 */
typedef struct {
    /* Detection counts */
    uint64_t total_validations;
    uint64_t data_sources_validated;
    uint64_t data_sources_blocked;
    uint64_t poisoning_scans;
    uint64_t poisoning_detections;
    uint64_t poisoning_by_type[SECURITY_POISONING_COUNT];

    /* Sanitization counts */
    uint64_t gradients_sanitized;
    uint64_t gradients_clipped_norm;
    uint64_t gradients_clipped_value;
    uint64_t gradients_bounded;

    /* Model integrity */
    uint64_t integrity_checks;
    uint64_t integrity_failures;
    uint64_t checkpoints_created;
    uint64_t rollbacks_performed;

    /* Concept drift */
    uint64_t drift_checks;
    uint64_t drift_detections;
    float max_drift_score;

    /* Suspicious samples */
    uint64_t suspicious_samples_total;
    uint64_t samples_quarantined;

    /* Connection status */
    bool training_connected;
    bool optimizer_connected;
    bool bbb_connected;
    bool anomaly_connected;
    bool bio_async_connected;

    /* Current state */
    security_training_phase_t current_phase;
    float current_threat_level;

    /* Timing */
    uint64_t total_updates;
    float avg_validation_time_us;
    float avg_poisoning_scan_time_us;
    uint64_t uptime_ms;
} security_training_stats_t;

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
int security_training_default_config(security_training_config_t* config);

/**
 * @brief Create security-training bridge
 *
 * WHAT: Initialize security-training integration
 * WHY:  Set up bidirectional security-training coordination
 * HOW:  Allocate state, initialize mutex, register bio-async
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
security_training_bridge_t* security_training_bridge_create(
    const security_training_config_t* config
);

/**
 * @brief Destroy security-training bridge
 *
 * WHAT: Clean up resources
 * WHY:  Proper resource deallocation
 * HOW:  Free mutex, unregister bio-async, free structure
 *
 * @param bridge Bridge to destroy
 */
void security_training_bridge_destroy(security_training_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect to training pipeline
 *
 * WHAT: Link to training pipeline for coordination
 * WHY:  Enable training protection and monitoring
 * HOW:  Store opaque handle
 *
 * @param bridge Security-training bridge
 * @param training Training pipeline (opaque pointer)
 * @return 0 on success, error code on failure
 */
int security_training_connect_training_pipeline(
    security_training_bridge_t* bridge,
    void* training
);

/**
 * @brief Connect to optimizer
 *
 * WHAT: Link to optimizer for gradient sanitization
 * WHY:  Enable gradient clipping and bounds
 * HOW:  Store handle for gradient operations
 *
 * @param bridge Security-training bridge
 * @param optimizer Optimizer context
 * @return 0 on success, error code on failure
 */
int security_training_connect_optimizer(
    security_training_bridge_t* bridge,
    nimcp_optimizer_context_t* optimizer
);

/**
 * @brief Connect to Blood-Brain Barrier
 *
 * WHAT: Link to BBB for security coordination
 * WHY:  Report threats, use signing keys
 * HOW:  Store handle for BBB operations
 *
 * @param bridge Security-training bridge
 * @param bbb BBB system handle
 * @return 0 on success, error code on failure
 */
int security_training_connect_bbb(
    security_training_bridge_t* bridge,
    bbb_system_t bbb
);

/**
 * @brief Connect to anomaly detector
 *
 * WHAT: Link to anomaly detector for poisoning detection
 * WHY:  Leverage ML-based anomaly detection
 * HOW:  Store handle for anomaly queries
 *
 * @param bridge Security-training bridge
 * @param detector Anomaly detector handle
 * @return 0 on success, error code on failure
 */
int security_training_connect_anomaly_detector(
    security_training_bridge_t* bridge,
    nimcp_anomaly_detector_t detector
);

/**
 * @brief Disconnect training pipeline
 *
 * @param bridge Security-training bridge
 * @return 0 on success
 */
int security_training_disconnect_training_pipeline(
    security_training_bridge_t* bridge
);

/**
 * @brief Disconnect optimizer
 *
 * @param bridge Security-training bridge
 * @return 0 on success
 */
int security_training_disconnect_optimizer(
    security_training_bridge_t* bridge
);

/**
 * @brief Disconnect BBB
 *
 * @param bridge Security-training bridge
 * @return 0 on success
 */
int security_training_disconnect_bbb(
    security_training_bridge_t* bridge
);

/**
 * @brief Disconnect anomaly detector
 *
 * @param bridge Security-training bridge
 * @return 0 on success
 */
int security_training_disconnect_anomaly_detector(
    security_training_bridge_t* bridge
);

/* ============================================================================
 * Data Source Validation API
 * ============================================================================ */

/**
 * @brief Validate data source trustworthiness
 *
 * WHAT: Verify data source meets trust requirements
 * WHY:  Prevent use of compromised or untrusted data
 * HOW:  Check trust level, provenance, recent anomalies
 *
 * VALIDATION STEPS:
 * 1. Look up source in registry (or create entry)
 * 2. Check trust level against minimum required
 * 3. Verify no recent anomalies from source
 * 4. Update validation timestamp
 *
 * @param bridge Security-training bridge
 * @param source_name Data source identifier
 * @return true if source is trusted, false otherwise
 */
bool security_training_validate_data_source(
    security_training_bridge_t* bridge,
    const char* source_name
);

/**
 * @brief Set trust level for data source
 *
 * WHAT: Update trust level for a data source
 * WHY:  Adjust trust based on verification or incidents
 * HOW:  Update registry entry
 *
 * @param bridge Security-training bridge
 * @param source_name Data source identifier
 * @param trust_level New trust level
 * @return 0 on success, error code on failure
 */
int security_training_set_source_trust(
    security_training_bridge_t* bridge,
    const char* source_name,
    security_data_trust_t trust_level
);

/**
 * @brief Block data source
 *
 * WHAT: Block a compromised data source
 * WHY:  Prevent further use of malicious data
 * HOW:  Mark source as blocked in registry
 *
 * @param bridge Security-training bridge
 * @param source_name Data source to block
 * @return 0 on success, error code on failure
 */
int security_training_block_source(
    security_training_bridge_t* bridge,
    const char* source_name
);

/**
 * @brief Get data source trust level
 *
 * WHAT: Query current trust level for source
 * WHY:  Determine validation requirements
 * HOW:  Look up in registry
 *
 * @param bridge Security-training bridge
 * @param source_name Data source identifier
 * @return Trust level (UNTRUSTED if not found)
 */
security_data_trust_t security_training_get_source_trust(
    const security_training_bridge_t* bridge,
    const char* source_name
);

/* ============================================================================
 * Poisoning Detection API
 * ============================================================================ */

/**
 * @brief Detect data poisoning attempts
 *
 * WHAT: Scan training batch for poisoning attacks
 * WHY:  Prevent model corruption from malicious data
 * HOW:  Multiple detection methods based on attack type
 *
 * DETECTION METHODS:
 * - Label flip: Statistical analysis of label distribution
 * - Backdoor: Pattern matching + input anomaly detection
 * - Trojan: Behavior analysis + model divergence
 * - Gradient manipulation: Gradient statistics anomaly
 *
 * @param bridge Security-training bridge
 * @param data Batch data to scan
 * @param data_size Size of batch data
 * @param labels Labels for batch (if available)
 * @param num_samples Number of samples in batch
 * @param result Output: detection result
 * @return 0 on success, error code on failure
 */
int security_training_detect_poisoning(
    security_training_bridge_t* bridge,
    const void* data,
    size_t data_size,
    const int32_t* labels,
    uint32_t num_samples,
    security_poisoning_result_t* result
);

/**
 * @brief Report suspicious sample
 *
 * WHAT: Report a sample flagged by training as suspicious
 * WHY:  Aggregate suspicion signals for security analysis
 * HOW:  Add to suspicious sample list, trigger anomaly check
 *
 * @param bridge Security-training bridge
 * @param sample_index Index of suspicious sample
 * @param suspicion_score Suspicion score [0-1]
 * @param reason Reason for suspicion
 * @return 0 on success, error code on failure
 */
int security_training_report_suspicious_sample(
    security_training_bridge_t* bridge,
    uint32_t sample_index,
    float suspicion_score,
    const char* reason
);

/**
 * @brief Quarantine suspicious samples
 *
 * WHAT: Remove suspicious samples from training set
 * WHY:  Prevent potential poisoning from affecting model
 * HOW:  Mark samples for exclusion
 *
 * @param bridge Security-training bridge
 * @param sample_indices Array of sample indices to quarantine
 * @param num_samples Number of samples
 * @return 0 on success, error code on failure
 */
int security_training_quarantine_samples(
    security_training_bridge_t* bridge,
    const uint32_t* sample_indices,
    uint32_t num_samples
);

/* ============================================================================
 * Gradient Sanitization API
 * ============================================================================ */

/**
 * @brief Sanitize gradients
 *
 * WHAT: Apply security bounds/clipping to gradients
 * WHY:  Prevent gradient manipulation attacks
 * HOW:  Clip norm, clip values, apply bounds based on config
 *
 * SANITIZATION MODES:
 * - CLIP_NORM: ||g|| = min(||g||, clip_norm)
 * - CLIP_VALUE: g_i = clamp(g_i, -clip_value, clip_value)
 * - BOUND: g_i = clamp(g_i, min_bound, max_bound)
 * - DIFFERENTIAL: g_i += Laplace(epsilon)
 *
 * NOTE: Modifies gradients in-place
 *
 * @param bridge Security-training bridge
 * @param gradients Gradient array (modified in-place)
 * @param num_params Number of parameters
 * @return 0 on success, error code on failure
 */
int security_training_sanitize_gradients(
    security_training_bridge_t* bridge,
    float* gradients,
    uint32_t num_params
);

/**
 * @brief Check gradient for anomalies
 *
 * WHAT: Detect anomalous gradient patterns
 * WHY:  Identify gradient manipulation attacks
 * HOW:  Statistical analysis of gradient properties
 *
 * @param bridge Security-training bridge
 * @param gradients Gradient array
 * @param num_params Number of parameters
 * @param anomaly_score Output: anomaly score [0-1]
 * @return true if anomaly detected, false otherwise
 */
bool security_training_check_gradient_anomaly(
    security_training_bridge_t* bridge,
    const float* gradients,
    uint32_t num_params,
    float* anomaly_score
);

/**
 * @brief Set gradient sanitization parameters
 *
 * WHAT: Update gradient sanitization configuration
 * WHY:  Dynamic adjustment based on threat level
 * HOW:  Update config values
 *
 * @param bridge Security-training bridge
 * @param mode Sanitization mode
 * @param clip_norm Norm clipping threshold
 * @param clip_value Value clipping threshold
 * @return 0 on success, error code on failure
 */
int security_training_set_gradient_params(
    security_training_bridge_t* bridge,
    security_grad_sanitize_mode_t mode,
    float clip_norm,
    float clip_value
);

/* ============================================================================
 * Model Integrity API
 * ============================================================================ */

/**
 * @brief Verify model integrity
 *
 * WHAT: Check model weights against last checkpoint
 * WHY:  Detect unauthorized model modifications
 * HOW:  Compute hash, compare with checkpoint
 *
 * @param bridge Security-training bridge
 * @param model_weights Model weight array
 * @param num_weights Number of weights
 * @return Integrity verification result
 */
security_integrity_result_t security_training_verify_model_integrity(
    security_training_bridge_t* bridge,
    const float* model_weights,
    uint32_t num_weights
);

/**
 * @brief Create secure model checkpoint
 *
 * WHAT: Save model state with integrity protection
 * WHY:  Enable rollback to known-good state
 * HOW:  Compute hash, optionally sign with BBB key
 *
 * @param bridge Security-training bridge
 * @param checkpoint_name Name for checkpoint
 * @param model_weights Model weight array
 * @param num_weights Number of weights
 * @param step Current training step
 * @return 0 on success, error code on failure
 */
int security_training_checkpoint_model(
    security_training_bridge_t* bridge,
    const char* checkpoint_name,
    const float* model_weights,
    uint32_t num_weights,
    uint64_t step
);

/**
 * @brief Rollback model to checkpoint
 *
 * WHAT: Restore model from secure checkpoint
 * WHY:  Recover from detected tampering or poisoning
 * HOW:  Verify checkpoint integrity, restore weights
 *
 * @param bridge Security-training bridge
 * @param checkpoint_name Name of checkpoint to restore
 * @param model_weights Output: restored model weights
 * @param num_weights Expected number of weights
 * @return 0 on success, error code on failure
 */
int security_training_rollback_model(
    security_training_bridge_t* bridge,
    const char* checkpoint_name,
    float* model_weights,
    uint32_t num_weights
);

/**
 * @brief Get checkpoint information
 *
 * WHAT: Query metadata for a checkpoint
 * WHY:  Determine checkpoint validity and provenance
 * HOW:  Look up in checkpoint registry
 *
 * @param bridge Security-training bridge
 * @param checkpoint_name Checkpoint to query
 * @param info Output: checkpoint information
 * @return 0 on success, error code on failure
 */
int security_training_get_checkpoint_info(
    const security_training_bridge_t* bridge,
    const char* checkpoint_name,
    security_checkpoint_info_t* info
);

/**
 * @brief List available checkpoints
 *
 * WHAT: Get list of all secure checkpoints
 * WHY:  Determine available rollback points
 * HOW:  Return checkpoint names and metadata
 *
 * @param bridge Security-training bridge
 * @param checkpoints Output: array of checkpoint info
 * @param max_checkpoints Size of output array
 * @return Number of checkpoints, or negative on error
 */
int security_training_list_checkpoints(
    const security_training_bridge_t* bridge,
    security_checkpoint_info_t* checkpoints,
    uint32_t max_checkpoints
);

/* ============================================================================
 * Concept Drift Detection API
 * ============================================================================ */

/**
 * @brief Detect concept drift
 *
 * WHAT: Monitor for unexpected distribution shifts
 * WHY:  Detect gradual poisoning or data corruption
 * HOW:  Compare current statistics with baseline
 *
 * DRIFT INDICATORS:
 * - Loss distribution shift
 * - Gradient statistics shift
 * - Activation pattern shift
 * - Label distribution shift
 *
 * @param bridge Security-training bridge
 * @param current_features Current batch features
 * @param num_features Number of features
 * @param drift_score Output: drift score [0-1]
 * @return true if significant drift detected
 */
bool security_training_detect_concept_drift(
    security_training_bridge_t* bridge,
    const float* current_features,
    uint32_t num_features,
    float* drift_score
);

/**
 * @brief Update drift baseline
 *
 * WHAT: Update baseline statistics for drift detection
 * WHY:  Establish normal distribution for comparison
 * HOW:  Running average of feature statistics
 *
 * @param bridge Security-training bridge
 * @param features Feature values to include
 * @param num_features Number of features
 * @return 0 on success, error code on failure
 */
int security_training_update_drift_baseline(
    security_training_bridge_t* bridge,
    const float* features,
    uint32_t num_features
);

/**
 * @brief Reset drift baseline
 *
 * WHAT: Clear drift detection baseline
 * WHY:  Start fresh after intentional distribution change
 * HOW:  Zero out baseline statistics
 *
 * @param bridge Security-training bridge
 * @return 0 on success, error code on failure
 */
int security_training_reset_drift_baseline(
    security_training_bridge_t* bridge
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update security -> training effects
 *
 * WHAT: Compute security constraints for training
 * WHY:  Provide protection measures to training pipeline
 * HOW:  Aggregate threat signals, compute bounds
 *
 * @param bridge Security-training bridge
 * @return 0 on success, error code on failure
 */
int security_training_update_security_effects(
    security_training_bridge_t* bridge
);

/**
 * @brief Update training -> security effects
 *
 * WHAT: Process training signals for security
 * WHY:  Detect threats from training behavior
 * HOW:  Analyze training metrics for anomalies
 *
 * @param bridge Security-training bridge
 * @param loss Current loss value
 * @param gradient_norm Current gradient norm
 * @param step Current training step
 * @return 0 on success, error code on failure
 */
int security_training_update_training_effects(
    security_training_bridge_t* bridge,
    float loss,
    float gradient_norm,
    uint64_t step
);

/**
 * @brief Get security effects for training
 *
 * WHAT: Query current security constraints
 * WHY:  Training pipeline needs protection parameters
 * HOW:  Copy internal effects to output
 *
 * @param bridge Security-training bridge
 * @param effects Output: security effects
 * @return 0 on success, error code on failure
 */
int security_training_get_security_effects(
    const security_training_bridge_t* bridge,
    security_training_effects_t* effects
);

/**
 * @brief Get training effects for security
 *
 * WHAT: Query current training signals
 * WHY:  Security needs training state for threat detection
 * HOW:  Copy internal effects to output
 *
 * @param bridge Security-training bridge
 * @param effects Output: training effects
 * @return 0 on success, error code on failure
 */
int security_training_get_training_effects(
    const security_training_bridge_t* bridge,
    training_security_effects_t* effects
);

/* ============================================================================
 * Bio-async Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register with inter-module messaging
 * WHY:  Enable async communication with other modules
 * HOW:  Register module, create inbox
 *
 * @param bridge Security-training bridge
 * @return 0 on success, error code on failure
 */
int security_training_connect_bio_async(security_training_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister from messaging
 * WHY:  Clean shutdown
 * HOW:  Unregister module
 *
 * @param bridge Security-training bridge
 * @return 0 on success, error code on failure
 */
int security_training_disconnect_bio_async(security_training_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Security-training bridge
 * @return true if connected
 */
bool security_training_is_bio_async_connected(
    const security_training_bridge_t* bridge
);

/* ============================================================================
 * Query and Statistics API
 * ============================================================================ */

/**
 * @brief Get current phase
 *
 * @param bridge Security-training bridge
 * @return Current phase
 */
security_training_phase_t security_training_get_phase(
    const security_training_bridge_t* bridge
);

/**
 * @brief Get current threat level
 *
 * @param bridge Security-training bridge
 * @return Threat level [0-1]
 */
float security_training_get_threat_level(
    const security_training_bridge_t* bridge
);

/**
 * @brief Check if under active attack
 *
 * @param bridge Security-training bridge
 * @return true if attack detected
 */
bool security_training_is_under_attack(
    const security_training_bridge_t* bridge
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Security-training bridge
 * @param stats Output: statistics
 * @return 0 on success, error code on failure
 */
int security_training_get_stats(
    const security_training_bridge_t* bridge,
    security_training_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Security-training bridge
 * @return 0 on success, error code on failure
 */
int security_training_reset_stats(security_training_bridge_t* bridge);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

/**
 * @brief Convert poisoning type to string
 *
 * @param type Poisoning type
 * @return Human-readable string
 */
const char* security_poisoning_type_to_string(security_poisoning_type_t type);

/**
 * @brief Convert trust level to string
 *
 * @param trust Trust level
 * @return Human-readable string
 */
const char* security_trust_level_to_string(security_data_trust_t trust);

/**
 * @brief Convert phase to string
 *
 * @param phase Bridge phase
 * @return Human-readable string
 */
const char* security_training_phase_to_string(security_training_phase_t phase);

/**
 * @brief Convert integrity result to string
 *
 * @param result Integrity result
 * @return Human-readable string
 */
const char* security_integrity_result_to_string(security_integrity_result_t result);

/**
 * @brief Convert gradient sanitize mode to string
 *
 * @param mode Sanitization mode
 * @return Human-readable string
 */
const char* security_grad_sanitize_mode_to_string(security_grad_sanitize_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_TRAINING_BRIDGE_H */
