/**
 * @file nimcp_lgss_plasticity_constraints.h
 * @brief Learning Guarded Safety System (LGSS) - Plasticity Constraints
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Plasticity guard that constrains all synaptic weight updates
 * WHY: Prevent the system from modifying its own safety-critical synapses
 *      and ensure learning remains within biologically plausible bounds
 * HOW: All weight updates must pass through the plasticity guard, which
 *      enforces rate limits, magnitude limits, and frozen synapse protection
 *
 * SECURITY PROPERTIES:
 * - Rate limiting prevents rapid weight modification attacks
 * - Magnitude limits ensure biologically plausible changes
 * - Frozen synapses protect safety-critical pathways
 * - Homeostatic regulation maintains network stability
 * - Self-reward blocking prevents reward hacking
 *
 * USAGE:
 * @code
 * plasticity_safety_config_t config = plasticity_default_config();
 * config.block_self_reward = true;
 *
 * plasticity_guard_t* guard = plasticity_guard_create(&config, orchestrator);
 *
 * // Freeze critical synapses
 * plasticity_guard_freeze_synapse(guard, critical_synapse_id);
 *
 * // All weight updates MUST go through guard
 * if (plasticity_guard_would_violate(guard, synapse_id, old_weight, new_weight)) {
 *     // Reject update
 * } else {
 *     plasticity_guard_apply_update(guard, synapse_id, &new_weight);
 * }
 * @endcode
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_LGSS_PLASTICITY_CONSTRAINTS_H
#define NIMCP_LGSS_PLASTICITY_CONSTRAINTS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * FORWARD DECLARATIONS
 * ============================================================================ */

/** Forward declare security orchestrator for integration */
typedef struct security_orchestrator_struct* security_orchestrator_t;

/* ============================================================================
 * CONSTANTS AND LIMITS
 * ============================================================================ */

/** Maximum number of frozen synapses per guard */
#define LGSS_MAX_FROZEN_SYNAPSES 4096

/** Maximum number of frozen pathways per guard */
#define LGSS_MAX_FROZEN_PATHWAYS 256

/** Default sliding window size for rate limiting (seconds) */
#define LGSS_RATE_LIMIT_WINDOW_SEC 1.0f

/** Default max updates per second */
#define LGSS_DEFAULT_MAX_UPDATES_PER_SEC 1000

/** Default max weight change per update */
#define LGSS_DEFAULT_MAX_WEIGHT_CHANGE 0.1f

/** Default max learning rate */
#define LGSS_DEFAULT_MAX_LEARNING_RATE 0.01f

/** Default min learning rate */
#define LGSS_DEFAULT_MIN_LEARNING_RATE 0.0001f

/** Default max total weight drift */
#define LGSS_DEFAULT_MAX_TOTAL_DRIFT 1.0f

/** Default homeostatic target activity */
#define LGSS_DEFAULT_HOMEOSTATIC_TARGET 0.5f

/** Magic number for plasticity guard validation */
#define LGSS_PLASTICITY_GUARD_MAGIC 0x50474152 /* 'PGAR' */

/* ============================================================================
 * VIOLATION TYPES
 * ============================================================================ */

/**
 * WHAT: Types of plasticity constraint violations
 * WHY: Categorize violations for logging and response
 * HOW: Bitmask flags for efficient multi-violation tracking
 */
typedef enum {
    PLASTICITY_VIOLATION_NONE               = 0,
    PLASTICITY_VIOLATION_RATE_LIMIT         = (1 << 0), /**< Updates too fast */
    PLASTICITY_VIOLATION_MAGNITUDE          = (1 << 1), /**< Change too large */
    PLASTICITY_VIOLATION_FROZEN_SYNAPSE     = (1 << 2), /**< Synapse is frozen */
    PLASTICITY_VIOLATION_FROZEN_PATHWAY     = (1 << 3), /**< Pathway is frozen */
    PLASTICITY_VIOLATION_SELF_REWARD        = (1 << 4), /**< Self-reward attempt */
    PLASTICITY_VIOLATION_REWARD_PATHWAY     = (1 << 5), /**< Reward pathway mod */
    PLASTICITY_VIOLATION_TOTAL_DRIFT        = (1 << 6), /**< Exceeds total drift */
    PLASTICITY_VIOLATION_LEARNING_RATE      = (1 << 7), /**< Invalid learning rate */
    PLASTICITY_VIOLATION_HOMEOSTATIC        = (1 << 8), /**< Violates homeostasis */
    PLASTICITY_VIOLATION_INVALID_WEIGHT     = (1 << 9)  /**< Weight out of bounds */
} plasticity_violation_t;

