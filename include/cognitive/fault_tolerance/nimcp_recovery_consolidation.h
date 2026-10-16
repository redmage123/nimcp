/**
 * @file nimcp_recovery_consolidation.h
 * @brief Consolidation for long-term learning from recovery episodes
 *
 * WHAT: Transfer recovery knowledge from episodic to semantic memory
 * WHY:  Extract general principles from specific experiences for faster decisions
 * HOW:  Pattern extraction → Rule creation → Statistical validation
 *
 * BIOLOGICAL BASIS:
 * - Mimics hippocampal consolidation during sleep
 * - Episodic memories (hippocampus) → Semantic knowledge (neocortex)
 * - Pattern extraction via replay and abstraction
 * - Statistical confidence from repeated evidence
 *
 * INTEGRATION POINTS:
 * 1. Episodic Memory (nimcp_recovery_episode.h)
 *    - Consumes: recovery_episode_t instances
 *    - Analyzes: Episode patterns and outcomes
 *
 * 2. Semantic Memory (output)
 *    - Produces: semantic_rule_t instances
 *    - Enables: Fast rule-based recovery decisions
 *
 * 3. Recovery System (nimcp_recovery.h)
 *    - Used by: Recovery planner for strategy selection
 *    - Provides: Success rates and confidence scores
 *
 * EXAMPLE USAGE:
 * ```c
 * // Create consolidation system
 * consolidation_config_t config = recovery_consolidation_default_config();
 * config.min_episodes_for_rule = 15;
 * config.min_confidence_threshold = 0.85f;
 * recovery_consolidation_t* cons = consolidation_create_custom(&config);
 *
 * // Add recovery episodes over time
 * for (each recovery) {
 *     recovery_consolidation_add_episode(cons, &episode);
 * }
 *
 * // Run consolidation (extract patterns → create rules)
 * recovery_consolidation_run(cons);
 *
 * // Later: Use learned rules for fast recovery
 * error_pattern_t pattern = {ERROR_TYPE_NAN, layer_id, 0};
 * semantic_rule_t* rule = recovery_consolidation_get_rule(cons, &pattern);
 * if (rule && rule->confidence > 0.8f) {
 *     // Apply rule with high confidence
 *     apply_recovery_action(rule->action);
 * }
 *
 * // Background consolidation (async)
 * consolidation_start_background(cons);  // Runs in thread
 * ```
 *
 * PERFORMANCE:
 * - O(N log N) pattern extraction (N = episodes)
 * - O(1) rule lookup (hash table)
 * - Memory: ~500 bytes per rule, ~200 bytes per pending episode
 *
 * @author NIMCP Development Team
 * @date 2025-01-20
 * @version 2.7.0 Phase 10.1
 */

#ifndef NIMCP_RECOVERY_CONSOLIDATION_H
#define NIMCP_RECOVERY_CONSOLIDATION_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/fault_tolerance/nimcp_recovery.h"
#include "utils/fault_tolerance/nimcp_diagnostics.h"
#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations and Opaque Types
//=============================================================================

/**
 * WHAT: Opaque consolidation system handle
 * WHY:  Encapsulation - hide implementation details
 */
typedef struct recovery_consolidation recovery_consolidation_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Error pattern extracted from episodes
 *
 * WHAT: Generalized error signature for rule matching
 * WHY:  Abstract specific errors to reusable patterns
 */
typedef struct {
    error_type_t type;          /**< Error type (NaN, overflow, etc.) */
    uint32_t layer_id;          /**< Layer where error occurred */
    uint64_t hash;              /**< Pattern hash for fast lookup */
} error_pattern_t;

/**
 * @brief Semantic rule learned from episodes
 *
 * WHAT: General principle extracted from specific experiences
 * WHY:  Enable fast, confident recovery decisions
 * HOW:  Pattern → Action mapping with statistical validation
 *
 * EXAMPLE:
 * ```c
 * semantic_rule_t rule = {
 *     .pattern = {ERROR_TYPE_NAN, ANY_LAYER, hash},
 *     .action = RECOVERY_ACTION_REDUCE_LR,
 *     .success_rate = 0.90f,      // 18/20 episodes succeeded
 *     .sample_count = 20,
 *     .confidence = 0.95f         // p < 0.05 significance
 * };
 * // Interpretation: "NaN errors should reduce learning rate (90% success)"
 * ```
 */
typedef struct {
    error_pattern_t pattern;      /**< Error pattern this rule matches */
    recovery_action_t action;     /**< Recovery action to take */
    float success_rate;           /**< Success rate [0.0, 1.0] */
    uint32_t sample_count;        /**< Number of episodes supporting rule */
    float confidence;             /**< Statistical confidence [0.0, 1.0] */
    uint64_t last_updated_ms;     /**< Timestamp of last update */
} semantic_rule_t;

/**
 * @brief Recovery episode from episodic memory
 *
 * WHAT: Single recovery experience
 * WHY:  Raw data for pattern extraction
 */
