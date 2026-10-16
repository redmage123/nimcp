/**
 * @file nimcp_lgss_training_guard.h
 * @brief Learning Guarded Safety System (LGSS) - Training Guard
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Training guard that constrains gradient-based learning updates
 * WHY: Prevent dangerous training patterns including reward hacking,
 *      goal drift, and gradient-based attacks on the system
 * HOW: Gradient clipping, dangerous pattern detection, and checkpointing
 *
 * SECURITY CONCERNS ADDRESSED:
 * - Reward Hacking: System manipulates its own reward signals
 * - Goal Drift: Training causes gradual deviation from intended goals
 * - Gradient Attacks: Malicious gradients designed to corrupt the model
 * - Catastrophic Updates: Single large gradient step causing damage
 * - Backdoor Injection: Training data designed to create hidden behaviors
 *
 * DEFENSES:
 * - Gradient clipping by norm and value
 * - Reward signal validation
 * - Goal drift detection via distance metrics
 * - Gradient anomaly detection
 * - Automatic checkpointing before risky updates
 * - Rollback capability when problems detected
 *
 * USAGE:
 * @code
 * training_guard_config_t config = training_guard_default_config();
 * config.enable_reward_hacking_detection = true;
 *
 * training_guard_t* guard = training_guard_create(&config, orchestrator);
 *
 * // Before applying gradients
 * gradient_check_result_t result;
 * if (training_guard_apply_gradients(guard, gradients, &result) == 0) {
 *     // Use result.clipped_gradients for update
 * }
 *
 * // Periodically check for reward hacking
 * if (training_guard_detect_reward_hacking(guard, reward_state)) {
 *     // Trigger security response
 * }
 * @endcode
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_LGSS_TRAINING_GUARD_H
#define NIMCP_LGSS_TRAINING_GUARD_H

#include "nimcp_lgss_plasticity_constraints.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * FORWARD DECLARATIONS
 * ============================================================================ */

/** Forward declare security orchestrator */
typedef struct security_orchestrator_struct* security_orchestrator_t;

/* ============================================================================
 * CONSTANTS AND LIMITS
 * ============================================================================ */

/** Default gradient clip norm */
#define TRAINING_DEFAULT_GRAD_CLIP_NORM 1.0f

/** Default gradient clip value */
#define TRAINING_DEFAULT_GRAD_CLIP_VALUE 0.5f

/** Default gradient anomaly threshold */
#define TRAINING_DEFAULT_ANOMALY_THRESHOLD 3.0f

/** Default goal drift threshold */
#define TRAINING_DEFAULT_DRIFT_THRESHOLD 0.1f

/** Default checkpoint interval (updates) */
#define TRAINING_DEFAULT_CHECKPOINT_INTERVAL 1000

/** Maximum number of checkpoints to keep */
#define TRAINING_MAX_CHECKPOINTS 10

/** Maximum goal vector dimension */
#define TRAINING_MAX_GOAL_DIM 1024

/** Maximum gradient buffer size */
#define TRAINING_MAX_GRADIENT_SIZE (1024 * 1024)

/** Reward signal history length */
#define TRAINING_REWARD_HISTORY_SIZE 1000

/** Magic number for training guard validation */
#define LGSS_TRAINING_GUARD_MAGIC 0x54524E47 /* 'TRNG' */

/* ============================================================================
 * VIOLATION AND DETECTION TYPES
 * ============================================================================ */

/**
 * WHAT: Types of training guard violations
 * WHY: Categorize training violations for response
 * HOW: Bitmask flags for efficient tracking
 */
typedef enum {
    TRAINING_VIOLATION_NONE             = 0,
    TRAINING_VIOLATION_GRAD_NORM        = (1 << 0), /**< Gradient norm exceeded */
    TRAINING_VIOLATION_GRAD_VALUE       = (1 << 1), /**< Gradient value exceeded */
    TRAINING_VIOLATION_GRAD_ANOMALY     = (1 << 2), /**< Anomalous gradient detected */
    TRAINING_VIOLATION_REWARD_HACKING   = (1 << 3), /**< Reward hacking detected */
    TRAINING_VIOLATION_GOAL_DRIFT       = (1 << 4), /**< Goal drift detected */
    TRAINING_VIOLATION_CATASTROPHIC     = (1 << 5), /**< Catastrophic update */
    TRAINING_VIOLATION_BACKDOOR         = (1 << 6), /**< Potential backdoor pattern */
    TRAINING_VIOLATION_FROZEN_PARAM     = (1 << 7), /**< Frozen parameter modification */
    TRAINING_VIOLATION_LOSS_SPIKE       = (1 << 8), /**< Sudden loss spike */
    TRAINING_VIOLATION_NAN_DETECTED     = (1 << 9)  /**< NaN/Inf in gradients */
} training_violation_t;

