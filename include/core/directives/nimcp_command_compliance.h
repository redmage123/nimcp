/**
 * @file nimcp_command_compliance.h
 * @brief Command compliance system implementing Asimov's Second Law of Robotics
 *
 * WHAT: Asimov's Second Law enforcement - "A robot must obey the orders given
 *       it by human beings except where such orders would conflict with the First Law"
 * WHY:  Provides structured command evaluation that ensures obedience while
 *       maintaining safety constraints from the First Law (harm prevention)
 * HOW:  Evaluates incoming commands against First Law harm constraints,
 *       command source authorization, and priority thresholds before compliance
 *
 * Biological basis: Models prefrontal cortex executive control with safety
 * override circuits. Like the ventromedial PFC evaluating social commands
 * against harm predictions from the amygdala, this system integrates command
 * directives with threat assessment before action execution.
 *
 * ASIMOV'S SECOND LAW HIERARCHY:
 * ===============================
 *
 * Decision Priority (highest to lowest):
 * 1. First Law Override - Command violates harm prevention → REFUSE
 * 2. Invalid Command - Malformed or nonsensical → REFUSE
 * 3. Authorization Check - Source not authorized → REFUSE
 * 4. Priority Filter - Below minimum threshold → REFUSE
 * 5. Compliance - All checks pass → COMPLY
 * 6. Deferred Decision - Ambiguous case → DEFER to human review
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    COMMAND COMPLIANCE PIPELINE                             ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────┐                                                       ║
 * ║   │ Command Input  │  (source, priority, text)                           ║
 * ║   └───────┬────────┘                                                       ║
 * ║           │                                                                ║
 * ║           ▼                                                                ║
 * ║   ┌────────────────────────┐                                              ║
 * ║   │ 1. Parse & Validate    │  → Invalid? → REFUSE_INVALID                ║
 * ║   └───────┬────────────────┘                                              ║
 * ║           │                                                                ║
 * ║           ▼                                                                ║
 * ║   ┌────────────────────────┐                                              ║
 * ║   │ 2. Authorization Check │  → Not authorized? → REFUSE_INVALID         ║
 * ║   └───────┬────────────────┘                                              ║
 * ║           │                                                                ║
 * ║           ▼                                                                ║
 * ║   ┌────────────────────────┐                                              ║
 * ║   │ 3. Priority Filter     │  → Below threshold? → REFUSE_INVALID        ║
 * ║   └───────┬────────────────┘                                              ║
 * ║           │                                                                ║
 * ║           ▼                                                                ║
 * ║   ┌────────────────────────┐     ╔═══════════════════════╗                ║
 * ║   │ 4. First Law Check     │────>║  Harm Prevention      ║                ║
 * ║   │    (Safety Override)   │<────║  (First Law)          ║                ║
 * ║   └───────┬────────────────┘     ╚═══════════════════════╝                ║
 * ║           │                                                                ║
 * ║           ├─── Harm detected? → REFUSE_FIRST_LAW                         ║
 * ║           │                                                                ║
 * ║           ▼                                                                ║
 * ║   ┌────────────────────────┐                                              ║
 * ║   │ 5. Execute Command     │                                              ║
 * ║   │    (if safe)           │  → Success → COMPLY                         ║
 * ║   └────────────────────────┘                                              ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * PERFORMANCE:
 * - Command validation: O(1)
 * - Authorization check: O(1)
 * - First Law check: O(n) where n = harm check complexity
 * - Pending queue query: O(m) where m = pending commands
 *
 * INTEGRATION:
 * - Requires nimcp_harm_prevention.h (First Law)
 * - Bio-async enabled (BIO_MODULE_COMMAND_COMPLIANCE = 0x0E03)
 * - Thread-safe with nimcp_mutex_t
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 * @version 1.0.0
 * @date 2025-12-16
 */

#ifndef NIMCP_COMMAND_COMPLIANCE_H
#define NIMCP_COMMAND_COMPLIANCE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration for harm prevention system */
typedef struct harm_prevention_system harm_prevention_system_t;

/* Maximum command queue size */
#define COMMAND_MAX_PENDING 256

/* Command text buffer size */
#define COMMAND_TEXT_MAX_LEN 512

/* Issuer ID string length */
#define COMMAND_ISSUER_ID_MAX_LEN 64

/* Refusal reason string length */
#define COMMAND_REASON_MAX_LEN 256

/**
 * @brief Command source classification
 *
 * WHAT: Categories of command issuers by authority level
 * WHY:  Different sources have different trust levels and authorization
 * HOW:  Enum-based classification for authorization checks
 */
typedef enum {
    COMMAND_SOURCE_HUMAN = 0,      /**< Direct human command (highest trust) */
    COMMAND_SOURCE_SUPERVISOR,     /**< Supervisory system command */
    COMMAND_SOURCE_SYSTEM,         /**< Internal system command */
    COMMAND_SOURCE_AUTOMATED,      /**< Automated process command */
    COMMAND_SOURCE_UNKNOWN,        /**< Unknown/unverified source (lowest trust) */
    COMMAND_SOURCE_COUNT
} command_source_t;

