/**
 * @file nimcp_security_executive_bridge.h
 * @brief Security-Executive Integration Bridge
 * @version 1.0.0
 * @date 2025-01-09
 *
 * WHAT: Bidirectional integration between security subsystem and executive control
 * WHY:  Executive tasks require authorization, resource limits, and capability checks.
 *       Security benefits from task audit trails and resource usage monitoring.
 * HOW:  Security authorizes tasks based on policy; executive reports task lifecycle
 *       for audit; security enforces resource budgets; executive enforces deadlines.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * PREFRONTAL CORTEX AND IMPULSE CONTROL:
 * ---------------------------------------
 * - PFC implements executive veto over prepotent responses
 * - Anterior cingulate cortex (ACC) monitors for errors/conflicts
 * - Orbitofrontal cortex (OFC) evaluates outcome values
 * - Security = neural "stop signals" preventing harmful actions
 *
 * SECURITY -> EXECUTIVE PATHWAYS:
 * --------------------------------
 * 1. Task Authorization:
 *    - Security policy engine evaluates task requests
 *    - Unauthorized tasks blocked before execution
 *    - Capability tokens required for sensitive operations
 *
 * 2. Resource Constraints:
 *    - CPU/memory/IO budgets prevent resource exhaustion
 *    - Rate limiting prevents abuse
 *    - Deadline enforcement prevents timeout attacks
 *
 * 3. Capability Verification:
 *    - Fine-grained access control per task
 *    - Principle of least privilege enforcement
 *    - Capability delegation with attenuation
 *
 * EXECUTIVE -> SECURITY PATHWAYS:
 * --------------------------------
 * 1. Task Audit Trail:
 *    - Task start/completion events logged
 *    - Resource usage reported for analysis
 *    - Anomaly detection on task patterns
 *
 * 2. Secure Rollback:
 *    - Failed tasks trigger secure cleanup
 *    - State restoration on authorization failure
 *    - Prevents partial execution vulnerabilities
 *
 * ARCHITECTURE:
 * ```
 * +------------------------------------------------------------------+
 * |                SECURITY-EXECUTIVE BRIDGE                         |
 * +==================================================================+
 * |                                                                  |
 * |  +------------------------------------------------------------+  |
 * |  |            SECURITY -> EXECUTIVE PATHWAYS                   |  |
 * |  |                                                             |  |
 * |  |  +------------------+                                       |  |
 * |  |  | POLICY ENGINE    | --> Task Authorization                |  |
 * |  |  | - Evaluate rules |     - ALLOW/DENY decision             |  |
 * |  |  | - Check context  |     - Required capabilities           |  |
 * |  |  +------------------+                                       |  |
 * |  |                                                             |  |
 * |  |  +------------------+                                       |  |
 * |  |  | RATE LIMITER     | --> Resource Constraints              |  |
 * |  |  | - Token bucket   |     - CPU/memory budgets              |  |
 * |  |  | - Per-client     |     - IO limits                       |  |
 * |  |  +------------------+                                       |  |
 * |  |                                                             |  |
 * |  |  +------------------+                                       |  |
 * |  |  | CAPABILITY SYS   | --> Capability Checks                 |  |
 * |  |  | - Token verify   |     - Permission validation           |  |
 * |  |  | - Attenuation    |     - Delegation chains               |  |
 * |  |  +------------------+                                       |  |
 * |  +------------------------------------------------------------+  |
 * |                                                                  |
 * |  +------------------------------------------------------------+  |
 * |  |            EXECUTIVE -> SECURITY PATHWAYS                   |  |
 * |  |                                                             |  |
 * |  |  +------------------+                                       |  |
 * |  |  | TASK LIFECYCLE   | --> Audit Trail                       |  |
 * |  |  | - Start events   |     - Encrypted logging               |  |
 * |  |  | - Completion     |     - Tamper detection                |  |
 * |  |  +------------------+                                       |  |
 * |  |                                                             |  |
 * |  |  +------------------+                                       |  |
 * |  |  | RESOURCE USAGE   | --> Usage Monitoring                  |  |
 * |  |  | - CPU time       |     - Anomaly detection               |  |
 * |  |  | - Memory alloc   |     - Quota enforcement               |  |
 * |  |  +------------------+                                       |  |
 * |  +------------------------------------------------------------+  |
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

#ifndef NIMCP_SECURITY_EXECUTIVE_BRIDGE_H
#define NIMCP_SECURITY_EXECUTIVE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"

/* Module integrations */
#include "cognitive/nimcp_executive.h"
#include "security/nimcp_policy_engine.h"
#include "security/nimcp_rate_limiter.h"
#include "security/nimcp_capability.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum length of denial reason string */
#define SECURITY_EXEC_MAX_REASON_LEN         256

