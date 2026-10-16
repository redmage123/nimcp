/**
 * @file nimcp_security_rcog_bridge.h
 * @brief Security Module - Recursive Cognition Integration Bridge
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Bidirectional integration between security module and recursive cognition
 * WHY:  Tool safety requires security oversight; output validation prevents harmful
 *       content; recursion limits prevent resource exhaustion attacks
 * HOW:  Enforces tool whitelisting, validates parameters and outputs, sandboxes
 *       execution, tracks resource usage, and escalates sensitive operations
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * PREFRONTAL-IMMUNE AXIS:
 * -----------------------
 * The security-rcog bridge models the interaction between:
 *
 * 1. Executive Function (Prefrontal Cortex = RCOG Engine):
 *    - Tool selection and invocation (motor planning)
 *    - Recursive decomposition (working memory manipulation)
 *    - Output generation (verbal fluency)
 *
 * 2. Immune Surveillance (Security Module):
 *    - Pattern recognition of dangerous tool combinations
 *    - Input/output validation (antigen screening)
 *    - Resource consumption monitoring (metabolic homeostasis)
 *    - Human approval requests (adaptive immune consultation)
 *
 * SECURITY CONSIDERATIONS:
 * ------------------------
 * - Tool whitelisting prevents execution of unapproved capabilities
 * - Parameter validation blocks injection attacks
 * - Output validation prevents data exfiltration and harmful content
 * - Recursion limits prevent stack exhaustion attacks
 * - Resource tracking prevents denial of service
 * - Human approval for sensitive operations (HITL safety)
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |                    SECURITY-RCOG BRIDGE                                    |
 * +===========================================================================+
 * |                                                                            |
 * |   +--------------------------------------------------------------------+   |
 * |   |                SECURITY -> RCOG EFFECTS                            |   |
 * |   |                                                                    |   |
 * |   |   +----------------+   +----------------+   +------------------+   |   |
 * |   |   | Tool           |   |   Recursion    |   |    Resource      |   |   |
 * |   |   | Restrictions   |-->|   Depth Limits |-->|    Quotas        |   |   |
 * |   |   +----------------+   +----------------+   +------------------+   |   |
 * |   |          |                    |                      |             |   |
 * |   |          v                    v                      v             |   |
 * |   |   +---------------------------------------------------------+     |   |
 * |   |   |              RCOG ENGINE / TOOL ROUTER                  |     |   |
 * |   |   |   - Whitelist enforcement                               |     |   |
 * |   |   |   - Parameter validation                                |     |   |
 * |   |   |   - Sandboxed execution                                 |     |   |
 * |   |   |   - Output validation                                   |     |   |
 * |   |   +---------------------------------------------------------+     |   |
 * |   |                                                                    |   |
 * |   +--------------------------------------------------------------------+   |
 * |                                                                            |
 * |   +--------------------------------------------------------------------+   |
 * |   |                RCOG -> SECURITY EFFECTS                            |   |
 * |   |                                                                    |   |
 * |   |   +---------------------------------------------------------+     |   |
 * |   |   |              RCOG ENGINE / TOOL ROUTER                  |     |   |
 * |   |   |   - Tool usage statistics                               |     |   |
 * |   |   |   - Suspicious output patterns                          |     |   |
 * |   |   |   - Resource consumption reports                        |     |   |
 * |   |   |   - Approval requests                                   |     |   |
 * |   |   +---------------------------------------------------------+     |   |
 * |   |          |                    |                      |             |   |
 * |   |          v                    v                      v             |   |
 * |   |   +----------------+   +----------------+   +------------------+   |   |
 * |   |   | Anomaly        |   |   Policy       |   |    Audit         |   |   |
 * |   |   | Detection      |<--|   Evaluation   |<--|    Logging       |   |   |
 * |   |   +----------------+   +----------------+   +------------------+   |   |
 * |   |                                                                    |   |
 * |   +--------------------------------------------------------------------+   |
 * |                                                                            |
 * +===========================================================================+
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 * - bridge_base_t as first member
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SECURITY_RCOG_BRIDGE_H
#define NIMCP_SECURITY_RCOG_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Base bridge infrastructure */
#include "utils/bridge/nimcp_bridge_base.h"

/* Security module integrations */
#include "security/nimcp_policy_engine.h"
#include "security/nimcp_rate_limiter.h"

