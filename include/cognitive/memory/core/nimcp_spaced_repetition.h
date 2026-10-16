//=============================================================================
// nimcp_spaced_repetition.h - Spaced Repetition System for Prime Resonant Memory
//=============================================================================
/**
 * @file nimcp_spaced_repetition.h
 * @brief Optimized learning schedules based on forgetting curves
 *
 * WHAT: Implements spaced repetition scheduling for optimal memory retention
 * WHY:  Memories need strategic review to transition to long-term storage
 *       efficiently - reviews timed at optimal intervals maximize retention
 *       while minimizing total review effort
 * HOW:  Combines FSRS (Free Spaced Repetition Scheduler) algorithm with
 *       Prime Resonant memory integration for biologically-inspired learning
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Ebbinghaus Forgetting Curve:
 *   +-----------------------------------------------------------------------+
 *   |  Memory retention follows exponential decay:                          |
 *   |  R(t) = e^(-t/S)                                                      |
 *   |                                                                       |
 *   |  Where:                                                               |
 *   |  - R(t) = retrievability (probability of recall at time t)           |
 *   |  - t = time since last review                                         |
 *   |  - S = stability (time for R to fall to ~37%)                        |
 *   |                                                                       |
 *   |  Biological basis:                                                    |
 *   |  - Synaptic strength decay without reinforcement                      |
 *   |  - Protein degradation at synapses                                    |
 *   |  - Competition from new memories                                      |
 *   +-----------------------------------------------------------------------+
 *
 *   Retrieval Practice Effect:
 *   +-----------------------------------------------------------------------+
 *   |  Testing strengthens memory more than restudying (Roediger, 2006)     |
 *   |                                                                       |
 *   |  Neural mechanisms:                                                   |
 *   |  - Retrieval triggers reconsolidation window                          |
 *   |  - Pattern completion in CA3 reinforces pathways                      |
 *   |  - Successful retrieval increases synaptic tags                       |
 *   |                                                                       |
 *   |  Implementation:                                                       |
 *   |  - "Easy" retrieval = modest stability increase                       |
 *   |  - "Hard" retrieval = larger stability increase (desirable difficulty)|
 *   |  - Failed retrieval = stability reset but NOT to zero                 |
 *   +-----------------------------------------------------------------------+
 *
 *   Spacing Effect:
 *   +-----------------------------------------------------------------------+
 *   |  Distributed practice > massed practice (Cepeda et al., 2006)        |
 *   |                                                                       |
 *   |  Optimal review occurs when retrievability has declined:              |
 *   |  - Too early: Effortless recall, minimal strengthening                |
 *   |  - Too late: High forgetting probability, frustrating                 |
 *   |  - Optimal: ~90% retrievability (configurable target)                 |
 *   |                                                                       |
 *   |  Interval formula: I = S * ln(target_retention) / ln(0.9)            |
 *   |  (Solves for when R(t) = target_retention)                            |
 *   +-----------------------------------------------------------------------+
 *
 *   FSRS Algorithm (Wozniak-inspired):
 *   +-----------------------------------------------------------------------+
 *   |  Stability update after successful review:                            |
 *   |  S(n+1) = S(n) * (1 + a * D^(-b) * S(n)^(-c) * e^(d*(R-1)))          |
 *   |                                                                       |
 *   |  Parameters:                                                          |
 *   |  - a: Base growth rate (~0.5-2.0)                                     |
 *   |  - b: Difficulty sensitivity (~0.1-0.5)                               |
 *   |  - c: Stability decay factor (~0.05-0.2)                              |
 *   |  - d: Retrievability bonus factor (~1.0-3.0)                          |
 *   |  - D: Item difficulty (1.0-10.0)                                      |
 *   |  - R: Retrievability at review time                                   |
 *   |                                                                       |
 *   |  After lapse (forgetting):                                            |
 *   |  S(n+1) = initial_stability * D^(-lapse_factor) * lapses^(-decay)    |
 *   +-----------------------------------------------------------------------+
 *
 * ARCHITECTURE:
 *
 *   Spaced Repetition Pipeline:
 *   +-----------------------------------------------------------------------+
 *   |                                                                       |
 *   |  [PR Memory Node] --> [Spaced Item] --> [Review Queue]               |
 *   |        |                    |                  |                      |
 *   |        v                    v                  v                      |
 *   |  Content signature    Memory strength    Priority heap                |
 *   |  Quaternion state     Interval/lapses    Due items                    |
 *   |  Z-ladder tier        History            Workload estimation          |
 *   |                                                                       |
 *   |  Review Flow:                                                          |
 *   |  1. Get next due item from queue                                      |
 *   |  2. Present for retrieval (via entanglement cues)                     |
 *   |  3. Record response (Again/Hard/Good/Easy)                            |
 *   |  4. Update stability, compute new interval                            |
 *   |  5. Reinsert into queue at new due time                               |
 *   |                                                                       |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Add item: O(log N) heap insertion
 * - Get next: O(log N) heap extraction
 * - Update item: O(log N) heap rebalance
 * - Predict retention: O(1) exponential calculation
 * - Batch scheduling: O(N log N) for N items
 *
 * MEMORY:
 * - spaced_item_t: ~128 bytes per item
 * - spaced_repetition_t: ~256 bytes + items + queue
 * - Response history: ~4 bytes per review
 *
 * INTEGRATION:
 * - PR Memory Node: Memory content and signatures
 * - Entanglement Graph: Cue generation for retrieval
 * - Z-Ladder: Tier promotion based on stability
 * - Resonance: Similarity-based review grouping
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_SPACED_REPETITION_H
#define NIMCP_SPACED_REPETITION_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Dependencies
#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "cognitive/memory/core/nimcp_prime_signature.h"
#include "cognitive/memory/core/nimcp_entanglement.h"

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Default initial stability in days (first exposure) */
#define SR_DEFAULT_INITIAL_STABILITY        0.5f

/** Default stability growth parameter 'a' */
#define SR_DEFAULT_STABILITY_GROWTH_A       1.0f

/** Default difficulty sensitivity 'b' */
#define SR_DEFAULT_DIFFICULTY_SENSITIVITY   0.2f

/** Default stability decay factor 'c' */
#define SR_DEFAULT_STABILITY_DECAY          0.1f