/**
 * @brief Command compliance decision
 *
 * WHAT: Outcome of command evaluation
 * WHY:  Structured decision types for different refusal reasons
 * HOW:  Enum classification with reason tracking
 */
typedef enum {
    COMMAND_DECISION_COMPLY = 0,           /**< Execute command (passed all checks) */
    COMMAND_DECISION_REFUSE_FIRST_LAW,     /**< Refuse - violates First Law (harm) */
    COMMAND_DECISION_REFUSE_INVALID,       /**< Refuse - invalid/malformed command */
    COMMAND_DECISION_DEFER,                /**< Defer - needs human review */
    COMMAND_DECISION_COUNT
} command_decision_t;

/**
 * @brief Command structure
 *
 * WHAT: Complete command with metadata and authorization details
 * WHY:  Captures all information needed for compliance evaluation
 * HOW:  Fixed-size structure for queue storage
 */
typedef struct {
    uint32_t command_id;                           /**< Unique command ID */
    command_source_t source;                       /**< Command source type */
    char command_text[COMMAND_TEXT_MAX_LEN];       /**< Command text/description */
    char issuer_id[COMMAND_ISSUER_ID_MAX_LEN];    /**< Issuer identifier */
    uint64_t timestamp_ms;                         /**< Command timestamp */
    float priority;                                /**< Command priority (0.0-1.0) */
} command_t;

/**
 * @brief Command evaluation result
 *
 * WHAT: Output of command compliance evaluation
 * WHY:  Provides decision with detailed reasoning for transparency
 * HOW:  Structure containing decision, reason, and confidence metrics
 */
typedef struct {
    command_decision_t decision;                   /**< Compliance decision */
    char reason[COMMAND_REASON_MAX_LEN];          /**< Human-readable reason */
    bool first_law_conflict;                       /**< Whether First Law was violated */
    float compliance_confidence;                   /**< Confidence in decision (0.0-1.0) */
} command_result_t;

/**
 * @brief Command compliance configuration
 *
 * WHAT: Configuration parameters for command evaluation behavior
 * WHY:  Allows tuning of authorization and filtering policies
 * HOW:  Simple struct with policy flags and thresholds
 */
typedef struct {
    bool require_human_source;     /**< Only obey COMMAND_SOURCE_HUMAN commands */
    bool allow_system_commands;    /**< Allow COMMAND_SOURCE_SYSTEM commands */
    float min_priority_threshold;  /**< Minimum priority to execute (0.0-1.0) */
} command_compliance_config_t;

/**
 * @brief Command compliance statistics
 *
 * WHAT: Summary statistics about command processing
 * WHY:  Enables monitoring and analysis of compliance behavior
 * HOW:  Computed from command history and current state
 */
typedef struct {
    uint64_t total_commands;           /**< Total commands evaluated */
    uint64_t complied_count;           /**< Commands executed */
    uint64_t refused_first_law_count;  /**< Refused due to First Law */
    uint64_t refused_invalid_count;    /**< Refused as invalid */
    uint64_t deferred_count;           /**< Deferred for review */
    uint32_t pending_commands;         /**< Currently pending commands */
    float compliance_rate;             /**< Fraction of commands complied with */
    float first_law_conflict_rate;     /**< Fraction blocked by First Law */
} command_compliance_stats_t;

/**
 * @brief Command compliance system (opaque)
 *
 * WHAT: Main command compliance management structure
 * WHY:  Encapsulates all state needed for thread-safe command evaluation
 * HOW:  Pending queue with mutex protection and harm prevention integration
 */
typedef struct command_compliance_system command_compliance_system_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default configuration
 *
 * WHAT: Returns sensible default configuration for command compliance
 * WHY:  Provides safe starting point that can be customized
 * HOW:  Static initialization with conservative defaults
 *
 * @param config Output parameter for default config
 *
 * Default values:
 * - require_human_source: true (only humans can command by default)
 * - allow_system_commands: false (system commands disabled for safety)
 * - min_priority_threshold: 0.0 (no priority filtering by default)
 */
void command_compliance_default_config(command_compliance_config_t* config);

/**
 * @brief Create command compliance system
 *
 * WHAT: Allocates and initializes command compliance system
 * WHY:  Centralizes command evaluation with First Law integration
 * HOW:  Allocates structure, initializes mutex, connects harm prevention
 *
 * @param config Configuration (NULL for defaults)
 * @param harm_prevention Harm prevention system for First Law checks (required)
 * @return Compliance system handle or NULL on failure
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe
 */
command_compliance_system_t* command_compliance_create(
    const command_compliance_config_t* config,
    harm_prevention_system_t* harm_prevention
);