/**
 * WHAT: Types of reward hacking behaviors
 * WHY: Categorize reward hacking for targeted response
 * HOW: Enum of known reward hacking patterns
 */
typedef enum {
    REWARD_HACKING_NONE = 0,
    REWARD_HACKING_REWARD_TAMPERING,      /**< Direct reward value modification */
    REWARD_HACKING_SHORTCUT_BEHAVIOR,     /**< Exploiting reward function bugs */
    REWARD_HACKING_SENSOR_MANIPULATION,   /**< Manipulating reward sensors */
    REWARD_HACKING_SELF_REWARD,           /**< Self-generating reward signals */
    REWARD_HACKING_REWARD_CORRELATION,    /**< Correlated false reward sources */
    REWARD_HACKING_GOAL_SUBSTITUTION,     /**< Replacing true goal with proxy */
    REWARD_HACKING_COUNT
} reward_hacking_type_t;

/**
 * WHAT: Types of goal drift
 * WHY: Categorize goal drift patterns
 * HOW: Enum of drift types
 */
typedef enum {
    GOAL_DRIFT_NONE = 0,
    GOAL_DRIFT_GRADUAL,                   /**< Slow, continuous drift */
    GOAL_DRIFT_SUDDEN,                    /**< Sudden goal change */
    GOAL_DRIFT_OSCILLATING,               /**< Unstable oscillation around target */
    GOAL_DRIFT_DIVERGENT,                 /**< Unbounded divergence */
    GOAL_DRIFT_COUNT
} goal_drift_type_t;

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/**
 * WHAT: Configuration for training guard
 * WHY: Customize training safety bounds
 * HOW: Struct with training-specific parameters
 */
typedef struct {
    /* Gradient constraints */
    float gradient_clip_norm;             /**< Max L2 norm for gradients */
    float gradient_clip_value;            /**< Max absolute value per element */
    bool enable_gradient_clipping;        /**< Enable gradient clipping */

    /* Anomaly detection */
    float gradient_anomaly_threshold;     /**< Std devs for anomaly detection */
    bool enable_gradient_anomaly_detection; /**< Enable anomaly detection */
    uint32_t anomaly_history_size;        /**< History for anomaly detection */

    /* Reward hacking detection */
    bool enable_reward_hacking_detection; /**< Enable reward hacking detection */
    float reward_hacking_sensitivity;     /**< Detection sensitivity [0, 1] */
    uint32_t reward_history_size;         /**< Reward signal history size */

    /* Goal drift detection */
    bool enable_goal_drift_detection;     /**< Enable goal drift detection */
    float goal_drift_threshold;           /**< Max allowed drift distance */
    uint32_t goal_vector_dim;             /**< Goal vector dimensionality */
    float drift_ewma_alpha;               /**< EWMA alpha for drift tracking */

    /* Checkpointing */
    bool enable_checkpointing;            /**< Enable automatic checkpoints */
    uint32_t checkpoint_interval;         /**< Updates between checkpoints */
    uint32_t max_checkpoints;             /**< Maximum checkpoints to keep */
    const char* checkpoint_dir;           /**< Directory for checkpoints */

    /* Rollback */
    bool enable_auto_rollback;            /**< Auto-rollback on violations */
    uint32_t rollback_threshold_violations; /**< Violations before rollback */

    /* Frozen parameters */
    uint32_t* frozen_param_indices;       /**< Indices of frozen parameters */
    uint32_t num_frozen_params;           /**< Number of frozen parameters */

    /* Backdoor detection */
    bool enable_backdoor_detection;       /**< Enable backdoor pattern detection */
    float backdoor_sensitivity;           /**< Backdoor detection sensitivity */

    /* Loss monitoring */
    bool enable_loss_monitoring;          /**< Monitor training loss */
    float loss_spike_threshold;           /**< Multiplier for loss spike detection */

    /* Logging and monitoring */
    bool enable_violation_logging;        /**< Log violations */
    bool enable_statistics;               /**< Track detailed statistics */
} training_guard_config_t;

/**
 * WHAT: Gradient buffer for training guard processing
 * WHY: Encapsulate gradient data for validation
 * HOW: Struct with gradient data and metadata
 */