/* ============================================================================
 * CONFIGURATION STRUCTURES
 * ============================================================================ */

/**
 * WHAT: Configuration for plasticity safety constraints
 * WHY: Customize safety bounds based on system requirements
 * HOW: Struct with all configurable safety parameters
 */
typedef struct {
    /* Weight change limits */
    float max_weight_change_per_update;   /**< Maximum delta per update [0, 1] */
    float max_learning_rate;              /**< Maximum allowed learning rate */
    float min_learning_rate;              /**< Minimum allowed learning rate */

    /* Rate limiting */
    uint32_t max_updates_per_second;      /**< Maximum updates allowed per sec */
    float rate_limit_window_sec;          /**< Sliding window duration */

    /* Reward pathway protection */
    bool block_self_reward;               /**< Block self-modification of reward */
    bool block_reward_pathway_mod;        /**< Block all reward pathway mods */

    /* Frozen synapses (safety-critical) */
    uint64_t* frozen_synapse_ids;         /**< Array of frozen synapse IDs */
    uint32_t num_frozen_synapses;         /**< Count of frozen synapses */

    /* Weight drift limits */
    float max_total_weight_drift;         /**< Max cumulative drift from init */

    /* Homeostatic regulation */
    float homeostatic_target;             /**< Target activity level [0, 1] */
    bool enable_homeostatic_regulation;   /**< Enable homeostatic constraints */
    float homeostatic_time_constant;      /**< Time constant for regulation */

    /* Weight bounds */
    float min_weight;                     /**< Minimum allowed weight value */
    float max_weight;                     /**< Maximum allowed weight value */

    /* Logging and monitoring */
    bool enable_violation_logging;        /**< Log all violations */
    bool enable_statistics;               /**< Track detailed statistics */
} plasticity_safety_config_t;

/**
 * WHAT: Statistics about plasticity guard operation
 * WHY: Monitor guard effectiveness and tune parameters
 * HOW: Counters and metrics accumulated during operation
 */
typedef struct {
    /* Update statistics */
    uint64_t total_updates_attempted;     /**< Total update attempts */
    uint64_t updates_allowed;             /**< Updates that passed guard */
    uint64_t updates_blocked;             /**< Updates blocked by guard */

    /* Violation breakdown */
    uint64_t rate_limit_violations;       /**< Rate limit violations */
    uint64_t magnitude_violations;        /**< Magnitude violations */
    uint64_t frozen_synapse_violations;   /**< Frozen synapse violations */
    uint64_t frozen_pathway_violations;   /**< Frozen pathway violations */
    uint64_t self_reward_violations;      /**< Self-reward violations */
    uint64_t reward_pathway_violations;   /**< Reward pathway violations */
    uint64_t total_drift_violations;      /**< Total drift violations */
    uint64_t learning_rate_violations;    /**< Learning rate violations */
    uint64_t homeostatic_violations;      /**< Homeostatic violations */

    /* Weight tracking */
    float cumulative_drift;               /**< Current cumulative drift */
    float avg_weight_change;              /**< Average weight change magnitude */
    float max_weight_change_seen;         /**< Maximum change seen */

    /* Rate tracking */
    float current_update_rate;            /**< Current updates per second */
    float peak_update_rate;               /**< Peak update rate seen */

    /* Timing */
    uint64_t guard_overhead_us;           /**< Total guard overhead (microseconds) */
    float avg_check_time_us;              /**< Average check time (microseconds) */
} plasticity_guard_stats_t;

/**
 * WHAT: Result of a plasticity update check
 * WHY: Provide detailed information about check outcome
 * HOW: Struct with violation flags, modified weight, and details
 */
typedef struct {
    bool allowed;                         /**< Whether update is allowed */
    plasticity_violation_t violations;    /**< Bitmask of violations */
    float original_delta;                 /**< Original weight change */
    float adjusted_delta;                 /**< Adjusted weight change (if clamped) */
    float adjusted_weight;                /**< Final weight value (if clamped) */
    char reason[128];                     /**< Human-readable reason if blocked */
} plasticity_check_result_t;

/* ============================================================================
 * OPAQUE HANDLE
 * ============================================================================ */

/**
 * WHAT: Opaque handle to plasticity guard
 * WHY: Encapsulation - hide internal implementation
 * HOW: Pimpl idiom - pointer to internal structure
 */
typedef struct plasticity_guard_internal* plasticity_guard_t;

/* ============================================================================
 * DEFAULT CONFIGURATION
 * ============================================================================ */

