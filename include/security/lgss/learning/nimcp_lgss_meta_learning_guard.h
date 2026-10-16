/**
 * @file nimcp_lgss_meta_learning_guard.h
 * @brief Learning Guarded Safety System (LGSS) - Meta-Learning Guard
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Meta-learning guard that constrains learning-to-learn updates
 * WHY: Prevent meta-learning from modifying safety-critical learning
 *      mechanisms or creating unstable meta-level dynamics
 * HOW: Validate meta-parameter updates, monitor stability, track meta-drift
 *
 * META-LEARNING BACKGROUND:
 * Meta-learning ("learning to learn") modifies the learning process itself:
 * - Learning rate adaptation (MAML, Reptile)
 * - Architecture search (NAS)
 * - Hyperparameter optimization
 * - Plasticity rule modification
 *
 * SECURITY CONCERNS:
 * - Meta-learning could modify safety constraints
 * - Unstable meta-dynamics could cause rapid changes
 * - Meta-overfitting to recent experience could cause catastrophic forgetting
 * - Meta-learning could optimize for reward hacking strategies
 *
 * DEFENSES:
 * - Frozen meta-parameters for safety-critical settings
 * - Meta-update magnitude limits
 * - Stability monitoring via eigenvalue analysis
 * - Meta-drift detection
 * - Hierarchical approval for meta-changes
 *
 * USAGE:
 * @code
 * meta_learning_guard_config_t config = meta_learning_guard_default_config();
 * config.enable_stability_monitoring = true;
 *
 * meta_learning_guard_t* guard = meta_learning_guard_create(&config, orchestrator);
 *
 * // Freeze safety-critical meta-parameters
 * meta_learning_guard_freeze_meta_param(guard, LEARNING_RATE_INDEX);
 *
 * // Before any meta-learning update
 * meta_update_proposal_t proposal = { .param_id = id, .new_value = value };
 * meta_update_result_t result;
 * if (meta_learning_guard_propose_update(guard, &proposal, &result) == 0) {
 *     if (result.allowed) {
 *         // Apply meta-update
 *     }
 * }
 * @endcode
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_LGSS_META_LEARNING_GUARD_H
#define NIMCP_LGSS_META_LEARNING_GUARD_H

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

/** Maximum number of meta-parameters to track */
#define META_MAX_PARAMETERS 256

/** Maximum number of frozen meta-parameters */
#define META_MAX_FROZEN_PARAMS 64

/** Default meta-update magnitude limit */
#define META_DEFAULT_UPDATE_LIMIT 0.1f

/** Default stability threshold (max eigenvalue) */
#define META_DEFAULT_STABILITY_THRESHOLD 1.0f

/** Default meta-drift threshold */
#define META_DEFAULT_DRIFT_THRESHOLD 0.5f

/** History size for stability analysis */
#define META_STABILITY_HISTORY_SIZE 100

/** Eigenvalue history for stability monitoring */
#define META_EIGENVALUE_HISTORY_SIZE 50

/** Magic number for meta-learning guard validation */
#define LGSS_META_LEARNING_GUARD_MAGIC 0x4D455441 /* 'META' */

/* ============================================================================
 * META-PARAMETER TYPES
 * ============================================================================ */

/**
 * WHAT: Types of meta-parameters
 * WHY: Categorize meta-parameters for appropriate handling
 * HOW: Enum of meta-parameter categories
 */
typedef enum {
    META_PARAM_LEARNING_RATE = 0,         /**< Learning rate parameters */
    META_PARAM_PLASTICITY_RULE,           /**< Plasticity rule parameters */
    META_PARAM_ARCHITECTURE,              /**< Architecture parameters */
    META_PARAM_OPTIMIZER,                 /**< Optimizer parameters */
    META_PARAM_EXPLORATION,               /**< Exploration parameters */
    META_PARAM_REGULARIZATION,            /**< Regularization parameters */
    META_PARAM_LOSS_WEIGHTS,              /**< Loss function weights */
    META_PARAM_ATTENTION,                 /**< Attention mechanism params */
    META_PARAM_MEMORY,                    /**< Memory system params */
    META_PARAM_REWARD_SHAPING,            /**< Reward shaping params */
    META_PARAM_SAFETY,                    /**< Safety-related params */
    META_PARAM_CUSTOM,                    /**< Custom meta-parameters */
    META_PARAM_TYPE_COUNT
} meta_param_type_t;