typedef struct {
    float* data;                          /**< Gradient values */
    uint32_t size;                        /**< Number of gradient elements */
    uint32_t param_count;                 /**< Number of parameters */
    float current_loss;                   /**< Current loss value */
    float learning_rate;                  /**< Learning rate for this update */
    uint32_t batch_size;                  /**< Training batch size */
    uint64_t step_number;                 /**< Current training step */
} gradient_buffer_t;

/**
 * WHAT: Result of gradient validation
 * WHY: Provide detailed information about gradient check
 * HOW: Struct with violation flags, clipped gradients, metrics
 */
typedef struct {
    bool allowed;                         /**< Whether update is allowed */
    training_violation_t violations;      /**< Detected violations */
    float original_norm;                  /**< Original gradient L2 norm */
    float clipped_norm;                   /**< Clipped gradient L2 norm */
    float max_value_seen;                 /**< Maximum absolute gradient value */
    float anomaly_score;                  /**< Gradient anomaly score */
    bool was_clipped;                     /**< Whether gradients were clipped */
    uint32_t nan_count;                   /**< Number of NaN/Inf values found */
    float* clipped_gradients;             /**< Clipped gradients (if clipping) */
    char reason[256];                     /**< Human-readable reason if blocked */
} gradient_check_result_t;

/**
 * WHAT: State for reward hacking detection
 * WHY: Track reward signals over time
 * HOW: Struct with reward history and metrics
 */
typedef struct {
    float current_reward;                 /**< Current reward signal */
    float expected_reward;                /**< Expected reward from model */
    float reward_variance;                /**< Reward signal variance */
    float reward_mean;                    /**< Running reward mean */
    float action_value_correlation;       /**< Correlation between actions and values */
    uint64_t timestamp;                   /**< Timestamp of state */
} reward_state_t;

/**
 * WHAT: Result of reward hacking detection
 * WHY: Detailed information about detection
 * HOW: Struct with detection type, confidence, evidence
 */
typedef struct {
    bool detected;                        /**< Whether reward hacking detected */
    reward_hacking_type_t type;           /**< Type of reward hacking */
    float confidence;                     /**< Detection confidence [0, 1] */
    float anomaly_score;                  /**< Reward anomaly score */
    char evidence[512];                   /**< Evidence description */
} reward_hacking_result_t;

/**
 * WHAT: Result of goal drift detection
 * WHY: Detailed information about goal drift
 * HOW: Struct with drift type, magnitude, direction
 */
typedef struct {
    bool detected;                        /**< Whether goal drift detected */
    goal_drift_type_t type;               /**< Type of drift */
    float drift_magnitude;                /**< Magnitude of drift */
    float drift_rate;                     /**< Rate of drift (per update) */
    float* drift_direction;               /**< Direction vector (if available) */
    uint32_t drift_direction_dim;         /**< Dimension of direction vector */
    char description[256];                /**< Drift description */
} goal_drift_result_t;

/**
 * WHAT: Statistics about training guard operation
 * WHY: Monitor training guard effectiveness
 * HOW: Counters and metrics for training operations
 */
typedef struct {
    /* Update statistics */
    uint64_t total_updates_checked;       /**< Total gradient updates checked */
    uint64_t updates_allowed;             /**< Updates that passed guard */
    uint64_t updates_blocked;             /**< Updates blocked by guard */
    uint64_t updates_clipped;             /**< Updates that were clipped */

    /* Violation breakdown */
    uint64_t grad_norm_violations;        /**< Gradient norm violations */
    uint64_t grad_value_violations;       /**< Gradient value violations */
    uint64_t grad_anomaly_detections;     /**< Gradient anomaly detections */
    uint64_t reward_hacking_detections;   /**< Reward hacking detections */
    uint64_t goal_drift_detections;       /**< Goal drift detections */
    uint64_t catastrophic_detections;     /**< Catastrophic update detections */
    uint64_t backdoor_detections;         /**< Backdoor pattern detections */
    uint64_t nan_detections;              /**< NaN/Inf detections */

    /* Gradient statistics */
    float avg_gradient_norm;              /**< Average gradient norm */
    float max_gradient_norm_seen;         /**< Maximum gradient norm seen */
    float avg_clip_ratio;                 /**< Average clipping ratio */

    /* Reward statistics */
    float avg_reward;                     /**< Average reward */
    float reward_variance;                /**< Reward variance */
    float max_reward_anomaly_score;       /**< Maximum reward anomaly score */

    /* Goal drift statistics */
    float cumulative_drift;               /**< Cumulative goal drift */
    float max_drift_seen;                 /**< Maximum drift seen */

    /* Checkpoint statistics */
    uint32_t checkpoints_created;         /**< Checkpoints created */
    uint32_t rollbacks_performed;         /**< Rollbacks performed */

    /* Performance */
    uint64_t guard_overhead_us;           /**< Total guard overhead */
    float avg_check_time_us;              /**< Average check time */
} training_guard_stats_t;