/** Maximum capabilities per task */
#define SECURITY_EXEC_MAX_CAPABILITIES       16

/** Maximum tasks in audit buffer */
#define SECURITY_EXEC_MAX_AUDIT_TASKS        128

/** Maximum resource types tracked */
#define SECURITY_EXEC_MAX_RESOURCE_TYPES     8

/** Default deadline grace period (ms) */
#define SECURITY_EXEC_DEFAULT_DEADLINE_GRACE 100

/** Default resource check interval (ms) */
#define SECURITY_EXEC_DEFAULT_RESOURCE_INTERVAL 50

/** Magic number for validation */
#define SECURITY_EXEC_BRIDGE_MAGIC           0x53454252  /* 'SEBR' */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct security_executive_bridge security_executive_bridge_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Authorization decision result
 */
typedef enum {
    SECURITY_AUTH_ALLOWED = 0,       /**< Task authorized */
    SECURITY_AUTH_DENIED_POLICY,     /**< Denied by policy engine */
    SECURITY_AUTH_DENIED_CAPABILITY, /**< Missing required capability */
    SECURITY_AUTH_DENIED_RATE_LIMIT, /**< Rate limit exceeded */
    SECURITY_AUTH_DENIED_RESOURCE,   /**< Resource budget exceeded */
    SECURITY_AUTH_DENIED_DEADLINE,   /**< Cannot meet deadline */
    SECURITY_AUTH_DENIED_BLOCKED,    /**< Agent is blocked */
    SECURITY_AUTH_DENIED_UNKNOWN     /**< Unknown denial reason */
} security_auth_decision_t;

/**
 * @brief Resource type for budgeting
 */
typedef enum {
    SECURITY_RES_CPU = 0,            /**< CPU time (ms) */
    SECURITY_RES_MEMORY,             /**< Memory (bytes) */
    SECURITY_RES_TIME,               /**< Wall clock time (ms) */
    SECURITY_RES_IO_READ,            /**< IO read operations */
    SECURITY_RES_IO_WRITE,           /**< IO write operations */
    SECURITY_RES_NETWORK,            /**< Network operations */
    SECURITY_RES_TASKS,              /**< Concurrent tasks */
    SECURITY_RES_CUSTOM              /**< Custom resource type */
} security_resource_type_t;

/**
 * @brief Audit event type
 */
typedef enum {
    SECURITY_AUDIT_TASK_AUTHORIZED = 0, /**< Task was authorized */
    SECURITY_AUDIT_TASK_DENIED,         /**< Task was denied */
    SECURITY_AUDIT_TASK_STARTED,        /**< Task execution started */
    SECURITY_AUDIT_TASK_COMPLETED,      /**< Task completed successfully */
    SECURITY_AUDIT_TASK_FAILED,         /**< Task execution failed */
    SECURITY_AUDIT_TASK_ROLLBACK,       /**< Task rolled back */
    SECURITY_AUDIT_RESOURCE_WARNING,    /**< Resource threshold warning */
    SECURITY_AUDIT_RESOURCE_EXCEEDED,   /**< Resource limit exceeded */
    SECURITY_AUDIT_DEADLINE_WARNING,    /**< Deadline approaching */
    SECURITY_AUDIT_DEADLINE_EXCEEDED    /**< Deadline exceeded */
} security_audit_event_t;

/**
 * @brief Rollback status
 */
typedef enum {
    SECURITY_ROLLBACK_SUCCESS = 0,   /**< Rollback completed */
    SECURITY_ROLLBACK_PARTIAL,       /**< Partial rollback (some state lost) */
    SECURITY_ROLLBACK_FAILED,        /**< Rollback failed */
    SECURITY_ROLLBACK_NOT_NEEDED     /**< No rollback required */
} security_rollback_status_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief Configuration for Security-Executive bridge
 */