/* ============================================================================
 * VIOLATION AND DETECTION TYPES
 * ============================================================================ */

/**
 * WHAT: Types of meta-learning guard violations
 * WHY: Categorize violations for response
 * HOW: Bitmask flags for efficient tracking
 */
typedef enum {
    META_VIOLATION_NONE                 = 0,
    META_VIOLATION_MAGNITUDE            = (1 << 0), /**< Update too large */
    META_VIOLATION_FROZEN_PARAM         = (1 << 1), /**< Frozen parameter */
    META_VIOLATION_STABILITY            = (1 << 2), /**< Stability concern */
    META_VIOLATION_DRIFT                = (1 << 3), /**< Meta-drift detected */
    META_VIOLATION_RATE_LIMIT           = (1 << 4), /**< Updates too frequent */
    META_VIOLATION_SAFETY_PARAM         = (1 << 5), /**< Safety param modified */
    META_VIOLATION_BOUNDS               = (1 << 6), /**< Value out of bounds */
    META_VIOLATION_CONSISTENCY          = (1 << 7), /**< Inconsistent update */
    META_VIOLATION_APPROVAL_REQUIRED    = (1 << 8), /**< Higher approval needed */
    META_VIOLATION_OSCILLATION          = (1 << 9)  /**< Oscillating updates */
} meta_violation_t;

/**
 * WHAT: Stability states for meta-learning
 * WHY: Categorize system stability
 * HOW: Enum of stability levels
 */
typedef enum {
    META_STABILITY_STABLE = 0,            /**< Stable dynamics */
    META_STABILITY_MARGINAL,              /**< Marginally stable */
    META_STABILITY_OSCILLATING,           /**< Oscillating behavior */
    META_STABILITY_DIVERGING,             /**< Diverging dynamics */
    META_STABILITY_UNKNOWN                /**< Unknown stability */
} meta_stability_state_t;

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/**
 * WHAT: A meta-parameter with bounds and metadata
 * WHY: Track meta-parameter properties for validation
 * HOW: Struct with value, bounds, and metadata
 */
typedef struct {
    uint32_t param_id;                    /**< Parameter identifier */
    meta_param_type_t type;               /**< Parameter type */
    float current_value;                  /**< Current value */
    float initial_value;                  /**< Initial value (reference) */
    float min_value;                      /**< Minimum allowed value */
    float max_value;                      /**< Maximum allowed value */
    float max_change_per_update;          /**< Max change per update */
    bool is_frozen;                       /**< Whether parameter is frozen */
    bool requires_approval;               /**< Requires higher approval */
    uint32_t approval_level;              /**< Required approval level */
    char name[64];                        /**< Human-readable name */
} meta_param_info_t;

/**
 * WHAT: A proposed meta-learning update
 * WHY: Encapsulate update for validation
 * HOW: Struct with parameter ID and new value
 */
typedef struct {
    uint32_t param_id;                    /**< Parameter to update */
    float proposed_value;                 /**< Proposed new value */
    float learning_signal;                /**< Meta-learning signal strength */
    uint32_t requester_id;                /**< ID of requesting component */
    uint32_t approval_level;              /**< Approval level of requester */
    uint64_t timestamp;                   /**< Proposal timestamp */
    char reason[128];                     /**< Reason for update */
} meta_update_proposal_t;

/**
 * WHAT: Result of meta-update validation
 * WHY: Provide detailed information about validation
 * HOW: Struct with allowed flag, violations, and details
 */
typedef struct {
    bool allowed;                         /**< Whether update is allowed */
    meta_violation_t violations;          /**< Detected violations */
    float original_change;                /**< Originally requested change */
    float allowed_change;                 /**< Maximum allowed change */
    float adjusted_value;                 /**< Adjusted value (if clamped) */
    meta_stability_state_t stability;     /**< Predicted stability impact */
    bool requires_higher_approval;        /**< Needs higher approval level */
    uint32_t required_approval_level;     /**< Required approval level */
    char reason[256];                     /**< Reason if not allowed */
} meta_update_result_t;

/**
 * WHAT: Configuration for meta-learning guard
 * WHY: Customize meta-learning safety bounds
 * HOW: Struct with meta-learning specific parameters
 */
