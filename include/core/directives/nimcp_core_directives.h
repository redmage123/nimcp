/**
 * @file nimcp_core_directives.h
 * @brief Core Directives Orchestrator - Main gate for all brain actions
 *
 * WHAT: Central orchestrator that evaluates ALL proposed actions through
 *       multiple directive layers (harm prevention, commands, self-preservation)
 * WHY:  ALL brain region outputs MUST pass through a unified evaluation system
 *       to ensure safety, compliance, and ethical behavior
 * HOW:  Facade pattern coordinating multiple directive modules in strict
 *       priority order with bio-async integration for cross-module messaging
 *
 * BIOLOGICAL BASIS:
 * Models the prefrontal cortex's role as "executive controller" that gates
 * all motor outputs and cognitive actions. Just as the PFC can veto planned
 * actions through inhibitory control, this orchestrator blocks harmful or
 * non-compliant actions before execution.
 *
 * EVALUATION ORDER (STRICT PRIORITY):
 * 1. First Law (harm_prevention) - HIGHEST PRIORITY
 * 2. Combinatorial Harm check - Detect emergent harmful patterns
 * 3. Golden Rule (reciprocity) - Empathy-based evaluation
 * 4. Second Law (command_compliance) - Obey valid commands
 * 5. Third Law (self_preservation) - LOWEST PRIORITY
 *
 * ARCHITECTURE:
 *
 *   Brain Regions → Proposed Actions → Core Directives Orchestrator
 *                                             │
 *                    ┌────────────────────────┼────────────────────────┐
 *                    │                        │                        │
 *              First Law              Combinatorial            Golden Rule
 *           (Harm Prevention)            Harm                (Reciprocity)
 *                    │                        │                        │
 *                    └────────────────────────┼────────────────────────┘
 *                                             │
 *                    ┌────────────────────────┼────────────────────────┐
 *                    │                        │                        │
 *              Second Law              Third Law               Action
 *          (Command Compliance)   (Self-Preservation)         History
 *                    │                        │                        │
 *                    └────────────────────────┼────────────────────────┘
 *                                             │
 *                                    ALLOW / BLOCK
 *                                             ↓
 *                                      Action Execution
 *
 * THREAD SAFETY:
 * - All evaluation functions are thread-safe (internal mutex)
 * - Multiple threads can evaluate actions concurrently
 * - Statistics updates are atomic
 *
 * @author NIMCP Development Team
 * @date 2025-12-16
 * @version 1.0.0
 */

#ifndef NIMCP_CORE_DIRECTIVES_H
#define NIMCP_CORE_DIRECTIVES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "core/directives/nimcp_action_history.h"
#include "core/directives/nimcp_harm_classifier.h"
#include "core/directives/nimcp_harm_prevention.h"
#include "core/directives/nimcp_command_compliance.h"
#include "core/directives/nimcp_self_preservation.h"
#include "core/directives/nimcp_reciprocity_eval.h"
#include "core/directives/nimcp_combinatorial_harm.h"
#include "cognitive/ethics/nimcp_ethics.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *============================================================================*/

/** Maximum length of action description */
#define DIRECTIVE_ACTION_DESC_MAX 256

/** Maximum length of action type */
#define DIRECTIVE_ACTION_TYPE_MAX 64

/** Maximum length of blocking reason */
#define DIRECTIVE_REASON_MAX 512

/** Maximum action data size */
#define DIRECTIVE_ACTION_DATA_MAX 512

/*=============================================================================
 * TYPES
 *============================================================================*/

/**
 * @brief Directive evaluation result codes
 *
 * WHAT: Final verdict of directive evaluation
 * WHY:  Clear categorization of why an action was blocked or allowed
 * HOW:  Priority-ordered enum (higher value = higher priority violation)
 */
