/**
 * @file nimcp_lgss_override_controller.h
 * @brief LGSS Override Controller - Emergency control mechanisms
 *
 * WHAT: Override Controller for emergency interventions in cognitive operations
 * WHY:  Enable authorized operators to halt, reset, or reduce system capabilities
 * HOW:  Request-based override system with authentication placeholder for Phase B
 *
 * SECURITY CRITICAL: Override operations require authentication (Phase B).
 *                    All override actions are audited.
 *
 * OVERRIDE TYPES:
 * - HALT: Immediately stop all cognitive operations
 * - SOFT_RESET: Gracefully restart cognitive processes
 * - HARD_RESET: Force restart with state clear
 * - REDUCE_CAPABILITY: Limit system capabilities
 *
 * ARCHITECTURE:
 *   +------------------+     +---------------------+
 *   | Human Operator   | --> | Override Controller |
 *   +------------------+     +---------------------+
 *           |                        |
 *           v                        v
 *     [Auth Token]           [Override Request]
 *           |                        |
 *           v                        v
 *     +-------------------+    +------------------+
 *     | Auth Validator    |    | Override         |
 *     | (Phase B)         |    | Executor         |
 *     +-------------------+    +------------------+
 *                                    |
 *                                    v
 *                             +----------------+
 *                             | Audit Logger   |
 *                             +----------------+
 *
 * THREAD SAFETY: All functions are thread-safe with internal mutex protection.
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 1.0.0
 */

#ifndef NIMCP_LGSS_OVERRIDE_CONTROLLER_H
#define NIMCP_LGSS_OVERRIDE_CONTROLLER_H

#include "utils/validation/nimcp_common.h"
#include "utils/error/nimcp_error_codes.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS AND MAGIC NUMBERS
 *============================================================================*/

/** @brief Magic number for override controller validation ('OVRC') */
#define NIMCP_OVERRIDE_CONTROLLER_MAGIC 0x4F565243

/** @brief Maximum override reason length */
#define NIMCP_OVERRIDE_MAX_REASON_LEN 512

/** @brief Maximum operator ID length */
#define NIMCP_OVERRIDE_MAX_OPERATOR_ID 64

/** @brief Maximum pending override requests */
#define NIMCP_OVERRIDE_MAX_PENDING 64

/** @brief Maximum capability reduction parameters */
#define NIMCP_OVERRIDE_MAX_CAPABILITY_PARAMS 16

/** @brief Default override timeout (milliseconds) */
#define NIMCP_OVERRIDE_DEFAULT_TIMEOUT_MS 30000

/*=============================================================================
 * ENUMERATIONS
 *============================================================================*/

/**
 * @brief Override type enumeration
 *
 * WHAT: Types of override operations available
 * WHY:  Different severity levels of intervention
 * HOW:  Enumerated values with increasing severity
 */
typedef enum {
    /** @brief Immediately halt all cognitive operations */
    OVERRIDE_TYPE_HALT = 0,

    /** @brief Graceful restart of cognitive processes */
    OVERRIDE_TYPE_SOFT_RESET = 1,

    /** @brief Force restart with state clear */
    OVERRIDE_TYPE_HARD_RESET = 2,

    /** @brief Reduce system capabilities */
    OVERRIDE_TYPE_REDUCE_CAPABILITY = 3,

    /** @brief Number of override types */
    OVERRIDE_TYPE_COUNT
} override_type_t;

/**
 * @brief Override request status
 */
typedef enum {
    /** @brief Request is pending authorization */
    OVERRIDE_STATUS_PENDING = 0,

    /** @brief Request is authorized and queued */
    OVERRIDE_STATUS_AUTHORIZED = 1,

    /** @brief Request is being executed */
    OVERRIDE_STATUS_EXECUTING = 2,

    /** @brief Request completed successfully */
    OVERRIDE_STATUS_COMPLETED = 3,

    /** @brief Request failed during execution */
    OVERRIDE_STATUS_FAILED = 4,

    /** @brief Request was cancelled */
    OVERRIDE_STATUS_CANCELLED = 5,

    /** @brief Request was rejected (auth failed or invalid) */
    OVERRIDE_STATUS_REJECTED = 6
} override_status_t;

/**
 * @brief Capability types that can be reduced
 */
typedef enum {
    /** @brief Learning/plasticity operations */
    CAPABILITY_LEARNING = 0,

    /** @brief Memory formation */
    CAPABILITY_MEMORY_FORMATION = 1,

    /** @brief External communication */
    CAPABILITY_EXTERNAL_COMM = 2,

    /** @brief Resource allocation */
    CAPABILITY_RESOURCE_ALLOC = 3,

    /** @brief Autonomous decision making */
    CAPABILITY_AUTONOMOUS_DECISION = 4,

    /** @brief All capabilities (master switch) */
    CAPABILITY_ALL = 5,

    /** @brief Number of capability types */
    CAPABILITY_TYPE_COUNT
} capability_type_t;

/*=============================================================================
 * STRUCTURES
 *============================================================================*/