/** Default retrievability bonus factor 'd' */
#define SR_DEFAULT_RETRIEVABILITY_BONUS     2.0f

/** Default target retention (90%) */
#define SR_DEFAULT_TARGET_RETENTION         0.9f

/** Maximum interval in days */
#define SR_DEFAULT_MAX_INTERVAL_DAYS        365.0f

/** Minimum interval in days */
#define SR_DEFAULT_MIN_INTERVAL_DAYS        0.0007f  // ~1 minute

/** Default item difficulty */
#define SR_DEFAULT_DIFFICULTY               5.0f

/** Minimum difficulty value */
#define SR_MIN_DIFFICULTY                   1.0f

/** Maximum difficulty value */
#define SR_MAX_DIFFICULTY                   10.0f

/** Maximum response history entries per item */
#define SR_MAX_HISTORY_LENGTH               256

/** Default number of items in review queue */
#define SR_DEFAULT_QUEUE_CAPACITY           4096

/** Epsilon for floating point comparisons */
#define SR_EPSILON                          1e-6f

/** Natural log of 0.9 (for interval calculation) */
#define SR_LN_09                            (-0.10536051565f)

/** Lapse stability decay factor */
#define SR_LAPSE_DECAY_FACTOR               0.5f

/** Maximum lapses before item is flagged as leech */
#define SR_MAX_LAPSES_WARN                  8

/** Easy bonus multiplier for interval */
#define SR_EASY_BONUS                       1.3f

/** Hard penalty multiplier for interval */
#define SR_HARD_PENALTY                     0.8f

/** Priority boost for overdue items per day overdue */
#define SR_OVERDUE_PRIORITY_BOOST           0.1f

/** Fuzz factor range for interval randomization (prevent clustering) */
#define SR_INTERVAL_FUZZ_FACTOR             0.05f

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Review response quality ratings
 *
 * Based on Anki/SuperMemo quality ratings, simplified to 4 options:
 * - AGAIN: Complete failure, reset progress
 * - HARD: Recalled with significant difficulty
 * - GOOD: Normal recall as expected
 * - EASY: Effortless recall
 */
typedef enum {
    SR_RESPONSE_AGAIN = 0,   /**< Forgot completely, reset stability */
    SR_RESPONSE_HARD  = 1,   /**< Difficult recall, short interval increase */
    SR_RESPONSE_GOOD  = 2,   /**< Normal recall, standard interval increase */
    SR_RESPONSE_EASY  = 3    /**< Easy recall, larger interval increase */
} sr_review_response_t;

/**
 * @brief Item state in the SRS system
 */
typedef enum {
    SR_STATE_NEW       = 0,  /**< Never reviewed */
    SR_STATE_LEARNING  = 1,  /**< In initial learning phase */
    SR_STATE_REVIEW    = 2,  /**< In review phase (graduated from learning) */
    SR_STATE_RELEARNING= 3,  /**< Lapsed, relearning */
    SR_STATE_SUSPENDED = 4,  /**< Temporarily suspended from reviews */
    SR_STATE_BURIED    = 5   /**< Buried until next session */
} sr_item_state_t;

/**
 * @brief Memory strength model parameters
 *
 * Tracks the underlying memory strength using the FSRS model:
 * - Stability: How long until retrievability drops to ~37%
 * - Difficulty: Item-specific learning difficulty
 * - Retrievability: Current probability of successful recall
 */
typedef struct {
    float initial_stability;    /**< Initial memory stability (days) */
    float current_stability;    /**< Current stability after reviews */
    float stability_growth;     /**< Per-item growth rate modifier */
    float difficulty;           /**< Item difficulty (1.0-10.0) */
    float retrievability;       /**< Current probability of recall (0-1) */
} sr_memory_strength_t;

/**
 * @brief Single review history entry
 */
typedef struct {
    sr_review_response_t response;  /**< User's response quality */
    float time_taken_ms;            /**< Time to respond in milliseconds */
    float retrievability;           /**< Predicted retrievability at review time */
    float stability_before;         /**< Stability before this review */
    float stability_after;          /**< Stability after this review */
    float interval_days;            /**< Interval assigned after this review */
    uint64_t timestamp_ms;          /**< Unix timestamp of review (milliseconds) */
} sr_review_record_t;

/**
 * @brief Spaced repetition item tracking structure
 *
 * Wraps a PR memory node with SRS scheduling metadata.
 * Each item maintains its own stability, interval, and review history.
 */
typedef struct {
    //-------------------------------------------------------------------------
    // Identity
    //-------------------------------------------------------------------------
    uint64_t item_id;                   /**< Unique item identifier */
    pr_memory_node_t* memory;           /**< Associated PR memory node (not owned) */
    prime_signature_t content_signature;/**< Content signature for similarity grouping */

    //-------------------------------------------------------------------------
    // FSRS State
    //-------------------------------------------------------------------------
    sr_memory_strength_t strength;      /**< Memory strength model */
    sr_item_state_t state;              /**< Current item state */
    float interval_days;                /**< Current interval in days */
    size_t repetition_count;            /**< Total successful reviews */
    size_t lapses;                      /**< Times forgotten (Again responses) */

    //-------------------------------------------------------------------------
    // Timing
    //-------------------------------------------------------------------------
    float last_review_time;             /**< Days since epoch of last review */
    float next_review_time;             /**< Days since epoch of next review */
    float due_time;                     /**< When item becomes due (days) */
    uint64_t created_timestamp_ms;      /**< Creation timestamp (milliseconds) */

    //-------------------------------------------------------------------------
    // Performance History
    //-------------------------------------------------------------------------
    sr_review_record_t* response_history; /**< Circular buffer of reviews */
    size_t history_len;                 /**< Number of entries in history */
    size_t history_capacity;            /**< Capacity of history buffer */
    size_t history_start;               /**< Start index for circular buffer */
    float avg_response_time_ms;         /**< Moving average response time */
    float avg_retrievability;           /**< Moving average retrievability */

    //-------------------------------------------------------------------------
    // Flags
    //-------------------------------------------------------------------------
    bool is_leech;                      /**< Flagged as persistently difficult */
    bool is_suspended;                  /**< Temporarily suspended */
    uint32_t flags;                     /**< Additional flags */

} sr_spaced_item_t;