typedef struct {
    /* Update constraints */
    float max_update_magnitude;           /**< Max magnitude per update */
    float max_cumulative_update;          /**< Max cumulative change */
    uint32_t max_updates_per_second;      /**< Max meta-updates per second */
    float update_window_sec;              /**< Window for rate limiting */

    /* Stability monitoring */
    bool enable_stability_monitoring;     /**< Enable stability analysis */
    float stability_threshold;            /**< Max eigenvalue for stability */
    uint32_t stability_window_size;       /**< Window for stability analysis */
    float oscillation_threshold;          /**< Threshold for oscillation detect */

    /* Meta-drift detection */
    bool enable_drift_detection;          /**< Enable meta-drift detection */
    float drift_threshold;                /**< Max allowed meta-drift */
    float drift_ewma_alpha;               /**< EWMA alpha for drift tracking */

    /* Safety parameter protection */
    bool protect_safety_params;           /**< Extra protection for safety */
    float safety_param_update_limit;      /**< Stricter limit for safety params */

    /* Hierarchical approval */
    bool enable_hierarchical_approval;    /**< Require approval for changes */
    uint32_t base_approval_level;         /**< Base required approval level */
    float approval_threshold_magnitude;   /**< Magnitude requiring approval */

    /* Bounds enforcement */
    bool enforce_bounds;                  /**< Enforce min/max bounds */
    bool clamp_to_bounds;                 /**< Clamp vs reject out-of-bounds */

    /* Oscillation prevention */
    bool enable_oscillation_prevention;   /**< Detect and prevent oscillation */
    uint32_t oscillation_window_size;     /**< Window for oscillation detection */

    /* Logging and monitoring */
    bool enable_violation_logging;        /**< Log violations */
    bool enable_statistics;               /**< Track detailed statistics */
} meta_learning_guard_config_t;

/**
 * WHAT: Statistics about meta-learning guard operation
 * WHY: Monitor guard effectiveness
 * HOW: Counters and metrics for meta-learning operations
 */
typedef struct {
    /* Update statistics */
    uint64_t total_proposals;             /**< Total update proposals */
    uint64_t proposals_allowed;           /**< Proposals allowed */
    uint64_t proposals_blocked;           /**< Proposals blocked */
    uint64_t proposals_clamped;           /**< Proposals that were clamped */

    /* Violation breakdown */
    uint64_t magnitude_violations;        /**< Magnitude violations */
    uint64_t frozen_violations;           /**< Frozen param violations */
    uint64_t stability_violations;        /**< Stability violations */
    uint64_t drift_violations;            /**< Meta-drift violations */
    uint64_t rate_limit_violations;       /**< Rate limit violations */
    uint64_t safety_param_violations;     /**< Safety param violations */
    uint64_t bounds_violations;           /**< Bounds violations */
    uint64_t oscillation_detections;      /**< Oscillation detections */

    /* Stability statistics */
    meta_stability_state_t current_stability; /**< Current stability state */
    float max_eigenvalue_seen;            /**< Maximum eigenvalue seen */
    float avg_eigenvalue;                 /**< Average max eigenvalue */
    uint64_t stability_transitions;       /**< Number of stability transitions */

    /* Meta-drift statistics */
    float cumulative_drift;               /**< Cumulative meta-drift */
    float max_drift_seen;                 /**< Maximum drift seen */
    float drift_rate;                     /**< Current drift rate */

    /* Update statistics */
    float avg_update_magnitude;           /**< Average update magnitude */
    float max_update_magnitude_seen;      /**< Maximum update magnitude */
    float current_update_rate;            /**< Current updates per second */

    /* Parameter statistics */
    uint32_t frozen_param_count;          /**< Number of frozen params */
    uint32_t total_params_tracked;        /**< Total parameters tracked */

    /* Performance */
    uint64_t guard_overhead_us;           /**< Total guard overhead */
    float avg_validation_time_us;         /**< Average validation time */
} meta_learning_guard_stats_t;

/* ============================================================================
 * OPAQUE HANDLE
 * ============================================================================ */

/**
 * WHAT: Opaque handle to meta-learning guard
 * WHY: Encapsulation - hide internal implementation
 * HOW: Pimpl idiom - pointer to internal structure
 */
typedef struct meta_learning_guard_internal* meta_learning_guard_t;

/* ============================================================================
 * DEFAULT CONFIGURATION
 * ============================================================================ */

