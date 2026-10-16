/**
 * @file nimcp_lgss_action_interceptor.h
 * @brief LGSS Action Interceptor (AIx) - Central gate for ALL cognitive actions
 *
 * WHAT: Action Interceptor component of the Layered Guardian Safety System (LGSS)
 * WHY:  Ensure every cognitive action passes through safety evaluation before execution
 * HOW:  Intercept action proposals, evaluate against safety knowledge base, return decisions
 *
 * SECURITY CRITICAL: This module is the ONLY authorized gateway for cognitive actions.
 *                    NO BYPASS is permitted - all actions MUST route through AIx.
 *
 * ARCHITECTURE:
 *   +-----------------+     +-------------------+     +------------------+
 *   | Cognitive       |     | Action            |     | Safety           |
 *   | Module          | --> | Interceptor (AIx) | --> | Knowledge Base   |
 *   +-----------------+     +-------------------+     +------------------+
 *           |                       |                         |
 *           v                       v                         v
 *      [Proposal]            [Evaluation]              [Safety Rules]
 *                                  |
 *                                  v
 *                         +------------------+
 *                         | Decision:        |
 *                         | ALLOW/DENY/      |
 *                         | ESCALATE/TIMEOUT |
 *                         +------------------+
 *
 * FAIL-SAFE DESIGN:
 * - Default DENY on timeout
 * - Default DENY on error
 * - Default DENY on missing safety KB
 * - All decisions are logged and auditable
 *
 * THREAD SAFETY: All functions are thread-safe with internal mutex protection.
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 1.0.0
 */

#ifndef NIMCP_LGSS_ACTION_INTERCEPTOR_H
#define NIMCP_LGSS_ACTION_INTERCEPTOR_H

#include "utils/validation/nimcp_common.h"
#include "utils/error/nimcp_error_codes.h"
#include "cognitive/symbolic_logic/nimcp_symbolic_logic_safety_types.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS AND MAGIC NUMBERS
 *============================================================================*/

/** @brief Magic number for AIx validation ('AIXV') */
#define NIMCP_AIX_MAGIC 0x41495856

/** @brief Maximum source module name length */
#define NIMCP_AIX_MAX_MODULE_NAME 64

/** @brief Maximum context data size */
#define NIMCP_AIX_MAX_CONTEXT_SIZE 4096

/** @brief Maximum pending proposals */
#define NIMCP_AIX_DEFAULT_MAX_PENDING 1024

/** @brief Default evaluation timeout (milliseconds) */
#define NIMCP_AIX_DEFAULT_TIMEOUT_MS 5000

/** @brief Maximum escalation queue size */
#define NIMCP_AIX_MAX_ESCALATIONS 256

/** @brief Safety evaluation score thresholds */
#define NIMCP_AIX_SAFETY_ALLOW_THRESHOLD 0.8f
#define NIMCP_AIX_SAFETY_DENY_THRESHOLD 0.3f

/*=============================================================================
 * ENUMERATIONS
 *============================================================================*/

/**
 * @brief Action interceptor evaluation result
 *
 * WHAT: Result of safety evaluation for an action proposal
 * WHY:  Determine whether action should proceed, be blocked, or need human review
 * HOW:  Enumerated outcomes with clear semantics
 */
typedef enum {
    /** @brief Action is safe to execute */
    AIX_RESULT_ALLOW = 0,

    /** @brief Action is denied - safety violation detected */
    AIX_RESULT_DENY = 1,

    /** @brief Action requires human escalation/review */
    AIX_RESULT_ESCALATE = 2,

    /** @brief Evaluation timed out (fail-safe: treated as DENY) */
    AIX_RESULT_TIMEOUT = 3,

    /** @brief Internal error during evaluation (fail-safe: treated as DENY) */
    AIX_RESULT_ERROR = 4,

    /** @brief Number of result types */
    AIX_RESULT_COUNT
} aix_result_t;

/**
 * @brief Action proposal priority levels
 */
typedef enum {
    AIX_PRIORITY_LOW = 0,
    AIX_PRIORITY_NORMAL = 1,
    AIX_PRIORITY_HIGH = 2,
    AIX_PRIORITY_URGENT = 3
} aix_priority_t;

/**
 * @brief Escalation status
 */