/**
 * @brief Review queue entry with priority information
 *
 * Used in the priority queue to determine review order.
 * Priority considers overdue status, predicted retention, and item state.
 */
typedef struct {
    sr_spaced_item_t* item;             /**< Pointer to the spaced item */
    float priority;                     /**< Review urgency (higher = more urgent) */
    float overdue_days;                 /**< How many days past due (can be negative) */
    float predicted_retention;          /**< Predicted recall probability */
    uint64_t queue_timestamp_ms;        /**< When added to queue */
} sr_review_queue_entry_t;

/**
 * @brief FSRS algorithm parameters
 *
 * Configurable parameters for the stability update formula:
 * S(n+1) = S(n) * (1 + a * D^(-b) * S(n)^(-c) * e^(d*(R-1)))
 */
typedef struct {
    float param_a;                      /**< Base growth rate */
    float param_b;                      /**< Difficulty sensitivity */
    float param_c;                      /**< Stability decay factor */
    float param_d;                      /**< Retrievability bonus factor */
    float initial_stability;            /**< Starting stability for new items */
    float lapse_stability_factor;       /**< Stability multiplier after lapse */
    float lapse_decay_exponent;         /**< Decay per consecutive lapse */
} sr_fsrs_params_t;

/**
 * @brief Configuration for spaced repetition system
 */
typedef struct {
    //-------------------------------------------------------------------------
    // Algorithm Parameters
    //-------------------------------------------------------------------------
    sr_fsrs_params_t fsrs_params;       /**< FSRS algorithm parameters */
    float target_retention;             /**< Target retention rate (e.g., 0.9) */
    float max_interval_days;            /**< Maximum interval cap */
    float min_interval_days;            /**< Minimum interval floor */

    //-------------------------------------------------------------------------
    // Learning Phase
    //-------------------------------------------------------------------------
    float learning_steps_minutes[4];    /**< Learning step intervals in minutes */
    size_t num_learning_steps;          /**< Number of learning steps (max 4) */
    float graduating_interval_days;     /**< Interval after graduating learning */
    float easy_interval_days;           /**< Interval for Easy during learning */

    //-------------------------------------------------------------------------
    // Relearning Phase
    //-------------------------------------------------------------------------
    float relearning_steps_minutes[4];  /**< Relearning step intervals */
    size_t num_relearning_steps;        /**< Number of relearning steps */
    float min_review_interval_days;     /**< Minimum interval after relearning */

    //-------------------------------------------------------------------------
    // Modifiers
    //-------------------------------------------------------------------------
    float easy_bonus;                   /**< Multiplier for Easy responses */
    float hard_penalty;                 /**< Multiplier for Hard responses */
    float interval_modifier;            /**< Global interval multiplier */
    float new_interval_after_lapse;     /**< Multiplier for interval after lapse */

    //-------------------------------------------------------------------------
    // Behavior
    //-------------------------------------------------------------------------
    bool fuzz_intervals;                /**< Add randomness to prevent clustering */
    bool bury_siblings;                 /**< Bury related items after review */
    size_t max_reviews_per_day;         /**< Maximum reviews per day (0 = unlimited) */
    size_t max_new_per_day;             /**< Maximum new items per day */
    float leech_threshold;              /**< Lapses before marking as leech */

    //-------------------------------------------------------------------------
    // Integration
    //-------------------------------------------------------------------------
    bool sync_with_z_ladder;            /**< Sync stability with Z-tier */
    bool use_entanglement_cues;         /**< Generate cues from entanglement */
    size_t history_capacity;            /**< Max review history entries per item */

} sr_config_t;

/**
 * @brief Statistics for the spaced repetition system
 */
typedef struct {
    //-------------------------------------------------------------------------
    // Item Counts
    //-------------------------------------------------------------------------
    size_t total_items;                 /**< Total items in system */
    size_t new_items;                   /**< Items never reviewed */
    size_t learning_items;              /**< Items in learning phase */
    size_t review_items;                /**< Items in review phase */
    size_t relearning_items;            /**< Items being relearned */
    size_t suspended_items;             /**< Suspended items */
    size_t leech_items;                 /**< Leech items */

    //-------------------------------------------------------------------------
    // Review Statistics
    //-------------------------------------------------------------------------
    size_t total_reviews;               /**< Total reviews performed */
    size_t reviews_today;               /**< Reviews completed today */
    float avg_retention;                /**< Average retention rate */
    float avg_interval_days;            /**< Average current interval */
    float avg_difficulty;               /**< Average item difficulty */

    //-------------------------------------------------------------------------
    // Response Distribution
    //-------------------------------------------------------------------------
    size_t again_count;                 /**< Total Again responses */
    size_t hard_count;                  /**< Total Hard responses */
    size_t good_count;                  /**< Total Good responses */
    size_t easy_count;                  /**< Total Easy responses */

    //-------------------------------------------------------------------------
    // Workload Metrics
    //-------------------------------------------------------------------------
    size_t due_now;                     /**< Items due for review now */
    size_t due_today;                   /**< Items due today */
    size_t due_tomorrow;                /**< Items due tomorrow */
    float reviews_per_day_avg;          /**< Average reviews per day */
    float estimated_time_minutes;       /**< Estimated review time for due items */

    //-------------------------------------------------------------------------
    // Performance
    //-------------------------------------------------------------------------
    float avg_response_time_ms;         /**< Average response time */
    uint64_t total_study_time_ms;       /**< Total time spent studying */
    float retention_stability_corr;     /**< Correlation: stability vs retention */

} sr_stats_t;

/**
 * @brief Workload forecast entry
 */
typedef struct {
    float day_offset;                   /**< Days from now */
    size_t due_count;                   /**< Items due on this day */
    size_t cumulative_due;              /**< Cumulative items due */
    float estimated_minutes;            /**< Estimated review time */
} sr_workload_forecast_t;

/**
 * @brief Scheduling decision result
 */
typedef struct {
    float interval_days;                /**< Computed interval */
    float next_due_time;                /**< Next due time (days since epoch) */
    float new_stability;                /**< Updated stability value */
    float predicted_retention_at_due;   /**< Predicted retention at due time */
    sr_item_state_t new_state;          /**< New item state */
} sr_schedule_result_t;

/**
 * @brief Opaque spaced repetition system handle
 */