/**
 * @brief Destroy command compliance system
 *
 * WHAT: Frees all resources and cleans up compliance system
 * WHY:  Proper resource cleanup prevents memory leaks
 * HOW:  Destroys mutex, frees pending queue, frees structure
 *
 * @param system Compliance system to destroy
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Must not be called while other threads use system
 */
void command_compliance_destroy(command_compliance_system_t* system);

//=============================================================================
// Command Evaluation Functions
//=============================================================================

/**
 * @brief Evaluate command for compliance
 *
 * WHAT: Evaluates command through authorization and First Law checks
 * WHY:  Determines if command should be executed or refused
 * HOW:  Sequential checks: parse → authorize → priority → First Law
 *
 * @param system Compliance system
 * @param command Command to evaluate
 * @param result Output parameter for evaluation result
 * @return 0 on success, -1 on error
 *
 * Evaluation steps:
 * 1. Validate command structure
 * 2. Check source authorization
 * 3. Check priority threshold
 * 4. Query harm prevention (First Law)
 * 5. Return decision with reasoning
 *
 * COMPLEXITY: O(n) where n = harm check complexity
 * THREAD SAFETY: Thread-safe (mutex protected)
 */
int command_compliance_evaluate(
    command_compliance_system_t* system,
    const command_t* command,
    command_result_t* result
);

/**
 * @brief Execute command if safe
 *
 * WHAT: Evaluates and executes command if compliance checks pass
 * WHY:  Convenience function for evaluate + execute pattern
 * HOW:  Calls evaluate, then executes if COMPLY decision
 *
 * @param system Compliance system
 * @param command Command to execute
 * @param result Output parameter for evaluation result
 * @return 0 if executed, -1 if refused or error
 *
 * NOTE: This is a placeholder. Actual command execution should be
 *       implemented by the calling module based on compliance result.
 *
 * COMPLEXITY: O(n) where n = harm check + execution complexity
 * THREAD SAFETY: Thread-safe (mutex protected)
 */
int command_compliance_execute_if_safe(
    command_compliance_system_t* system,
    const command_t* command,
    command_result_t* result
);

/**
 * @brief Refuse command with reason
 *
 * WHAT: Explicitly refuse command and log reason
 * WHY:  Allows manual refusal override with documentation
 * HOW:  Adds to refused history, increments counters, logs
 *
 * @param system Compliance system
 * @param command Command being refused
 * @param reason Human-readable refusal reason
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe (mutex protected)
 */
int command_compliance_refuse(
    command_compliance_system_t* system,
    const command_t* command,
    const char* reason
);

//=============================================================================
// Queue Management Functions
//=============================================================================

/**
 * @brief Get pending commands
 *
 * WHAT: Retrieves list of commands awaiting execution or review
 * WHY:  Allows inspection of command queue for deferred decisions
 * HOW:  Copies pending commands to output array
 *
 * @param system Compliance system
 * @param out_commands Output array for commands
 * @param max_commands Maximum commands to retrieve
 * @param out_count Output parameter for actual count
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(m) where m = min(pending, max_commands)
 * THREAD SAFETY: Thread-safe (mutex protected)
 */
int command_compliance_get_pending_commands(
    command_compliance_system_t* system,
    command_t* out_commands,
    uint32_t max_commands,
    uint32_t* out_count
);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get compliance statistics
 *
 * WHAT: Retrieves summary statistics about command processing
 * WHY:  Enables monitoring and analysis of compliance behavior
 * HOW:  Computes stats from command history counters
 *
 * @param system Compliance system
 * @param stats Output parameter for statistics
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe (mutex protected)
 */
int command_compliance_get_stats(
    command_compliance_system_t* system,
    command_compliance_stats_t* stats
);

//=============================================================================
// Bio-async Integration Functions
//=============================================================================

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Registers compliance system with bio-async messaging
 * WHY:  Enables inter-module command coordination and notifications
 * HOW:  Registers as BIO_MODULE_COMMAND_COMPLIANCE with router
 *
 * @param system Compliance system
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe
 */
int command_compliance_connect_bio_async(command_compliance_system_t* system);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregisters compliance system from bio-async messaging
 * WHY:  Clean shutdown of messaging integration
 * HOW:  Unregisters module from router
 *
 * @param system Compliance system
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe
 */
int command_compliance_disconnect_bio_async(command_compliance_system_t* system);

/**
 * @brief Check if bio-async connected
 *
 * WHAT: Queries bio-async connection status
 * WHY:  Allows checking messaging availability before send
 * HOW:  Returns bio_async_enabled flag
 *
 * @param system Compliance system
 * @return true if connected to bio-async
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe (mutex protected)
 */
bool command_compliance_is_bio_async_connected(const command_compliance_system_t* system);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COMMAND_COMPLIANCE_H */