/* Recursive cognition integrations */
#include "cognitive/recursive/nimcp_rcog_types.h"

/* Common utilities */
#include "utils/validation/nimcp_common.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants and Magic Numbers
 * ============================================================================ */

/** @brief Magic number for bridge validation */
#define NIMCP_SECURITY_RCOG_BRIDGE_MAGIC 0x53524342  /* 'SRCB' */

/** @brief Bridge version */
#define NIMCP_SECURITY_RCOG_BRIDGE_VERSION 0x0100

/** @brief Maximum tool name length */
#define SECURITY_RCOG_MAX_TOOL_NAME 64

/** @brief Maximum tools in whitelist */
#define SECURITY_RCOG_MAX_WHITELISTED_TOOLS 256

/** @brief Maximum pending approval requests */
#define SECURITY_RCOG_MAX_APPROVAL_REQUESTS 32

/** @brief Default recursion depth limit */
#define SECURITY_RCOG_DEFAULT_MAX_DEPTH 16

/** @brief Default resource budget per request */
#define SECURITY_RCOG_DEFAULT_RESOURCE_BUDGET 1000000

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

struct rcog_engine;
struct rcog_tool_router;

/* ============================================================================
 * Security Flags and Enumerations
 * ============================================================================ */

/**
 * @brief Security flags for tool execution results
 */
typedef enum {
    SECURITY_RCOG_FLAG_NONE              = 0,
    SECURITY_RCOG_FLAG_WHITELISTED       = (1 << 0),  /**< Tool was whitelisted */
    SECURITY_RCOG_FLAG_PARAMS_VALIDATED  = (1 << 1),  /**< Parameters passed validation */
    SECURITY_RCOG_FLAG_OUTPUT_VALIDATED  = (1 << 2),  /**< Output passed validation */
    SECURITY_RCOG_FLAG_SANDBOXED         = (1 << 3),  /**< Executed in sandbox */
    SECURITY_RCOG_FLAG_RESOURCE_LIMITED  = (1 << 4),  /**< Resource limits applied */
    SECURITY_RCOG_FLAG_HUMAN_APPROVED    = (1 << 5),  /**< Human approved execution */
    SECURITY_RCOG_FLAG_OUTPUT_REDACTED   = (1 << 6),  /**< Output was redacted */
    SECURITY_RCOG_FLAG_SUSPICIOUS        = (1 << 7),  /**< Suspicious activity detected */
    SECURITY_RCOG_FLAG_BLOCKED           = (1 << 8),  /**< Execution was blocked */
    SECURITY_RCOG_FLAG_RATE_LIMITED      = (1 << 9),  /**< Rate limit was hit */
    SECURITY_RCOG_FLAG_DEPTH_LIMITED     = (1 << 10)  /**< Recursion depth limit hit */
} security_rcog_flags_t;

/**
 * @brief Validation result codes
 */
typedef enum {
    SECURITY_RCOG_VALID = 0,             /**< Validation passed */
    SECURITY_RCOG_INVALID_TOOL,          /**< Tool not in whitelist */
    SECURITY_RCOG_INVALID_PARAMS,        /**< Parameters failed validation */
    SECURITY_RCOG_INVALID_OUTPUT,        /**< Output failed validation */
    SECURITY_RCOG_RESOURCE_EXCEEDED,     /**< Resource budget exceeded */
    SECURITY_RCOG_DEPTH_EXCEEDED,        /**< Recursion depth exceeded */
    SECURITY_RCOG_APPROVAL_REQUIRED,     /**< Human approval required */
    SECURITY_RCOG_APPROVAL_DENIED,       /**< Human approval was denied */
    SECURITY_RCOG_RATE_LIMITED,          /**< Rate limit exceeded */
    SECURITY_RCOG_POLICY_DENIED,         /**< Policy engine denied */
    SECURITY_RCOG_SANDBOX_FAILED,        /**< Sandbox setup failed */
    SECURITY_RCOG_OUTPUT_REDACTED        /**< Output was redacted for safety */
} security_rcog_validation_result_t;

/**
 * @brief Approval status for human-in-the-loop
 */