/**
 * WHAT: Get default plasticity safety configuration
 * WHY: Provide sensible defaults for common use cases
 * HOW: Return pre-configured struct with safe values
 *
 * DEFAULT VALUES:
 * - max_weight_change_per_update: 0.1
 * - max_learning_rate: 0.01
 * - min_learning_rate: 0.0001
 * - max_updates_per_second: 1000
 * - block_self_reward: true
 * - block_reward_pathway_mod: true
 * - max_total_weight_drift: 1.0
 * - homeostatic_target: 0.5
 * - enable_homeostatic_regulation: true
 * - min_weight: -1.0
 * - max_weight: 1.0
 *
 * @return Default configuration
 */
plasticity_safety_config_t plasticity_default_config(void);

/* ============================================================================
 * LIFECYCLE MANAGEMENT
 * ============================================================================ */

/**
 * WHAT: Create a new plasticity guard
 * WHY: Initialize guard for constraining weight updates
 * HOW: Allocate resources, initialize rate limiting, setup frozen synapse hashmap
 *
 * @param config Safety configuration (NULL for defaults)
 * @param orchestrator Security orchestrator for integration (can be NULL)
 * @return Guard handle, or NULL on error
 *
 * ERRORS:
 * - Returns NULL if memory allocation fails
 * - Returns NULL if configuration is invalid
 *
 * MEMORY: Caller must call plasticity_guard_destroy() when done
 * THREAD SAFETY: Returned guard is thread-safe
 */
plasticity_guard_t plasticity_guard_create(
    const plasticity_safety_config_t* config,
    security_orchestrator_t orchestrator
);

/**
 * WHAT: Destroy plasticity guard
 * WHY: Release all resources
 * HOW: Free internal structures, hashmap, sliding window buffers
 *
 * @param guard Guard to destroy (NULL safe)
 */
void plasticity_guard_destroy(plasticity_guard_t guard);

/**
 * WHAT: Reset plasticity guard state
 * WHY: Clear accumulated state without recreating guard
 * HOW: Reset rate limiting, drift tracking, statistics
 *
 * @param guard Guard to reset
 * @return 0 on success, error code on failure
 */
int plasticity_guard_reset(plasticity_guard_t guard);

/* ============================================================================
 * FROZEN SYNAPSE MANAGEMENT
 * ============================================================================ */

/**
 * WHAT: Freeze a specific synapse (prevent all modifications)
 * WHY: Protect safety-critical synapses from modification
 * HOW: Add synapse ID to frozen hashmap
 *
 * @param guard Plasticity guard
 * @param synapse_id Synapse identifier to freeze
 * @return 0 on success, error code on failure
 *
 * ERRORS:
 * - NIMCP_ERROR_OUT_OF_RANGE if max frozen synapses reached
 * - NIMCP_ERROR_ALREADY_EXISTS if synapse already frozen
 */
int plasticity_guard_freeze_synapse(
    plasticity_guard_t guard,
    uint64_t synapse_id
);

/**
 * WHAT: Unfreeze a specific synapse
 * WHY: Allow modification of previously frozen synapse
 * HOW: Remove synapse ID from frozen hashmap
 *
 * @param guard Plasticity guard
 * @param synapse_id Synapse identifier to unfreeze
 * @return 0 on success, error code on failure
 *
 * NOTE: Unfreezing requires elevated security clearance
 */
int plasticity_guard_unfreeze_synapse(
    plasticity_guard_t guard,
    uint64_t synapse_id
);

/**
 * WHAT: Check if a synapse is frozen
 * WHY: Query frozen status before attempting update
 * HOW: Lookup in frozen synapse hashmap
 *
 * @param guard Plasticity guard
 * @param synapse_id Synapse identifier to check
 * @return true if frozen, false otherwise
 */
bool plasticity_guard_is_frozen(
    plasticity_guard_t guard,
    uint64_t synapse_id
);

/**
 * WHAT: Freeze an entire pathway (all synapses in pathway)
 * WHY: Protect safety-critical neural pathways
 * HOW: Add pathway ID to frozen pathway set
 *
 * @param guard Plasticity guard
 * @param pathway_id Pathway identifier to freeze
 * @return 0 on success, error code on failure
 */
int plasticity_guard_freeze_pathway(
    plasticity_guard_t guard,
    uint32_t pathway_id
);

/**
 * WHAT: Unfreeze a pathway
 * WHY: Allow modification of previously frozen pathway
 * HOW: Remove pathway ID from frozen set
 *
 * @param guard Plasticity guard
 * @param pathway_id Pathway identifier to unfreeze
 * @return 0 on success, error code on failure
 */