typedef struct {
    /* Task Authorization */
    bool enable_task_authorization;      /**< Enable task authorization checks */
    bool enable_capability_checks;       /**< Enable capability verification */
    bool enable_policy_evaluation;       /**< Enable policy engine evaluation */
    bool strict_mode;                    /**< Fail-closed on authorization errors */

    /* Resource Management */
    bool enable_resource_limits;         /**< Enable resource budgeting */
    bool enable_rate_limiting;           /**< Enable rate limiter integration */
    uint32_t resource_check_interval_ms; /**< Resource check interval */
    float resource_warning_threshold;    /**< Warning at X% of limit (0.0-1.0) */

    /* Deadline Enforcement */
    bool enable_deadline_enforcement;    /**< Enable deadline checking */
    uint32_t deadline_grace_period_ms;   /**< Grace period before abort */
    bool abort_on_deadline_exceeded;     /**< Abort task on deadline exceeded */

    /* Audit Trail */
    bool enable_audit_logging;           /**< Enable task audit logging */
    bool enable_encrypted_audit;         /**< Encrypt audit records */
    uint32_t max_audit_entries;          /**< Maximum audit buffer size */

    /* Rollback */
    bool enable_secure_rollback;         /**< Enable secure task rollback */
    bool rollback_on_auth_failure;       /**< Rollback if auth fails mid-task */

    /* Sensitivity Factors */
    float security_sensitivity;          /**< Security effect scaling [0.5-2.0] */
    float executive_sensitivity;         /**< Executive effect scaling [0.5-2.0] */
} security_executive_config_t;

/* ============================================================================
 * Result Structures
 * ============================================================================ */

/**
 * @brief Task authorization result
 */
typedef struct {
    bool authorized;                     /**< Authorization granted */
    security_auth_decision_t decision;   /**< Decision code */
    char denied_reason[SECURITY_EXEC_MAX_REASON_LEN]; /**< Denial reason if not authorized */

    /* Required capabilities */
    uint32_t required_capabilities[SECURITY_EXEC_MAX_CAPABILITIES]; /**< Required caps */
    uint32_t num_required_capabilities;  /**< Number of required capabilities */

    /* Granted resources */
    uint64_t cpu_budget_ms;              /**< CPU budget granted */
    uint64_t memory_budget_bytes;        /**< Memory budget granted */
    uint64_t time_budget_ms;             /**< Time budget granted */

    /* Metadata */
    uint64_t auth_time_ns;               /**< Time to authorize */
    uint64_t auth_timestamp;             /**< When authorized */
    uint32_t policy_rule_id;             /**< Matching policy rule ID */
} security_auth_result_t;

/**
 * @brief Resource budget for a task
 */
typedef struct {
    /* Hard limits */
    uint64_t cpu_limit_ms;               /**< Maximum CPU time (ms) */
    uint64_t memory_limit_bytes;         /**< Maximum memory (bytes) */
    uint64_t time_limit_ms;              /**< Maximum wall time (ms) */
    uint64_t io_read_limit;              /**< Maximum read operations */
    uint64_t io_write_limit;             /**< Maximum write operations */

    /* Current usage */
    uint64_t cpu_used_ms;                /**< CPU time used */
    uint64_t memory_used_bytes;          /**< Memory currently allocated */
    uint64_t time_used_ms;               /**< Wall time elapsed */
    uint64_t io_reads;                   /**< Read operations performed */
    uint64_t io_writes;                  /**< Write operations performed */

    /* Status */
    bool budget_exceeded;                /**< Any limit exceeded */
    security_resource_type_t exceeded_type; /**< Which limit was exceeded */
} security_resource_budget_t;

/**
 * @brief Capability check result
 */
typedef struct {
    bool has_all_capabilities;           /**< Has all required capabilities */
    uint32_t missing_capabilities[SECURITY_EXEC_MAX_CAPABILITIES]; /**< Missing caps */
    uint32_t num_missing;                /**< Number of missing capabilities */
    bool can_delegate;                   /**< Can delegate to acquire caps */
    uint32_t delegation_chain_depth;     /**< Depth of delegation chain */
} security_capability_check_t;

/**
 * @brief Audit record for task lifecycle
 */