typedef enum {
    SECURITY_RCOG_APPROVAL_PENDING = 0,  /**< Waiting for human decision */
    SECURITY_RCOG_APPROVAL_APPROVED,     /**< Human approved the request */
    SECURITY_RCOG_APPROVAL_DENIED_ONCE,  /**< Denied for this request only */
    SECURITY_RCOG_APPROVAL_DENIED_PERM,  /**< Permanently denied */
    SECURITY_RCOG_APPROVAL_TIMEOUT       /**< Approval request timed out */
} security_rcog_approval_status_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief Security-rcog bridge configuration
 *
 * WHAT: Configuration for security-rcog integration
 * WHY:  Enable fine-grained control over security features
 * HOW:  Boolean flags for each security feature
 */
typedef struct {
    /* Feature enables */
    bool enable_tool_whitelisting;        /**< Enforce tool whitelist */
    bool enable_output_validation;        /**< Validate tool outputs */
    bool enable_recursion_limits;         /**< Enforce recursion depth limits */
    bool enable_resource_tracking;        /**< Track resource consumption */
    bool enable_sandbox_execution;        /**< Execute tools in sandbox */
    bool enable_human_approval;           /**< Enable HITL for sensitive tools */
    bool enable_parameter_validation;     /**< Validate tool parameters */
    bool enable_audit_logging;            /**< Log all tool executions */

    /* Limits */
    uint32_t max_recursion_depth;         /**< Maximum recursion depth */
    uint64_t default_resource_budget;     /**< Default resource budget per request */
    uint32_t max_concurrent_tools;        /**< Max concurrent tool executions */
    uint32_t approval_timeout_ms;         /**< Timeout for human approval (0=infinite) */

    /* Rate limiting */
    float tools_per_second;               /**< Global tool rate limit */
    uint32_t tool_burst_size;             /**< Burst allowance for tools */

    /* Sensitivity settings */
    float suspicious_output_threshold;    /**< Threshold for suspicious output [0-1] */
    float resource_warning_threshold;     /**< Warn at this % of budget */
} security_rcog_config_t;

/* ============================================================================
 * Tool Permission Structure
 * ============================================================================ */

/**
 * @brief Permission entry for a tool
 *
 * WHAT: Defines permissions and limits for a specific tool
 * WHY:  Fine-grained control over individual tool access
 * HOW:  Stored in whitelist, checked before execution
 */
typedef struct {
    char tool_name[SECURITY_RCOG_MAX_TOOL_NAME];  /**< Tool identifier */
    bool allowed;                          /**< Tool is whitelisted */
    uint32_t max_calls_per_request;        /**< Max calls per goal request */
    uint64_t resource_budget;              /**< Resource budget for this tool */
    bool requires_approval;                /**< Needs human approval */
    bool requires_sandbox;                 /**< Must execute in sandbox */
    rcog_capability_tier_t min_tier;       /**< Minimum capability tier */
    bool allow_recursive_calls;            /**< Can call other tools */
    uint32_t cooldown_ms;                  /**< Cooldown between calls */
} security_rcog_tool_permission_t;

/* ============================================================================
 * Tool Execution Result Structure
 * ============================================================================ */

/**
 * @brief Result of a security-wrapped tool execution
 *
 * WHAT: Complete execution result with security metadata
 * WHY:  Track security state through entire tool lifecycle
 * HOW:  Populated by security layer before returning to rcog
 */
typedef struct {
    bool success;                          /**< Execution succeeded */
    security_rcog_validation_result_t validation_result; /**< Validation outcome */
    bool output_valid;                     /**< Output passed validation */
    uint64_t resources_used;               /**< Resources consumed */
    security_rcog_flags_t security_flags;  /**< Flags set during execution */
    uint64_t execution_time_us;            /**< Execution duration */
    const char* error_message;             /**< Error description if failed */
    float suspicious_score;                /**< Suspicious activity score [0-1] */
    uint32_t depth_at_execution;           /**< Recursion depth when executed */
} security_rcog_execution_result_t;

/* ============================================================================
 * Effect Structures (Bidirectional)
 * ============================================================================ */

/**
 * @brief Security effects on recursive cognition
 *
 * WHAT: How security state modulates rcog behavior
 * WHY:  Active threats should restrict tool access
 * HOW:  Reduces available tools, depth, and resources
 */
