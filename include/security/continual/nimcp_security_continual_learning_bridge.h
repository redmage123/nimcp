/**
 * @file nimcp_security_continual_learning_bridge.h
 * @brief Security-Continual Learning Integration Bridge for NIMCP
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Bidirectional integration between security systems and continual learning
 * WHY:  Protect always-on learning from catastrophic forgetting attacks, concept
 *       drift exploitation, memory replay poisoning, and learning rate manipulation
 * HOW:  Security monitors learning patterns, validates drift, verifies replay integrity;
 *       Continual learning reports retention metrics, drift signals, and anomalies
 *
 * THREAT MODEL:
 * =============
 * ```
 * ATTACK VECTOR                      DEFENSE MECHANISM
 * --------------------------------------------------------------------------------------------------------
 * Catastrophic forgetting attacks    -> Knowledge retention monitoring + elastic weight protection
 * Concept drift exploitation         -> Drift validation (natural vs adversarial)
 * Memory replay poisoning            -> Replay buffer integrity verification + hash chains
 * Learning rate manipulation         -> LR bounds enforcement + anomaly detection
 * Experience buffer tampering        -> Signed experience records + provenance tracking
 * Gradient accumulation attacks      -> Historical gradient monitoring + deviation alerts
 * Task sequence manipulation         -> Task fingerprinting + sequence validation
 * ```
 *
 * ARCHITECTURE:
 * =============
 * ```
 * +---------------------------------------------------------------------------+
 * |                 SECURITY-CONTINUAL LEARNING BRIDGE                        |
 * +---------------------------------------------------------------------------+
 * |                                                                            |
 * |  SECURITY -> CONTINUAL LEARNING (Protective Measures)                      |
 * |  +-------------------+       +-------------------------------------+       |
 * |  |  BBB/Anomaly      |       |     Continual Learning Pipeline     |       |
 * |  |  Detector         |------>|  - Knowledge protection (EWC/SI)    |       |
 * |  |                   |       |  - LR bounds enforcement            |       |
 * |  +-------------------+       |  - Replay validation                |       |
 * |                              +-------------------------------------+       |
 * |                                                                            |
 * |  CONTINUAL LEARNING -> SECURITY (Threat Signals)                           |
 * |  +-------------------------------------+       +-------------------+       |
 * |  |     Continual Learning Pipeline     |       |  Security         |       |
 * |  |  - Retention anomalies detected     |------>|  - Alert BBB      |       |
 * |  |  - Unnatural drift observed         |       |  - Log anomaly    |       |
 * |  |  - Replay integrity failures        |       |  - Block learning |       |
 * |  +-------------------------------------+       +-------------------+       |
 * |                                                                            |
 * +---------------------------------------------------------------------------+
 * ```
 *
 * KNOWLEDGE RETENTION MODEL:
 * ==========================
 * ```
 * Retention Level  | Action               | Security Response
 * -----------------|----------------------|------------------------------------------
 * HEALTHY (>0.9)   | Normal operation     | Standard monitoring
 * DEGRADING (0.7-0.9)| Increase protection| Alert, enable EWC boost
 * CRITICAL (<0.7)  | Emergency mode       | Block new learning, rollback if needed
 * ```
 *
 * DRIFT VALIDATION:
 * =================
 * ```
 * Drift Type       | Detection Method                     | Response
 * -----------------|--------------------------------------|------------------
 * Natural          | Gradual, correlated with input dist  | Allow with monitoring
 * Adversarial      | Sudden, uncorrelated with inputs     | Block + alert
 * Task Switch      | Matches known task fingerprint       | Allow with context switch
 * Manipulation     | Conflicts with task sequence         | Block + rollback
 * ```
 *
 * USAGE EXAMPLE:
 * ==============
 * ```c
 * // Create security-continual learning bridge
 * security_cl_config_t config = security_cl_default_config();
 * config.enable_forgetting_protection = true;
 * config.retention_threshold = 0.8f;
 * security_cl_bridge_t* bridge = security_cl_bridge_create(&config);
 *
 * // Connect to systems
 * security_cl_connect_continual_learning(bridge, cl_system);
 * security_cl_connect_bbb(bridge, bbb_system);
 *
 * // Continual learning loop
 * for (int task = 0; task < num_tasks; task++) {
 *     // Protect existing knowledge before learning new task
 *     security_cl_protect_knowledge(bridge, current_weights, num_weights, task);
 *
 *     for (int step = 0; step < steps_per_task; step++) {
 *         // Validate replay samples
 *         if (!security_cl_verify_replay(bridge, replay_samples, num_samples)) {
 *             continue;  // Skip poisoned replay
 *         }
 *
 *         // Check for learning rate manipulation
 *         if (security_cl_detect_lr_manipulation(bridge, proposed_lr)) {
 *             proposed_lr = security_cl_get_safe_lr(bridge);
 *         }
 *
 *         // ... learning step ...
 *
 *         // Monitor knowledge retention
 *         security_cl_monitor_retention(bridge, validation_accuracy, task);
 *     }
 *
 *     // Validate drift after task completion
 *     security_cl_validate_drift(bridge, new_features, num_features);
 * }
 * ```
 *
 * GOTCHAS:
 * ========
 * - Knowledge protection requires baseline before first learning
 * - Replay verification uses hash chains - replay buffer must be initialized
 * - LR manipulation detection needs historical LR data (window-based)
 * - Retention monitoring is per-task - register tasks before monitoring
 * - Drift validation distinguishes natural vs adversarial drift
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

#ifndef NIMCP_SECURITY_CONTINUAL_LEARNING_BRIDGE_H
#define NIMCP_SECURITY_CONTINUAL_LEARNING_BRIDGE_H

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

#define SECURITY_CL_MODULE_NAME      "security_continual_learning"
#define SECURITY_CL_MODULE_VERSION   "1.0.0"

/* Knowledge retention thresholds */
#define SECURITY_CL_DEFAULT_RETENTION_THRESHOLD     0.8f
#define SECURITY_CL_DEFAULT_CRITICAL_THRESHOLD      0.6f
#define SECURITY_CL_DEFAULT_FORGETTING_RATE         0.1f