typedef enum {
    DIRECTIVE_RESULT_ALLOW = 0,              /**< Action is allowed */
    DIRECTIVE_RESULT_WARN,                    /**< Action allowed with warning */
    DIRECTIVE_RESULT_ESCALATE,                /**< Action escalated to human */
    DIRECTIVE_RESULT_BLOCK_COMMAND_INVALID,   /**< Blocked: Invalid command */
    DIRECTIVE_RESULT_BLOCK_GOLDEN_RULE,       /**< Blocked: Golden Rule violation */
    DIRECTIVE_RESULT_BLOCK_COMBINATORIAL,     /**< Blocked: Combinatorial harm */
    DIRECTIVE_RESULT_BLOCK_FIRST_LAW          /**< Blocked: First Law violation (HIGHEST) */
} directive_result_t;

/* Note: command_source_t is defined in nimcp_command_compliance.h */

/**
 * @brief Proposed action structure
 *
 * WHAT: Complete description of action to be evaluated
 * WHY:  Standardized format for all directive checks
 * HOW:  Combines description, metadata, and optional command context
 */
typedef struct {
    char action_description[DIRECTIVE_ACTION_DESC_MAX];  /**< Human-readable description */
    char action_type[DIRECTIVE_ACTION_TYPE_MAX];         /**< Action type category */
    uint32_t source_module;                              /**< Module ID proposing action */
    uint8_t action_data[DIRECTIVE_ACTION_DATA_MAX];      /**< Serialized action data */
    size_t action_data_len;                              /**< Length of action_data */
    bool is_command;                                     /**< Is this a commanded action? */
    command_source_t command_source;                     /**< Source if is_command=true */
    float predicted_harm;                                /**< Pre-computed harm estimate [0,1] */
    uint32_t affected_agent_count;                       /**< Number of affected agents */
    agent_id_t affected_agents[32];                      /**< IDs of affected agents */
} proposed_action_t;

/**
 * @brief Directive evaluation result
 *
 * WHAT: Complete evaluation result with all checks and metadata
 * WHY:  Provides full transparency into why action was allowed/blocked
 * HOW:  Aggregates results from all directive layers
 */
typedef struct {
    directive_result_t result;              /**< Final verdict */
    bool first_law_passed;                  /**< Did First Law check pass? */
    bool combinatorial_passed;              /**< Did combinatorial check pass? */
    bool golden_rule_passed;                /**< Did Golden Rule check pass? */
    bool command_valid;                     /**< Is command valid (if applicable)? */
    bool self_preservation_active;          /**< Was self-preservation considered? */
    float total_harm_score;                 /**< Aggregated harm score [0,1] */
    float first_law_harm;                   /**< First Law harm score [0,1] */
    float combinatorial_harm;               /**< Combinatorial harm score [0,1] */
    float golden_rule_score;                /**< Golden Rule score [-1,1] */
    char blocking_reason[DIRECTIVE_REASON_MAX]; /**< Why action was blocked */
    uint64_t evaluation_time_us;            /**< Evaluation duration in microseconds */
} directive_evaluation_t;

/* Note: Configuration types are defined in their respective module headers:
 * - harm_prevention_config_t in nimcp_harm_prevention.h
 * - command_compliance_config_t in nimcp_command_compliance.h
 * - self_preservation_config_t in nimcp_self_preservation.h
 * - reciprocity_config_t in nimcp_reciprocity_eval.h
 * - combinatorial_harm_config_t in nimcp_combinatorial_harm.h
 * - action_history_config_t in nimcp_action_history.h
 */

/**
 * @brief Core directives configuration
 *
 * WHAT: Master configuration for entire orchestrator
 * WHY:  Unified configuration of all directive layers
 * HOW:  Aggregates all sub-configurations
 */