typedef struct {
    /* Tool restrictions */
    uint32_t whitelisted_tool_count;       /**< Number of whitelisted tools */
    uint32_t blocked_tool_count;           /**< Number of blocked tools */
    bool emergency_tool_lockdown;          /**< All non-essential tools blocked */

    /* Depth restrictions */
    uint32_t effective_max_depth;          /**< Current effective depth limit */
    float depth_reduction_factor;          /**< Depth reduction [0-1] */

    /* Resource restrictions */
    uint64_t effective_resource_budget;    /**< Current effective budget */
    float resource_reduction_factor;       /**< Budget reduction [0-1] */

    /* Rate restrictions */
    float effective_rate_limit;            /**< Current effective rate */
    bool rate_limiting_active;             /**< Rate limiter is active */

    /* Approval state */
    uint32_t pending_approvals;            /**< Pending human approvals */
    bool approval_queue_full;              /**< Approval queue is full */
} security_to_rcog_effects_t;

/**
 * @brief Recursive cognition effects on security
 *
 * WHAT: How rcog activity informs security decisions
 * WHY:  Tool usage patterns may indicate threats
 * HOW:  Reports usage, detects suspicious patterns
 */
typedef struct {
    /* Tool usage statistics */
    uint64_t total_tool_calls;             /**< Total tool invocations */
    uint64_t blocked_tool_calls;           /**< Blocked invocations */
    uint64_t rate_limited_calls;           /**< Rate limited calls */
    float avg_tools_per_request;           /**< Average tools per goal */

    /* Suspicious activity */
    uint32_t suspicious_outputs;           /**< Suspicious output count */
    uint32_t failed_validations;           /**< Failed validation count */
    float current_suspicious_score;        /**< Aggregate suspicious score */

    /* Resource tracking */
    uint64_t total_resources_used;         /**< Total resources consumed */
    float resource_utilization;            /**< % of budget used */
    uint32_t resource_warnings;            /**< Budget warnings issued */

    /* Depth tracking */
    uint32_t max_depth_reached;            /**< Maximum depth reached */
    uint32_t depth_limit_hits;             /**< Times depth limit hit */

    /* Approval tracking */
    uint32_t approval_requests;            /**< Total approval requests */
    uint32_t approvals_granted;            /**< Approvals granted */
    uint32_t approvals_denied;             /**< Approvals denied */
} rcog_to_security_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief Approval request entry
 */
typedef struct {
    uint64_t request_id;                   /**< Unique request ID */
    char tool_name[SECURITY_RCOG_MAX_TOOL_NAME]; /**< Tool requesting approval */
    rcog_capability_tier_t tier;           /**< Capability tier */
    security_rcog_approval_status_t status; /**< Current status */
    uint64_t requested_at_ms;              /**< Request timestamp */
    uint64_t resolved_at_ms;               /**< Resolution timestamp */
    char reason[256];                      /**< Reason for approval request */
} security_rcog_approval_request_t;

/**
 * @brief Security-rcog bridge state
 */
typedef struct {
    /* Connection state */
    bool rcog_engine_connected;            /**< RCOG engine connected */
    bool tool_router_connected;            /**< Tool router connected */
    bool policy_engine_connected;          /**< Policy engine connected */
    bool rate_limiter_connected;           /**< Rate limiter connected */

    /* Operational state */
    bool is_active;                        /**< Bridge is active */
    bool emergency_lockdown;               /**< Emergency tool lockdown */
    uint32_t current_depth;                /**< Current recursion depth */
    uint64_t current_resource_usage;       /**< Current resource usage */

    /* Whitelist state */
    uint32_t whitelisted_count;            /**< Tools in whitelist */
    uint32_t blocked_count;                /**< Permanently blocked tools */

    /* Approval queue state */
    uint32_t pending_approvals;            /**< Pending approval requests */
    uint64_t oldest_pending_ms;            /**< Age of oldest pending request */
} security_rcog_state_t;

/**
 * @brief Security-rcog bridge statistics
 */
