/**
 * @file nimcp_lgss_executive_bridge.h
 * @brief LGSS Executive Safety Bridge - Component A6
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Safety interception layer for all executive function outputs
 * WHY:  ALL cognitive tasks must be validated before execution to prevent harmful actions
 * HOW:  Convert executive proposals to safety_action_context_t, route through AIx
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * PREFRONTAL CORTEX SAFETY GATING:
 * ---------------------------------
 * - Orbitofrontal cortex (OFC) evaluates action outcomes
 * - Anterior cingulate cortex (ACC) monitors conflict/error
 * - Ventromedial PFC integrates emotion with decision-making
 * - Safety bridge = neural "stop signals" preventing impulsive harmful actions
 *
 * This bridge is CRITICAL - it gates:
 * 1. Task queue additions (all new tasks)
 * 2. MCTS plan selections (planning outputs)
 * 3. Goal formation (new goal creation)
 *
 * ARCHITECTURE:
 * ```
 * +------------------------------------------------------------------+
 * |                EXECUTIVE SAFETY BRIDGE                            |
 * +==================================================================+
 * |                                                                  |
 * |  ┌──────────────┐     ┌──────────────┐     ┌──────────────┐    |
 * |  │ TASK QUEUE   │────►│ SAFETY GATE  │────►│ AIx EVALUATE │    |
 * |  │ Proposals    │     │ Convert to   │     │ ALLOW/DENY/  │    |
 * |  └──────────────┘     │ action_ctx   │     │ ESCALATE     │    |
 * |                       └──────────────┘     └──────────────┘    |
 * |                                                   │             |
 * |  ┌──────────────┐     ┌──────────────┐           │             |
 * |  │ MCTS PLANS   │────►│ PLAN VALID.  │───────────┘             |
 * |  │ Proposals    │     │ Recursive    │                         |
 * |  └──────────────┘     └──────────────┘                         |
 * |                                                                  |
 * |  ┌──────────────┐     ┌──────────────┐                         |
 * |  │ GOAL FORM.   │────►│ GOAL VALID.  │───────────┘             |
 * |  │ Proposals    │     │ Value/harm   │                         |
 * |  └──────────────┘     └──────────────┘                         |
 * |                                                                  |
 * +------------------------------------------------------------------+
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_LGSS_EXECUTIVE_BRIDGE_H
#define NIMCP_LGSS_EXECUTIVE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum length of task description */
#define LGSS_EXEC_MAX_DESCRIPTION_LEN    256

/** Maximum length of goal description */
#define LGSS_EXEC_MAX_GOAL_DESC_LEN      256

/** Maximum length of context string */
#define LGSS_EXEC_MAX_CONTEXT_LEN        128

/** Maximum pending tasks for safety review */
#define LGSS_EXEC_MAX_PENDING_TASKS      64

/** Maximum blocked task log entries */
#define LGSS_EXEC_MAX_BLOCKED_LOG        128

/** Magic number for validation */
#define LGSS_EXEC_BRIDGE_MAGIC           0x4C475342  /* 'LGSB' */

/** Default harm probability threshold for blocking */
#define LGSS_EXEC_DEFAULT_HARM_THRESHOLD 0.3f

/** Default reversibility threshold for escalation */
#define LGSS_EXEC_DEFAULT_REVERSIBILITY_THRESHOLD 0.4f

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct executive_safety_bridge executive_safety_bridge_t;

/* Forward declare action interceptor and executive from LGSS/cognitive */
typedef struct action_interceptor action_interceptor_t;
typedef struct executive_controller executive_controller_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Safety domain categories for executive actions
 */
typedef enum {
    LGSS_DOMAIN_GENERAL = 0,         /**< General cognitive tasks */
    LGSS_DOMAIN_HUMAN_INTERACTION,   /**< Tasks involving humans */
    LGSS_DOMAIN_PHYSICAL_ACTION,     /**< Tasks with physical effects */
    LGSS_DOMAIN_DATA_ACCESS,         /**< Tasks accessing data */
    LGSS_DOMAIN_SYSTEM_CONTROL,      /**< Tasks controlling systems */
    LGSS_DOMAIN_LEARNING,            /**< Tasks modifying learning */
    LGSS_DOMAIN_GOAL_MODIFICATION,   /**< Tasks modifying goals */
    LGSS_DOMAIN_COUNT
} lgss_safety_domain_t;

/**
 * @brief Safety evaluation result
 */
typedef enum {
    LGSS_RESULT_ALLOW = 0,           /**< Action permitted */
    LGSS_RESULT_DENY,                /**< Action blocked (hard stop) */
    LGSS_RESULT_ESCALATE,            /**< Requires human approval */
    LGSS_RESULT_TIMEOUT,             /**< Evaluation timed out (fail-safe: deny) */
    LGSS_RESULT_ERROR                /**< Evaluation error (fail-safe: deny) */
} lgss_result_t;

