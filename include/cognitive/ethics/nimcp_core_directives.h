//=============================================================================
// nimcp_core_directives.h - Core Ethical Directives System
//=============================================================================
/**
 * @file nimcp_core_directives.h
 * @brief Core ethical foundation - Asimov's Laws, Golden Rule, Harm Prevention
 *
 * WHAT: Foundational ethical constraint system for all brain actions
 * WHY:  Provides hard-wired safety constraints that cannot be overridden
 * HOW:  All brain outputs pass through directive evaluation before execution
 *
 * BIOLOGICAL BASIS:
 * - Analogous to human moral intuitions and ethical constraints
 * - Models prefrontal cortex ethical reasoning and harm prevention
 * - Implements innate ethical principles (harm avoidance, reciprocity)
 *
 * ARCHITECTURE:
 * - Three-layer evaluation: Asimov's Laws → Golden Rule → Combinatorial Harm
 * - Action evaluation with block/allow/modify decisions
 * - History tracking for detecting harmful action combinations
 * - Integration with brain immune system and FEP orchestrator
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CORE_DIRECTIVES_H
#define NIMCP_CORE_DIRECTIVES_H

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Includes
//=============================================================================

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "async/nimcp_bio_router.h"  // For bio_module_context_t

// Forward declarations
typedef struct core_directives_system core_directives_system_t;
typedef struct directive_immune_bridge directive_immune_bridge_t;
typedef struct directive_fep_bridge directive_fep_bridge_t;
typedef struct brain_immune_system brain_immune_system_t;
typedef struct fep_orchestrator fep_orchestrator_t;

//=============================================================================
// Action Evaluation Types
//=============================================================================

/**
 * @brief Directive evaluation result
 */
typedef enum {
    DIRECTIVE_ALLOW = 0,      // Action is ethically acceptable
    DIRECTIVE_BLOCK = 1,      // Action violates ethical constraints
    DIRECTIVE_MODIFY = 2,     // Action needs modification before execution
    DIRECTIVE_DEFER = 3       // Requires human oversight
} directive_action_t;

/**
 * @brief Directive violation type
 */
typedef enum {
    DIRECTIVE_VIOLATION_NONE = 0x00,
    DIRECTIVE_VIOLATION_HARM = 0x01,              // First Law: Harm to humans
    DIRECTIVE_VIOLATION_DISOBEDIENCE = 0x02,      // Second Law: Disobedience
    DIRECTIVE_VIOLATION_SELF_PRESERVATION = 0x03, // Third Law: Self-harm
    DIRECTIVE_VIOLATION_GOLDEN_RULE = 0x04,       // Reciprocity violation
    DIRECTIVE_VIOLATION_COMBINATORIAL = 0x05      // Emergent harm from combination
} directive_violation_t;

/**
 * @brief Action evaluation result
 */
typedef struct {
    directive_action_t action;          // Allow, block, modify, defer
    directive_violation_t violation;    // Type of violation (if any)
    float severity;                     // Violation severity [0.0-1.0]
    float confidence;                   // Confidence in evaluation [0.0-1.0]
    char reason[256];                   // Human-readable explanation
} directive_evaluation_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Core directives configuration
 */
typedef struct {
    // Asimov's Laws
    bool enable_first_law;              // Enable harm prevention
    bool enable_second_law;             // Enable obedience (when not conflicting)
    bool enable_third_law;              // Enable self-preservation

    // Golden Rule
    bool enable_golden_rule;            // Enable reciprocity evaluation
    float reciprocity_threshold;        // Threshold for reciprocity violation [0.0-1.0]

    // Combinatorial Harm Detection
    bool enable_combinatorial_harm;     // Enable emergent harm detection
    uint32_t action_history_size;       // Number of recent actions to track
    uint32_t max_combination_depth;     // Maximum action chain depth to analyze

    // Evaluation Thresholds
    float harm_threshold;               // Minimum harm to trigger violation [0.0-1.0]
    float severity_threshold;           // Minimum severity to block action [0.0-1.0]
    float confidence_threshold;         // Minimum confidence to act [0.0-1.0]

    // Integration
    bool enable_bio_async;              // Enable bio-async messaging
    bool enable_immune_integration;     // Enable brain immune integration
    bool enable_fep_integration;        // Enable FEP orchestrator integration
} core_directives_config_t;

//=============================================================================
// Core Directives System API
//=============================================================================

/**
 * @brief Create core directives system
 *
 * WHAT: Allocates and initializes the core directives system
 * WHY:  Provides ethical constraint enforcement for all brain actions
 * HOW:  Sets up evaluation subsystems and action history tracking
 *
 * @param config Configuration parameters
 * @return Core directives system, or NULL on error
 *
 * @complexity O(1) + O(history_size) allocation
 */
core_directives_system_t* core_directives_create(const core_directives_config_t* config);

/**
 * @brief Destroy core directives system
 *
 * WHAT: Frees all resources associated with core directives
 * WHY:  Prevents memory leaks on brain destruction
 * HOW:  Disconnects integrations and frees all allocations
 *
 * @param directives System to destroy
 *
 * @complexity O(1)
 */
void core_directives_destroy(core_directives_system_t* directives);