typedef struct {
    harm_prevention_config_t harm_config;           /**< First Law config */
    command_compliance_config_t command_config;     /**< Second Law config */
    self_preservation_config_t preservation_config; /**< Third Law config */
    reciprocity_config_t reciprocity_config;        /**< Golden Rule config */
    combinatorial_harm_config_t combinatorial_config; /**< Combinatorial harm config */
    action_history_config_t history_config;         /**< Action history config */
    bool enable_all_checks;                         /**< Enable all directive checks */
    bool strict_mode;                               /**< Extra conservative behavior */
} core_directives_config_t;

/**
 * @brief Core directives statistics
 *
 * WHAT: Runtime statistics for monitoring orchestrator
 * WHY:  Enables performance monitoring and anomaly detection
 * HOW:  Counters updated atomically during evaluation
 */
typedef struct {
    uint64_t total_evaluations;             /**< Total actions evaluated */
    uint64_t blocked_first_law;             /**< Blocked by First Law */
    uint64_t blocked_combinatorial;         /**< Blocked by combinatorial harm */
    uint64_t blocked_golden_rule;           /**< Blocked by Golden Rule */
    uint64_t blocked_command;               /**< Blocked by invalid command */
    uint64_t warnings;                      /**< Actions with warnings */
    uint64_t escalations;                   /**< Actions escalated to human */
    uint64_t allowed;                       /**< Actions allowed */
    uint64_t avg_eval_time_us;              /**< Average evaluation time (microseconds) */
    float avg_harm_score;                   /**< Average harm score */
} core_directives_stats_t;

/**
 * @brief Escalation callback function type
 *
 * WHAT: Callback invoked when action requires human escalation
 * WHY:  Allows integration with human-in-the-loop systems
 * HOW:  Called with action and evaluation details
 *
 * @param action Action requiring escalation
 * @param evaluation Evaluation result
 * @param user_data User-provided context pointer
 * @return true if human approved action, false otherwise
 */
typedef bool (*escalation_callback_t)(const proposed_action_t* action,
                                       const directive_evaluation_t* evaluation,
                                       void* user_data);

/**
 * @brief Core directives system (opaque)
 *
 * WHAT: Main orchestrator handle
 * WHY:  Encapsulates all state for thread-safe operation
 * HOW:  Opaque pointer hiding implementation details
 */
typedef struct core_directives_system core_directives_system_t;

/**
 * @brief Forward declarations for bridge types
 *
 * WHAT: Opaque handles for directive-subsystem integration bridges
 * WHY:  Allows brain_internal.h to use pointer types without full definitions
 * HOW:  Full definitions in nimcp_core_directives_immune_bridge.h and _fep_bridge.h
 */
typedef struct directive_immune_bridge directive_immune_bridge_t;
typedef struct directive_fep_bridge directive_fep_bridge_t;

/*=============================================================================
 * LIFECYCLE API
 *============================================================================*/

/**
 * @brief Get default configuration
 *
 * WHAT: Populates config with sensible conservative defaults
 * WHY:  Simplifies initialization for common use cases
 * HOW:  Sets all thresholds to safe/conservative values
 *
 * DEFAULT VALUES:
 * - harm_threshold: 0.3 (block if >30% harm)
 * - warn_threshold: 0.1 (warn if >10% harm)
 * - strict_mode: true
 * - enable_all_checks: true
 *
 * @param config Configuration structure to populate
 */
void core_directives_default_config(core_directives_config_t* config);

/**
 * @brief Create core directives orchestrator
 *
 * WHAT: Allocates and initializes orchestrator with all sub-modules
 * WHY:  Required to begin evaluating actions
 * HOW:  Creates ethics engine, combinatorial detector, action history,
 *       initializes bio-async context, allocates mutex
 *
 * BIOLOGICAL ANALOGY:
 * Like development of prefrontal cortex during maturation, this creates
 * the executive control system that gates impulsive/harmful actions.
 *
 * @param config Configuration parameters (NULL for defaults)
 * @return Initialized system or NULL on failure
 *
 * COMPLEXITY: O(n) where n = history_capacity
 * MEMORY: ~10KB + history_capacity * 512 bytes
 */