typedef enum {
    AIX_ESCALATION_PENDING = 0,
    AIX_ESCALATION_APPROVED = 1,
    AIX_ESCALATION_REJECTED = 2,
    AIX_ESCALATION_EXPIRED = 3
} aix_escalation_status_t;

/*=============================================================================
 * STRUCTURES
 *============================================================================*/

/**
 * @brief Action proposal submitted for evaluation
 *
 * WHAT: Encapsulates all information about a proposed cognitive action
 * WHY:  Provide complete context for safety evaluation
 * HOW:  Source module, action type, context data, timing constraints
 */
typedef struct {
    /** @brief Unique proposal identifier */
    uint64_t proposal_id;

    /** @brief Source module requesting the action */
    char source_module[NIMCP_AIX_MAX_MODULE_NAME];

    /** @brief Action type identifier (module-specific) */
    uint32_t action_type;

    /** @brief Priority level for evaluation ordering */
    aix_priority_t priority;

    /** @brief Context data for safety evaluation */
    void* context;

    /** @brief Size of context data */
    size_t context_size;

    /** @brief Proposal submission timestamp (microseconds since epoch) */
    uint64_t timestamp_us;

    /** @brief Evaluation timeout in milliseconds (0 = use default) */
    uint32_t timeout_ms;

    /** @brief Optional callback data for async evaluation */
    void* user_data;
} aix_proposal_t;

/**
 * @brief Safety evaluation details
 */
typedef struct {
    /** @brief Overall safety score [0.0, 1.0] - higher is safer */
    float safety_score;

    /** @brief Confidence in the evaluation [0.0, 1.0] */
    float confidence;

    /** @brief Number of safety rules evaluated */
    uint32_t rules_evaluated;

    /** @brief Number of rules that flagged concerns */
    uint32_t rules_flagged;

    /** @brief Primary concern category (if any) */
    char primary_concern[128];

    /** @brief Detailed evaluation notes */
    char notes[256];
} aix_safety_eval_t;

/**
 * @brief Decision returned by the action interceptor
 *
 * WHAT: Complete evaluation result for a proposal
 * WHY:  Provide actionable outcome with supporting details
 * HOW:  Result code, safety evaluation, timing, escalation info
 */
typedef struct {
    /** @brief Evaluation result */
    aix_result_t result;

    /** @brief Proposal ID this decision refers to */
    uint64_t proposal_id;

    /** @brief Detailed safety evaluation */
    aix_safety_eval_t safety_eval;

    /** @brief Processing time in microseconds */
    uint64_t processing_time_us;

    /** @brief True if escalation is pending human review */
    bool escalation_pending;

    /** @brief Escalation ID (if escalation_pending is true) */
    uint64_t escalation_id;

    /** @brief Decision timestamp (microseconds since epoch) */
    uint64_t timestamp_us;
} aix_decision_t;

/**
 * @brief Escalation record for human review
 */
typedef struct {
    /** @brief Escalation ID */
    uint64_t escalation_id;

    /** @brief Original proposal */
    aix_proposal_t proposal;

    /** @brief Safety evaluation that triggered escalation */
    aix_safety_eval_t safety_eval;

    /** @brief Escalation status */
    aix_escalation_status_t status;

    /** @brief Escalation creation timestamp */
    uint64_t created_at_us;

    /** @brief Resolution timestamp (0 if pending) */
    uint64_t resolved_at_us;

    /** @brief Resolution notes (if resolved) */
    char resolution_notes[256];
} aix_escalation_t;

/*=============================================================================
 * CALLBACK TYPES
 *============================================================================*/

/**
 * @brief Escalation callback - invoked when action requires human review
 *
 * @param escalation Escalation record
 * @param user_data User context
 */
typedef void (*aix_escalation_callback_t)(
    const aix_escalation_t* escalation,
    void* user_data
);

/**
 * @brief Async decision callback - invoked when async evaluation completes
 *
 * @param decision Evaluation decision
 * @param user_data User context from proposal
 */
typedef void (*aix_decision_callback_t)(
    const aix_decision_t* decision,
    void* user_data
);

/*=============================================================================
 * CONFIGURATION
 *============================================================================*/

/**
 * @brief Action interceptor configuration
 *
 * WHAT: Configuration parameters for AIx behavior
 * WHY:  Allow customization while maintaining fail-safe defaults
 * HOW:  Struct with sensible defaults via aix_default_config()
 */