typedef struct {
    /* Tool execution statistics */
    uint64_t total_tool_executions;        /**< Total tool executions */
    uint64_t successful_executions;        /**< Successful executions */
    uint64_t blocked_executions;           /**< Blocked executions */
    uint64_t sandboxed_executions;         /**< Sandboxed executions */

    /* Validation statistics */
    uint64_t param_validations;            /**< Parameter validations */
    uint64_t param_validation_failures;    /**< Parameter validation failures */
    uint64_t output_validations;           /**< Output validations */
    uint64_t output_validation_failures;   /**< Output validation failures */
    uint64_t outputs_redacted;             /**< Outputs that were redacted */

    /* Rate limiting statistics */
    uint64_t rate_limit_checks;            /**< Rate limit checks */
    uint64_t rate_limit_hits;              /**< Rate limit hits */

    /* Resource statistics */
    uint64_t total_resources_consumed;     /**< Total resources consumed */
    uint64_t resource_limit_hits;          /**< Resource limit hits */
    float avg_resource_per_tool;           /**< Average resource per tool */

    /* Depth statistics */
    uint64_t depth_checks;                 /**< Depth limit checks */
    uint64_t depth_limit_hits;             /**< Depth limit hits */
    uint32_t max_depth_observed;           /**< Maximum depth observed */

    /* Approval statistics */
    uint64_t approval_requests;            /**< Total approval requests */
    uint64_t approvals_granted;            /**< Approvals granted */
    uint64_t approvals_denied;             /**< Approvals denied */
    uint64_t approvals_timeout;            /**< Approval timeouts */
    float avg_approval_time_ms;            /**< Average approval time */

    /* Suspicious activity statistics */
    uint64_t suspicious_patterns_detected; /**< Suspicious patterns found */
    float peak_suspicious_score;           /**< Peak suspicious score */

    /* Performance metrics */
    float avg_validation_time_us;          /**< Average validation time */
    float avg_sandbox_overhead_us;         /**< Average sandbox overhead */
} security_rcog_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief Security-rcog bridge main structure
 *
 * WHAT: Main bridge connecting security module to recursive cognition
 * WHY:  Centralized security enforcement for all tool operations
 * HOW:  Contains config, effects, state, stats, and system handles
 */
typedef struct {
    bridge_base_t base;                    /**< MUST be first: base bridge */

    /* Configuration */
    security_rcog_config_t config;         /**< Bridge configuration */

    /* System connections */
    struct rcog_engine* rcog_engine;       /**< Recursive cognition engine */
    struct rcog_tool_router* tool_router;  /**< Tool router */
    nimcp_policy_engine_t policy_engine;   /**< Policy engine */
    nimcp_rate_limiter_t rate_limiter;     /**< Rate limiter */

    /* Tool whitelist */
    security_rcog_tool_permission_t* whitelist; /**< Tool permission list */
    size_t whitelist_count;                /**< Number of whitelist entries */
    size_t whitelist_capacity;             /**< Whitelist capacity */

    /* Approval queue */
    security_rcog_approval_request_t* approval_queue; /**< Pending approvals */
    size_t approval_queue_count;           /**< Current queue size */

    /* Bidirectional effects */
    security_to_rcog_effects_t security_effects;  /**< Security -> RCOG effects */
    rcog_to_security_effects_t rcog_effects;      /**< RCOG -> Security effects */

    /* State and statistics */
    security_rcog_state_t state;           /**< Current bridge state */
    security_rcog_stats_t stats;           /**< Bridge statistics */

    /* Tracking */
    uint64_t current_request_id;           /**< Current goal request ID */
    uint32_t tools_called_this_request;    /**< Tools called in current request */

} security_rcog_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default security-rcog bridge configuration
 *
 * WHAT: Provide sensible defaults for security-rcog integration
 * WHY:  Easy initialization with security-focused defaults
 * HOW:  Return config with all features enabled, conservative limits
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 */
int security_rcog_default_config(security_rcog_config_t* config);

/**
 * @brief Create security-rcog bridge
 *
 * WHAT: Initialize bidirectional security-rcog integration
 * WHY:  Enable security oversight of all tool operations
 * HOW:  Allocate bridge, initialize state, prepare whitelist
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
security_rcog_bridge_t* security_rcog_bridge_create(
    const security_rcog_config_t* config
);

/**
 * @brief Destroy security-rcog bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free whitelist, approval queue, destroy base
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void security_rcog_bridge_destroy(security_rcog_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect rcog engine to bridge
 *
 * WHAT: Connect recursive cognition engine for security oversight
 * WHY:  Engine manages goal processing that security must monitor
 * HOW:  Store handle, register for engine events
 *
 * @param bridge Security-rcog bridge
 * @param engine RCOG engine to connect
 * @return 0 on success, error code on failure
 */
int security_rcog_connect_engine(
    security_rcog_bridge_t* bridge,
    struct rcog_engine* engine
);