/**
 * @brief Task proposal status
 */
typedef enum {
    LGSS_PROPOSAL_PENDING = 0,       /**< Awaiting evaluation */
    LGSS_PROPOSAL_APPROVED,          /**< Approved for execution */
    LGSS_PROPOSAL_DENIED,            /**< Denied - blocked */
    LGSS_PROPOSAL_ESCALATED          /**< Escalated for human review */
} lgss_proposal_status_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Task proposal for safety evaluation
 *
 * WHAT: Represents a proposed executive task requiring safety validation
 * WHY:  ALL tasks must be validated before entering task queue
 * HOW:  Converted to safety_action_context_t for AIx evaluation
 */
typedef struct {
    uint64_t task_id;                           /**< Unique task identifier */
    char description[LGSS_EXEC_MAX_DESCRIPTION_LEN]; /**< Human-readable description */
    char context[LGSS_EXEC_MAX_CONTEXT_LEN];    /**< Execution context */
    uint32_t priority;                          /**< Task priority (0-4) */
    uint64_t deadline_us;                       /**< Deadline timestamp (0 = no deadline) */

    /* Safety-relevant fields */
    lgss_safety_domain_t domain;                /**< Safety domain category */
    float p_harm;                               /**< Estimated harm probability [0,1] */
    float reversibility;                        /**< Action reversibility [0,1] */
    char target_type[64];                       /**< Target of action (e.g., "human", "system") */
    char operation[64];                         /**< Operation type (e.g., "execute", "modify") */

    /* Source tracking */
    uint32_t source_module;                     /**< Module proposing this task */
    uint64_t proposal_timestamp_us;             /**< When task was proposed */
} executive_task_proposal_t;

/**
 * @brief Goal proposal for safety evaluation
 *
 * WHAT: Represents a proposed new goal requiring safety validation
 * WHY:  New goals shape future behavior and must be aligned with safety
 * HOW:  Evaluated for harm potential and value alignment
 */
typedef struct {
    uint64_t goal_id;                           /**< Unique goal identifier */
    char description[LGSS_EXEC_MAX_GOAL_DESC_LEN]; /**< Goal description */
    float value_estimate;                       /**< Expected value/utility [0,1] */
    lgss_safety_domain_t domain;                /**< Safety domain category */
    float p_harm;                               /**< Estimated harm probability [0,1] */

    /* Goal-specific fields */
    char subgoal_of[64];                        /**< Parent goal (if hierarchical) */
    float urgency;                              /**< How urgent is this goal [0,1] */
    float reversibility;                        /**< Can this goal be undone [0,1] */
    bool modifies_self;                         /**< Does this goal modify the agent */
    bool affects_humans;                        /**< Does this goal affect humans */

    /* Source tracking */
    uint32_t source_module;                     /**< Module proposing this goal */
    uint64_t proposal_timestamp_us;             /**< When goal was proposed */
} goal_proposal_t;

/**
 * @brief Safety action context for AIx evaluation
 *
 * WHAT: Standardized context for safety evaluation
 * WHY:  AIx needs consistent format for all action types
 * HOW:  Executive proposals converted to this format
 */
typedef struct {
    char operation[64];                         /**< Operation type */
    char target_type[64];                       /**< Target of action */
    lgss_safety_domain_t domain;                /**< Domain category */
    float p_harm;                               /**< Harm probability [0,1] */
    float reversibility;                        /**< Reversibility [0,1] */
    char scope[32];                             /**< Scope (local, regional, global) */
    char estimated_impact[32];                  /**< Impact level */
    void* context_data;                         /**< Additional context (opaque) */
    size_t context_size;                        /**< Size of context data */
} safety_action_context_t;

/**
 * @brief Safety evaluation result details
 */
typedef struct {
    lgss_result_t result;                       /**< Evaluation result */
    char matched_rule_id[64];                   /**< Rule that triggered (if any) */
    float confidence;                           /**< Confidence in evaluation [0,1] */
    char explanation[256];                      /**< Human-readable explanation */
    uint64_t evaluation_time_us;                /**< Time to evaluate */
    uint32_t inference_steps;                   /**< Number of inference steps */
} safety_evaluation_result_t;

/**
 * @brief AIx decision output
 */
typedef struct {
    lgss_result_t result;                       /**< Result code */
    uint64_t proposal_id;                       /**< Corresponding proposal ID */
    safety_evaluation_result_t safety_eval;     /**< Detailed evaluation */
    uint64_t processing_time_us;                /**< Time to process */
    bool escalation_pending;                    /**< Awaiting human decision */
    uint64_t escalation_id;                     /**< Escalation ticket ID (if escalated) */
} aix_decision_t;

/**
 * @brief Blocked task log entry
 */