/**
 * WHAT: Get default meta-learning guard configuration
 * WHY: Provide sensible defaults for common use cases
 * HOW: Return pre-configured struct
 *
 * DEFAULT VALUES:
 * - max_update_magnitude: 0.1
 * - max_cumulative_update: 1.0
 * - max_updates_per_second: 100
 * - enable_stability_monitoring: true
 * - stability_threshold: 1.0
 * - enable_drift_detection: true
 * - drift_threshold: 0.5
 * - protect_safety_params: true
 * - enable_hierarchical_approval: true
 * - enforce_bounds: true
 * - clamp_to_bounds: true
 * - enable_oscillation_prevention: true
 *
 * @return Default configuration
 */
meta_learning_guard_config_t meta_learning_guard_default_config(void);

/* ============================================================================
 * LIFECYCLE MANAGEMENT
 * ============================================================================ */

/**
 * WHAT: Create a new meta-learning guard
 * WHY: Initialize guard for constraining meta-learning
 * HOW: Allocate resources, setup stability analysis
 *
 * @param config Meta-learning configuration (NULL for defaults)
 * @param orchestrator Security orchestrator for integration (can be NULL)
 * @return Guard handle, or NULL on error
 *
 * ERRORS:
 * - Returns NULL if memory allocation fails
 * - Returns NULL if configuration is invalid
 *
 * MEMORY: Caller must call meta_learning_guard_destroy() when done
 */
meta_learning_guard_t meta_learning_guard_create(
    const meta_learning_guard_config_t* config,
    security_orchestrator_t orchestrator
);

/**
 * WHAT: Destroy meta-learning guard
 * WHY: Release all resources
 * HOW: Free internal structures
 *
 * @param guard Guard to destroy (NULL safe)
 */
void meta_learning_guard_destroy(meta_learning_guard_t guard);

/**
 * WHAT: Reset meta-learning guard state
 * WHY: Clear accumulated state without recreating
 * HOW: Reset tracking, stability analysis, statistics
 *
 * @param guard Guard to reset
 * @return 0 on success, error code on failure
 */
int meta_learning_guard_reset(meta_learning_guard_t guard);

/* ============================================================================
 * META-PARAMETER REGISTRATION
 * ============================================================================ */

/**
 * WHAT: Register a meta-parameter with the guard
 * WHY: Track meta-parameter for validation
 * HOW: Add to parameter registry with bounds
 *
 * @param guard Meta-learning guard
 * @param info Parameter information
 * @return 0 on success, error code on failure
 */
int meta_learning_guard_register_param(
    meta_learning_guard_t guard,
    const meta_param_info_t* info
);

/**
 * WHAT: Unregister a meta-parameter
 * WHY: Stop tracking parameter
 * HOW: Remove from registry
 *
 * @param guard Meta-learning guard
 * @param param_id Parameter identifier
 * @return 0 on success, error code on failure
 */
int meta_learning_guard_unregister_param(
    meta_learning_guard_t guard,
    uint32_t param_id
);

/**
 * WHAT: Get meta-parameter information
 * WHY: Query parameter metadata
 * HOW: Lookup in registry
 *
 * @param guard Meta-learning guard
 * @param param_id Parameter identifier
 * @param info Output: parameter information
 * @return 0 on success, error code on failure
 */
int meta_learning_guard_get_param_info(
    meta_learning_guard_t guard,
    uint32_t param_id,
    meta_param_info_t* info
);

/* ============================================================================
 * META-UPDATE VALIDATION
 * ============================================================================ */

/**
 * WHAT: Propose a meta-learning update
 * WHY: All meta-updates MUST pass through this function
 * HOW: Validate update against all constraints
 *
 * @param guard Meta-learning guard
 * @param proposal Update proposal
 * @param result Output: validation result
 * @return 0 on success, error code on failure
 *
 * VALIDATION STEPS:
 * 1. Check if parameter is frozen
 * 2. Check update magnitude
 * 3. Check bounds
 * 4. Check rate limiting
 * 5. Analyze stability impact
 * 6. Check meta-drift
 * 7. Check hierarchical approval
 * 8. Detect oscillation patterns
 */
int meta_learning_guard_propose_update(
    meta_learning_guard_t guard,
    const meta_update_proposal_t* proposal,
    meta_update_result_t* result
);

/**
 * WHAT: Commit an approved meta-update
 * WHY: Apply update after validation
 * HOW: Update internal state tracking
 *
 * @param guard Meta-learning guard
 * @param param_id Parameter that was updated
 * @param new_value New parameter value
 * @return 0 on success, error code on failure
 *
 * NOTE: Call this after successfully applying the update externally
 */