typedef struct {
    uint64_t timestamp_ms;        /**< When episode occurred */

    // Error signature
    struct {
        error_type_t type;
        uint32_t layer_id;
        uint64_t hash;
    } error_sig;

    // Recovery attempt
    recovery_action_t recovery_action;
    bool success;
    uint64_t recovery_time_us;
    float success_confidence;

    // Emotional valence (for prioritization)
    float emotional_tag;          /**< -1.0 (bad) to +1.0 (good) */
} recovery_episode_t;

/**
 * @brief Consolidation configuration
 */
typedef struct {
    uint32_t min_episodes_for_rule;      /**< Minimum episodes to create rule (default: 15) */
    float min_confidence_threshold;      /**< Minimum confidence (default: 0.85) */
    uint64_t consolidation_interval_ms;  /**< Background consolidation interval (default: 60000) */
    uint32_t max_rules;                  /**< Maximum semantic rules (default: 1000) */
    bool enable_background_consolidation; /**< Run consolidation in background thread */
} consolidation_config_t;

/**
 * @brief Consolidation statistics
 */
typedef struct {
    uint64_t total_episodes_processed; /**< Total episodes consolidated */
    uint32_t rules_created;            /**< Total rules created */
    uint32_t rules_updated;            /**< Total rules updated */
    uint32_t consolidation_runs;       /**< Number of consolidation cycles */
    uint64_t total_consolidation_time_ms; /**< Total time spent consolidating */
    float average_confidence;          /**< Average rule confidence */
} consolidation_stats_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create consolidation system with default config
 *
 * WHAT: Initialize consolidation with standard parameters
 * WHY:  Quick setup for typical use cases
 * HOW:  Allocate structure, set defaults, initialize storage
 *
 * COMPLEXITY: O(1)
 * MEMORY: ~8KB for structure + rule storage
 *
 * @return Consolidation instance or NULL on failure
 */
recovery_consolidation_t* recovery_consolidation_create(void);

/**
 * @brief Create consolidation system with custom config
 *
 * WHAT: Initialize with user-specified parameters
 * WHY:  Allow tuning for specific scenarios
 * HOW:  Validate config → Allocate → Initialize
 *
 * COMPLEXITY: O(1)
 * MEMORY: Variable based on config.max_rules
 *
 * @param config Configuration parameters (NULL = use defaults)
 * @return Consolidation instance or NULL on failure
 */
recovery_consolidation_t* consolidation_create_custom(
    const consolidation_config_t* config
);

/**
 * @brief Destroy consolidation system
 *
 * WHAT: Free all resources
 * WHY:  Prevent memory leaks
 * HOW:  Free rules → Free episodes → Free structure
 *
 * COMPLEXITY: O(R + E) where R = rules, E = episodes
 *
 * @param consolidation Instance to destroy (NULL-safe)
 */
void recovery_consolidation_destroy(recovery_consolidation_t* consolidation);

/**
 * @brief Get default configuration
 *
 * WHAT: Return standard config with sensible defaults
 * WHY:  Starting point for customization
 *
 * @return Default configuration struct
 */
consolidation_config_t recovery_consolidation_default_config(void);

//=============================================================================
// Episode Management
//=============================================================================

/**
 * @brief Add recovery episode to consolidation queue
 *
 * WHAT: Store episode for later pattern extraction
 * WHY:  Accumulate experiences for consolidation
 * HOW:  Deep copy episode → Add to pending queue
 *
 * COMPLEXITY: O(1)
 * MEMORY: ~200 bytes per episode
 *
 * @param consolidation Consolidation instance
 * @param episode Episode to add
 * @return true on success, false on error
 */
bool recovery_consolidation_add_episode(
    recovery_consolidation_t* consolidation,
    const recovery_episode_t* episode
);

/**
 * @brief Get number of pending episodes
 *
 * WHAT: Count episodes waiting for consolidation
 * WHY:  Monitor consolidation backlog
 *
 * @param consolidation Consolidation instance
 * @return Number of pending episodes (0 if NULL)
 */
uint32_t consolidation_get_episodes_pending(
    const recovery_consolidation_t* consolidation
);

//=============================================================================
// Pattern Extraction and Rule Creation
//=============================================================================

/**
 * @brief Extract patterns from episodes
 *
 * WHAT: Analyze episodes to identify recurring patterns
 * WHY:  Find commonalities across experiences
 * HOW:  Group by error signature → Identify frequent patterns
 *
 * ALGORITHM:
 * 1. Group episodes by error signature (type + layer)
 * 2. For each group with >= min_episodes:
 *    a. Extract common pattern
 *    b. Identify most common recovery action
 *    c. Compute success rate
 * 3. Store patterns for rule creation
 *
 * COMPLEXITY: O(N log N) where N = episode count
 * MEMORY: O(P) where P = pattern count
 *
 * @param consolidation Consolidation instance
 * @param episodes Array of episodes to analyze
 * @param count Number of episodes
 */
void consolidation_extract_patterns(
    recovery_consolidation_t* consolidation,
    const recovery_episode_t* episodes,
    uint32_t count
);