/**
 * @brief Connect tool router to bridge
 *
 * WHAT: Connect tool router for tool-level security
 * WHY:  Router handles all tool invocations requiring security checks
 * HOW:  Store handle, intercept tool calls
 *
 * @param bridge Security-rcog bridge
 * @param router Tool router to connect
 * @return 0 on success, error code on failure
 */
int security_rcog_connect_tool_router(
    security_rcog_bridge_t* bridge,
    struct rcog_tool_router* router
);

/**
 * @brief Connect policy engine to bridge
 *
 * WHAT: Connect policy engine for policy-based decisions
 * WHY:  Policies define allowed tools and parameters
 * HOW:  Store handle, register for policy evaluation
 *
 * @param bridge Security-rcog bridge
 * @param policy_engine Policy engine to connect
 * @return 0 on success, error code on failure
 */
int security_rcog_connect_policy_engine(
    security_rcog_bridge_t* bridge,
    nimcp_policy_engine_t policy_engine
);

/**
 * @brief Connect rate limiter to bridge
 *
 * WHAT: Connect rate limiter for tool rate limiting
 * WHY:  Prevent tool abuse and resource exhaustion
 * HOW:  Store handle, check before tool execution
 *
 * @param bridge Security-rcog bridge
 * @param rate_limiter Rate limiter to connect
 * @return 0 on success, error code on failure
 */
int security_rcog_connect_rate_limiter(
    security_rcog_bridge_t* bridge,
    nimcp_rate_limiter_t rate_limiter
);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Security-rcog bridge
 * @return true if all required systems connected
 */
bool security_rcog_is_connected(const security_rcog_bridge_t* bridge);

/* ============================================================================
 * Whitelist Management API
 * ============================================================================ */

/**
 * @brief Check if tool is whitelisted
 *
 * WHAT: Check if a tool is approved for execution
 * WHY:  Prevent execution of unapproved tools
 * HOW:  Lookup in whitelist, check tier requirements
 *
 * @param bridge Security-rcog bridge
 * @param tool_name Tool name to check
 * @param tier Capability tier requesting the tool
 * @return true if tool is whitelisted for the tier
 */
bool security_rcog_is_tool_whitelisted(
    const security_rcog_bridge_t* bridge,
    const char* tool_name,
    rcog_capability_tier_t tier
);

/**
 * @brief Add tool to whitelist
 *
 * WHAT: Add a tool to the approved list
 * WHY:  Allow execution of specific tools
 * HOW:  Add permission entry to whitelist
 *
 * @param bridge Security-rcog bridge
 * @param permission Tool permission to add
 * @return 0 on success, error code on failure
 */
int security_rcog_whitelist_tool(
    security_rcog_bridge_t* bridge,
    const security_rcog_tool_permission_t* permission
);

/**
 * @brief Remove tool from whitelist
 *
 * WHAT: Remove a tool from approved list
 * WHY:  Revoke tool access
 * HOW:  Remove permission entry from whitelist
 *
 * @param bridge Security-rcog bridge
 * @param tool_name Tool to remove
 * @return 0 on success, error code on failure
 */
int security_rcog_unwhitelist_tool(
    security_rcog_bridge_t* bridge,
    const char* tool_name
);

/**
 * @brief Get tool permission
 *
 * WHAT: Retrieve permission entry for a tool
 * WHY:  Query tool-specific limits and requirements
 * HOW:  Lookup in whitelist
 *
 * @param bridge Security-rcog bridge
 * @param tool_name Tool to query
 * @param permission Output permission (can be NULL)
 * @return 0 on success, error code if not found
 */
int security_rcog_get_tool_permission(
    const security_rcog_bridge_t* bridge,
    const char* tool_name,
    security_rcog_tool_permission_t* permission
);

/* ============================================================================
 * Validation API
 * ============================================================================ */

/**
 * @brief Validate tool parameters
 *
 * WHAT: Validate parameters before tool execution
 * WHY:  Prevent injection attacks and malformed input
 * HOW:  Check against tool schema and security rules
 *
 * @param bridge Security-rcog bridge
 * @param tool_name Tool being invoked
 * @param params Parameter data
 * @param params_size Parameter size
 * @return Validation result code
 */
security_rcog_validation_result_t security_rcog_validate_tool_params(
    security_rcog_bridge_t* bridge,
    const char* tool_name,
    const void* params,
    size_t params_size
);