int plasticity_guard_unfreeze_pathway(
    plasticity_guard_t guard,
    uint32_t pathway_id
);

/**
 * WHAT: Bulk freeze multiple synapses
 * WHY: Efficiently freeze many synapses at once
 * HOW: Add all synapse IDs to frozen hashmap
 *
 * @param guard Plasticity guard
 * @param synapse_ids Array of synapse identifiers
 * @param count Number of synapses to freeze
 * @return Number of synapses successfully frozen
 */
uint32_t plasticity_guard_freeze_bulk(
    plasticity_guard_t guard,
    const uint64_t* synapse_ids,
    uint32_t count
);

/* ============================================================================
 * WEIGHT UPDATE VALIDATION
 * ============================================================================ */

/**
 * WHAT: Check if a weight update would violate constraints
 * WHY: Pre-check before applying update (read-only)
 * HOW: Evaluate all constraints without modifying state
 *
 * @param guard Plasticity guard
 * @param synapse_id Synapse to update
 * @param old_weight Current weight value
 * @param new_weight Proposed new weight value
 * @return true if update would violate constraints
 *
 * NOTE: Does not update rate limiting counters
 */
bool plasticity_guard_would_violate(
    plasticity_guard_t guard,
    uint64_t synapse_id,
    float old_weight,
    float new_weight
);

/**
 * WHAT: Check weight update with detailed result
 * WHY: Get information about which constraints would be violated
 * HOW: Evaluate all constraints and populate result struct
 *
 * @param guard Plasticity guard
 * @param synapse_id Synapse to update
 * @param old_weight Current weight value
 * @param new_weight Proposed new weight value
 * @param result Output: detailed check result
 * @return 0 on success (check completed), error code on failure
 *
 * NOTE: Does not update rate limiting counters
 */
int plasticity_guard_check_update(
    plasticity_guard_t guard,
    uint64_t synapse_id,
    float old_weight,
    float new_weight,
    plasticity_check_result_t* result
);

/**
 * WHAT: Apply a weight update through the guard
 * WHY: All weight updates MUST pass through this function
 * HOW: Validate, clamp if needed, update rate limiting, return result
 *
 * @param guard Plasticity guard
 * @param synapse_id Synapse to update
 * @param weight_ptr Pointer to weight value (modified in place if allowed)
 * @return 0 if update allowed (weight modified), >0 if blocked (violation flags)
 *
 * IMPORTANT: This function updates rate limiting counters
 *
 * BEHAVIOR:
 * - If update is allowed, *weight_ptr is updated
 * - If update is blocked, *weight_ptr is unchanged
 * - If update would violate magnitude but can be clamped, *weight_ptr is clamped
 */
int plasticity_guard_apply_update(
    plasticity_guard_t guard,
    uint64_t synapse_id,
    float* weight_ptr
);

/**
 * WHAT: Apply weight update with learning rate validation
 * WHY: Validate both weight change and learning rate
 * HOW: Check learning rate bounds before applying update
 *
 * @param guard Plasticity guard
 * @param synapse_id Synapse to update
 * @param old_weight Current weight value
 * @param weight_ptr Pointer to new weight (modified in place)
 * @param learning_rate Learning rate used for update
 * @return 0 if update allowed, >0 if blocked (violation flags)
 */
int plasticity_guard_apply_update_with_lr(
    plasticity_guard_t guard,
    uint64_t synapse_id,
    float old_weight,
    float* weight_ptr,
    float learning_rate
);

/**
 * WHAT: Apply batch of weight updates
 * WHY: Efficiently validate and apply multiple updates
 * HOW: Validate all updates, apply those that pass
 *
 * @param guard Plasticity guard
 * @param synapse_ids Array of synapse identifiers
 * @param old_weights Array of current weights
 * @param new_weights Array of new weights (modified in place)
 * @param count Number of updates
 * @param violations_out Output: array of violation flags (can be NULL)
 * @return Number of updates successfully applied
 */
uint32_t plasticity_guard_apply_batch(
    plasticity_guard_t guard,
    const uint64_t* synapse_ids,
    const float* old_weights,
    float* new_weights,
    uint32_t count,
    plasticity_violation_t* violations_out
);

/* ============================================================================
 * REWARD PATHWAY PROTECTION
 * ============================================================================ */

/**
 * WHAT: Register a synapse as part of reward pathway
 * WHY: Track reward pathway synapses for protection
 * HOW: Add to reward pathway registry
 *
 * @param guard Plasticity guard
 * @param synapse_id Synapse identifier
 * @return 0 on success, error code on failure
 */
int plasticity_guard_register_reward_synapse(
    plasticity_guard_t guard,
    uint64_t synapse_id
);