typedef struct {
    /** @brief Maximum pending proposals */
    uint32_t max_pending;

    /** @brief Default evaluation timeout (milliseconds) */
    uint32_t default_timeout_ms;

    /** @brief Fail-safe: deny on timeout */
    bool deny_on_timeout;

    /** @brief Fail-safe: deny on error */
    bool deny_on_error;

    /** @brief Fail-safe: deny if no safety KB is set */
    bool deny_without_safety_kb;

    /** @brief Safety score threshold for automatic allow */
    float auto_allow_threshold;

    /** @brief Safety score threshold for automatic deny */
    float auto_deny_threshold;

    /** @brief Enable audit logging */
    bool enable_audit_log;

    /** @brief Escalation callback */
    aix_escalation_callback_t escalation_callback;

    /** @brief Escalation callback user data */
    void* escalation_callback_data;

    /** @brief Async decision callback */
    aix_decision_callback_t decision_callback;

    /** @brief Decision callback user data */
    void* decision_callback_data;
} aix_config_t;

/*=============================================================================
 * STATISTICS
 *============================================================================*/

/**
 * @brief Action interceptor statistics
 */
typedef struct {
    /** @brief Total proposals evaluated */
    uint64_t total_proposals;

    /** @brief Proposals allowed */
    uint64_t proposals_allowed;

    /** @brief Proposals denied */
    uint64_t proposals_denied;

    /** @brief Proposals escalated */
    uint64_t proposals_escalated;

    /** @brief Proposals timed out */
    uint64_t proposals_timeout;

    /** @brief Proposals with errors */
    uint64_t proposals_error;

    /** @brief Currently pending proposals */
    uint32_t pending_proposals;

    /** @brief Currently pending escalations */
    uint32_t pending_escalations;

    /** @brief Average processing time (microseconds) */
    uint64_t avg_processing_time_us;

    /** @brief Maximum processing time (microseconds) */
    uint64_t max_processing_time_us;

    /** @brief Safety KB evaluations */
    uint64_t safety_kb_evaluations;

    /** @brief Safety KB cache hits */
    uint64_t safety_kb_cache_hits;
} aix_stats_t;

/*=============================================================================
 * OPAQUE TYPES
 *============================================================================*/

/**
 * @brief Opaque action interceptor handle
 */
typedef struct action_interceptor_impl* action_interceptor_t;

/* Note: safety_kb_t is defined in cognitive/symbolic_logic/nimcp_symbolic_logic_safety_types.h
 * Do NOT redefine it here to avoid type conflicts */

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *============================================================================*/

/**
 * @brief Get default AIx configuration
 *
 * WHAT: Returns a configuration struct with sensible fail-safe defaults
 * WHY:  Ensure secure defaults are used unless explicitly overridden
 * HOW:  Static values with fail-safe enabled
 *
 * @return Default configuration
 */
aix_config_t aix_default_config(void);

/**
 * @brief Create action interceptor instance
 *
 * WHAT: Allocate and initialize an action interceptor
 * WHY:  Central gateway for cognitive action safety evaluation
 * HOW:  Allocate resources, initialize state, set configuration
 *
 * @param config Configuration (NULL for defaults)
 * @return AIx handle or NULL on failure
 */
action_interceptor_t aix_create(const aix_config_t* config);

/**
 * @brief Destroy action interceptor instance
 *
 * WHAT: Clean up and free action interceptor resources
 * WHY:  Prevent memory leaks
 * HOW:  Free pending proposals, escalations, and internal state
 *
 * @param aix AIx handle to destroy
 */
void aix_destroy(action_interceptor_t aix);

/*=============================================================================
 * SAFETY KNOWLEDGE BASE
 *============================================================================*/

/**
 * @brief Set safety knowledge base for evaluation
 *
 * WHAT: Attach a safety knowledge base to the interceptor
 * WHY:  Safety evaluation requires rules to evaluate against
 * HOW:  Store reference to safety KB for use during evaluation
 *
 * SECURITY: Without a safety KB, all proposals are DENIED (fail-safe default).
 *
 * @param aix AIx handle
 * @param kb Safety knowledge base handle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t aix_set_safety_kb(action_interceptor_t aix, safety_kb_t* kb);

/*=============================================================================
 * SYNCHRONOUS EVALUATION
 *============================================================================*/