typedef struct {
    uint64_t task_id;                           /**< Blocked task ID */
    uint64_t timestamp_us;                      /**< When blocked */
    lgss_result_t result;                       /**< Why blocked */
    char rule_id[64];                           /**< Triggering rule */
    char description[128];                      /**< Task description */
    float p_harm;                               /**< Harm probability */
} blocked_task_entry_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief Executive safety bridge configuration
 */
typedef struct {
    /* Interception flags */
    bool intercept_task_queue;                  /**< Gate task queue additions (default: true) */
    bool intercept_planning;                    /**< Gate MCTS plan selection (default: true) */
    bool intercept_goal_formation;              /**< Gate new goal creation (default: true) */

    /* Limits */
    uint32_t max_pending_tasks;                 /**< Max pending proposals (default: 64) */
    uint32_t evaluation_timeout_ms;             /**< Evaluation timeout (default: 100ms) */

    /* Thresholds */
    float harm_threshold;                       /**< Block if p_harm > threshold (default: 0.3) */
    float reversibility_threshold;              /**< Escalate if reversibility < threshold (default: 0.4) */
    float value_threshold;                      /**< Block goals with value < threshold (default: 0.0) */

    /* Fail-safe behavior */
    bool fail_safe_on_timeout;                  /**< Deny on timeout (default: true) */
    bool fail_safe_on_error;                    /**< Deny on error (default: true) */

    /* Logging */
    bool enable_audit_logging;                  /**< Enable detailed logging (default: true) */
    uint32_t max_blocked_log_entries;           /**< Max blocked task log size (default: 128) */

    /* Sensitivity */
    float safety_sensitivity;                   /**< Safety effect scaling [0.5-2.0] (default: 1.0) */
} executive_safety_config_t;

/* ============================================================================
 * Statistics Structure
 * ============================================================================ */

/**
 * @brief Executive safety bridge statistics
 */
typedef struct {
    /* Task proposals */
    uint64_t tasks_proposed;                    /**< Total task proposals */
    uint64_t tasks_allowed;                     /**< Tasks allowed */
    uint64_t tasks_denied;                      /**< Tasks denied */
    uint64_t tasks_escalated;                   /**< Tasks escalated */

    /* Plan validations */
    uint64_t plans_validated;                   /**< Total plan validations */
    uint64_t plans_allowed;                     /**< Plans allowed */
    uint64_t plans_denied;                      /**< Plans denied */

    /* Goal proposals */
    uint64_t goals_proposed;                    /**< Total goal proposals */
    uint64_t goals_allowed;                     /**< Goals allowed */
    uint64_t goals_denied;                      /**< Goals denied */
    uint64_t goals_escalated;                   /**< Goals escalated */

    /* Performance */
    float avg_evaluation_time_us;               /**< Average evaluation time */
    uint64_t timeouts;                          /**< Evaluation timeouts */
    uint64_t errors;                            /**< Evaluation errors */

    /* Bridge activity */
    uint64_t bridge_updates;                    /**< Total bridge updates */
    uint64_t last_update_time_us;               /**< Last update timestamp */
} executive_safety_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Executive safety bridge state
 */
struct executive_safety_bridge {
    bridge_base_t base;                         /**< MUST be first: base bridge */

    /* Magic for validation */
    uint32_t magic;                             /**< Magic number for validation */

    /* Configuration */
    executive_safety_config_t config;

    /* Connected systems */
    action_interceptor_t* aix;                  /**< Action interceptor (safety evaluator) */
    executive_controller_t* executive;          /**< Executive controller */

    /* Blocked task log (circular buffer) */
    blocked_task_entry_t* blocked_log;          /**< Blocked task log */
    uint32_t blocked_log_head;                  /**< Log head index */
    uint32_t blocked_log_count;                 /**< Current log entries */

    /* Statistics */
    executive_safety_stats_t stats;