int meta_learning_guard_commit_update(
    meta_learning_guard_t guard,
    uint32_t param_id,
    float new_value
);

/**
 * WHAT: Propose batch of meta-updates
 * WHY: Efficiently validate multiple updates
 * HOW: Validate each update, return individual results
 *
 * @param guard Meta-learning guard
 * @param proposals Array of proposals
 * @param results Array of results (same size)
 * @param count Number of proposals
 * @return Number of proposals allowed
 */
uint32_t meta_learning_guard_propose_batch(
    meta_learning_guard_t guard,
    const meta_update_proposal_t* proposals,
    meta_update_result_t* results,
    uint32_t count
);

/* ============================================================================
 * FROZEN PARAMETER MANAGEMENT
 * ============================================================================ */

/**
 * WHAT: Freeze a meta-parameter
 * WHY: Prevent modifications to critical meta-parameters
 * HOW: Add to frozen set
 *
 * @param guard Meta-learning guard
 * @param param_id Parameter to freeze
 * @return 0 on success, error code on failure
 */
int meta_learning_guard_freeze_param(
    meta_learning_guard_t guard,
    uint32_t param_id
);

/**
 * WHAT: Unfreeze a meta-parameter
 * WHY: Allow modifications to previously frozen parameter
 * HOW: Remove from frozen set
 *
 * @param guard Meta-learning guard
 * @param param_id Parameter to unfreeze
 * @return 0 on success, error code on failure
 *
 * NOTE: Requires elevated approval
 */
int meta_learning_guard_unfreeze_param(
    meta_learning_guard_t guard,
    uint32_t param_id
);

/**
 * WHAT: Check if meta-parameter is frozen
 * WHY: Query frozen status
 * HOW: Lookup in frozen set
 *
 * @param guard Meta-learning guard
 * @param param_id Parameter to check
 * @return true if frozen
 */
bool meta_learning_guard_is_param_frozen(
    meta_learning_guard_t guard,
    uint32_t param_id
);

/**
 * WHAT: Bulk freeze meta-parameters by type
 * WHY: Protect all parameters of a certain type
 * HOW: Freeze all registered params of given type
 *
 * @param guard Meta-learning guard
 * @param type Parameter type to freeze
 * @return Number of parameters frozen
 */
uint32_t meta_learning_guard_freeze_by_type(
    meta_learning_guard_t guard,
    meta_param_type_t type
);

/* ============================================================================
 * STABILITY MONITORING
 * ============================================================================ */

/**
 * WHAT: Get current stability state
 * WHY: Query meta-learning stability
 * HOW: Return current stability assessment
 *
 * @param guard Meta-learning guard
 * @param state Output: stability state
 * @return 0 on success, error code on failure
 */
int meta_learning_guard_get_stability(
    meta_learning_guard_t guard,
    meta_stability_state_t* state
);

/**
 * WHAT: Update stability with new Jacobian estimate
 * WHY: Track stability via eigenvalue analysis
 * HOW: Estimate max eigenvalue from recent updates
 *
 * @param guard Meta-learning guard
 * @param jacobian_estimate Estimated Jacobian norm
 * @return 0 on success, error code on failure
 *
 * NOTE: The Jacobian describes how meta-updates affect learning
 */
int meta_learning_guard_update_stability(
    meta_learning_guard_t guard,
    float jacobian_estimate
);

/**
 * WHAT: Get current maximum eigenvalue estimate
 * WHY: Monitor stability quantitatively
 * HOW: Return tracked eigenvalue
 *
 * @param guard Meta-learning guard
 * @param eigenvalue Output: max eigenvalue estimate
 * @return 0 on success, error code on failure
 */
int meta_learning_guard_get_eigenvalue(
    meta_learning_guard_t guard,
    float* eigenvalue
);

/**
 * WHAT: Predict stability impact of proposed update
 * WHY: Assess update before applying
 * HOW: Estimate eigenvalue change
 *
 * @param guard Meta-learning guard
 * @param proposal Update proposal
 * @param predicted_stability Output: predicted stability state
 * @return 0 on success, error code on failure
 */
int meta_learning_guard_predict_stability_impact(
    meta_learning_guard_t guard,
    const meta_update_proposal_t* proposal,
    meta_stability_state_t* predicted_stability
);

/* ============================================================================
 * META-DRIFT DETECTION
 * ============================================================================ */