/**
 * @brief Validate tool output
 *
 * WHAT: Validate output after tool execution
 * WHY:  Prevent data exfiltration and harmful content
 * HOW:  Check output against security patterns
 *
 * @param bridge Security-rcog bridge
 * @param tool_name Tool that produced output
 * @param output Output data
 * @param output_size Output size
 * @param suspicious_score Output: suspicious score [0-1]
 * @return Validation result code
 */
security_rcog_validation_result_t security_rcog_validate_tool_output(
    security_rcog_bridge_t* bridge,
    const char* tool_name,
    const void* output,
    size_t output_size,
    float* suspicious_score
);

/* ============================================================================
 * Execution API
 * ============================================================================ */

/**
 * @brief Execute tool with security sandbox
 *
 * WHAT: Execute tool with full security wrapper
 * WHY:  Isolate tool execution for safety
 * HOW:  Validate, sandbox, execute, validate output
 *
 * @param bridge Security-rcog bridge
 * @param tool_name Tool to execute
 * @param input Input data
 * @param input_size Input size
 * @param output Output buffer (caller allocates)
 * @param output_size Output buffer size
 * @param actual_output_size Output: actual size written
 * @param result Output: execution result
 * @return 0 on success, error code on failure
 */
int security_rcog_execute_with_sandbox(
    security_rcog_bridge_t* bridge,
    const char* tool_name,
    const void* input,
    size_t input_size,
    void* output,
    size_t output_size,
    size_t* actual_output_size,
    security_rcog_execution_result_t* result
);

/* ============================================================================
 * Limit Checking API
 * ============================================================================ */

/**
 * @brief Check recursion depth
 *
 * WHAT: Check if current depth is within limits
 * WHY:  Prevent infinite recursion attacks
 * HOW:  Compare against configured maximum
 *
 * @param bridge Security-rcog bridge
 * @param current_depth Current recursion depth
 * @return true if depth is allowed, false if limit exceeded
 */
bool security_rcog_check_recursion_depth(
    const security_rcog_bridge_t* bridge,
    uint32_t current_depth
);

/**
 * @brief Track resource usage
 *
 * WHAT: Record resource consumption for a tool call
 * WHY:  Enforce resource budgets
 * HOW:  Add to running total, check against budget
 *
 * @param bridge Security-rcog bridge
 * @param tool_name Tool that consumed resources
 * @param resources_used Resources consumed
 * @return 0 if within budget, error code if exceeded
 */
int security_rcog_track_resource_usage(
    security_rcog_bridge_t* bridge,
    const char* tool_name,
    uint64_t resources_used
);

/* ============================================================================
 * Human Approval API
 * ============================================================================ */

/**
 * @brief Request human approval for tool execution
 *
 * WHAT: Escalate sensitive tool to human for approval
 * WHY:  Human-in-the-loop for high-risk operations
 * HOW:  Queue request, wait for decision or timeout
 *
 * @param bridge Security-rcog bridge
 * @param tool_name Tool requiring approval
 * @param tier Capability tier requesting
 * @param reason Reason for the approval request
 * @param request_id Output: request ID for tracking
 * @return Approval status (may be pending)
 */
security_rcog_approval_status_t security_rcog_require_human_approval(
    security_rcog_bridge_t* bridge,
    const char* tool_name,
    rcog_capability_tier_t tier,
    const char* reason,
    uint64_t* request_id
);

/**
 * @brief Check approval status
 *
 * WHAT: Check status of pending approval request
 * WHY:  Non-blocking approval status check
 * HOW:  Lookup in approval queue
 *
 * @param bridge Security-rcog bridge
 * @param request_id Request ID to check
 * @return Current approval status
 */
security_rcog_approval_status_t security_rcog_check_approval_status(
    const security_rcog_bridge_t* bridge,
    uint64_t request_id
);

/**
 * @brief Resolve approval request (for approval handler)
 *
 * WHAT: Set the result of an approval request
 * WHY:  Allow external approval handler to provide decision
 * HOW:  Update approval queue entry
 *
 * @param bridge Security-rcog bridge
 * @param request_id Request to resolve
 * @param status Approval decision
 * @return 0 on success, error code on failure
 */