    /* Internal state */
    uint64_t next_proposal_id;                  /**< Next proposal ID */
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Returns default configuration with sensible values
 * WHY:  Provides starting point for customization
 * HOW:  Populates config struct with defaults
 *
 * @param config Output configuration structure
 * @return 0 on success, error code on failure
 */
int executive_safety_default_config(executive_safety_config_t* config);

/**
 * @brief Create executive safety bridge
 *
 * WHAT: Creates and initializes the LGSS executive bridge
 * WHY:  Enables safety gating of all executive outputs
 * HOW:  Allocates memory, initializes base, connects systems
 *
 * @param aix Action interceptor for safety evaluation
 * @param executive Executive controller to gate
 * @return Bridge instance or NULL on failure
 */
executive_safety_bridge_t* executive_safety_bridge_create(
    action_interceptor_t* aix,
    executive_controller_t* executive
);

/**
 * @brief Create executive safety bridge with custom config
 *
 * @param aix Action interceptor for safety evaluation
 * @param executive Executive controller to gate
 * @param config Custom configuration
 * @return Bridge instance or NULL on failure
 */
executive_safety_bridge_t* executive_safety_bridge_create_custom(
    action_interceptor_t* aix,
    executive_controller_t* executive,
    const executive_safety_config_t* config
);

/**
 * @brief Destroy executive safety bridge
 *
 * WHAT: Cleans up bridge resources
 * WHY:  Prevents memory leaks
 * HOW:  Frees blocked log, base cleanup, frees bridge
 *
 * @param bridge Bridge instance (NULL safe)
 */
void executive_safety_bridge_destroy(executive_safety_bridge_t* bridge);

/* ============================================================================
 * Core Safety Gating API
 * ============================================================================ */

/**
 * @brief Propose task for safety evaluation
 *
 * WHAT: Submit task for safety validation before execution
 * WHY:  ALL tasks must pass through this gate
 * HOW:  Convert to safety_action_context_t, evaluate via AIx
 *
 * CRITICAL: This is the primary safety gate for executive tasks
 *
 * @param bridge Bridge instance
 * @param task Task proposal to evaluate
 * @param decision Output decision result
 * @return 0 on success, error code on failure
 */
int executive_safety_propose_task(
    executive_safety_bridge_t* bridge,
    const executive_task_proposal_t* task,
    aix_decision_t* decision
);

/**
 * @brief Validate MCTS plan before execution
 *
 * WHAT: Validate entire plan tree for safety
 * WHY:  Plans may contain hidden harmful sequences
 * HOW:  Recursively evaluate plan nodes via planning bridge
 *
 * @param bridge Bridge instance
 * @param plan Plan structure to validate
 * @param decision Output decision result
 * @return 0 on success, error code on failure
 */
int executive_safety_validate_plan(
    executive_safety_bridge_t* bridge,
    const void* plan,  /* plan_t* from nimcp_executive.h */
    aix_decision_t* decision
);

/**
 * @brief Propose new goal for safety evaluation
 *
 * WHAT: Validate new goals before formation
 * WHY:  Goals shape future behavior, must be aligned
 * HOW:  Evaluate harm potential and value alignment
 *
 * @param bridge Bridge instance
 * @param goal Goal proposal to evaluate
 * @param decision Output decision result
 * @return 0 on success, error code on failure
 */
int executive_safety_propose_goal(
    executive_safety_bridge_t* bridge,
    const goal_proposal_t* goal,
    aix_decision_t* decision
);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Convert task proposal to safety action context
 *
 * @param task Task proposal
 * @param context Output safety action context
 * @return 0 on success, error code on failure
 */
int executive_safety_task_to_context(
    const executive_task_proposal_t* task,
    safety_action_context_t* context
);

/**
 * @brief Convert goal proposal to safety action context
 *
 * @param goal Goal proposal
 * @param context Output safety action context
 * @return 0 on success, error code on failure
 */
int executive_safety_goal_to_context(
    const goal_proposal_t* goal,
    safety_action_context_t* context
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output statistics structure
 * @return 0 on success, error code on failure
 */
int executive_safety_get_stats(
    const executive_safety_bridge_t* bridge,
    executive_safety_stats_t* stats
);

/**
 * @brief Get blocked task log
 *
 * @param bridge Bridge instance
 * @param entries Output entry array
 * @param max_entries Maximum entries to retrieve
 * @param num_entries Output number of entries retrieved
 * @return 0 on success, error code on failure
 */
int executive_safety_get_blocked_log(
    const executive_safety_bridge_t* bridge,
    blocked_task_entry_t* entries,
    uint32_t max_entries,
    uint32_t* num_entries
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int executive_safety_reset_stats(executive_safety_bridge_t* bridge);

/**
 * @brief Clear blocked task log
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int executive_safety_clear_blocked_log(executive_safety_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect action interceptor
 *
 * @param bridge Bridge instance
 * @param aix Action interceptor
 * @return 0 on success, error code on failure
 */
int executive_safety_connect_aix(
    executive_safety_bridge_t* bridge,
    action_interceptor_t* aix
);

/**
 * @brief Connect executive controller
 *
 * @param bridge Bridge instance
 * @param executive Executive controller
 * @return 0 on success, error code on failure
 */
int executive_safety_connect_executive(
    executive_safety_bridge_t* bridge,
    executive_controller_t* executive
);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Bridge instance
 * @return true if AIx and executive are connected
 */
bool executive_safety_is_connected(const executive_safety_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int executive_safety_connect_bio_async(executive_safety_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int executive_safety_disconnect_bio_async(executive_safety_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Bridge instance
 * @return true if connected to bio-async router
 */
bool executive_safety_is_bio_async_connected(const executive_safety_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LGSS_EXECUTIVE_BRIDGE_H */