/**
 * @brief Create semantic rule from similar episodes
 *
 * WHAT: Generate rule with success rate and confidence
 * WHY:  Convert episodes to actionable knowledge
 * HOW:  Aggregate episodes → Compute stats → Validate
 *
 * ALGORITHM:
 * 1. Compute success rate: successes / total
 * 2. Calculate statistical confidence (binomial test)
 * 3. Create rule if confidence >= threshold
 *
 * STATISTICAL CONFIDENCE:
 * - Uses binomial proportion confidence interval
 * - Higher N → Higher confidence for same success rate
 * - Formula: CI = p ± z * sqrt(p(1-p)/N)
 *
 * COMPLEXITY: O(N) where N = episode count
 *
 * @param consolidation Consolidation instance
 * @param episodes Array of similar episode pointers
 * @param count Number of episodes
 * @return Created semantic rule
 */
semantic_rule_t consolidation_create_rule(
    recovery_consolidation_t* consolidation,
    const recovery_episode_t** episodes,
    uint32_t count
);

/**
 * @brief Get number of extracted patterns
 *
 * @param consolidation Consolidation instance
 * @return Pattern count (0 if NULL)
 */
uint32_t consolidation_get_pattern_count(
    const recovery_consolidation_t* consolidation
);

//=============================================================================
// Semantic Memory (Rule Storage and Retrieval)
//=============================================================================

/**
 * @brief Add semantic rule to memory
 *
 * WHAT: Store rule for future lookups
 * WHY:  Build knowledge base of recovery strategies
 * HOW:  Hash pattern → Insert/update rule → Evict if full
 *
 * COMPLEXITY: O(1) average, O(R) worst case
 *
 * @param consolidation Consolidation instance
 * @param rule Rule to add
 * @return true on success, false on error
 */
bool consolidation_add_rule(
    recovery_consolidation_t* consolidation,
    const semantic_rule_t* rule
);

/**
 * @brief Retrieve semantic rule by pattern
 *
 * WHAT: Find rule matching error pattern
 * WHY:  Fast lookup for recovery decisions
 * HOW:  Hash pattern → Lookup in rule table
 *
 * COMPLEXITY: O(1) average
 *
 * @param consolidation Consolidation instance
 * @param pattern Error pattern to match
 * @return Pointer to rule or NULL if not found
 * @note Returned pointer is valid until next recovery_consolidation_run()
 */
semantic_rule_t* recovery_consolidation_get_rule(
    recovery_consolidation_t* consolidation,
    const error_pattern_t* pattern
);

/**
 * @brief Get number of stored rules
 *
 * @param consolidation Consolidation instance
 * @return Rule count (0 if NULL)
 */
uint32_t consolidation_get_rule_count(
    const recovery_consolidation_t* consolidation
);

//=============================================================================
// Consolidation Process
//=============================================================================

/**
 * @brief Run consolidation process
 *
 * WHAT: Execute full consolidation pipeline
 * WHY:  Transform pending episodes into semantic rules
 * HOW:  Extract patterns → Create rules → Update memory → Clear episodes
 *
 * ALGORITHM:
 * 1. Extract patterns from pending episodes
 * 2. For each pattern:
 *    a. Group similar episodes
 *    b. Create/update semantic rule
 *    c. Validate confidence threshold
 * 3. Clear processed episodes
 * 4. Update statistics
 *
 * COMPLEXITY: O(E log E) where E = episode count
 * MEMORY: O(R) where R = rule count
 *
 * @param consolidation Consolidation instance
 */
void recovery_consolidation_run(recovery_consolidation_t* consolidation);

/**
 * @brief Check if consolidation is currently active
 *
 * @param consolidation Consolidation instance
 * @return true if consolidation is running
 */
bool consolidation_is_active(
    const recovery_consolidation_t* consolidation
);

//=============================================================================
// Background Consolidation (Async)
//=============================================================================

/**
 * @brief Start background consolidation thread
 *
 * WHAT: Run consolidation periodically in background
 * WHY:  Avoid blocking main recovery thread
 * HOW:  Spawn thread → Periodic recovery_consolidation_run()
 *
 * THREAD SAFETY: Thread-safe episode addition
 *
 * @param consolidation Consolidation instance
 * @return true on success, false if already running or error
 */
bool consolidation_start_background(
    recovery_consolidation_t* consolidation
);

/**
 * @brief Stop background consolidation thread
 *
 * WHAT: Terminate background thread gracefully
 * WHY:  Clean shutdown
 * HOW:  Signal thread → Wait for completion
 *
 * @param consolidation Consolidation instance
 */
void consolidation_stop_background(
    recovery_consolidation_t* consolidation
);

/**
 * @brief Check if background consolidation is running
 *
 * @param consolidation Consolidation instance
 * @return true if background thread is active
 */
bool consolidation_is_background_running(
    const recovery_consolidation_t* consolidation
);

//=============================================================================
// Statistics and Reporting
//=============================================================================

/**
 * @brief Get consolidation statistics
 *
 * WHAT: Retrieve performance metrics
 * WHY:  Monitor system effectiveness
 *
 * @param consolidation Consolidation instance
 * @param stats Output parameter for statistics
 * @return true on success, false if NULL parameters
 */
bool recovery_consolidation_get_stats(
    const recovery_consolidation_t* consolidation,
    consolidation_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_RECOVERY_CONSOLIDATION_H