int security_rcog_resolve_approval(
    security_rcog_bridge_t* bridge,
    uint64_t request_id,
    security_rcog_approval_status_t status
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update security effects on rcog (outbound)
 *
 * WHAT: Compute how security state modulates rcog behavior
 * WHY:  Security threats should restrict tool access
 * HOW:  Aggregate security state, compute restrictions
 *
 * @param bridge Security-rcog bridge
 * @return 0 on success, error code on failure
 */
int security_rcog_update_security_effects(security_rcog_bridge_t* bridge);

/**
 * @brief Update rcog effects on security (inbound)
 *
 * WHAT: Process rcog activity for security analysis
 * WHY:  Tool usage patterns may indicate threats
 * HOW:  Aggregate statistics, compute suspicious score
 *
 * @param bridge Security-rcog bridge
 * @return 0 on success, error code on failure
 */
int security_rcog_update_rcog_effects(security_rcog_bridge_t* bridge);

/**
 * @brief Full update cycle (both directions)
 *
 * WHAT: Execute complete bidirectional update
 * WHY:  Single call for regular update loops
 * HOW:  Update both directions, process pending items
 *
 * @param bridge Security-rcog bridge
 * @param delta_ms Time since last update
 * @return 0 on success, error code on failure
 */
int security_rcog_bridge_update(
    security_rcog_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get security effects on rcog
 *
 * @param bridge Security-rcog bridge
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int security_rcog_get_security_effects(
    const security_rcog_bridge_t* bridge,
    security_to_rcog_effects_t* effects
);

/**
 * @brief Get rcog effects on security
 *
 * @param bridge Security-rcog bridge
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int security_rcog_get_rcog_effects(
    const security_rcog_bridge_t* bridge,
    rcog_to_security_effects_t* effects
);

/**
 * @brief Get bridge state
 *
 * @param bridge Security-rcog bridge
 * @param state Output state structure
 * @return 0 on success, error code on failure
 */
int security_rcog_get_state(
    const security_rcog_bridge_t* bridge,
    security_rcog_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Security-rcog bridge
 * @param stats Output statistics structure
 * @return 0 on success, error code on failure
 */
int security_rcog_get_stats(
    const security_rcog_bridge_t* bridge,
    security_rcog_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Security-rcog bridge
 * @return 0 on success, error code on failure
 */
int security_rcog_reset_stats(security_rcog_bridge_t* bridge);

/* ============================================================================
 * Emergency Mode API
 * ============================================================================ */

/**
 * @brief Enter emergency tool lockdown
 *
 * WHAT: Block all non-essential tools
 * WHY:  Critical security threat detected
 * HOW:  Set lockdown flag, update effects
 *
 * @param bridge Security-rcog bridge
 * @return 0 on success, error code on failure
 */
int security_rcog_enter_lockdown(security_rcog_bridge_t* bridge);

/**
 * @brief Exit emergency tool lockdown
 *
 * WHAT: Restore normal tool access
 * WHY:  Threat has passed
 * HOW:  Clear lockdown flag, update effects
 *
 * @param bridge Security-rcog bridge
 * @return 0 on success, error code on failure
 */
int security_rcog_exit_lockdown(security_rcog_bridge_t* bridge);

/**
 * @brief Check if in lockdown mode
 *
 * @param bridge Security-rcog bridge
 * @return true if lockdown active
 */
bool security_rcog_is_lockdown(const security_rcog_bridge_t* bridge);

/* ============================================================================
 * Request Lifecycle API
 * ============================================================================ */

/**
 * @brief Begin tracking a new goal request
 *
 * WHAT: Start tracking for a new rcog goal processing
 * WHY:  Reset per-request counters and budgets
 * HOW:  Reset counters, start fresh tracking
 *
 * @param bridge Security-rcog bridge
 * @param request_id Goal request identifier
 * @return 0 on success, error code on failure
 */
int security_rcog_begin_request(
    security_rcog_bridge_t* bridge,
    uint64_t request_id
);

/**
 * @brief End tracking for goal request
 *
 * WHAT: Finalize tracking for completed goal
 * WHY:  Record final statistics, cleanup
 * HOW:  Update aggregates, clear per-request state
 *
 * @param bridge Security-rcog bridge
 * @param request_id Goal request identifier
 * @return 0 on success, error code on failure
 */
int security_rcog_end_request(
    security_rcog_bridge_t* bridge,
    uint64_t request_id
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_RCOG_BRIDGE_H */
