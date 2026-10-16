/**
 * @file nimcp_reasoning_integration.h
 * @brief Cognitive Integration for Logic & Reasoning System
 * @version 1.0.0
 * @date 2025-11-20
 *
 * WHAT: Cognitive layer integration for symbolic reasoning and inference
 * WHY:  Connect logic events to attention, curiosity, working memory, and executive systems
 * HOW:  Event-driven hooks that respond to reasoning events and modulate cognitive resources
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex integrates reasoning with executive control
 * - Hippocampus consolidates important logical rules to long-term memory
 * - Dopaminergic system rewards novel fact discovery (curiosity drive)
 * - Attention focuses on contradictions and unexpected inferences
 *
 * COGNITIVE INTEGRATION ARCHITECTURE:
 * 1. **Attention Integration**: Novel facts and contradictions receive attention boost
 * 2. **Curiosity Integration**: Unexplained facts trigger exploratory reasoning
 * 3. **Working Memory Integration**: Active inferences stored in WM (7±2 limit)
 * 4. **Executive Integration**: Complex proofs use planning and task switching
 * 5. **Consolidation Integration**: Important rules consolidated to long-term memory
 *
 * EVENT FLOW EXAMPLE:
 * 1. Reasoner derives novel fact → publishes EVENT_NOVEL_FACT_DERIVED
 * 2. Attention hook → boosts attention to novel fact (salience +0.3)
 * 3. Curiosity hook → increases curiosity drive to explore related concepts
 * 4. Working memory hook → stores active inference in WM slot
 * 5. Executive hook → plans multi-step proof for complex goals
 * 6. Consolidation hook → stores important rules to long-term memory
 *
 * INTEGRATION POINTS:
 * - Event Bus (include/core/events/nimcp_event_bus.h): Logic events (0xC000-0xC00C)
 * - Attention (src/cognitive/fault_tolerance/nimcp_fault_attention.h): Salience weighting
 * - Curiosity (src/cognitive/curiosity/nimcp_curiosity.h): Exploration drive
 * - Working Memory (src/cognitive/working_memory/nimcp_working_memory.h): 7±2 buffer
 * - Executive (src/cognitive/executive/nimcp_executive.h): Planning and control
 * - Consolidation (src/cognitive/consolidation/nimcp_consolidation.h): Long-term storage
 *
 * PERFORMANCE:
 * - Hook execution: <50μs per event
 * - Memory overhead: ~4KB for integration state
 * - Thread-safe: Yes (uses mutex for state protection)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_REASONING_INTEGRATION_H
#define NIMCP_REASONING_INTEGRATION_H

#include <stdint.h>
#include <stdbool.h>
#include "core/events/nimcp_event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define REASONING_MAX_ACTIVE_INFERENCES 7     /**< Miller's 7±2 working memory limit */
#define REASONING_NOVEL_FACT_SALIENCE 0.8f    /**< Salience boost for novel facts */
#define REASONING_CONTRADICTION_SALIENCE 1.0f /**< Max salience for contradictions */
#define REASONING_CURIOSITY_BOOST 0.3f        /**< Curiosity increase for unexplained facts */
#define REASONING_CONSOLIDATION_THRESHOLD 5   /**< Min rule usage before consolidation */

//=============================================================================
// Types and Structures
//=============================================================================

/**
 * @brief Reasoning integration configuration
 *
 * WHAT: Configuration parameters for cognitive integration
 * WHY:  Allow customization of integration behavior
 * HOW:  Weight factors control strength of cognitive responses
 */
typedef struct {
    // Integration enable flags
    bool enable_attention_integration;      /**< Enable attention focusing */
    bool enable_curiosity_integration;      /**< Enable curiosity-driven exploration */
    bool enable_working_memory_integration; /**< Enable WM storage of inferences */
    bool enable_executive_integration;      /**< Enable executive planning */
    bool enable_consolidation_integration;  /**< Enable long-term consolidation */

    // Attention configuration
    float novel_fact_salience_boost;        /**< Salience increase for novel facts [0.0, 1.0] */
    float contradiction_salience_boost;     /**< Salience increase for contradictions [0.0, 1.0] */
    float proof_found_salience_boost;       /**< Salience increase for successful proofs [0.0, 1.0] */

    // Curiosity configuration
    float unexplained_curiosity_boost;      /**< Curiosity increase for proof failures [0.0, 1.0] */
    float novel_fact_curiosity_boost;       /**< Curiosity increase for new facts [0.0, 1.0] */

    // Working memory configuration
    uint32_t max_active_inferences;         /**< Maximum concurrent inferences in WM */
    float inference_decay_tau_ms;           /**< WM decay time constant (ms) */

    // Executive configuration
    uint32_t min_proof_steps_for_planning;  /**< Min steps to trigger executive planning */
    float planning_priority;                /**< Task priority for reasoning plans [0.0, 1.0] */

    // Consolidation configuration
    uint32_t min_rule_uses_for_consolidation; /**< Min uses before consolidating rule */
    float consolidation_threshold;          /**< Importance threshold for consolidation [0.0, 1.0] */
} reasoning_integration_config_t;