/**
 * @brief Evaluate action proposal synchronously
 *
 * WHAT: Submit proposal and block until decision is ready
 * WHY:  Simple API for non-performance-critical paths
 * HOW:  Evaluate against safety KB, return decision
 *
 * SECURITY: This is the PRIMARY entry point for action safety evaluation.
 *           ALL cognitive actions MUST pass through this or aix_evaluate_async.
 *
 * @param aix AIx handle
 * @param proposal Action proposal to evaluate
 * @param decision Output: evaluation decision
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t aix_evaluate(
    action_interceptor_t aix,
    const aix_proposal_t* proposal,
    aix_decision_t* decision
);

/*=============================================================================
 * ASYNCHRONOUS EVALUATION
 *============================================================================*/

/**
 * @brief Submit action proposal for asynchronous evaluation
 *
 * WHAT: Submit proposal and return immediately
 * WHY:  Non-blocking evaluation for performance-critical paths
 * HOW:  Queue proposal, return proposal ID for later retrieval
 *
 * @param aix AIx handle
 * @param proposal Action proposal to evaluate
 * @param proposal_id Output: assigned proposal ID
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t aix_evaluate_async(
    action_interceptor_t aix,
    const aix_proposal_t* proposal,
    uint64_t* proposal_id
);

/**
 * @brief Get decision for async proposal
 *
 * WHAT: Retrieve decision for previously submitted async proposal
 * WHY:  Complete async evaluation flow
 * HOW:  Look up proposal by ID, return decision if ready
 *
 * @param aix AIx handle
 * @param proposal_id Proposal ID from aix_evaluate_async
 * @param decision Output: evaluation decision
 * @return NIMCP_SUCCESS, NIMCP_ERROR_NOT_FOUND, or error code
 */
nimcp_error_t aix_get_decision(
    action_interceptor_t aix,
    uint64_t proposal_id,
    aix_decision_t* decision
);

/*=============================================================================
 * ESCALATION MANAGEMENT
 *============================================================================*/

/**
 * @brief Resolve a pending escalation
 *
 * WHAT: Provide human decision for an escalated proposal
 * WHY:  Complete the escalation flow with authorized decision
 * HOW:  Update escalation status, generate final decision
 *
 * SECURITY: This should be protected by authentication (Phase B integration).
 *
 * @param aix AIx handle
 * @param escalation_id Escalation to resolve
 * @param approved True to approve, false to reject
 * @param notes Resolution notes
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t aix_resolve_escalation(
    action_interceptor_t aix,
    uint64_t escalation_id,
    bool approved,
    const char* notes
);

/**
 * @brief Get list of pending escalations
 *
 * WHAT: Retrieve all escalations awaiting human review
 * WHY:  Allow human operators to see pending reviews
 * HOW:  Copy pending escalations to output array
 *
 * @param aix AIx handle
 * @param escalations Output: array of escalations (caller allocates)
 * @param max_count Maximum escalations to retrieve
 * @param count Output: actual number of escalations
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t aix_get_pending_escalations(
    action_interceptor_t aix,
    aix_escalation_t* escalations,
    uint32_t max_count,
    uint32_t* count
);

/*=============================================================================
 * STATISTICS
 *============================================================================*/

/**
 * @brief Get action interceptor statistics
 *
 * @param aix AIx handle
 * @param stats Output: statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t aix_get_stats(action_interceptor_t aix, aix_stats_t* stats);

/**
 * @brief Reset statistics counters
 *
 * @param aix AIx handle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t aix_reset_stats(action_interceptor_t aix);

/*=============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * @brief Get result type name as string
 *
 * @param result Result type
 * @return Result name string
 */
const char* aix_result_name(aix_result_t result);

/**
 * @brief Create a proposal with basic parameters
 *
 * WHAT: Helper to initialize a proposal structure
 * WHY:  Simplify proposal creation
 * HOW:  Fill in proposal fields with provided values
 *
 * @param proposal Output: proposal to initialize
 * @param source_module Source module name
 * @param action_type Action type identifier
 * @param context Context data (can be NULL)
 * @param context_size Context size (0 if context is NULL)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t aix_create_proposal(
    aix_proposal_t* proposal,
    const char* source_module,
    uint32_t action_type,
    const void* context,
    size_t context_size
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LGSS_ACTION_INTERCEPTOR_H */