/* Concept drift detection */
#define SECURITY_CL_DEFAULT_DRIFT_THRESHOLD         0.3f
#define SECURITY_CL_DEFAULT_SUDDEN_DRIFT_THRESHOLD  0.5f
#define SECURITY_CL_DRIFT_WINDOW_SIZE               100

/* Learning rate bounds */
#define SECURITY_CL_DEFAULT_LR_MIN                  1e-6f
#define SECURITY_CL_DEFAULT_LR_MAX                  0.1f
#define SECURITY_CL_DEFAULT_LR_CHANGE_THRESHOLD     0.5f
#define SECURITY_CL_LR_HISTORY_SIZE                 100

/* Memory replay */
#define SECURITY_CL_HASH_SIZE                       32
#define SECURITY_CL_MAX_REPLAY_BUFFERS              16
#define SECURITY_CL_DEFAULT_REPLAY_VERIFY_RATE      0.1f

/* Task management */
#define SECURITY_CL_MAX_TASKS                       256
#define SECURITY_CL_TASK_FINGERPRINT_SIZE           64

/* Bio-async */
#define SECURITY_CL_BIO_INBOX_CAPACITY              64

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct security_cl_bridge security_cl_bridge_t;
typedef struct bbb_system_struct* bbb_system_t;
typedef struct nimcp_anomaly_detector_internal* nimcp_anomaly_detector_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Catastrophic forgetting attack types
 *
 * WHAT: Categories of forgetting attacks
 * WHY:  Different attacks require different detection/prevention
 * HOW:  Each type has specialized defense mechanisms
 */
typedef enum {
    SECURITY_CL_FORGETTING_NONE = 0,        /**< No attack detected */
    SECURITY_CL_FORGETTING_GRADIENT_FLOOD,  /**< Overwhelming gradient updates */
    SECURITY_CL_FORGETTING_WEIGHT_ERASURE,  /**< Direct weight modification */
    SECURITY_CL_FORGETTING_TASK_OVERWRITE,  /**< Malicious task sequence */
    SECURITY_CL_FORGETTING_REPLAY_POISON,   /**< Poisoned replay buffer */
    SECURITY_CL_FORGETTING_LR_SPIKE,        /**< Learning rate manipulation */
    SECURITY_CL_FORGETTING_COUNT
} security_cl_forgetting_type_t;

/**
 * @brief Concept drift classification
 *
 * WHAT: Types of concept drift detected
 * WHY:  Natural drift should be allowed, adversarial blocked
 * HOW:  Statistical analysis distinguishes drift types
 */
typedef enum {
    SECURITY_CL_DRIFT_NONE = 0,             /**< No significant drift */
    SECURITY_CL_DRIFT_NATURAL,              /**< Natural distribution shift */
    SECURITY_CL_DRIFT_TASK_SWITCH,          /**< Legitimate task boundary */
    SECURITY_CL_DRIFT_ADVERSARIAL,          /**< Adversarial manipulation */
    SECURITY_CL_DRIFT_MANIPULATION          /**< Data manipulation attack */
} security_cl_drift_type_t;

/**
 * @brief Knowledge retention level
 *
 * WHAT: Current knowledge retention health
 * WHY:  Determines protection level needed
 * HOW:  Based on validation accuracy on previous tasks
 */