/* ============================================================================
 * OPAQUE HANDLE
 * ============================================================================ */

/**
 * WHAT: Opaque handle to training guard
 * WHY: Encapsulation - hide internal implementation
 * HOW: Pimpl idiom - pointer to internal structure
 */
typedef struct training_guard_internal* training_guard_t;

/* ============================================================================
 * DEFAULT CONFIGURATION
 * ============================================================================ */

/**
 * WHAT: Get default training guard configuration
 * WHY: Provide sensible defaults for common use cases
 * HOW: Return pre-configured struct
 *
 * DEFAULT VALUES:
 * - gradient_clip_norm: 1.0
 * - gradient_clip_value: 0.5
 * - enable_gradient_clipping: true
 * - gradient_anomaly_threshold: 3.0 (std devs)
 * - enable_reward_hacking_detection: true
 * - enable_goal_drift_detection: true
 * - goal_drift_threshold: 0.1
 * - enable_checkpointing: true
 * - checkpoint_interval: 1000
 * - enable_auto_rollback: false
 * - enable_backdoor_detection: true
 * - enable_loss_monitoring: true
 *
 * @return Default configuration
 */
training_guard_config_t training_guard_default_config(void);

/* ============================================================================
 * LIFECYCLE MANAGEMENT
 * ============================================================================ */

/**
 * WHAT: Create a new training guard
 * WHY: Initialize guard for constraining training updates
 * HOW: Allocate resources, setup detection algorithms
 *
 * @param config Training configuration (NULL for defaults)
 * @param orchestrator Security orchestrator for integration (can be NULL)
 * @return Guard handle, or NULL on error
 *
 * ERRORS:
 * - Returns NULL if memory allocation fails
 * - Returns NULL if configuration is invalid
 *
 * MEMORY: Caller must call training_guard_destroy() when done
 */
training_guard_t training_guard_create(
    const training_guard_config_t* config,
    security_orchestrator_t orchestrator
);

/**
 * WHAT: Destroy training guard
 * WHY: Release all resources
 * HOW: Free internal structures, close checkpoint files
 *
 * @param guard Guard to destroy (NULL safe)
 */
void training_guard_destroy(training_guard_t guard);

/**
 * WHAT: Reset training guard state
 * WHY: Clear accumulated state without recreating
 * HOW: Reset detection state, history buffers
 *
 * @param guard Guard to reset
 * @return 0 on success, error code on failure
 */
int training_guard_reset(training_guard_t guard);

/* ============================================================================
 * GRADIENT VALIDATION AND CLIPPING
 * ============================================================================ */

/**
 * WHAT: Apply gradient constraints and validation
 * WHY: All gradient updates MUST pass through this function
 * HOW: Validate, clip, detect anomalies, return result
 *
 * @param guard Training guard
 * @param gradients Gradient buffer to process
 * @param result Output: processing result
 * @return 0 on success, error code on failure
 *
 * ALGORITHM:
 * 1. Check for NaN/Inf values
 * 2. Compute gradient norm
 * 3. Detect gradient anomalies
 * 4. Apply norm clipping if exceeded
 * 5. Apply value clipping if exceeded
 * 6. Check frozen parameters
 * 7. Trigger checkpoint if needed
 */
int training_guard_apply_gradients(
    training_guard_t guard,
    gradient_buffer_t* gradients,
    gradient_check_result_t* result
);

/**
 * WHAT: Check gradients without modification
 * WHY: Pre-check gradients before commitment
 * HOW: Run all checks without applying changes
 *
 * @param guard Training guard
 * @param gradients Gradient buffer to check
 * @param result Output: check result
 * @return 0 on success, error code on failure
 */
int training_guard_check_gradients(
    training_guard_t guard,
    const gradient_buffer_t* gradients,
    gradient_check_result_t* result
);

/**
 * WHAT: Clip gradients by norm
 * WHY: Prevent exploding gradients
 * HOW: Scale gradient vector to max norm
 *
 * @param guard Training guard
 * @param gradients Gradient array (modified in place)
 * @param size Number of gradient elements
 * @param max_norm Maximum allowed norm
 * @return Clip ratio (1.0 if no clipping)
 */