/**
 * @brief Active inference state
 *
 * WHAT: Tracks an active inference in working memory
 * WHY:  Maintain context for multi-step reasoning
 * HOW:  Stores goal, current state, and progress
 */
typedef struct {
    char goal[256];                /**< Inference goal description */
    uint32_t step_count;           /**< Number of reasoning steps taken */
    uint64_t start_time_ms;        /**< Inference start timestamp */
    float salience;                /**< Current salience [0.0, 1.0] */
    bool is_active;                /**< Whether inference is ongoing */
    uint32_t inference_id;         /**< Unique inference identifier */
} active_inference_t;

/**
 * @brief Rule usage tracking for consolidation
 *
 * WHAT: Tracks rule usage frequency for consolidation decisions
 * WHY:  Consolidate frequently-used rules to long-term memory
 * HOW:  Count uses, compute importance, trigger consolidation
 */
typedef struct {
    char rule[512];                /**< Rule string representation */
    uint32_t use_count;            /**< Number of times rule used */
    uint32_t success_count;        /**< Number of successful applications */
    float importance;              /**< Computed importance [0.0, 1.0] */
    uint64_t first_used_ms;        /**< First usage timestamp */
    uint64_t last_used_ms;         /**< Last usage timestamp */
    bool consolidated;             /**< Whether rule consolidated to LTM */
} rule_usage_t;

/**
 * @brief Reasoning integration statistics
 */
typedef struct {
    uint64_t total_events_processed;        /**< Total logic events processed */
    uint64_t attention_boosts_applied;      /**< Attention adjustments made */
    uint64_t curiosity_triggers;            /**< Curiosity explorations triggered */
    uint64_t wm_inferences_stored;          /**< Inferences stored in working memory */
    uint64_t executive_plans_created;       /**< Executive plans generated */
    uint64_t rules_consolidated;            /**< Rules consolidated to LTM */
    uint32_t current_active_inferences;     /**< Current inferences in WM */
    uint32_t current_tracked_rules;         /**< Current rules tracked for consolidation */
    float avg_hook_execution_time_us;       /**< Average hook execution time */
} reasoning_integration_stats_t;

/**
 * @brief Reasoning integration instance (opaque)
 */
typedef struct reasoning_integration reasoning_integration_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create reasoning integration with default configuration
 *
 * WHAT: Allocates and initializes cognitive integration for reasoning
 * WHY:  Entry point for reasoning-cognitive coupling
 * HOW:  Allocates structure, sets default config, initializes statistics
 *
 * COMPLEXITY: O(1)
 * MEMORY: ~8KB for structure + tracking arrays
 *
 * @param event_bus Event bus to subscribe to logic events (non-NULL)
 * @return Integration handle, NULL on failure
 *
 * @note Subscribes to all logic events (0xC000-0xC00C)
 * @note All integration hooks enabled by default
 */
reasoning_integration_t* reasoning_integration_create(event_bus_t event_bus);

/**
 * @brief Create reasoning integration with custom configuration
 *
 * WHAT: Creates integration with specified configuration
 * WHY:  Allow domain-specific integration strategies
 * HOW:  Validates config, allocates structure, applies configuration
 *
 * COMPLEXITY: O(1)
 * MEMORY: ~8KB for structure + tracking arrays
 *
 * @param event_bus Event bus to subscribe to logic events (non-NULL)
 * @param config Configuration parameters (NULL for defaults)
 * @return Integration handle, NULL on invalid config
 *
 * @note Validates all config parameters are in valid ranges
 */
reasoning_integration_t* reasoning_integration_create_custom(
    event_bus_t event_bus,
    const reasoning_integration_config_t* config
);