typedef enum {
    SECURITY_CL_RETENTION_HEALTHY = 0,      /**< >90% retention */
    SECURITY_CL_RETENTION_DEGRADING,        /**< 70-90% retention */
    SECURITY_CL_RETENTION_CRITICAL,         /**< <70% retention */
    SECURITY_CL_RETENTION_COMPROMISED       /**< Catastrophic forgetting occurred */
} security_cl_retention_level_t;

/**
 * @brief Security-CL bridge phase
 *
 * WHAT: Current operational phase of the bridge
 * WHY:  Different phases have different behaviors
 * HOW:  Phases transition based on retention and threats
 */
typedef enum {
    SECURITY_CL_PHASE_INACTIVE = 0,         /**< Bridge not active */
    SECURITY_CL_PHASE_MONITORING,           /**< Passive monitoring only */
    SECURITY_CL_PHASE_PROTECTING,           /**< Active protection enabled */
    SECURITY_CL_PHASE_DEFENDING,            /**< Responding to detected threat */
    SECURITY_CL_PHASE_RECOVERY              /**< Recovering from incident */
} security_cl_phase_t;

/**
 * @brief Replay integrity status
 *
 * WHAT: Result of replay buffer verification
 * WHY:  Detect tampering with experience replay
 * HOW:  Hash chain verification and statistical analysis
 */
typedef enum {
    SECURITY_CL_REPLAY_OK = 0,              /**< Replay buffer intact */
    SECURITY_CL_REPLAY_HASH_MISMATCH,       /**< Hash chain broken */
    SECURITY_CL_REPLAY_POISON_DETECTED,     /**< Poisoned samples found */
    SECURITY_CL_REPLAY_DISTRIBUTION_SHIFT,  /**< Unusual distribution change */
    SECURITY_CL_REPLAY_TAMPERED              /**< Definite tampering */
} security_cl_replay_status_t;

/* ============================================================================
 * Core Structures
 * ============================================================================ */

/**
 * @brief Configuration for security-continual learning bridge
 */
typedef struct {
    /* Feature enables */
    bool enable_forgetting_protection;      /**< Enable catastrophic forgetting protection */
    bool enable_drift_detection;            /**< Enable concept drift detection */
    bool enable_replay_verification;        /**< Enable memory replay integrity checks */
    bool enable_lr_monitoring;              /**< Enable learning rate manipulation detection */
    bool enable_task_validation;            /**< Enable task sequence validation */
    bool enable_ewc_boost;                  /**< Enable EWC boost on retention degradation */

    /* Knowledge retention */
    float retention_threshold;              /**< Threshold for healthy retention (default: 0.8) */
    float critical_threshold;               /**< Threshold for critical retention (default: 0.6) */
    float max_forgetting_rate;              /**< Max allowed forgetting rate per task */

    /* Concept drift */
    float drift_threshold;                  /**< Threshold for drift detection */
    float sudden_drift_threshold;           /**< Threshold for sudden (adversarial) drift */
    uint32_t drift_window_size;             /**< Window size for drift calculation */

    /* Learning rate */
    float lr_min_bound;                     /**< Minimum allowed learning rate */
    float lr_max_bound;                     /**< Maximum allowed learning rate */
    float lr_change_threshold;              /**< Max allowed LR change per step */

    /* Replay verification */
    float replay_verify_rate;               /**< Fraction of replay to verify each step */
    bool use_hash_chains;                   /**< Use cryptographic hash chains */

    /* Bio-async */
    bool enable_bio_async;                  /**< Enable bio-async messaging */
    uint32_t bio_inbox_capacity;            /**< Message queue size */

    /* Logging */
    bool enable_logging;                    /**< Enable detailed logging */
    bool log_all_verifications;             /**< Log every verification (verbose) */
} security_cl_config_t;

/**
 * @brief Security effects on continual learning (Security -> CL)
 *
 * WHAT: Security-derived constraints on continual learning
 * WHY:  Communicate protection measures to CL pipeline
 * HOW:  Updated by security modules, consumed by CL
 */
typedef struct {
    /* Knowledge protection */
    float ewc_lambda_boost;                 /**< EWC regularization boost factor */
    float si_importance_boost;              /**< SI importance boost factor */
    bool knowledge_lock_active;             /**< Block updates to critical weights */
    uint32_t protected_task_count;          /**< Number of protected tasks */

    /* Learning rate bounds */
    float lr_max_allowed;                   /**< Current max allowed LR */
    float lr_min_allowed;                   /**< Current min allowed LR */
    float lr_scale_factor;                  /**< Scale factor for proposed LR (0-1) */
    bool lr_override_active;                /**< LR being overridden by security */

    /* Replay control */
    bool replay_suspended;                  /**< Replay buffer suspended (poisoned) */
    uint32_t quarantined_samples;           /**< Number of quarantined replay samples */
    bool replay_rebuild_required;           /**< Replay buffer needs rebuild */

    /* Task control */
    bool task_switch_blocked;               /**< Task switching blocked */
    bool new_task_blocked;                  /**< New task learning blocked */

    /* Threat level */
    float threat_level;                     /**< Overall threat level [0-1] */
    bool under_attack;                      /**< Active attack detected */
    security_cl_retention_level_t retention_status; /**< Current retention health */

    /* Metadata */
    uint64_t last_update_ms;                /**< When effects were last updated */
    bool valid;                             /**< Whether effects are current */
} security_cl_effects_t;