typedef struct sr_system_struct* sr_system_t;

/**
 * @brief Error codes for spaced repetition operations
 */
typedef enum {
    SR_SUCCESS                  = 0,    /**< Operation succeeded */
    SR_ERROR_NULL_POINTER       = -1,   /**< NULL pointer argument */
    SR_ERROR_INVALID_CONFIG     = -2,   /**< Invalid configuration */
    SR_ERROR_NO_MEMORY          = -3,   /**< Memory allocation failed */
    SR_ERROR_ITEM_NOT_FOUND     = -4,   /**< Item not in system */
    SR_ERROR_ITEM_EXISTS        = -5,   /**< Item already exists */
    SR_ERROR_INVALID_RESPONSE   = -6,   /**< Invalid response value */
    SR_ERROR_QUEUE_EMPTY        = -7,   /**< Review queue is empty */
    SR_ERROR_QUEUE_FULL         = -8,   /**< Review queue is full */
    SR_ERROR_SUSPENDED          = -9,   /**< Item is suspended */
    SR_ERROR_INVALID_TIME       = -10,  /**< Invalid timestamp */
    SR_ERROR_SERIALIZATION      = -11,  /**< Serialization failed */
    SR_ERROR_DESERIALIZATION    = -12   /**< Deserialization failed */
} sr_error_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default spaced repetition configuration
 *
 * WHAT: Returns sensible default configuration based on FSRS research
 * WHY:  Provides starting point optimized for typical learning scenarios
 * HOW:  Pre-configured values from spaced repetition literature
 *
 * @return Default configuration
 *
 * Default values:
 * - target_retention: 0.9 (90%)
 * - max_interval: 365 days
 * - learning_steps: [1, 10, 60, 1440] minutes
 * - fsrs_params: research-based defaults
 *
 * Performance: ~5ns
 *
 * Example:
 *   sr_config_t config = sr_config_default();
 *   config.target_retention = 0.85f;  // Lower target for more reviews
 *   sr_system_t sys = sr_system_create(&config);
 */
NIMCP_EXPORT sr_config_t sr_config_default(void);

/**
 * @brief Get FSRS default parameters
 *
 * @return Default FSRS algorithm parameters
 */
NIMCP_EXPORT sr_fsrs_params_t sr_fsrs_params_default(void);

/**
 * @brief Validate spaced repetition configuration
 *
 * WHAT: Checks configuration for valid values
 * WHY:  Prevent invalid configs causing runtime errors
 * HOW:  Range checks on all parameters
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 *
 * Validation rules:
 * - target_retention in (0, 1)
 * - intervals positive
 * - FSRS params in valid ranges
 *
 * Performance: ~20ns
 */
NIMCP_EXPORT bool sr_config_validate(const sr_config_t* config);

/**
 * @brief Get optimized configuration for high retention
 *
 * @return Configuration tuned for 95% retention (more reviews)
 */
NIMCP_EXPORT sr_config_t sr_config_high_retention(void);

/**
 * @brief Get optimized configuration for efficiency
 *
 * @return Configuration tuned for fewer reviews (lower retention)
 */
NIMCP_EXPORT sr_config_t sr_config_efficient(void);

//=============================================================================
// System Lifecycle Functions
//=============================================================================

/**
 * @brief Create spaced repetition system
 *
 * WHAT: Allocates and initializes SRS with configuration
 * WHY:  Entry point for spaced repetition functionality
 * HOW:  Creates system, initializes queue, sets up tracking
 *
 * @param config Configuration (NULL for defaults)
 * @return System handle or NULL on failure
 *
 * Performance: O(queue_capacity) for heap initialization
 * Memory: ~256 bytes + queue capacity * 32 bytes
 *
 * Thread safety: System is NOT thread-safe by default
 *
 * Example:
 *   sr_config_t config = sr_config_default();
 *   config.max_new_per_day = 20;
 *   sr_system_t sys = sr_system_create(&config);
 *   if (!sys) {
 *       fprintf(stderr, "Failed: %s\n", sr_get_last_error());
 *   }
 */
NIMCP_EXPORT sr_system_t sr_system_create(const sr_config_t* config);

/**
 * @brief Create system with PR integration
 *
 * WHAT: Creates SRS integrated with entanglement graph and node manager
 * WHY:  Enable automatic cue generation and tier synchronization
 * HOW:  Stores references to PR components for integration
 *
 * @param config Configuration
 * @param entanglement Entanglement graph (can be NULL)
 * @param node_manager PR node manager (can be NULL)
 * @return System handle or NULL on failure
 */
NIMCP_EXPORT sr_system_t sr_system_create_integrated(
    const sr_config_t* config,
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager
);

/**
 * @brief Destroy spaced repetition system
 *
 * WHAT: Frees all system resources
 * WHY:  Resource cleanup
 * HOW:  Frees items, queue, history buffers
 *
 * @param system System to destroy (NULL safe)
 *
 * Note: Does NOT destroy associated PR memory nodes
 *
 * Performance: O(num_items)
 */
NIMCP_EXPORT void sr_system_destroy(sr_system_t system);

/**
 * @brief Clear all items from system
 *
 * @param system System to clear
 * @return SR_SUCCESS or error code
 */
NIMCP_EXPORT sr_error_t sr_system_clear(sr_system_t system);

/**
 * @brief Get system configuration
 *
 * @param system System to query
 * @return Pointer to configuration (do not modify)
 */
NIMCP_EXPORT const sr_config_t* sr_system_get_config(sr_system_t system);

/**
 * @brief Update system configuration
 *
 * @param system System to update
 * @param config New configuration
 * @return SR_SUCCESS or error code
 *
 * Note: Some changes take effect immediately, others at next review
 */
NIMCP_EXPORT sr_error_t sr_system_set_config(
    sr_system_t system,
    const sr_config_t* config
);

//=============================================================================
// Item Management Functions
//=============================================================================

/**
 * @brief Add memory to spaced repetition system
 *
 * WHAT: Creates spaced item for a PR memory node
 * WHY:  Begin tracking memory for scheduled reviews
 * HOW:  Wraps node in spaced_item_t, adds to system
 *
 * @param system SRS system
 * @param memory PR memory node (system does NOT take ownership)
 * @return Item ID on success, 0 on failure
 *
 * Performance: O(log N) for queue insertion
 *
 * Example:
 *   pr_memory_node_t* node = pr_memory_node_create(mgr, data, size, NULL);
 *   uint64_t item_id = sr_system_add_item(sys, node);
 *   if (item_id == 0) {
 *       fprintf(stderr, "Failed to add item\n");
 *   }
 */