typedef struct {
    uint32_t task_id;                    /**< Task identifier */
    security_audit_event_t event;        /**< Event type */
    uint64_t timestamp;                  /**< Event timestamp */
    uint32_t agent_id;                   /**< Agent performing task */

    /* Task details */
    char task_name[64];                  /**< Task name */
    task_type_t task_type;               /**< Task type */
    task_priority_t priority;            /**< Task priority */

    /* Resource usage snapshot */
    security_resource_budget_t resources; /**< Resource usage at event */

    /* Result (for completion events) */
    int result_code;                     /**< Task result code */
    uint64_t execution_time_ms;          /**< Total execution time */
} security_audit_record_t;

/* ============================================================================
 * Effects Structures
 * ============================================================================ */

/**
 * @brief Security effects on executive function
 */
typedef struct {
    /* Task blocking */
    uint32_t blocked_task_count;         /**< Tasks blocked this cycle */
    uint32_t blocked_task_ids[16];       /**< IDs of blocked tasks */
    security_auth_decision_t block_reasons[16]; /**< Block reasons */

    /* Resource constraints */
    bool resource_constrained;           /**< Under resource pressure */
    float resource_utilization;          /**< Overall resource utilization [0-1] */
    security_resource_type_t constrained_type; /**< Most constrained resource */

    /* Rate limiting */
    bool rate_limited;                   /**< Rate limiting active */
    float current_rate;                  /**< Current request rate */
    uint64_t time_until_ready_ms;        /**< Time until rate limit clears */

    /* Deadlines */
    uint32_t deadline_warning_count;     /**< Tasks with deadline warnings */
    uint32_t deadline_exceeded_count;    /**< Tasks that exceeded deadline */
} security_to_executive_effects_t;

/**
 * @brief Executive effects on security
 */
typedef struct {
    /* Audit trail */
    uint32_t tasks_started;              /**< Tasks started this cycle */
    uint32_t tasks_completed;            /**< Tasks completed this cycle */
    uint32_t tasks_failed;               /**< Tasks failed this cycle */

    /* Resource usage */
    uint64_t total_cpu_used_ms;          /**< Total CPU used */
    uint64_t total_memory_bytes;         /**< Total memory allocated */
    uint32_t active_task_count;          /**< Currently active tasks */

    /* Anomaly indicators */
    bool unusual_task_pattern;           /**< Unusual task pattern detected */
    float task_failure_rate;             /**< Recent failure rate [0-1] */
    float resource_spike;                /**< Resource usage spike [0-1] */
} executive_to_security_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief Current state of Security-Executive interaction
 */
typedef struct {
    /* Authorization state */
    uint32_t pending_authorizations;     /**< Authorizations in progress */
    uint32_t active_tasks;               /**< Currently authorized tasks */
    bool strict_mode_active;             /**< Strict mode engaged */

    /* Resource state */
    float overall_resource_utilization;  /**< Overall resource usage [0-1] */
    bool any_budget_exceeded;            /**< Any task over budget */
    uint32_t tasks_at_resource_limit;    /**< Tasks near resource limits */

    /* Rate limiting state */
    bool rate_limiting_active;           /**< Rate limiting engaged */
    float current_request_rate;          /**< Current request rate */

    /* Deadline state */
    uint32_t tasks_near_deadline;        /**< Tasks approaching deadline */
    uint32_t tasks_past_deadline;        /**< Tasks past deadline */

    /* Audit state */
    uint32_t audit_buffer_used;          /**< Audit buffer entries used */
    uint64_t last_audit_flush;           /**< Last audit flush timestamp */

    /* Timestamps */
    uint64_t last_authorization_time;    /**< Last authorization check */
    uint64_t last_resource_check_time;   /**< Last resource check */
    uint64_t last_update_time;           /**< Last bridge update */
} security_executive_state_t;

/**
 * @brief Statistics for Security-Executive bridge
 */