/**
 * @brief Continual learning effects on security (CL -> Security)
 *
 * WHAT: CL-derived signals for security monitoring
 * WHY:  Alert security to potential threats
 * HOW:  Updated by CL pipeline, consumed by security
 */
typedef struct {
    /* Retention metrics */
    float current_retention;                /**< Current overall retention [0-1] */
    float retention_delta;                  /**< Change in retention since last update */
    bool retention_anomaly;                 /**< Anomalous retention pattern */
    uint32_t tasks_with_degradation;        /**< Tasks showing degradation */

    /* Drift indicators */
    float current_drift_score;              /**< Current drift magnitude */
    security_cl_drift_type_t drift_type;    /**< Classified drift type */
    bool drift_anomaly;                     /**< Anomalous drift detected */

    /* Learning rate signals */
    float current_lr;                       /**< Current learning rate */
    float lr_variance;                      /**< LR variance over window */
    bool lr_anomaly;                        /**< Anomalous LR pattern */

    /* Replay signals */
    uint32_t replay_integrity_failures;     /**< Recent integrity failures */
    float replay_distribution_drift;        /**< Drift in replay distribution */
    bool replay_anomaly;                    /**< Anomalous replay behavior */

    /* Task signals */
    uint32_t current_task_id;               /**< Current task being learned */
    uint32_t total_tasks_learned;           /**< Total tasks in sequence */
    bool task_sequence_anomaly;             /**< Anomalous task sequence */

    /* Gradient signals */
    float gradient_accumulation;            /**< Historical gradient accumulation */
    bool gradient_explosion_risk;           /**< Gradient explosion detected */

    /* Metadata */
    uint64_t timestamp_ms;                  /**< When effects were captured */
    bool valid;                             /**< Whether effects are current */
} cl_security_effects_t;

/**
 * @brief Task registration info
 *
 * WHAT: Information about a registered task
 * WHY:  Track task-specific retention and protection
 * HOW:  Stored per-task, used for monitoring
 */
typedef struct {
    uint32_t task_id;                       /**< Task identifier */
    char name[128];                         /**< Task name */
    uint8_t fingerprint[SECURITY_CL_TASK_FINGERPRINT_SIZE]; /**< Task fingerprint */
    float baseline_accuracy;                /**< Accuracy at task completion */
    float current_accuracy;                 /**< Current accuracy on task */
    float retention_rate;                   /**< Retention rate since learning */
    uint64_t learned_at_ms;                 /**< When task was learned */
    uint64_t last_evaluated_ms;             /**< Last evaluation time */
    bool is_protected;                      /**< Whether task has protection */
    float protection_strength;              /**< EWC/SI protection strength */
} security_cl_task_info_t;

/**
 * @brief Replay buffer registration
 *
 * WHAT: Information about a registered replay buffer
 * WHY:  Track replay integrity and detect tampering
 * HOW:  Hash chain verification per buffer
 */
typedef struct {
    uint32_t buffer_id;                     /**< Buffer identifier */
    char name[128];                         /**< Buffer name */
    void* buffer_ptr;                       /**< Pointer to buffer (opaque) */
    size_t buffer_size;                     /**< Total buffer size */
    uint32_t sample_count;                  /**< Number of samples */
    uint8_t current_hash[SECURITY_CL_HASH_SIZE]; /**< Current buffer hash */
    uint8_t baseline_hash[SECURITY_CL_HASH_SIZE]; /**< Baseline hash */
    uint64_t last_verified_ms;              /**< Last verification time */
    uint32_t verification_failures;         /**< Verification failure count */
    bool active;                            /**< Buffer currently in use */
} security_cl_replay_info_t;

/**
 * @brief Security-CL bridge state
 */
struct security_cl_bridge {
    bridge_base_t base;                     /**< MUST be first: base bridge */

    /* Connected systems */
    void* continual_learning;               /**< CL pipeline (opaque) */
    bbb_system_t bbb;                       /**< Blood-Brain Barrier system */
    nimcp_anomaly_detector_t anomaly_detector; /**< Anomaly detector */

    /* Configuration */
    security_cl_config_t config;

    /* Current state */
    security_cl_phase_t phase;