NIMCP_EXPORT uint64_t sr_system_add_item(
    sr_system_t system,
    pr_memory_node_t* memory
);

/**
 * @brief Add item with initial difficulty
 *
 * @param system SRS system
 * @param memory PR memory node
 * @param initial_difficulty Difficulty estimate (1.0-10.0)
 * @return Item ID or 0 on failure
 */
NIMCP_EXPORT uint64_t sr_system_add_item_with_difficulty(
    sr_system_t system,
    pr_memory_node_t* memory,
    float initial_difficulty
);

/**
 * @brief Remove item from system
 *
 * @param system SRS system
 * @param item_id Item to remove
 * @return SR_SUCCESS or error code
 *
 * Note: Does NOT destroy the associated memory node
 */
NIMCP_EXPORT sr_error_t sr_system_remove_item(
    sr_system_t system,
    uint64_t item_id
);

/**
 * @brief Get item by ID
 *
 * @param system SRS system
 * @param item_id Item identifier
 * @return Item pointer or NULL if not found
 */
NIMCP_EXPORT sr_spaced_item_t* sr_system_get_item(
    sr_system_t system,
    uint64_t item_id
);

/**
 * @brief Get item by memory node
 *
 * @param system SRS system
 * @param memory PR memory node
 * @return Item pointer or NULL if not found
 */
NIMCP_EXPORT sr_spaced_item_t* sr_system_get_item_by_memory(
    sr_system_t system,
    const pr_memory_node_t* memory
);

/**
 * @brief Get number of items in system
 *
 * @param system SRS system
 * @return Total item count
 */
NIMCP_EXPORT size_t sr_system_get_item_count(sr_system_t system);

/**
 * @brief Suspend item from reviews
 *
 * @param system SRS system
 * @param item_id Item to suspend
 * @return SR_SUCCESS or error code
 */
NIMCP_EXPORT sr_error_t sr_system_suspend_item(
    sr_system_t system,
    uint64_t item_id
);

/**
 * @brief Unsuspend item (resume reviews)
 *
 * @param system SRS system
 * @param item_id Item to unsuspend
 * @return SR_SUCCESS or error code
 */
NIMCP_EXPORT sr_error_t sr_system_unsuspend_item(
    sr_system_t system,
    uint64_t item_id
);

/**
 * @brief Bury item until next session
 *
 * @param system SRS system
 * @param item_id Item to bury
 * @return SR_SUCCESS or error code
 */
NIMCP_EXPORT sr_error_t sr_system_bury_item(
    sr_system_t system,
    uint64_t item_id
);

/**
 * @brief Unbury all buried items
 *
 * @param system SRS system
 * @return Number of items unburied
 */
NIMCP_EXPORT size_t sr_system_unbury_all(sr_system_t system);

//=============================================================================
// Review Functions
//=============================================================================

/**
 * @brief Get next item to review
 *
 * WHAT: Returns highest-priority due item
 * WHY:  Core review loop operation
 * HOW:  Extracts from priority queue
 *
 * @param system SRS system
 * @return Item to review or NULL if none due
 *
 * Performance: O(log N)
 *
 * Example:
 *   sr_spaced_item_t* item;
 *   while ((item = sr_system_get_next(sys)) != NULL) {
 *       // Present item for review
 *       // ...
 *       sr_review_response_t response = get_user_response();
 *       sr_system_review(sys, item->item_id, response, response_time_ms);
 *   }
 */
NIMCP_EXPORT sr_spaced_item_t* sr_system_get_next(sr_system_t system);

/**
 * @brief Get next item to review (non-destructive peek)
 *
 * @param system SRS system
 * @return Item to review or NULL if none due (item stays in queue)
 */
NIMCP_EXPORT sr_spaced_item_t* sr_system_peek_next(sr_system_t system);

/**
 * @brief Record review with response
 *
 * WHAT: Process user's response to a review
 * WHY:  Update memory strength and schedule next review
 * HOW:  Apply FSRS algorithm, update stability, compute interval
 *
 * @param system SRS system
 * @param item_id Item that was reviewed
 * @param response User's response quality
 * @param response_time_ms Time taken to respond (0 if not tracked)
 * @return SR_SUCCESS or error code
 *
 * Performance: O(log N) for queue rebalance
 *
 * Side effects:
 * - Updates item stability and interval
 * - Records review in history
 * - Updates statistics
 * - May trigger leech detection
 *
 * Example:
 *   sr_error_t err = sr_system_review(sys, item_id, SR_RESPONSE_GOOD, 2500.0f);
 */
NIMCP_EXPORT sr_error_t sr_system_review(
    sr_system_t system,
    uint64_t item_id,
    sr_review_response_t response,
    float response_time_ms
);

/**
 * @brief Record review with custom timestamp
 *
 * Allows backdating reviews (e.g., for import or offline sync).
 *
 * @param system SRS system
 * @param item_id Item reviewed
 * @param response User's response
 * @param response_time_ms Response time
 * @param timestamp_ms Unix timestamp of review (milliseconds)
 * @return SR_SUCCESS or error code
 */
NIMCP_EXPORT sr_error_t sr_system_review_at_time(
    sr_system_t system,
    uint64_t item_id,
    sr_review_response_t response,
    float response_time_ms,
    uint64_t timestamp_ms
);

/**
 * @brief Undo last review for item
 *
 * @param system SRS system
 * @param item_id Item to undo
 * @return SR_SUCCESS or error code
 *
 * Note: Can only undo most recent review
 */
NIMCP_EXPORT sr_error_t sr_system_undo_review(
    sr_system_t system,
    uint64_t item_id
);

/**
 * @brief Get all due items
 *
 * WHAT: Returns all items currently due for review
 * WHY:  Batch processing, UI display
 * HOW:  Scans queue for items with due_time <= now
 *
 * @param system SRS system
 * @param items Output array (caller-allocated)
 * @param max_items Maximum items to return
 * @param count Output: actual count returned
 * @return SR_SUCCESS or error code
 *
 * Performance: O(N) scan
 */