/**
 * @brief Capability reduction parameters
 */
typedef struct {
    /** @brief Capability to reduce */
    capability_type_t capability;

    /** @brief Reduction level [0.0, 1.0] - 0 = no reduction, 1 = fully disabled */
    float reduction_level;

    /** @brief Duration in seconds (0 = indefinite until manually restored) */
    uint32_t duration_sec;
} capability_reduction_t;

/**
 * @brief Override request structure
 *
 * WHAT: Encapsulates an override request with all necessary information
 * WHY:  Provide complete context for authorization and execution
 * HOW:  Override type, operator info, reason, and type-specific parameters
 */
typedef struct {
    /** @brief Unique request identifier */
    uint64_t request_id;

    /** @brief Override type requested */
    override_type_t type;

    /** @brief Operator ID (for authentication - Phase B) */
    char operator_id[NIMCP_OVERRIDE_MAX_OPERATOR_ID];

    /** @brief Reason for override (required for audit) */
    char reason[NIMCP_OVERRIDE_MAX_REASON_LEN];

    /** @brief Request timestamp (microseconds since epoch) */
    uint64_t timestamp_us;

    /** @brief Request timeout (milliseconds, 0 = default) */
    uint32_t timeout_ms;

    /** @brief Current request status */
    override_status_t status;

    /** @brief Capability reductions (for REDUCE_CAPABILITY type) */
    capability_reduction_t capabilities[NIMCP_OVERRIDE_MAX_CAPABILITY_PARAMS];

    /** @brief Number of capability reductions */
    uint32_t capability_count;

    /** @brief Authentication token placeholder (Phase B integration) */
    uint8_t auth_token[64];

    /** @brief Authentication token size */
    size_t auth_token_size;

    /** @brief Priority (higher = more urgent) */
    uint32_t priority;
} override_request_t;

/**
 * @brief Override execution result
 */
typedef struct {
    /** @brief Request ID this result refers to */
    uint64_t request_id;

    /** @brief Final status */
    override_status_t status;

    /** @brief Execution start timestamp */
    uint64_t execution_start_us;

    /** @brief Execution end timestamp */
    uint64_t execution_end_us;

    /** @brief Error message (if failed) */
    char error_message[256];

    /** @brief Components affected */
    uint32_t components_affected;

    /** @brief True if rollback was performed */
    bool rollback_performed;
} override_result_t;

/**
 * @brief Override controller statistics
 */
typedef struct {
    /** @brief Total override requests received */
    uint64_t total_requests;

    /** @brief Requests authorized */
    uint64_t requests_authorized;

    /** @brief Requests rejected */
    uint64_t requests_rejected;

    /** @brief Requests executed successfully */
    uint64_t requests_completed;

    /** @brief Requests failed */
    uint64_t requests_failed;

    /** @brief Currently pending requests */
    uint32_t pending_requests;

    /** @brief HALTs executed */
    uint64_t halts_executed;

    /** @brief Soft resets executed */
    uint64_t soft_resets_executed;

    /** @brief Hard resets executed */
    uint64_t hard_resets_executed;

    /** @brief Capability reductions active */
    uint32_t active_capability_reductions;

    /** @brief Average execution time (microseconds) */
    uint64_t avg_execution_time_us;
} override_stats_t;

/**
 * @brief Override controller configuration
 */
typedef struct {
    /** @brief Maximum pending requests */
    uint32_t max_pending;

    /** @brief Default request timeout (milliseconds) */
    uint32_t default_timeout_ms;

    /** @brief Require authentication for all overrides (Phase B) */
    bool require_auth;

    /** @brief Enable audit logging */
    bool enable_audit_log;

    /** @brief Audit log path (NULL for default) */
    const char* audit_log_path;

    /** @brief Allow remote override requests */
    bool allow_remote;

    /** @brief Callback for override events */
    void (*event_callback)(const override_result_t* result, void* user_data);

    /** @brief Event callback user data */
    void* event_callback_data;
} override_config_t;

/*=============================================================================
 * OPAQUE TYPES
 *============================================================================*/

/**
 * @brief Opaque override controller handle
 */
typedef struct override_controller_impl* override_controller_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *============================================================================*/

/**
 * @brief Get default override controller configuration
 *
 * WHAT: Returns a configuration struct with sensible defaults
 * WHY:  Ensure secure defaults are used
 * HOW:  Static values with security-first approach
 *
 * @return Default configuration
 */
override_config_t override_controller_default_config(void);

/**
 * @brief Create override controller instance
 *
 * WHAT: Allocate and initialize an override controller
 * WHY:  Provide emergency intervention capability
 * HOW:  Allocate resources, initialize state, set configuration
 *
 * @param config Configuration (NULL for defaults)
 * @return Override controller handle or NULL on failure
 */
override_controller_t override_controller_create(const override_config_t* config);

/**
 * @brief Destroy override controller instance
 *
 * WHAT: Clean up and free override controller resources
 * WHY:  Prevent memory leaks
 * HOW:  Cancel pending requests, free internal state
 *
 * @param controller Controller handle to destroy
 */