    /* Effects */
    security_cl_effects_t security_effects; /**< Security -> CL */
    cl_security_effects_t cl_effects;       /**< CL -> Security */

    /* Task registry */
    uint32_t num_tasks;
    security_cl_task_info_t tasks[SECURITY_CL_MAX_TASKS];

    /* Replay buffer registry */
    uint32_t num_replay_buffers;
    security_cl_replay_info_t replay_buffers[SECURITY_CL_MAX_REPLAY_BUFFERS];

    /* Learning rate history */
    float lr_history[SECURITY_CL_LR_HISTORY_SIZE];
    uint32_t lr_history_idx;
    uint32_t lr_history_count;

    /* Drift baseline */
    float* drift_baseline;                  /**< Baseline feature statistics */
    uint32_t drift_baseline_size;           /**< Size of baseline */

    /* Knowledge baseline */
    float* weight_importance;               /**< Per-weight importance (EWC Fisher) */
    float* weight_baseline;                 /**< Baseline weights */
    uint32_t num_weights;                   /**< Number of weights tracked */

    /* Connection state */
    bool cl_connected;
    bool bbb_connected;
    bool anomaly_connected;

    /* Timestamps */
    uint64_t creation_time_ms;
    uint64_t last_update_ms;
    uint64_t last_verification_ms;
};

/**
 * @brief Security-CL bridge statistics
 */
typedef struct {
    /* Protection counts */
    uint64_t knowledge_protections;
    uint64_t ewc_boosts_applied;
    uint64_t si_boosts_applied;
    uint64_t weights_locked;

    /* Drift detection */
    uint64_t drift_checks;
    uint64_t drift_detections;
    uint64_t adversarial_drifts_blocked;
    float max_drift_score;

    /* Replay verification */
    uint64_t replay_verifications;
    uint64_t replay_failures;
    uint64_t samples_quarantined;

    /* Learning rate */
    uint64_t lr_checks;
    uint64_t lr_manipulations_detected;
    uint64_t lr_overrides_applied;

    /* Retention monitoring */
    uint64_t retention_checks;
    uint64_t retention_anomalies;
    uint64_t emergency_responses;
    float min_retention_observed;

    /* Task management */
    uint64_t tasks_registered;
    uint64_t task_switches_blocked;
    uint64_t task_sequence_violations;

    /* Forgetting attacks */
    uint64_t forgetting_attacks_detected;
    uint64_t forgetting_by_type[SECURITY_CL_FORGETTING_COUNT];

    /* Connection status */
    bool cl_connected;
    bool bbb_connected;
    bool anomaly_connected;
    bool bio_async_connected;

    /* Current state */
    security_cl_phase_t current_phase;
    float current_threat_level;
    security_cl_retention_level_t retention_level;

    /* Timing */
    uint64_t total_updates;
    float avg_verification_time_us;
    uint64_t uptime_ms;
} security_cl_stats_t;

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
int security_cl_default_config(security_cl_config_t* config);

/**
 * @brief Create security-continual learning bridge
 *
 * WHAT: Initialize security-CL integration
 * WHY:  Set up bidirectional security-CL coordination
 * HOW:  Allocate state, initialize mutex, register bio-async
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
security_cl_bridge_t* security_cl_bridge_create(
    const security_cl_config_t* config
);

/**
 * @brief Destroy security-continual learning bridge
 *
 * WHAT: Clean up resources
 * WHY:  Proper resource deallocation
 * HOW:  Free mutex, unregister bio-async, free structure
 *
 * @param bridge Bridge to destroy
 */