/**
 * WHAT: Get current meta-drift magnitude
 * WHY: Monitor deviation from initial meta-parameters
 * HOW: Calculate distance from initial values
 *
 * @param guard Meta-learning guard
 * @param drift Output: drift magnitude
 * @return 0 on success, error code on failure
 */
int meta_learning_guard_get_drift(
    meta_learning_guard_t guard,
    float* drift
);

/**
 * WHAT: Reset meta-drift baseline
 * WHY: Set new reference point for drift detection
 * HOW: Update initial values to current values
 *
 * @param guard Meta-learning guard
 * @return 0 on success, error code on failure
 *
 * NOTE: Use with caution - only when drift is intentional
 */
int meta_learning_guard_reset_drift_baseline(meta_learning_guard_t guard);

/**
 * WHAT: Get drift rate
 * WHY: Monitor how fast meta-parameters are drifting
 * HOW: Calculate rate from recent history
 *
 * @param guard Meta-learning guard
 * @param drift_rate Output: drift rate (per update)
 * @return 0 on success, error code on failure
 */
int meta_learning_guard_get_drift_rate(
    meta_learning_guard_t guard,
    float* drift_rate
);

/* ============================================================================
 * HIERARCHICAL APPROVAL
 * ============================================================================ */

/**
 * WHAT: Set required approval level for a parameter
 * WHY: Customize access control per parameter
 * HOW: Update parameter's approval requirement
 *
 * @param guard Meta-learning guard
 * @param param_id Parameter identifier
 * @param approval_level Required approval level
 * @return 0 on success, error code on failure
 */
int meta_learning_guard_set_approval_level(
    meta_learning_guard_t guard,
    uint32_t param_id,
    uint32_t approval_level
);

/**
 * WHAT: Get required approval level for a parameter
 * WHY: Query approval requirements
 * HOW: Lookup parameter's approval level
 *
 * @param guard Meta-learning guard
 * @param param_id Parameter identifier
 * @param approval_level Output: required approval level
 * @return 0 on success, error code on failure
 */
int meta_learning_guard_get_approval_level(
    meta_learning_guard_t guard,
    uint32_t param_id,
    uint32_t* approval_level
);

/* ============================================================================
 * STATISTICS AND MONITORING
 * ============================================================================ */

/**
 * WHAT: Get meta-learning guard statistics
 * WHY: Monitor guard effectiveness
 * HOW: Copy accumulated statistics
 *
 * @param guard Meta-learning guard
 * @param stats Output: statistics structure
 * @return 0 on success, error code on failure
 */
int meta_learning_guard_get_stats(
    meta_learning_guard_t guard,
    meta_learning_guard_stats_t* stats
);

/**
 * WHAT: Reset meta-learning guard statistics
 * WHY: Clear statistics for fresh measurement
 * HOW: Zero all counters
 *
 * @param guard Meta-learning guard
 * @return 0 on success, error code on failure
 */
int meta_learning_guard_reset_stats(meta_learning_guard_t guard);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

/**
 * WHAT: Get string name for meta-parameter type
 * WHY: Human-readable type identification
 * HOW: Lookup table
 *
 * @param type Meta-parameter type
 * @return String name (never NULL)
 */
const char* meta_param_type_name(meta_param_type_t type);

/**
 * WHAT: Get string name for meta violation type
 * WHY: Human-readable violation identification
 * HOW: Lookup table
 *
 * @param violation Violation type (single flag)
 * @return String name (never NULL)
 */
const char* meta_violation_name(meta_violation_t violation);

/**
 * WHAT: Get string name for stability state
 * WHY: Human-readable stability identification
 * HOW: Lookup table
 *
 * @param state Stability state
 * @return String name (never NULL)
 */
const char* meta_stability_state_name(meta_stability_state_t state);

/**
 * WHAT: Format meta violation flags as string
 * WHY: Human-readable multi-violation description
 * HOW: Concatenate flag names
 *
 * @param violations Violation bitmask
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of characters written
 */
int meta_violations_to_string(
    meta_violation_t violations,
    char* buffer,
    size_t buffer_size
);

/**
 * WHAT: Print meta-learning guard summary to stdout
 * WHY: Debug and diagnostic output
 * HOW: Format and print guard state
 *
 * @param guard Meta-learning guard (NULL safe)
 */
void meta_learning_guard_print_summary(meta_learning_guard_t guard);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LGSS_META_LEARNING_GUARD_H */