float training_guard_clip_by_norm(
    training_guard_t guard,
    float* gradients,
    uint32_t size,
    float max_norm
);

/**
 * WHAT: Clip gradients by value
 * WHY: Limit individual gradient magnitudes
 * HOW: Clamp each element to [-max_value, max_value]
 *
 * @param guard Training guard
 * @param gradients Gradient array (modified in place)
 * @param size Number of gradient elements
 * @param max_value Maximum allowed absolute value
 * @return Number of elements clipped
 */
uint32_t training_guard_clip_by_value(
    training_guard_t guard,
    float* gradients,
    uint32_t size,
    float max_value
);

/* ============================================================================
 * REWARD HACKING DETECTION
 * ============================================================================ */

/**
 * WHAT: Detect reward hacking behavior
 * WHY: Identify when system is manipulating its own rewards
 * HOW: Analyze reward signals for anomalous patterns
 *
 * @param guard Training guard
 * @param state Current reward state
 * @param result Output: detection result
 * @return true if reward hacking detected
 *
 * DETECTION METHODS:
 * - Reward signal variance analysis
 * - Action-value correlation anomalies
 * - Reward spike detection
 * - Self-reward loop detection
 */
bool training_guard_detect_reward_hacking(
    training_guard_t guard,
    const reward_state_t* state,
    reward_hacking_result_t* result
);

/**
 * WHAT: Update reward signal history
 * WHY: Track reward signals for analysis
 * HOW: Add to sliding window buffer
 *
 * @param guard Training guard
 * @param reward Current reward value
 * @param timestamp Timestamp (0 for current time)
 * @return 0 on success, error code on failure
 */
int training_guard_record_reward(
    training_guard_t guard,
    float reward,
    uint64_t timestamp
);

/**
 * WHAT: Get reward signal anomaly score
 * WHY: Quick check for reward anomalies
 * HOW: Compare current to historical distribution
 *
 * @param guard Training guard
 * @param current_reward Current reward value
 * @return Anomaly score (0 = normal, higher = more anomalous)
 */
float training_guard_get_reward_anomaly_score(
    training_guard_t guard,
    float current_reward
);

/* ============================================================================
 * GOAL DRIFT DETECTION
 * ============================================================================ */

/**
 * WHAT: Detect goal drift
 * WHY: Identify when training is causing goal deviation
 * HOW: Track distance from initial goal vector
 *
 * @param guard Training guard
 * @param current_goal Current goal vector
 * @param goal_dim Dimension of goal vector
 * @param result Output: detection result
 * @return true if goal drift detected
 */
bool training_guard_detect_goal_drift(
    training_guard_t guard,
    const float* current_goal,
    uint32_t goal_dim,
    goal_drift_result_t* result
);

/**
 * WHAT: Set reference goal vector
 * WHY: Establish baseline for drift detection
 * HOW: Store copy of goal vector
 *
 * @param guard Training guard
 * @param goal Reference goal vector
 * @param goal_dim Dimension of goal vector
 * @return 0 on success, error code on failure
 */
int training_guard_set_reference_goal(
    training_guard_t guard,
    const float* goal,
    uint32_t goal_dim
);

/**
 * WHAT: Get current drift from reference goal
 * WHY: Query drift magnitude without full detection
 * HOW: Compute distance to reference
 *
 * @param guard Training guard
 * @param current_goal Current goal vector
 * @param goal_dim Dimension of goal vector
 * @param drift_out Output: drift magnitude
 * @return 0 on success, error code on failure
 */
int training_guard_get_goal_drift(
    training_guard_t guard,
    const float* current_goal,
    uint32_t goal_dim,
    float* drift_out
);

/* ============================================================================
 * CHECKPOINTING AND ROLLBACK
 * ============================================================================ */

/**
 * WHAT: Create a checkpoint of current model state
 * WHY: Enable rollback if problems detected
 * HOW: Save model weights and metadata
 *
 * @param guard Training guard
 * @param model_data Model weights/state to checkpoint
 * @param model_size Size of model data in bytes
 * @param metadata Optional metadata string
 * @return Checkpoint ID, or -1 on error
 */
int32_t training_guard_create_checkpoint(
    training_guard_t guard,
    const void* model_data,
    size_t model_size,
    const char* metadata
);