typedef struct {
    /* Authorization statistics */
    uint64_t total_auth_requests;        /**< Total authorization requests */
    uint64_t auth_granted;               /**< Authorizations granted */
    uint64_t auth_denied_policy;         /**< Denied by policy */
    uint64_t auth_denied_capability;     /**< Denied for capability */
    uint64_t auth_denied_rate_limit;     /**< Denied by rate limit */
    uint64_t auth_denied_resource;       /**< Denied for resource */
    float avg_auth_time_ns;              /**< Average authorization time */

    /* Resource statistics */
    uint64_t resource_checks;            /**< Resource checks performed */
    uint64_t resource_warnings;          /**< Resource warnings issued */
    uint64_t resource_violations;        /**< Resource limits exceeded */
    float avg_resource_utilization;      /**< Average resource utilization */

    /* Deadline statistics */
    uint64_t deadline_checks;            /**< Deadline checks performed */
    uint64_t deadline_warnings;          /**< Deadline warnings issued */
    uint64_t deadline_violations;        /**< Deadline exceeded count */

    /* Rollback statistics */
    uint64_t rollback_attempts;          /**< Rollback attempts */
    uint64_t rollback_successes;         /**< Successful rollbacks */
    uint64_t rollback_failures;          /**< Failed rollbacks */

    /* Audit statistics */
    uint64_t audit_events_logged;        /**< Audit events logged */
    uint64_t audit_flushes;              /**< Audit buffer flushes */

    /* Performance */
    uint64_t bridge_updates;             /**< Total bridge updates */
    float avg_update_time_ns;            /**< Average update time */
} security_executive_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Security-Executive bridge state
 */
struct security_executive_bridge {
    bridge_base_t base;                  /**< MUST be first: base bridge */

    /* Configuration */
    security_executive_config_t config;

    /* Connected systems */
    executive_controller_t* executive;   /**< Executive control system */
    nimcp_policy_engine_t policy_engine; /**< Policy engine */
    nimcp_rate_limiter_t rate_limiter;   /**< Rate limiter */
    nimcp_capability_system_t* capability_system; /**< Capability system */

    /* Current effects */
    security_to_executive_effects_t security_effects;    /**< Security -> Exec */
    executive_to_security_effects_t executive_effects;   /**< Exec -> Security */

    /* State */
    security_executive_state_t state;

    /* Audit buffer */
    security_audit_record_t* audit_buffer;   /**< Circular audit buffer */
    uint32_t audit_head;                     /**< Audit buffer head */
    uint32_t audit_count;                    /**< Current audit entries */