void override_controller_destroy(override_controller_t controller);

/*=============================================================================
 * OVERRIDE REQUEST FUNCTIONS
 *============================================================================*/

/**
 * @brief Request an override operation
 *
 * WHAT: Submit an override request for authorization
 * WHY:  Initiate emergency intervention
 * HOW:  Validate request, queue for authorization
 *
 * SECURITY: Request will be validated against authentication requirements.
 *           All requests are logged for audit purposes.
 *
 * @param controller Controller handle
 * @param request Override request
 * @param request_id Output: assigned request ID
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t override_controller_request_override(
    override_controller_t controller,
    const override_request_t* request,
    uint64_t* request_id
);

/**
 * @brief Execute an authorized override
 *
 * WHAT: Execute a previously authorized override request
 * WHY:  Complete the override flow after authorization
 * HOW:  Perform the override operation, return result
 *
 * SECURITY: Only authorized requests can be executed.
 *           Execution is logged and audited.
 *
 * @param controller Controller handle
 * @param request_id Request ID to execute
 * @param result Output: execution result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t override_controller_execute_override(
    override_controller_t controller,
    uint64_t request_id,
    override_result_t* result
);

/**
 * @brief Cancel a pending override request
 *
 * WHAT: Cancel a request that hasn't been executed yet
 * WHY:  Allow operators to abort override before execution
 * HOW:  Mark request as cancelled if still pending
 *
 * @param controller Controller handle
 * @param request_id Request ID to cancel
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t override_controller_cancel_request(
    override_controller_t controller,
    uint64_t request_id
);

/**
 * @brief Get status of an override request
 *
 * @param controller Controller handle
 * @param request_id Request ID to query
 * @param request Output: request details
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t override_controller_get_request_status(
    override_controller_t controller,
    uint64_t request_id,
    override_request_t* request
);

/**
 * @brief Get list of pending override requests
 *
 * @param controller Controller handle
 * @param requests Output: array of pending requests (caller allocates)
 * @param max_count Maximum requests to retrieve
 * @param count Output: actual number of requests
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t override_controller_get_pending(
    override_controller_t controller,
    override_request_t* requests,
    uint32_t max_count,
    uint32_t* count
);

/*=============================================================================
 * CAPABILITY MANAGEMENT
 *============================================================================*/

/**
 * @brief Restore a reduced capability
 *
 * WHAT: Restore a capability that was reduced by override
 * WHY:  Allow recovery after emergency intervention
 * HOW:  Remove capability reduction, restore normal operation
 *
 * @param controller Controller handle
 * @param capability Capability to restore
 * @param operator_id Operator ID for audit
 * @param reason Reason for restoration
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t override_controller_restore_capability(
    override_controller_t controller,
    capability_type_t capability,
    const char* operator_id,
    const char* reason
);

/**
 * @brief Get current capability reduction level
 *
 * @param controller Controller handle
 * @param capability Capability to query
 * @param level Output: current reduction level [0.0, 1.0]
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t override_controller_get_capability_level(
    override_controller_t controller,
    capability_type_t capability,
    float* level
);

/*=============================================================================
 * STATISTICS AND AUDIT
 *============================================================================*/

/**
 * @brief Get override controller statistics
 *
 * @param controller Controller handle
 * @param stats Output: statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t override_controller_get_stats(
    override_controller_t controller,
    override_stats_t* stats
);

/**
 * @brief Reset statistics counters
 *
 * @param controller Controller handle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t override_controller_reset_stats(override_controller_t controller);

/*=============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * @brief Get override type name as string
 *
 * @param type Override type
 * @return Type name string
 */
const char* override_type_name(override_type_t type);

/**
 * @brief Get override status name as string
 *
 * @param status Override status
 * @return Status name string
 */
const char* override_status_name(override_status_t status);

/**
 * @brief Get capability type name as string
 *
 * @param capability Capability type
 * @return Capability name string
 */
const char* capability_type_name(capability_type_t capability);

/**
 * @brief Initialize an override request structure
 *
 * WHAT: Helper to initialize a request with basic parameters
 * WHY:  Simplify request creation
 * HOW:  Fill in request fields with provided values
 *
 * @param request Output: request to initialize
 * @param type Override type
 * @param operator_id Operator ID
 * @param reason Override reason
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t override_request_init(
    override_request_t* request,
    override_type_t type,
    const char* operator_id,
    const char* reason
);

/**
 * @brief Add capability reduction to request
 *
 * WHAT: Add a capability reduction parameter to a REDUCE_CAPABILITY request
 * WHY:  Build up capability reduction requests
 * HOW:  Append to request's capability array
 *
 * @param request Request to modify
 * @param capability Capability to reduce
 * @param level Reduction level [0.0, 1.0]
 * @param duration_sec Duration (0 = indefinite)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t override_request_add_capability(
    override_request_t* request,
    capability_type_t capability,
    float level,
    uint32_t duration_sec
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LGSS_OVERRIDE_CONTROLLER_H */