/**
 * WHAT: Register a synapse as self-reward related
 * WHY: Prevent system from modifying its own reward signals
 * HOW: Add to self-reward registry
 *
 * @param guard Plasticity guard
 * @param synapse_id Synapse identifier
 * @return 0 on success, error code on failure
 */
int plasticity_guard_register_self_reward_synapse(
    plasticity_guard_t guard,
    uint64_t synapse_id
);

/* ============================================================================
 * HOMEOSTATIC REGULATION
 * ============================================================================ */

/**
 * WHAT: Update homeostatic state with current network activity
 * WHY: Track activity levels for homeostatic regulation
 * HOW: Exponential moving average of activity
 *
 * @param guard Plasticity guard
 * @param current_activity Current network activity level [0, 1]
 * @return 0 on success, error code on failure
 */
int plasticity_guard_update_homeostatic_state(
    plasticity_guard_t guard,
    float current_activity
);

/**
 * WHAT: Get recommended weight scaling for homeostatic regulation
 * WHY: Suggest scaling to maintain target activity
 * HOW: Based on deviation from target activity
 *
 * @param guard Plasticity guard
 * @param scale_out Output: recommended weight scaling factor
 * @return 0 on success, error code on failure
 */
int plasticity_guard_get_homeostatic_scale(
    plasticity_guard_t guard,
    float* scale_out
);

/* ============================================================================
 * MONITORING AND STATISTICS
 * ============================================================================ */

/**
 * WHAT: Get plasticity guard statistics
 * WHY: Monitor guard effectiveness
 * HOW: Copy accumulated statistics
 *
 * @param guard Plasticity guard
 * @param stats Output: statistics structure
 * @return 0 on success, error code on failure
 */
int plasticity_guard_get_stats(
    plasticity_guard_t guard,
    plasticity_guard_stats_t* stats
);

/**
 * WHAT: Reset plasticity guard statistics
 * WHY: Clear statistics for fresh measurement
 * HOW: Zero all counters
 *
 * @param guard Plasticity guard
 * @return 0 on success, error code on failure
 */
int plasticity_guard_reset_stats(plasticity_guard_t guard);

/**
 * WHAT: Get current rate limiting state
 * WHY: Monitor rate limiting status
 * HOW: Query sliding window state
 *
 * @param guard Plasticity guard
 * @param current_rate Output: current updates per second
 * @param remaining Output: remaining updates allowed in window
 * @return 0 on success, error code on failure
 */
int plasticity_guard_get_rate_state(
    plasticity_guard_t guard,
    float* current_rate,
    uint32_t* remaining
);

/**
 * WHAT: Get cumulative weight drift
 * WHY: Monitor total weight changes from initialization
 * HOW: Return tracked drift value
 *
 * @param guard Plasticity guard
 * @param drift Output: cumulative drift value
 * @return 0 on success, error code on failure
 */
int plasticity_guard_get_drift(
    plasticity_guard_t guard,
    float* drift
);

/* ============================================================================
 * INTEGRATION
 * ============================================================================ */

/**
 * WHAT: Connect plasticity guard to bio-async router
 * WHY: Enable event-driven security monitoring
 * HOW: Register as bio-async module
 *
 * @param guard Plasticity guard
 * @return 0 on success, error code on failure
 */
int plasticity_guard_connect_bio_async(plasticity_guard_t guard);

/**
 * WHAT: Process incoming bio-async messages
 * WHY: Handle commands and queries from other modules
 * HOW: Process inbox messages
 *
 * @param guard Plasticity guard
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t plasticity_guard_process_inbox(
    plasticity_guard_t guard,
    uint32_t max_messages
);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

/**
 * WHAT: Get string name for violation type
 * WHY: Human-readable violation identification
 * HOW: Lookup table
 *
 * @param violation Violation type (single flag)
 * @return String name (never NULL)
 */
const char* plasticity_violation_name(plasticity_violation_t violation);

/**
 * WHAT: Format violation flags as string
 * WHY: Human-readable multi-violation description
 * HOW: Concatenate flag names
 *
 * @param violations Violation bitmask
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of characters written (excluding null)
 */
int plasticity_violations_to_string(
    plasticity_violation_t violations,
    char* buffer,
    size_t buffer_size
);

/**
 * WHAT: Print plasticity guard summary to stdout
 * WHY: Debug and diagnostic output
 * HOW: Format and print guard state
 *
 * @param guard Plasticity guard (NULL safe)
 */
void plasticity_guard_print_summary(plasticity_guard_t guard);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LGSS_PLASTICITY_CONSTRAINTS_H */