/**
 * @brief Destroy reasoning integration and free resources
 *
 * WHAT: Releases memory and unsubscribes from events
 * WHY:  Prevent memory leaks and clean shutdown
 * HOW:  Unsubscribes from event bus, frees tracking arrays and structure
 *
 * COMPLEXITY: O(1)
 * MEMORY: Frees ~8KB
 *
 * @param integration Integration handle (NULL safe)
 */
void reasoning_integration_destroy(reasoning_integration_t* integration);

//=============================================================================
// Cognitive Hook Functions (Called by Event Bus Callbacks)
//=============================================================================

/**
 * @brief Attention hook for novel facts
 *
 * WHAT: Boosts attention to novel facts and contradictions
 * WHY:  Focus cognitive resources on unexpected/important discoveries
 * HOW:  Increases salience weight in attention system
 *
 * TRIGGER EVENTS:
 * - EVENT_NOVEL_FACT_DERIVED: Salience boost +0.8
 * - EVENT_CONTRADICTION_DETECTED: Salience boost +1.0 (max)
 * - EVENT_PROOF_FOUND: Salience boost +0.5
 *
 * COMPLEXITY: O(1)
 * PERFORMANCE: <10μs
 *
 * @param integration Integration handle (non-NULL)
 * @param event Event that triggered hook (non-NULL)
 * @return true on success, false on error
 *
 * @note Requires attention integration enabled in config
 * @note Integrates with fault_attention system
 */
bool reasoning_attention_hook(
    reasoning_integration_t* integration,
    const brain_event_t* event
);

/**
 * @brief Curiosity hook for unexplained facts
 *
 * WHAT: Triggers curiosity-driven exploration for proof failures
 * WHY:  Drive discovery of new rules and explanations
 * HOW:  Increases curiosity drive and triggers exploration
 *
 * TRIGGER EVENTS:
 * - EVENT_PROOF_FAILED: Curiosity boost +0.3
 * - EVENT_UNIFICATION_FAILED: Curiosity boost +0.2
 * - EVENT_NOVEL_FACT_DERIVED: Curiosity boost +0.1
 *
 * COMPLEXITY: O(1)
 * PERFORMANCE: <15μs
 *
 * @param integration Integration handle (non-NULL)
 * @param event Event that triggered hook (non-NULL)
 * @return true on success, false on error
 *
 * @note Requires curiosity integration enabled in config
 * @note Integrates with nimcp_curiosity system
 */
bool reasoning_curiosity_hook(
    reasoning_integration_t* integration,
    const brain_event_t* event
);

/**
 * @brief Working memory hook for active inferences
 *
 * WHAT: Stores active inferences in working memory buffer
 * WHY:  Maintain context for multi-step reasoning (7±2 limit)
 * HOW:  Adds inference to WM, evicts oldest if capacity exceeded
 *
 * TRIGGER EVENTS:
 * - EVENT_LOGIC_INFERENCE_STARTED: Add inference to WM
 * - EVENT_LOGIC_INFERENCE_COMPLETE: Mark inference complete, allow eviction
 * - EVENT_FORWARD_CHAIN_STEP: Update step count for active inference
 * - EVENT_BACKWARD_CHAIN_STEP: Update step count for active inference
 *
 * COMPLEXITY: O(1)
 * PERFORMANCE: <20μs
 *
 * @param integration Integration handle (non-NULL)
 * @param event Event that triggered hook (non-NULL)
 * @return true on success, false on error
 *
 * @note Respects Miller's 7±2 working memory limit
 * @note Uses salience-based eviction when capacity exceeded
 * @note Integrates with nimcp_working_memory system
 */
bool reasoning_working_memory_hook(
    reasoning_integration_t* integration,
    const brain_event_t* event
);

/**
 * @brief Executive hook for multi-step proofs
 *
 * WHAT: Uses executive planning for complex multi-step proofs
 * WHY:  Decompose complex goals into manageable sub-goals
 * HOW:  Creates executive plan with proof steps as tasks
 *
 * TRIGGER EVENTS:
 * - EVENT_LOGIC_INFERENCE_STARTED: Check if complex, create plan if needed
 * - EVENT_FORWARD_CHAIN_STEP: Update plan progress
 * - EVENT_BACKWARD_CHAIN_STEP: Update plan progress
 * - EVENT_PROOF_FOUND: Mark plan complete
 * - EVENT_PROOF_FAILED: Mark plan failed
 *
 * COMPLEXITY: O(1)
 * PERFORMANCE: <30μs
 *
 * @param integration Integration handle (non-NULL)
 * @param event Event that triggered hook (non-NULL)
 * @return true on success, false on error
 *
 * @note Only triggers for proofs with >min_proof_steps_for_planning steps
 * @note Integrates with nimcp_executive system
 */