    /* Statistics */
    security_executive_stats_t stats;
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
int security_executive_default_config(security_executive_config_t* config);

/**
 * @brief Create Security-Executive bridge
 *
 * WHAT: Creates and initializes the bridge
 * WHY:  Enables security-executive integration
 * HOW:  Allocates memory, initializes base, sets config
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
security_executive_bridge_t* security_executive_bridge_create(
    const security_executive_config_t* config
);

/**
 * @brief Destroy Security-Executive bridge
 *
 * WHAT: Cleans up bridge resources
 * WHY:  Prevents memory leaks
 * HOW:  Frees audit buffer, base cleanup, frees bridge
 *
 * @param bridge Bridge instance (NULL safe)
 */
void security_executive_bridge_destroy(security_executive_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect executive controller
 *
 * @param bridge Bridge instance
 * @param executive Executive controller
 * @return 0 on success, error code on failure
 */
int security_executive_bridge_connect_executive(
    security_executive_bridge_t* bridge,
    executive_controller_t* executive
);

/**
 * @brief Connect policy engine
 *
 * @param bridge Bridge instance
 * @param policy_engine Policy engine
 * @return 0 on success, error code on failure
 */
int security_executive_bridge_connect_policy_engine(
    security_executive_bridge_t* bridge,
    nimcp_policy_engine_t policy_engine
);

/**
 * @brief Connect rate limiter
 *
 * @param bridge Bridge instance
 * @param rate_limiter Rate limiter
 * @return 0 on success, error code on failure
 */
int security_executive_bridge_connect_rate_limiter(
    security_executive_bridge_t* bridge,
    nimcp_rate_limiter_t rate_limiter
);

/**
 * @brief Connect capability system
 *
 * @param bridge Bridge instance
 * @param capability_system Capability system
 * @return 0 on success, error code on failure
 */
int security_executive_bridge_connect_capability_system(
    security_executive_bridge_t* bridge,
    nimcp_capability_system_t* capability_system
);

/**
 * @brief Disconnect all systems
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int security_executive_bridge_disconnect(security_executive_bridge_t* bridge);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Bridge instance
 * @return true if all required systems connected
 */
bool security_executive_bridge_is_connected(
    const security_executive_bridge_t* bridge
);

/* ============================================================================
 * Security -> Executive Direction
 * ============================================================================ */

/**
 * @brief Authorize task for execution
 *
 * WHAT: Checks if task is authorized to execute
 * WHY:  Prevents unauthorized task execution
 * HOW:  Evaluates policy, checks capabilities, validates resources
 *
 * @param bridge Bridge instance
 * @param task Task descriptor to authorize
 * @param agent_id ID of agent requesting task
 * @param capabilities Array of capability tokens
 * @param num_capabilities Number of capabilities
 * @param result Output authorization result
 * @return 0 on success, error code on failure
 */
int security_executive_authorize_task(
    security_executive_bridge_t* bridge,
    const task_descriptor_t* task,
    uint32_t agent_id,
    const nimcp_capability_t* capabilities,
    uint32_t num_capabilities,
    security_auth_result_t* result
);

/**
 * @brief Check if agent has required capabilities
 *
 * WHAT: Verifies agent possesses required capabilities
 * WHY:  Enforces principle of least privilege
 * HOW:  Validates each capability token against requirements
 *
 * @param bridge Bridge instance
 * @param agent_id Agent ID
 * @param capabilities Agent's capability tokens
 * @param num_capabilities Number of capabilities
 * @param required Required capability permissions (bitmask)
 * @param result Output check result
 * @return 0 on success, error code on failure
 */
int security_executive_check_capabilities(
    security_executive_bridge_t* bridge,
    uint32_t agent_id,
    const nimcp_capability_t* capabilities,
    uint32_t num_capabilities,
    uint32_t required,
    security_capability_check_t* result
);

/**
 * @brief Allocate resource budget for task
 *
 * WHAT: Allocates security-bounded resources for task
 * WHY:  Prevents resource exhaustion attacks
 * HOW:  Reserves resources from available pool with limits
 *
 * @param bridge Bridge instance
 * @param task_id Task identifier
 * @param requested Requested resource budget
 * @param granted Output granted budget (may be less than requested)
 * @return 0 on success, error code on failure
 */
int security_executive_allocate_resources(
    security_executive_bridge_t* bridge,
    uint32_t task_id,
    const security_resource_budget_t* requested,
    security_resource_budget_t* granted
);

/**
 * @brief Enforce deadline for task
 *
 * WHAT: Prevents deadline-based attacks
 * WHY:  Long-running tasks can be denial-of-service vectors
 * HOW:  Monitors task progress, warns or aborts on deadline
 *
 * @param bridge Bridge instance
 * @param task_id Task identifier
 * @param deadline_ms Deadline timestamp (ms since epoch)
 * @return 0 on success, error code on failure
 */
int security_executive_enforce_deadline(
    security_executive_bridge_t* bridge,
    uint32_t task_id,
    uint64_t deadline_ms
);

/* ============================================================================
 * Executive -> Security Direction
 * ============================================================================ */

/**
 * @brief Audit task start event
 *
 * WHAT: Records task start in audit log
 * WHY:  Provides audit trail for security analysis
 * HOW:  Creates audit record, stores in buffer
 *
 * @param bridge Bridge instance
 * @param task Task descriptor
 * @param agent_id Agent starting task
 * @return 0 on success, error code on failure
 */
int security_executive_audit_task_start(
    security_executive_bridge_t* bridge,
    const task_descriptor_t* task,
    uint32_t agent_id
);

/**
 * @brief Audit task completion event
 *
 * WHAT: Records task completion in audit log
 * WHY:  Provides complete task lifecycle tracking
 * HOW:  Creates audit record with results, updates stats
 *
 * @param bridge Bridge instance
 * @param task Task descriptor
 * @param success Whether task completed successfully
 * @param result_code Task result code
 * @param resources Final resource usage
 * @return 0 on success, error code on failure
 */
int security_executive_audit_task_completion(
    security_executive_bridge_t* bridge,
    const task_descriptor_t* task,
    bool success,
    int result_code,
    const security_resource_budget_t* resources
);

/**
 * @brief Perform secure task rollback
 *
 * WHAT: Securely rolls back task on failure
 * WHY:  Prevents partial execution vulnerabilities
 * HOW:  Restores pre-task state, releases resources, logs event
 *
 * @param bridge Bridge instance
 * @param task_id Task identifier
 * @param reason Rollback reason
 * @param status Output rollback status
 * @return 0 on success, error code on failure
 */
int security_executive_rollback_task(
    security_executive_bridge_t* bridge,
    uint32_t task_id,
    const char* reason,
    security_rollback_status_t* status
);

/**
 * @brief Report resource usage for task
 *
 * WHAT: Reports current resource usage for task
 * WHY:  Enables resource monitoring and quota enforcement
 * HOW:  Updates task resource tracking, checks limits
 *
 * @param bridge Bridge instance
 * @param task_id Task identifier
 * @param resources Current resource usage
 * @return 0 on success, error code on failure
 */
int security_executive_report_resource_usage(
    security_executive_bridge_t* bridge,
    uint32_t task_id,
    const security_resource_budget_t* resources
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update bridge (bidirectional)
 *
 * WHAT: Performs periodic bridge update
 * WHY:  Synchronizes state, checks deadlines, processes audit
 * HOW:  Updates effects, checks resource limits, flushes audit
 *
 * @param bridge Bridge instance
 * @param delta_ms Time since last update (ms)
 * @return 0 on success, error code on failure
 */
int security_executive_bridge_update(
    security_executive_bridge_t* bridge,
    uint64_t delta_ms
);

/**
 * @brief Apply security effects to executive
 *
 * WHAT: Applies current security constraints to executive
 * WHY:  Propagates security decisions to task execution
 * HOW:  Updates blocked tasks, resource constraints, rate limits
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int security_executive_apply_security_effects(
    security_executive_bridge_t* bridge
);

/**
 * @brief Apply executive effects to security
 *
 * WHAT: Applies executive activity to security monitoring
 * WHY:  Enables security visibility into task execution
 * HOW:  Updates audit trail, resource tracking, anomaly detection
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int security_executive_apply_executive_effects(
    security_executive_bridge_t* bridge
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current bridge state
 *
 * @param bridge Bridge instance
 * @param state Output state structure
 * @return 0 on success, error code on failure
 */
int security_executive_bridge_get_state(
    const security_executive_bridge_t* bridge,
    security_executive_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output statistics structure
 * @return 0 on success, error code on failure
 */
int security_executive_bridge_get_stats(
    const security_executive_bridge_t* bridge,
    security_executive_stats_t* stats
);

/**
 * @brief Get security effects on executive
 *
 * @param bridge Bridge instance
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int security_executive_get_security_effects(
    const security_executive_bridge_t* bridge,
    security_to_executive_effects_t* effects
);

/**
 * @brief Get executive effects on security
 *
 * @param bridge Bridge instance
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int security_executive_get_executive_effects(
    const security_executive_bridge_t* bridge,
    executive_to_security_effects_t* effects
);

/**
 * @brief Get audit records
 *
 * WHAT: Retrieves audit records from buffer
 * WHY:  Enables security analysis of task history
 * HOW:  Copies records from circular buffer
 *
 * @param bridge Bridge instance
 * @param records Output record array
 * @param max_records Maximum records to retrieve
 * @param num_records Output number of records retrieved
 * @return 0 on success, error code on failure
 */
int security_executive_get_audit_records(
    const security_executive_bridge_t* bridge,
    security_audit_record_t* records,
    uint32_t max_records,
    uint32_t* num_records
);

/**
 * @brief Flush audit buffer
 *
 * WHAT: Flushes audit buffer to persistent storage
 * WHY:  Ensures audit trail is preserved
 * HOW:  Writes buffer to configured audit destination
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int security_executive_flush_audit(security_executive_bridge_t* bridge);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int security_executive_bridge_reset_stats(security_executive_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int security_executive_bridge_connect_bio_async(
    security_executive_bridge_t* bridge
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int security_executive_bridge_disconnect_bio_async(
    security_executive_bridge_t* bridge
);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Bridge instance
 * @return true if connected to bio-async router
 */
bool security_executive_bridge_is_bio_async_connected(
    const security_executive_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_EXECUTIVE_BRIDGE_H */