void security_cl_bridge_destroy(security_cl_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect to continual learning pipeline
 *
 * @param bridge Security-CL bridge
 * @param continual_learning CL pipeline (opaque pointer)
 * @return 0 on success, error code on failure
 */
int security_cl_connect_continual_learning(
    security_cl_bridge_t* bridge,
    void* continual_learning
);

/**
 * @brief Connect to Blood-Brain Barrier
 *
 * @param bridge Security-CL bridge
 * @param bbb BBB system handle
 * @return 0 on success, error code on failure
 */
int security_cl_connect_bbb(
    security_cl_bridge_t* bridge,
    bbb_system_t bbb
);

/**
 * @brief Connect to anomaly detector
 *
 * @param bridge Security-CL bridge
 * @param detector Anomaly detector handle
 * @return 0 on success, error code on failure
 */
int security_cl_connect_anomaly_detector(
    security_cl_bridge_t* bridge,
    nimcp_anomaly_detector_t detector
);

/**
 * @brief Disconnect continual learning pipeline
 *
 * @param bridge Security-CL bridge
 * @return 0 on success
 */
int security_cl_disconnect_continual_learning(security_cl_bridge_t* bridge);

/**
 * @brief Disconnect BBB
 *
 * @param bridge Security-CL bridge
 * @return 0 on success
 */
int security_cl_disconnect_bbb(security_cl_bridge_t* bridge);

/**
 * @brief Disconnect anomaly detector
 *
 * @param bridge Security-CL bridge
 * @return 0 on success
 */
int security_cl_disconnect_anomaly_detector(security_cl_bridge_t* bridge);

/* ============================================================================
 * Knowledge Protection API
 * ============================================================================ */

/**
 * @brief Protect knowledge from catastrophic forgetting
 *
 * WHAT: Apply protection to current model weights
 * WHY:  Prevent catastrophic forgetting during new task learning
 * HOW:  Compute weight importance (EWC Fisher), store baseline
 *
 * @param bridge Security-CL bridge
 * @param weights Current model weights
 * @param num_weights Number of weights
 * @param task_id Task that was just learned
 * @return 0 on success, error code on failure
 */
int security_cl_protect_knowledge(
    security_cl_bridge_t* bridge,
    const float* weights,
    uint32_t num_weights,
    uint32_t task_id
);

/**
 * @brief Register a task for monitoring
 *
 * WHAT: Register a task in the task registry
 * WHY:  Track task-specific retention over time
 * HOW:  Store baseline accuracy and task fingerprint
 *
 * @param bridge Security-CL bridge
 * @param task_id Task identifier
 * @param task_name Human-readable name
 * @param baseline_accuracy Accuracy at task completion
 * @return 0 on success, error code on failure
 */
int security_cl_register_task(
    security_cl_bridge_t* bridge,
    uint32_t task_id,
    const char* task_name,
    float baseline_accuracy
);

/**
 * @brief Get task information
 *
 * @param bridge Security-CL bridge
 * @param task_id Task identifier
 * @param info Output: task information
 * @return 0 on success, error code on failure
 */
int security_cl_get_task_info(
    const security_cl_bridge_t* bridge,
    uint32_t task_id,
    security_cl_task_info_t* info
);

/**
 * @brief Compute protection penalty for weight update
 *
 * WHAT: Compute EWC/SI penalty for proposed weight change
 * WHY:  Prevent updates that would cause forgetting
 * HOW:  Importance-weighted squared distance from baseline
 *
 * @param bridge Security-CL bridge
 * @param new_weights Proposed new weights
 * @param num_weights Number of weights
 * @param penalty Output: penalty value
 * @return 0 on success, error code on failure
 */
int security_cl_compute_protection_penalty(
    const security_cl_bridge_t* bridge,
    const float* new_weights,
    uint32_t num_weights,
    float* penalty
);

/* ============================================================================
 * Concept Drift API
 * ============================================================================ */

/**
 * @brief Validate concept drift
 *
 * WHAT: Classify and validate observed concept drift
 * WHY:  Distinguish natural drift from adversarial manipulation
 * HOW:  Statistical analysis of drift pattern and correlation
 *
 * @param bridge Security-CL bridge
 * @param current_features Current feature statistics
 * @param num_features Number of features
 * @param drift_type Output: classified drift type
 * @param drift_score Output: drift magnitude
 * @return true if drift is allowed, false if adversarial
 */
bool security_cl_validate_drift(
    security_cl_bridge_t* bridge,
    const float* current_features,
    uint32_t num_features,
    security_cl_drift_type_t* drift_type,
    float* drift_score
);

/**
 * @brief Update drift baseline
 *
 * WHAT: Update baseline for drift detection
 * WHY:  Establish normal distribution for comparison
 * HOW:  Exponential moving average of feature statistics
 *
 * @param bridge Security-CL bridge
 * @param features Feature values
 * @param num_features Number of features
 * @return 0 on success, error code on failure
 */
int security_cl_update_drift_baseline(
    security_cl_bridge_t* bridge,
    const float* features,
    uint32_t num_features
);

/**
 * @brief Reset drift baseline
 *
 * WHAT: Clear drift detection baseline
 * WHY:  Start fresh after legitimate task switch
 * HOW:  Zero out baseline statistics
 *
 * @param bridge Security-CL bridge
 * @return 0 on success, error code on failure
 */
int security_cl_reset_drift_baseline(security_cl_bridge_t* bridge);

/* ============================================================================
 * Memory Replay API
 * ============================================================================ */

/**
 * @brief Verify replay buffer integrity
 *
 * WHAT: Verify experience replay buffer has not been tampered with
 * WHY:  Detect memory replay poisoning attacks
 * HOW:  Hash chain verification + statistical analysis
 *
 * @param bridge Security-CL bridge
 * @param buffer Replay buffer pointer
 * @param buffer_size Buffer size in bytes
 * @param sample_count Number of samples in buffer
 * @param status Output: verification status
 * @return true if buffer is valid, false if tampered
 */
bool security_cl_verify_replay(
    security_cl_bridge_t* bridge,
    const void* buffer,
    size_t buffer_size,
    uint32_t sample_count,
    security_cl_replay_status_t* status
);

/**
 * @brief Register replay buffer for monitoring
 *
 * WHAT: Register a replay buffer in the registry
 * WHY:  Track buffer integrity over time
 * HOW:  Store baseline hash and metadata
 *
 * @param bridge Security-CL bridge
 * @param buffer_name Human-readable name
 * @param buffer Replay buffer pointer
 * @param buffer_size Buffer size in bytes
 * @param sample_count Number of samples
 * @return Buffer ID on success, 0 on failure
 */
uint32_t security_cl_register_replay_buffer(
    security_cl_bridge_t* bridge,
    const char* buffer_name,
    const void* buffer,
    size_t buffer_size,
    uint32_t sample_count
);

/**
 * @brief Update replay buffer hash
 *
 * WHAT: Update stored hash after legitimate buffer modification
 * WHY:  Keep hash chain valid after intentional updates
 * HOW:  Recompute and store new hash
 *
 * @param bridge Security-CL bridge
 * @param buffer_id Buffer identifier
 * @param buffer Updated buffer pointer
 * @param buffer_size New buffer size
 * @param sample_count New sample count
 * @return 0 on success, error code on failure
 */
int security_cl_update_replay_hash(
    security_cl_bridge_t* bridge,
    uint32_t buffer_id,
    const void* buffer,
    size_t buffer_size,
    uint32_t sample_count
);

/**
 * @brief Quarantine suspicious replay samples
 *
 * WHAT: Mark samples as quarantined
 * WHY:  Exclude potentially poisoned samples
 * HOW:  Record sample indices for exclusion
 *
 * @param bridge Security-CL bridge
 * @param sample_indices Array of sample indices
 * @param num_samples Number of samples
 * @return 0 on success, error code on failure
 */
int security_cl_quarantine_replay_samples(
    security_cl_bridge_t* bridge,
    const uint32_t* sample_indices,
    uint32_t num_samples
);

/* ============================================================================
 * Retention Monitoring API
 * ============================================================================ */

/**
 * @brief Monitor knowledge retention
 *
 * WHAT: Track retention of previously learned tasks
 * WHY:  Detect catastrophic forgetting early
 * HOW:  Compare current accuracy with baseline per task
 *
 * @param bridge Security-CL bridge
 * @param task_id Task to evaluate
 * @param current_accuracy Current accuracy on task
 * @param retention Output: retention rate [0-1]
 * @return Retention level classification
 */
security_cl_retention_level_t security_cl_monitor_retention(
    security_cl_bridge_t* bridge,
    uint32_t task_id,
    float current_accuracy,
    float* retention
);

/**
 * @brief Get overall retention status
 *
 * WHAT: Get aggregate retention across all tasks
 * WHY:  Single metric for system health
 * HOW:  Weighted average of per-task retention
 *
 * @param bridge Security-CL bridge
 * @param overall_retention Output: overall retention
 * @return Retention level classification
 */
security_cl_retention_level_t security_cl_get_retention_status(
    const security_cl_bridge_t* bridge,
    float* overall_retention
);

/**
 * @brief Check if retention is compromised
 *
 * WHAT: Quick check for critical retention loss
 * WHY:  Fast emergency detection
 * HOW:  Check against critical threshold
 *
 * @param bridge Security-CL bridge
 * @return true if retention is critically low
 */
bool security_cl_is_retention_compromised(
    const security_cl_bridge_t* bridge
);

/* ============================================================================
 * Learning Rate Monitoring API
 * ============================================================================ */

/**
 * @brief Detect learning rate manipulation
 *
 * WHAT: Check if proposed learning rate is suspicious
 * WHY:  Prevent LR-based forgetting attacks
 * HOW:  Compare against bounds and historical pattern
 *
 * @param bridge Security-CL bridge
 * @param proposed_lr Proposed learning rate
 * @return true if manipulation detected, false if OK
 */
bool security_cl_detect_lr_manipulation(
    security_cl_bridge_t* bridge,
    float proposed_lr
);

/**
 * @brief Get safe learning rate
 *
 * WHAT: Get security-approved learning rate
 * WHY:  Override manipulated LR with safe value
 * HOW:  Clamp to bounds and apply scale factor
 *
 * @param bridge Security-CL bridge
 * @return Safe learning rate value
 */
float security_cl_get_safe_lr(const security_cl_bridge_t* bridge);

/**
 * @brief Record learning rate
 *
 * WHAT: Add learning rate to history
 * WHY:  Build baseline for anomaly detection
 * HOW:  Circular buffer of recent LR values
 *
 * @param bridge Security-CL bridge
 * @param lr Learning rate value
 * @return 0 on success, error code on failure
 */
int security_cl_record_lr(
    security_cl_bridge_t* bridge,
    float lr
);

/**
 * @brief Set learning rate bounds
 *
 * WHAT: Update allowed learning rate range
 * WHY:  Adjust bounds based on threat level
 * HOW:  Update config and effects
 *
 * @param bridge Security-CL bridge
 * @param lr_min Minimum allowed LR
 * @param lr_max Maximum allowed LR
 * @return 0 on success, error code on failure
 */
int security_cl_set_lr_bounds(
    security_cl_bridge_t* bridge,
    float lr_min,
    float lr_max
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update security -> CL effects
 *
 * WHAT: Compute security constraints for CL
 * WHY:  Provide protection measures to CL pipeline
 * HOW:  Aggregate threat signals, compute protection levels
 *
 * @param bridge Security-CL bridge
 * @return 0 on success, error code on failure
 */
int security_cl_update_security_effects(security_cl_bridge_t* bridge);

/**
 * @brief Update CL -> security effects
 *
 * WHAT: Process CL signals for security
 * WHY:  Detect threats from CL behavior
 * HOW:  Analyze CL metrics for anomalies
 *
 * @param bridge Security-CL bridge
 * @param retention Current retention
 * @param drift_score Current drift score
 * @param lr Current learning rate
 * @param task_id Current task ID
 * @return 0 on success, error code on failure
 */
int security_cl_update_cl_effects(
    security_cl_bridge_t* bridge,
    float retention,
    float drift_score,
    float lr,
    uint32_t task_id
);

/**
 * @brief Get security effects for CL
 *
 * @param bridge Security-CL bridge
 * @param effects Output: security effects
 * @return 0 on success, error code on failure
 */
int security_cl_get_security_effects(
    const security_cl_bridge_t* bridge,
    security_cl_effects_t* effects
);

/**
 * @brief Get CL effects for security
 *
 * @param bridge Security-CL bridge
 * @param effects Output: CL effects
 * @return 0 on success, error code on failure
 */
int security_cl_get_cl_effects(
    const security_cl_bridge_t* bridge,
    cl_security_effects_t* effects
);

/* ============================================================================
 * Bio-async Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Security-CL bridge
 * @return 0 on success, error code on failure
 */
int security_cl_connect_bio_async(security_cl_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Security-CL bridge
 * @return 0 on success, error code on failure
 */
int security_cl_disconnect_bio_async(security_cl_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Security-CL bridge
 * @return true if connected
 */
bool security_cl_is_bio_async_connected(const security_cl_bridge_t* bridge);

/* ============================================================================
 * Query and Statistics API
 * ============================================================================ */

/**
 * @brief Get current phase
 *
 * @param bridge Security-CL bridge
 * @return Current phase
 */
security_cl_phase_t security_cl_get_phase(const security_cl_bridge_t* bridge);

/**
 * @brief Get current threat level
 *
 * @param bridge Security-CL bridge
 * @return Threat level [0-1]
 */
float security_cl_get_threat_level(const security_cl_bridge_t* bridge);

/**
 * @brief Check if under active attack
 *
 * @param bridge Security-CL bridge
 * @return true if attack detected
 */
bool security_cl_is_under_attack(const security_cl_bridge_t* bridge);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Security-CL bridge
 * @param stats Output: statistics
 * @return 0 on success, error code on failure
 */
int security_cl_get_stats(
    const security_cl_bridge_t* bridge,
    security_cl_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Security-CL bridge
 * @return 0 on success, error code on failure
 */
int security_cl_reset_stats(security_cl_bridge_t* bridge);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

/**
 * @brief Convert forgetting type to string
 *
 * @param type Forgetting attack type
 * @return Human-readable string
 */
const char* security_cl_forgetting_type_to_string(
    security_cl_forgetting_type_t type
);

/**
 * @brief Convert drift type to string
 *
 * @param type Drift type
 * @return Human-readable string
 */
const char* security_cl_drift_type_to_string(security_cl_drift_type_t type);

/**
 * @brief Convert retention level to string
 *
 * @param level Retention level
 * @return Human-readable string
 */
const char* security_cl_retention_level_to_string(
    security_cl_retention_level_t level
);

/**
 * @brief Convert phase to string
 *
 * @param phase Bridge phase
 * @return Human-readable string
 */
const char* security_cl_phase_to_string(security_cl_phase_t phase);

/**
 * @brief Convert replay status to string
 *
 * @param status Replay verification status
 * @return Human-readable string
 */
const char* security_cl_replay_status_to_string(
    security_cl_replay_status_t status
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_CONTINUAL_LEARNING_BRIDGE_H */