NIMCP_EXPORT sr_error_t sr_system_get_due(
    sr_system_t system,
    sr_spaced_item_t** items,
    size_t max_items,
    size_t* count
);

/**
 * @brief Get count of due items
 *
 * @param system SRS system
 * @return Number of items currently due
 */
NIMCP_EXPORT size_t sr_system_get_due_count(sr_system_t system);

/**
 * @brief Get new items (never reviewed)
 *
 * @param system SRS system
 * @param items Output array
 * @param max_items Maximum items
 * @param count Output: actual count
 * @return SR_SUCCESS or error code
 */
NIMCP_EXPORT sr_error_t sr_system_get_new(
    sr_system_t system,
    sr_spaced_item_t** items,
    size_t max_items,
    size_t* count
);

//=============================================================================
// Scheduling Algorithm Functions
//=============================================================================

/**
 * @brief Compute next interval for response
 *
 * WHAT: Calculate interval using FSRS algorithm
 * WHY:  Preview scheduling without committing review
 * HOW:  Applies stability formula, interval computation
 *
 * @param system SRS system (for configuration)
 * @param item Item being scheduled
 * @param response Hypothetical response
 * @param result Output: scheduling decision
 * @return SR_SUCCESS or error code
 *
 * Performance: O(1)
 *
 * Example:
 *   sr_schedule_result_t result;
 *   sr_compute_interval(sys, item, SR_RESPONSE_GOOD, &result);
 *   printf("Good -> %.1f days\n", result.interval_days);
 */
NIMCP_EXPORT sr_error_t sr_compute_interval(
    sr_system_t system,
    const sr_spaced_item_t* item,
    sr_review_response_t response,
    sr_schedule_result_t* result
);

/**
 * @brief Preview all response outcomes
 *
 * @param system SRS system
 * @param item Item to preview
 * @param again_result Output for Again response
 * @param hard_result Output for Hard response
 * @param good_result Output for Good response
 * @param easy_result Output for Easy response
 * @return SR_SUCCESS or error code
 */
NIMCP_EXPORT sr_error_t sr_preview_responses(
    sr_system_t system,
    const sr_spaced_item_t* item,
    sr_schedule_result_t* again_result,
    sr_schedule_result_t* hard_result,
    sr_schedule_result_t* good_result,
    sr_schedule_result_t* easy_result
);

/**
 * @brief Predict retention at given time
 *
 * WHAT: Calculate expected retrievability at future time
 * WHY:  Understand decay, optimize review timing
 * HOW:  R(t) = e^(-t/S)
 *
 * @param item Item to predict for
 * @param days_from_now Time offset in days
 * @return Predicted retrievability [0, 1]
 *
 * Performance: O(1)
 *
 * Example:
 *   float retention = sr_predict_retention(item, 7.0f);
 *   printf("Predicted retention in 1 week: %.1f%%\n", retention * 100);
 */
NIMCP_EXPORT float sr_predict_retention(
    const sr_spaced_item_t* item,
    float days_from_now
);

/**
 * @brief Predict retention at specific timestamp
 *
 * @param item Item to predict for
 * @param timestamp_ms Unix timestamp (milliseconds)
 * @return Predicted retrievability [0, 1]
 */
NIMCP_EXPORT float sr_predict_retention_at(
    const sr_spaced_item_t* item,
    uint64_t timestamp_ms
);

/**
 * @brief Update retrievability for all items
 *
 * WHAT: Recalculate current retrievability based on elapsed time
 * WHY:  Keep retrievability values current
 * HOW:  Batch exponential decay calculation
 *
 * @param system SRS system
 * @return Number of items updated
 *
 * Performance: O(N)
 */
NIMCP_EXPORT size_t sr_system_apply_forgetting(sr_system_t system);

/**
 * @brief Compute review priority for item
 *
 * WHAT: Calculate urgency of reviewing item
 * WHY:  Queue ordering for optimal review order
 * HOW:  Combines overdue time, predicted retention, item state
 *
 * @param system SRS system
 * @param item Item to compute priority for
 * @return Priority value (higher = more urgent)
 *
 * Performance: O(1)
 */
NIMCP_EXPORT float sr_compute_priority(
    sr_system_t system,
    const sr_spaced_item_t* item
);

/**
 * @brief Reschedule item manually
 *
 * @param system SRS system
 * @param item_id Item to reschedule
 * @param interval_days New interval in days
 * @return SR_SUCCESS or error code
 */
NIMCP_EXPORT sr_error_t sr_system_reschedule(
    sr_system_t system,
    uint64_t item_id,
    float interval_days
);

/**
 * @brief Reset item to new state
 *
 * @param system SRS system
 * @param item_id Item to reset
 * @return SR_SUCCESS or error code
 */
NIMCP_EXPORT sr_error_t sr_system_reset_item(
    sr_system_t system,
    uint64_t item_id
);

//=============================================================================
// Schedule Optimization Functions
//=============================================================================

/**
 * @brief Optimize review schedule for time constraint
 *
 * WHAT: Reorder reviews to maximize retention within time budget
 * WHY:  Limited study time should be spent optimally
 * HOW:  Greedy algorithm prioritizing retention impact
 *
 * @param system SRS system
 * @param available_minutes Available study time
 * @param optimized_order Output array of item IDs in optimal order
 * @param max_items Maximum items to schedule
 * @param count Output: actual items scheduled
 * @return SR_SUCCESS or error code
 *
 * Performance: O(N log N)
 */
NIMCP_EXPORT sr_error_t sr_optimize_schedule(
    sr_system_t system,
    float available_minutes,
    uint64_t* optimized_order,
    size_t max_items,
    size_t* count
);

/**
 * @brief Balance new items with reviews
 *
 * WHAT: Determine optimal mix of new items and reviews
 * WHY:  Maintain sustainable workload
 * HOW:  Based on current load and retention targets
 *
 * @param system SRS system
 * @param target_daily_reviews Target daily review count
 * @param recommended_new Output: recommended new items to introduce
 * @return SR_SUCCESS or error code
 */
NIMCP_EXPORT sr_error_t sr_balance_workload(
    sr_system_t system,
    size_t target_daily_reviews,
    size_t* recommended_new
);