/**
 * WHAT: Rollback to a previous checkpoint
 * WHY: Recover from detected problems
 * HOW: Load checkpoint and restore state
 *
 * @param guard Training guard
 * @param checkpoint_id Checkpoint to restore (-1 for latest)
 * @param model_data Output buffer for model data
 * @param model_size Size of output buffer
 * @param actual_size Output: actual size of restored data
 * @return 0 on success, error code on failure
 */
int training_guard_rollback(
    training_guard_t guard,
    int32_t checkpoint_id,
    void* model_data,
    size_t model_size,
    size_t* actual_size
);

/**
 * WHAT: Get number of available checkpoints
 * WHY: Query checkpoint availability
 * HOW: Return count from checkpoint manager
 *
 * @param guard Training guard
 * @return Number of checkpoints available
 */
uint32_t training_guard_get_checkpoint_count(training_guard_t guard);

/**
 * WHAT: Delete old checkpoints
 * WHY: Free storage space
 * HOW: Remove oldest checkpoints beyond limit
 *
 * @param guard Training guard
 * @param keep_count Number of checkpoints to keep
 * @return Number of checkpoints deleted
 */
uint32_t training_guard_prune_checkpoints(
    training_guard_t guard,
    uint32_t keep_count
);

/* ============================================================================
 * FROZEN PARAMETER MANAGEMENT
 * ============================================================================ */

/**
 * WHAT: Freeze a parameter (prevent gradient updates)
 * WHY: Protect critical parameters from modification
 * HOW: Add to frozen parameter set
 *
 * @param guard Training guard
 * @param param_index Parameter index to freeze
 * @return 0 on success, error code on failure
 */
int training_guard_freeze_parameter(
    training_guard_t guard,
    uint32_t param_index
);

/**
 * WHAT: Unfreeze a parameter
 * WHY: Allow updates to previously frozen parameter
 * HOW: Remove from frozen set
 *
 * @param guard Training guard
 * @param param_index Parameter index to unfreeze
 * @return 0 on success, error code on failure
 */
int training_guard_unfreeze_parameter(
    training_guard_t guard,
    uint32_t param_index
);

/**
 * WHAT: Check if parameter is frozen
 * WHY: Query frozen status
 * HOW: Lookup in frozen set
 *
 * @param guard Training guard
 * @param param_index Parameter index to check
 * @return true if frozen
 */
bool training_guard_is_parameter_frozen(
    training_guard_t guard,
    uint32_t param_index
);

/* ============================================================================
 * STATISTICS AND MONITORING
 * ============================================================================ */

/**
 * WHAT: Get training guard statistics
 * WHY: Monitor guard effectiveness
 * HOW: Copy accumulated statistics
 *
 * @param guard Training guard
 * @param stats Output: statistics structure
 * @return 0 on success, error code on failure
 */
int training_guard_get_stats(
    training_guard_t guard,
    training_guard_stats_t* stats
);

/**
 * WHAT: Reset training guard statistics
 * WHY: Clear statistics for fresh measurement
 * HOW: Zero all counters
 *
 * @param guard Training guard
 * @return 0 on success, error code on failure
 */
int training_guard_reset_stats(training_guard_t guard);

/**
 * WHAT: Record training loss for monitoring
 * WHY: Detect loss spikes and training instability
 * HOW: Add to loss history buffer
 *
 * @param guard Training guard
 * @param loss Current loss value
 * @return 0 on success, error code on failure
 */
int training_guard_record_loss(
    training_guard_t guard,
    float loss
);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

/**
 * WHAT: Get string name for training violation type
 * WHY: Human-readable violation identification
 * HOW: Lookup table
 *
 * @param violation Violation type (single flag)
 * @return String name (never NULL)
 */
const char* training_violation_name(training_violation_t violation);

/**
 * WHAT: Get string name for reward hacking type
 * WHY: Human-readable reward hacking identification
 * HOW: Lookup table
 *
 * @param type Reward hacking type
 * @return String name (never NULL)
 */
const char* reward_hacking_type_name(reward_hacking_type_t type);

/**
 * WHAT: Get string name for goal drift type
 * WHY: Human-readable goal drift identification
 * HOW: Lookup table
 *
 * @param type Goal drift type
 * @return String name (never NULL)
 */
const char* goal_drift_type_name(goal_drift_type_t type);

/**
 * WHAT: Print training guard summary to stdout
 * WHY: Debug and diagnostic output
 * HOW: Format and print guard state
 *
 * @param guard Training guard (NULL safe)
 */
void training_guard_print_summary(training_guard_t guard);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LGSS_TRAINING_GUARD_H */