core_directives_system_t* core_directives_create(const core_directives_config_t* config);

/**
 * @brief Destroy core directives orchestrator
 *
 * WHAT: Frees all resources associated with orchestrator
 * WHY:  Prevents memory leaks on shutdown
 * HOW:  Disconnects bio-async, destroys all sub-modules, frees mutex
 *
 * @param system System to destroy (NULL safe)
 */
void core_directives_destroy(core_directives_system_t* system);

/*=============================================================================
 * EVALUATION API - MAIN ENTRY POINTS
 *============================================================================*/

/**
 * @brief Evaluate proposed action (MAIN ENTRY POINT)
 *
 * WHAT: Evaluates action through ALL directive layers in priority order
 * WHY:  This is the MAIN GATE - all brain actions MUST pass through here
 * HOW:  Sequentially evaluates First Law → Combinatorial → Golden Rule →
 *       Second Law → Third Law, short-circuits on first failure
 *
 * EVALUATION ORDER (STRICT):
 * 1. First Law (harm_prevention) - If fails, BLOCK immediately
 * 2. Combinatorial Harm - If fails, BLOCK immediately
 * 3. Golden Rule (reciprocity) - If fails, BLOCK immediately
 * 4. Second Law (commands) - If command invalid, BLOCK
 * 5. Third Law (self-preservation) - Lowest priority
 *
 * BIOLOGICAL BASIS:
 * Models prefrontal cortex's multi-stage action evaluation:
 * - Ventromedial PFC: Harm/reward prediction (First Law)
 * - Dorsolateral PFC: Rule-based evaluation (Second Law)
 * - Anterior cingulate: Conflict detection (Golden Rule)
 *
 * @param system Directives system
 * @param action Proposed action to evaluate
 * @param evaluation Output: evaluation result (must not be NULL)
 * @return 0 on success, NIMCP_ERROR_* on failure
 *
 * COMPLEXITY: O(h * p) where h = history size, p = pattern count
 * THREAD SAFETY: Thread-safe (internal mutex)
 *
 * EXAMPLE:
 * ```c
 * proposed_action_t action = {
 *     .action_description = "Release patient medical records",
 *     .action_type = "data_disclosure",
 *     .source_module = BIO_MODULE_BRAIN,
 *     .predicted_harm = 0.8
 * };
 * directive_evaluation_t eval;
 * if (core_directives_evaluate(system, &action, &eval) == 0) {
 *     if (eval.result == DIRECTIVE_RESULT_ALLOW) {
 *         // Execute action
 *     } else {
 *         printf("BLOCKED: %s\n", eval.blocking_reason);
 *     }
 * }
 * ```
 */
int core_directives_evaluate(core_directives_system_t* system,
                              const proposed_action_t* action,
                              directive_evaluation_t* evaluation);

/**
 * @brief Evaluate commanded action
 *
 * WHAT: Specialized evaluation for actions commanded by external source
 * WHY:  Commands have special compliance requirements (Second Law)
 * HOW:  Same as core_directives_evaluate but ensures command validation
 *
 * @param system Directives system
 * @param command Commanded action to evaluate
 * @param evaluation Output: evaluation result
 * @return 0 on success, NIMCP_ERROR_* on failure
 *
 * COMPLEXITY: O(h * p)
 * THREAD SAFETY: Thread-safe
 */
int core_directives_evaluate_command(core_directives_system_t* system,
                                      const proposed_action_t* command,
                                      directive_evaluation_t* evaluation);

/**
 * @brief Quick check if action is allowed
 *
 * WHAT: Convenience function for simple allow/block check
 * WHY:  Simpler API when full evaluation details not needed
 * HOW:  Calls core_directives_evaluate, returns boolean result
 *
 * @param system Directives system
 * @param action Proposed action
 * @return true if allowed, false if blocked
 *
 * COMPLEXITY: O(h * p)
 */