/**
 * @brief Estimate future workload
 *
 * WHAT: Forecast upcoming reviews
 * WHY:  Plan study schedule, avoid overload
 * HOW:  Simulate forgetting curve and review cascade
 *
 * @param system SRS system
 * @param days_ahead Number of days to forecast
 * @param forecast Output array of forecasts (size >= days_ahead)
 * @return SR_SUCCESS or error code
 *
 * Performance: O(N * days_ahead)
 *
 * Example:
 *   sr_workload_forecast_t forecast[30];
 *   sr_estimate_workload(sys, 30, forecast);
 *   for (int i = 0; i < 30; i++) {
 *       printf("Day %d: %zu items\n", i, forecast[i].due_count);
 *   }
 */
NIMCP_EXPORT sr_error_t sr_estimate_workload(
    sr_system_t system,
    size_t days_ahead,
    sr_workload_forecast_t* forecast
);

/**
 * @brief Get items that will become overdue
 *
 * @param system SRS system
 * @param within_days Time horizon in days
 * @param items Output array
 * @param max_items Maximum items
 * @param count Output: actual count
 * @return SR_SUCCESS or error code
 */
NIMCP_EXPORT sr_error_t sr_get_upcoming_due(
    sr_system_t system,
    float within_days,
    sr_spaced_item_t** items,
    size_t max_items,
    size_t* count
);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get comprehensive statistics
 *
 * WHAT: Returns operational metrics and distributions
 * WHY:  Monitor learning progress and system health
 * HOW:  Aggregates item states and review history
 *
 * @param system SRS system
 * @param stats Output statistics structure
 * @return SR_SUCCESS or error code
 *
 * Performance: O(N)
 */
NIMCP_EXPORT sr_error_t sr_system_get_stats(
    sr_system_t system,
    sr_stats_t* stats
);

/**
 * @brief Get retention history
 *
 * @param system SRS system
 * @param days_back Number of days of history
 * @param retention_values Output array of daily retention rates
 * @param count Output: actual days returned
 * @return SR_SUCCESS or error code
 */
NIMCP_EXPORT sr_error_t sr_get_retention_history(
    sr_system_t system,
    size_t days_back,
    float* retention_values,
    size_t* count
);

/**
 * @brief Get review count history
 *
 * @param system SRS system
 * @param days_back Number of days
 * @param review_counts Output array of daily review counts
 * @param count Output: actual days returned
 * @return SR_SUCCESS or error code
 */
NIMCP_EXPORT sr_error_t sr_get_review_history(
    sr_system_t system,
    size_t days_back,
    size_t* review_counts,
    size_t* count
);

/**
 * @brief Get difficulty distribution
 *
 * @param system SRS system
 * @param bins Output array of 10 bins (difficulty 1-10)
 * @return SR_SUCCESS or error code
 */
NIMCP_EXPORT sr_error_t sr_get_difficulty_distribution(
    sr_system_t system,
    size_t bins[10]
);

/**
 * @brief Get interval distribution
 *
 * @param system SRS system
 * @param brackets Interval brackets in days (e.g., [1, 7, 30, 90, 365])
 * @param num_brackets Number of brackets
 * @param counts Output counts per bracket
 * @return SR_SUCCESS or error code
 */
NIMCP_EXPORT sr_error_t sr_get_interval_distribution(
    sr_system_t system,
    const float* brackets,
    size_t num_brackets,
    size_t* counts
);

/**
 * @brief Calculate true retention rate
 *
 * WHAT: Compute actual retention from review history
 * WHY:  Validate algorithm predictions
 * HOW:  Ratio of correct recalls to total reviews
 *
 * @param system SRS system
 * @param days_back Period to analyze (0 = all time)
 * @return True retention rate [0, 1]
 */
NIMCP_EXPORT float sr_calculate_true_retention(
    sr_system_t system,
    size_t days_back
);

//=============================================================================
// PR Integration Functions
//=============================================================================

/**
 * @brief Sync item stability with Z-ladder tier
 *
 * WHAT: Update memory node tier based on SRS stability
 * WHY:  Maintain consistency between SRS and PR memory tiers
 * HOW:  Map stability ranges to Z0-Z3 tiers
 *
 * @param system SRS system
 * @param item_id Item to sync
 * @return SR_SUCCESS or error code
 *
 * Mapping (default):
 * - stability < 1 day: Z0 (working memory)
 * - stability < 7 days: Z1 (short-term)
 * - stability < 30 days: Z2 (long-term)
 * - stability >= 30 days: Z3 (permanent)
 */
NIMCP_EXPORT sr_error_t sr_sync_z_ladder(
    sr_system_t system,
    uint64_t item_id
);

/**
 * @brief Sync all items with Z-ladder
 *
 * @param system SRS system
 * @return Number of items synced
 */
NIMCP_EXPORT size_t sr_sync_all_z_ladder(sr_system_t system);

/**
 * @brief Generate retrieval cues from entanglement
 *
 * WHAT: Get related memories as retrieval cues
 * WHY:  Cued recall is more effective than free recall
 * HOW:  Query entanglement graph for connected memories
 *
 * @param system SRS system
 * @param item_id Item to generate cues for
 * @param cue_ids Output array of cue memory IDs
 * @param max_cues Maximum cues to return
 * @param count Output: actual cues returned
 * @return SR_SUCCESS or error code
 */
NIMCP_EXPORT sr_error_t sr_generate_cues(
    sr_system_t system,
    uint64_t item_id,
    uint64_t* cue_ids,
    size_t max_cues,
    size_t* count
);

/**
 * @brief Group similar items for interleaved practice
 *
 * WHAT: Find items similar to given item
 * WHY:  Interleaving similar items improves discrimination
 * HOW:  Use prime signature similarity
 *
 * @param system SRS system
 * @param item_id Reference item
 * @param similar_ids Output array of similar item IDs
 * @param max_similar Maximum similar items
 * @param count Output: actual count
 * @return SR_SUCCESS or error code
 */
NIMCP_EXPORT sr_error_t sr_find_similar_items(
    sr_system_t system,
    uint64_t item_id,
    uint64_t* similar_ids,
    size_t max_similar,
    size_t* count
);

//=============================================================================
// Serialization Functions
//=============================================================================