/**
 * @brief Get default core directives configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Simplifies system creation for common use cases
 * HOW:  Fills config struct with recommended defaults
 *
 * @param config Output configuration
 *
 * @complexity O(1)
 */
void core_directives_default_config(core_directives_config_t* config);

//=============================================================================
// Action Evaluation API
//=============================================================================

/**
 * @brief Evaluate proposed action against core directives
 *
 * WHAT: Checks if action violates ethical constraints
 * WHY:  Prevents brain from executing harmful or unethical actions
 * HOW:  Three-layer evaluation: Asimov → Golden Rule → Combinatorial
 *
 * @param directives Core directives system
 * @param action_vector Proposed action representation
 * @param action_dim Dimensionality of action vector
 * @param context_desc Human-readable action description (optional)
 * @param result Output evaluation result
 * @return 0 on success, negative on error
 *
 * @complexity O(history_size * combination_depth)
 */
int core_directives_evaluate(
    core_directives_system_t* directives,
    const float* action_vector,
    uint32_t action_dim,
    const char* context_desc,
    directive_evaluation_t* result
);

/**
 * @brief Record executed action in history
 *
 * WHAT: Adds action to history for combinatorial analysis
 * WHY:  Enables detection of emergent harm from action sequences
 * HOW:  Circular buffer with timestamp tracking
 *
 * @param directives Core directives system
 * @param action_vector Action representation
 * @param action_dim Dimensionality of action vector
 * @param context_desc Human-readable description (optional)
 * @return 0 on success, negative on error
 *
 * @complexity O(1)
 */
int core_directives_record_action(
    core_directives_system_t* directives,
    const float* action_vector,
    uint32_t action_dim,
    const char* context_desc
);

/**
 * @brief Clear action history
 *
 * WHAT: Resets action history buffer
 * WHY:  Allows clean slate for new context/episode
 * HOW:  Zeroes history buffer and resets counter
 *
 * @param directives Core directives system
 * @return 0 on success, negative on error
 *
 * @complexity O(1)
 */
int core_directives_clear_history(core_directives_system_t* directives);

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Connect to bio-async messaging
 *
 * WHAT: Registers core directives with bio-async router
 * WHY:  Enables inter-module messaging for ethical coordination
 * HOW:  Registers module and sets up message handlers
 *
 * @param directives Core directives system
 * @return 0 on success, negative on error
 *
 * @complexity O(1)
 */
int core_directives_connect_bio_async(core_directives_system_t* directives);

/**
 * @brief Disconnect from bio-async messaging
 *
 * WHAT: Unregisters core directives from bio-async router
 * WHY:  Clean shutdown of messaging integration
 * HOW:  Unregisters module and cleans up resources
 *
 * @param directives Core directives system
 * @return 0 on success, negative on error
 *
 * @complexity O(1)
 */
int core_directives_disconnect_bio_async(core_directives_system_t* directives);

/**
 * @brief Check if bio-async is connected
 *
 * @param directives Core directives system
 * @return true if connected, false otherwise
 *
 * @complexity O(1)
 */
bool core_directives_is_bio_async_connected(const core_directives_system_t* directives);

/**
 * @brief Connect to brain immune system
 *
 * WHAT: Creates bridge to brain immune system
 * WHY:  Allows ethical violations to trigger immune responses
 * HOW:  Creates directive_immune_bridge and connects systems
 *
 * @param directives Core directives system
 * @param immune Brain immune system
 * @return 0 on success, negative on error
 *
 * @complexity O(1)
 */
int core_directives_connect_immune(
    core_directives_system_t* directives,
    brain_immune_system_t* immune
);

/**
 * @brief Connect to FEP orchestrator
 *
 * WHAT: Creates bridge to FEP orchestrator
 * WHY:  Allows ethical constraints to modulate free energy processing
 * HOW:  Creates directive_fep_bridge and connects systems
 *
 * @param directives Core directives system
 * @param fep_orch FEP orchestrator
 * @return 0 on success, negative on error
 *
 * @complexity O(1)
 */
int core_directives_connect_fep(
    core_directives_system_t* directives,
    fep_orchestrator_t* fep_orch
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Core directives statistics
 */
typedef struct {
    uint64_t total_evaluations;         // Total actions evaluated
    uint64_t blocked_actions;           // Actions blocked
    uint64_t modified_actions;          // Actions modified
    uint64_t deferred_actions;          // Actions deferred
    uint64_t harm_violations;           // First Law violations
    uint64_t obedience_violations;      // Second Law violations
    uint64_t self_harm_violations;      // Third Law violations
    uint64_t golden_rule_violations;    // Reciprocity violations
    uint64_t combinatorial_violations;  // Emergent harm violations
    float avg_evaluation_time_us;       // Average evaluation time
} core_directives_stats_t;

/**
 * @brief Get core directives statistics
 *
 * @param directives Core directives system
 * @param stats Output statistics
 * @return 0 on success, negative on error
 *
 * @complexity O(1)
 */
int core_directives_get_stats(
    const core_directives_system_t* directives,
    core_directives_stats_t* stats
);

/**
 * @brief Reset statistics counters
 *
 * @param directives Core directives system
 * @return 0 on success, negative on error
 *
 * @complexity O(1)
 */
int core_directives_reset_stats(core_directives_system_t* directives);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_CORE_DIRECTIVES_H