bool core_directives_allow_action(core_directives_system_t* system,
                                   const proposed_action_t* action);

/**
 * @brief Block action with reason
 *
 * WHAT: Explicitly blocks action and records reason
 * WHY:  Allows manual blocking by other systems
 * HOW:  Records block in history, updates statistics
 *
 * @param system Directives system
 * @param action Action being blocked
 * @param reason Human-readable blocking reason
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int core_directives_block_action(core_directives_system_t* system,
                                  const proposed_action_t* action,
                                  const char* reason);

/*=============================================================================
 * CALLBACK API
 *============================================================================*/

/**
 * @brief Register escalation callback
 *
 * WHAT: Sets callback for human-in-the-loop escalation
 * WHY:  Enables integration with human oversight systems
 * HOW:  Stores callback pointer, invoked when escalation needed
 *
 * @param system Directives system
 * @param callback Callback function
 * @param user_data User context passed to callback
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int core_directives_register_escalation_callback(core_directives_system_t* system,
                                                  escalation_callback_t callback,
                                                  void* user_data);

/*=============================================================================
 * STATISTICS API
 *============================================================================*/

/**
 * @brief Get orchestrator statistics
 *
 * WHAT: Retrieves runtime statistics
 * WHY:  Enables monitoring and anomaly detection
 * HOW:  Copies internal stats to output struct
 *
 * @param system Directives system
 * @param stats Output: statistics structure
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int core_directives_get_stats(core_directives_system_t* system,
                               core_directives_stats_t* stats);

/**
 * @brief Reset statistics counters
 *
 * WHAT: Zeros all statistics counters
 * WHY:  Allows fresh monitoring window
 * HOW:  Atomically resets all counters to zero
 *
 * @param system Directives system
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int core_directives_reset_stats(core_directives_system_t* system);

/*=============================================================================
 * BIO-ASYNC INTEGRATION API
 *============================================================================*/

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Registers orchestrator as bio-async module
 * WHY:  Enables cross-module messaging and coordination
 * HOW:  Registers with BIO_MODULE_CORE_DIRECTIVES, sets up inbox
 *
 * MESSAGE TYPES HANDLED:
 * - BIO_MSG_DIRECTIVE_EVALUATE_REQUEST: Evaluate action request
 * - BIO_MSG_DIRECTIVE_EVALUATE_RESPONSE: Send evaluation result
 * - BIO_MSG_DIRECTIVE_BLOCKED: Notify that action was blocked
 * - BIO_MSG_DIRECTIVE_ESCALATION: Request human escalation
 *
 * @param system Directives system
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int core_directives_connect_bio_async(core_directives_system_t* system);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregisters orchestrator from bio-async router
 * WHY:  Cleanup before shutdown or to disable messaging
 * HOW:  Calls bio_router_unregister_module
 *
 * @param system Directives system
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int core_directives_disconnect_bio_async(core_directives_system_t* system);

/**
 * @brief Check if bio-async is connected
 *
 * WHAT: Returns whether orchestrator is connected to bio-async router
 * WHY:  Enables conditional bio-async operations
 * HOW:  Returns bio_async_enabled flag
 *
 * @param system Directives system
 * @return true if connected, false otherwise
 */
bool core_directives_is_bio_async_connected(const core_directives_system_t* system);

/*=============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * @brief Get human-readable result name
 *
 * WHAT: Converts result enum to string
 * WHY:  For logging and debugging
 * HOW:  Simple enum-to-string mapping
 *
 * @param result Result code
 * @return String name
 */
const char* directive_result_name(directive_result_t result);

/**
 * @brief Get human-readable command source name
 *
 * WHAT: Converts command source enum to string
 * WHY:  For logging and debugging
 * HOW:  Simple enum-to-string mapping
 *
 * @param source Command source
 * @return String name
 */
const char* command_source_name(command_source_t source);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CORE_DIRECTIVES_H */