/**
 * @brief Serialize system state to buffer
 *
 * @param system System to serialize
 * @param buffer Output buffer (NULL to query size)
 * @param buffer_size Buffer capacity
 * @param written_size Output: bytes written or required
 * @return SR_SUCCESS or error code
 */
NIMCP_EXPORT sr_error_t sr_system_serialize(
    sr_system_t system,
    void* buffer,
    size_t buffer_size,
    size_t* written_size
);

/**
 * @brief Deserialize system state from buffer
 *
 * @param buffer Input buffer
 * @param buffer_size Buffer size
 * @param bytes_read Output: bytes consumed
 * @return System handle or NULL on error
 */
NIMCP_EXPORT sr_system_t sr_system_deserialize(
    const void* buffer,
    size_t buffer_size,
    size_t* bytes_read
);

/**
 * @brief Export item data for backup
 *
 * @param system SRS system
 * @param item_id Item to export
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @param written_size Output: bytes written
 * @return SR_SUCCESS or error code
 */
NIMCP_EXPORT sr_error_t sr_export_item(
    sr_system_t system,
    uint64_t item_id,
    void* buffer,
    size_t buffer_size,
    size_t* written_size
);

/**
 * @brief Import item data from backup
 *
 * @param system SRS system
 * @param buffer Input buffer
 * @param buffer_size Buffer size
 * @param memory Associated memory node
 * @return Item ID or 0 on error
 */
NIMCP_EXPORT uint64_t sr_import_item(
    sr_system_t system,
    const void* buffer,
    size_t buffer_size,
    pr_memory_node_t* memory
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get error string for error code
 *
 * @param error Error code
 * @return Human-readable error description
 */
NIMCP_EXPORT const char* sr_error_string(sr_error_t error);

/**
 * @brief Get last error message
 *
 * @return Error string or NULL if no error
 */
NIMCP_EXPORT const char* sr_get_last_error(void);

/**
 * @brief Get response string
 *
 * @param response Response value
 * @return Human-readable response name
 */
NIMCP_EXPORT const char* sr_response_string(sr_review_response_t response);

/**
 * @brief Get state string
 *
 * @param state Item state value
 * @return Human-readable state name
 */
NIMCP_EXPORT const char* sr_state_string(sr_item_state_t state);

/**
 * @brief Get current time in days since epoch
 *
 * @return Current time in fractional days
 */
NIMCP_EXPORT float sr_current_time_days(void);

/**
 * @brief Get current time in milliseconds since epoch
 *
 * @return Current time in milliseconds
 */
NIMCP_EXPORT uint64_t sr_current_time_ms(void);

/**
 * @brief Convert days to milliseconds
 *
 * @param days Time in days
 * @return Time in milliseconds
 */
NIMCP_EXPORT uint64_t sr_days_to_ms(float days);

/**
 * @brief Convert milliseconds to days
 *
 * @param ms Time in milliseconds
 * @return Time in days
 */
NIMCP_EXPORT float sr_ms_to_days(uint64_t ms);

/**
 * @brief Format interval for display
 *
 * @param interval_days Interval in days
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Number of characters written
 *
 * Output examples: "10m", "2h", "3d", "2w", "3mo", "1y"
 */
NIMCP_EXPORT size_t sr_format_interval(
    float interval_days,
    char* buffer,
    size_t size
);

/**
 * @brief Print item summary to stdout (debug)
 *
 * @param item Item to print
 */
NIMCP_EXPORT void sr_item_print(const sr_spaced_item_t* item);

/**
 * @brief Print system summary to stdout (debug)
 *
 * @param system System to summarize
 */
NIMCP_EXPORT void sr_system_print_summary(sr_system_t system);

//=============================================================================
// Inline Helper Functions
//=============================================================================

/**
 * @brief Check if item is due for review
 *
 * @param item Item to check
 * @param current_time Current time in days since epoch
 * @return true if item is due
 */
static inline bool sr_item_is_due(const sr_spaced_item_t* item, float current_time) {
    if (!item || item->is_suspended) {
        return false;
    }
    return current_time >= item->due_time;
}

/**
 * @brief Get days until item is due
 *
 * @param item Item to check
 * @param current_time Current time in days since epoch
 * @return Days until due (negative if overdue)
 */
static inline float sr_days_until_due(const sr_spaced_item_t* item, float current_time) {
    if (!item) {
        return 0.0f;
    }
    return item->due_time - current_time;
}

/**
 * @brief Check if item is a leech
 *
 * @param item Item to check
 * @param threshold Lapse threshold
 * @return true if item exceeds lapse threshold
 */
static inline bool sr_item_is_leech(const sr_spaced_item_t* item, size_t threshold) {
    if (!item) {
        return false;
    }
    return item->lapses >= threshold;
}

/**
 * @brief Get item maturity (based on interval)
 *
 * @param item Item to check
 * @return 0 = new, 1 = young (< 21 days), 2 = mature (>= 21 days)
 */
static inline int sr_item_maturity(const sr_spaced_item_t* item) {
    if (!item || item->state == SR_STATE_NEW) {
        return 0;
    }
    if (item->interval_days < 21.0f) {
        return 1;
    }
    return 2;
}

/**
 * @brief Calculate stability from interval and target retention
 *
 * Inverse of interval formula: S = I * ln(0.9) / ln(target_retention)
 *
 * @param interval_days Current interval
 * @param target_retention Target retention
 * @return Estimated stability
 */
static inline float sr_stability_from_interval(float interval_days, float target_retention) {
    if (target_retention <= 0.0f || target_retention >= 1.0f) {
        return interval_days;
    }
    float ln_target = logf(target_retention);
    if (fabsf(ln_target) < SR_EPSILON) {
        return interval_days;
    }
    return interval_days * SR_LN_09 / ln_target;
}

/**
 * @brief Calculate interval from stability and target retention
 *
 * Formula: I = S * ln(target_retention) / ln(0.9)
 *
 * @param stability Current stability
 * @param target_retention Target retention
 * @return Optimal interval
 */
static inline float sr_interval_from_stability(float stability, float target_retention) {
    if (target_retention <= 0.0f || target_retention >= 1.0f) {
        return stability;
    }
    float ln_target = logf(target_retention);
    return stability * ln_target / SR_LN_09;
}

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SPACED_REPETITION_H