bool reasoning_executive_hook(
    reasoning_integration_t* integration,
    const brain_event_t* event
);

/**
 * @brief Consolidation hook for important rules
 *
 * WHAT: Consolidates frequently-used rules to long-term memory
 * WHY:  Improve reasoning efficiency by storing important patterns
 * HOW:  Tracks rule usage, consolidates when threshold exceeded
 *
 * TRIGGER EVENTS:
 * - EVENT_RULE_ADDED: Track new rule for consolidation
 * - EVENT_FORWARD_CHAIN_STEP: Increment rule usage count
 * - EVENT_BACKWARD_CHAIN_STEP: Increment rule usage count
 * - EVENT_PROOF_FOUND: Increment success count for used rules
 *
 * ALGORITHM:
 * 1. Track rule usage and success rate
 * 2. Compute importance: (success_count / use_count) * log(use_count)
 * 3. If importance > threshold AND use_count > min_uses: consolidate
 *
 * COMPLEXITY: O(n) where n = number of tracked rules
 * PERFORMANCE: <50μs
 *
 * @param integration Integration handle (non-NULL)
 * @param event Event that triggered hook (non-NULL)
 * @return true on success, false on error
 *
 * @note Integrates with nimcp_consolidation system
 * @note Prevents redundant consolidation via consolidated flag
 */
bool reasoning_consolidation_hook(
    reasoning_integration_t* integration,
    const brain_event_t* event
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get active inferences in working memory
 *
 * WHAT: Retrieves all active inferences currently in WM
 * WHY:  Inspect reasoning state for monitoring or debugging
 * HOW:  Copies active inference array to output
 *
 * @param integration Integration handle (non-NULL)
 * @param inferences Output array (non-NULL, size ≥ max_active_inferences)
 * @param max_count Maximum inferences to copy
 * @return Number of inferences copied, 0 on error
 */
uint32_t reasoning_integration_get_active_inferences(
    const reasoning_integration_t* integration,
    active_inference_t* inferences,
    uint32_t max_count
);

/**
 * @brief Get tracked rules for consolidation
 *
 * WHAT: Retrieves all rules being tracked for consolidation
 * WHY:  Monitor rule learning and consolidation progress
 * HOW:  Copies rule usage array to output
 *
 * @param integration Integration handle (non-NULL)
 * @param rules Output array (non-NULL)
 * @param max_count Maximum rules to copy
 * @return Number of rules copied, 0 on error
 */
uint32_t reasoning_integration_get_tracked_rules(
    const reasoning_integration_t* integration,
    rule_usage_t* rules,
    uint32_t max_count
);

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get current configuration
 *
 * @param integration Integration handle (non-NULL)
 * @param config Output parameter for configuration (non-NULL)
 * @return true on success, false on NULL parameters
 */
bool reasoning_integration_get_config(
    const reasoning_integration_t* integration,
    reasoning_integration_config_t* config
);

/**
 * @brief Update configuration
 *
 * @param integration Integration handle (non-NULL)
 * @param config New configuration (non-NULL, validated)
 * @return true on success, false if config invalid
 */
bool reasoning_integration_set_config(
    reasoning_integration_t* integration,
    const reasoning_integration_config_t* config
);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get integration statistics
 *
 * @param integration Integration handle (non-NULL)
 * @param stats Output parameter for statistics (non-NULL)
 * @return true on success, false on NULL parameters
 */
bool reasoning_integration_get_stats(
    const reasoning_integration_t* integration,
    reasoning_integration_stats_t* stats
);

/**
 * @brief Reset statistics counters
 *
 * @param integration Integration handle (non-NULL)
 * @return true on success, false on NULL parameter
 */
bool reasoning_integration_reset_stats(reasoning_integration_t* integration);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get default configuration
 *
 * @return Default configuration structure
 */
reasoning_integration_config_t reasoning_integration_default_config(void);

/**
 * @brief Validate configuration
 *
 * @param config Configuration to validate (non-NULL)
 * @return true if valid, false otherwise
 */
bool reasoning_integration_validate_config(
    const reasoning_integration_config_t* config
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_REASONING_INTEGRATION_H
