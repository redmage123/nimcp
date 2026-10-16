/**
 * @file nimcp_hierarchical_recovery.h
 * @brief Hierarchical Recovery Orchestration System
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Multi-level recovery coordination (local, regional, global)
 * WHY:  Prevent cascading failures, enable efficient recovery at each level
 * HOW:  Tree-based recovery hierarchy, escalation policies, circuit breakers
 *
 * BIOLOGICAL BASIS:
 * - Autonomic nervous system (local reflexes before brain involvement)
 * - Immune system hierarchy (innate → adaptive → systemic)
 * - Pain response (spinal reflex → cortical processing → behavioral)
 *
 * RECOVERY HIERARCHY:
 * Level 0 (Node):     Local recovery, microsecond response
 * Level 1 (Pod):      Pod-level coordination, millisecond response
 * Level 2 (Region):   Regional orchestration, second response
 * Level 3 (Global):   Global coordination, multi-second response
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HIERARCHICAL_RECOVERY_H
#define NIMCP_HIERARCHICAL_RECOVERY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define HR_MAX_LEVELS 4                     /**< Max hierarchy levels */
#define HR_MAX_CHILDREN 16                  /**< Max children per node */
#define HR_MAX_POLICIES 32                  /**< Max escalation policies */
#define HR_MAX_CIRCUITS 64                  /**< Max circuit breakers */
#define HR_DEFAULT_TIMEOUT_MS 1000          /**< Default recovery timeout */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Recovery hierarchy levels
 */
typedef enum {
    HR_LEVEL_NODE = 0,      /**< Single node (microseconds) */
    HR_LEVEL_POD,           /**< Pod of nodes (milliseconds) */
    HR_LEVEL_REGION,        /**< Regional cluster (seconds) */
    HR_LEVEL_GLOBAL         /**< Global coordination (multi-second) */
} hr_level_t;

/**
 * @brief Escalation triggers
 */
typedef enum {
    HR_ESCALATE_TIMEOUT = 0,    /**< Recovery timeout exceeded */
    HR_ESCALATE_FAILURE,        /**< Recovery attempt failed */
    HR_ESCALATE_THRESHOLD,      /**< Failure count threshold */
    HR_ESCALATE_RESOURCE,       /**< Insufficient local resources */
    HR_ESCALATE_CASCADE,        /**< Cascading failure detected */
    HR_ESCALATE_MANUAL          /**< Manual escalation request */
} hr_escalation_trigger_t;

/**
 * @brief Circuit breaker states
 */
typedef enum {
    HR_CIRCUIT_CLOSED = 0,  /**< Normal operation */
    HR_CIRCUIT_OPEN,        /**< Failures detected, blocking */
    HR_CIRCUIT_HALF_OPEN    /**< Testing if recovered */
} hr_circuit_state_t;

/**
 * @brief Recovery action results
 */
typedef enum {
    HR_RESULT_SUCCESS = 0,  /**< Recovery successful */
    HR_RESULT_PARTIAL,      /**< Partial recovery */
    HR_RESULT_FAILED,       /**< Recovery failed */
    HR_RESULT_ESCALATED,    /**< Escalated to higher level */
    HR_RESULT_TIMEOUT,      /**< Recovery timeout */
    HR_RESULT_CIRCUIT_OPEN  /**< Circuit breaker triggered */
} hr_result_t;

/**
 * @brief Cascade prevention strategies
 */
typedef enum {
    HR_CASCADE_NONE = 0,        /**< No cascade prevention */
    HR_CASCADE_ISOLATION,       /**< Isolate failing component */
    HR_CASCADE_SHEDDING,        /**< Load shedding */
    HR_CASCADE_BACKPRESSURE,    /**< Apply backpressure */
    HR_CASCADE_BULKHEAD         /**< Bulkhead isolation */
} hr_cascade_strategy_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Circuit breaker configuration
 */
typedef struct {
    char name[64];                  /**< Circuit name */
    uint32_t failure_threshold;     /**< Failures before opening */
    uint32_t success_threshold;     /**< Successes to close */
    uint32_t timeout_ms;            /**< Open duration */
    uint32_t half_open_max;         /**< Max half-open attempts */
} hr_circuit_config_t;

/**
 * @brief Circuit breaker state
 */
typedef struct {
    hr_circuit_config_t config;     /**< Circuit configuration */
    hr_circuit_state_t state;       /**< Current state */
    uint32_t failure_count;         /**< Recent failures */
    uint32_t success_count;         /**< Recent successes */
    uint64_t last_failure_ms;       /**< Last failure timestamp */
    uint64_t last_state_change_ms;  /**< Last state transition */
    uint64_t total_calls;           /**< Total calls through circuit */
    uint64_t total_failures;        /**< Total historical failures */
} hr_circuit_breaker_t;

/**
 * @brief Escalation policy
 */
typedef struct {
    char name[64];                      /**< Policy name */
    hr_level_t source_level;            /**< Level triggering escalation */
    hr_level_t target_level;            /**< Level to escalate to */
    hr_escalation_trigger_t trigger;    /**< Trigger condition */
    uint32_t threshold;                 /**< Trigger threshold */
    uint32_t timeout_ms;                /**< Timeout before escalation */
    bool enabled;                       /**< Policy enabled */
} hr_escalation_policy_t;

/**
 * @brief Recovery context at a hierarchy level
 */
typedef struct {
    uint32_t node_id;               /**< Node identifier */
    hr_level_t level;               /**< Hierarchy level */
    uint32_t parent_id;             /**< Parent node (0 if root) */
    uint32_t children[HR_MAX_CHILDREN]; /**< Child nodes */
    uint32_t child_count;           /**< Number of children */
    float health_score;             /**< Current health (0-100) */
    uint32_t active_recoveries;     /**< Ongoing recovery count */
    uint64_t last_recovery_ms;      /**< Last recovery timestamp */
    bool is_healthy;                /**< Overall health status */
} hr_node_context_t;

/**
 * @brief Recovery request
 */
typedef struct {
    uint32_t request_id;            /**< Unique request ID */
    uint32_t source_node_id;        /**< Node requesting recovery */
    hr_level_t current_level;       /**< Current handling level */
    hr_level_t max_level;           /**< Maximum escalation level */
    uint32_t fault_type;            /**< Type of fault */
    uint32_t fault_severity;        /**< Fault severity (0-100) */
    uint32_t attempt_count;         /**< Recovery attempts */
    uint64_t created_at_ms;         /**< Request creation time */
    uint64_t timeout_ms;            /**< Request timeout */
    void* context_data;             /**< Additional context */
    size_t context_size;            /**< Context data size */
} hr_recovery_request_t;

/**
 * @brief Recovery response
 */
typedef struct {
    uint32_t request_id;            /**< Corresponding request ID */
    hr_result_t result;             /**< Recovery result */
    hr_level_t handling_level;      /**< Level that handled recovery */
    uint32_t handling_node_id;      /**< Node that performed recovery */
    uint64_t duration_ms;           /**< Recovery duration */
    char message[256];              /**< Result message */
    void* recovery_data;            /**< Recovery output data */
    size_t recovery_size;           /**< Output data size */
} hr_recovery_response_t;

/**
 * @brief Cascade failure detection
 */
typedef struct {
    uint32_t affected_nodes[HR_MAX_CHILDREN];
    uint32_t affected_count;
    hr_cascade_strategy_t strategy;
    float cascade_probability;      /**< Likelihood of cascade */
    uint32_t estimated_impact;      /**< Estimated nodes at risk */
    bool cascade_detected;
} hr_cascade_info_t;

/**
 * @brief Configuration for hierarchical recovery
 */
typedef struct {
    uint32_t node_id;               /**< This node's ID */
    hr_level_t node_level;          /**< This node's level */
    uint32_t parent_id;             /**< Parent node ID */
    uint32_t timeout_per_level_ms[HR_MAX_LEVELS]; /**< Timeout per level */
    uint32_t max_recovery_attempts; /**< Max attempts per level */
    bool enable_cascade_prevention; /**< Enable cascade detection */
    hr_cascade_strategy_t cascade_strategy; /**< Cascade strategy */
    bool enable_circuit_breakers;   /**< Enable circuit breakers */
    bool enable_bio_async;          /**< Use bio-async messaging */
} hr_config_t;

/**
 * @brief Statistics for hierarchical recovery
 */
typedef struct {
    uint64_t total_requests;
    uint64_t requests_per_level[HR_MAX_LEVELS];
    uint64_t successful_per_level[HR_MAX_LEVELS];
    uint64_t failed_per_level[HR_MAX_LEVELS];
    uint64_t escalations_per_level[HR_MAX_LEVELS];
    uint64_t cascades_prevented;
    uint64_t circuit_breaker_trips;
    uint64_t avg_recovery_time_ms;
    uint64_t max_recovery_time_ms;
} hr_stats_t;

/**
 * @brief Recovery handler callback
 */
typedef hr_result_t (*hr_recovery_handler_t)(
    const hr_recovery_request_t* request,
    hr_recovery_response_t* response,
    void* user_data
);

/**
 * @brief Recovery completion callback
 *
 * WHAT: Callback invoked when recovery completes
 * WHY:  Enable immune system to release anti-inflammatory signals
 * HOW:  Called after successful recovery
 */
typedef void (*hr_recovery_completion_callback_t)(
    const hr_recovery_request_t* request,
    const hr_recovery_response_t* response,
    void* user_data
);

/**
 * @brief Opaque hierarchical recovery handle
 */
typedef struct hr_context hr_context_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create hierarchical recovery context
 *
 * WHAT: Initialize hierarchical recovery system
 * WHY:  Required before any recovery operations
 * HOW:  Allocate context, configure levels, initialize breakers
 *
 * @param config Configuration
 * @return HR context or NULL on failure
 */
hr_context_t* hr_create(const hr_config_t* config);

/**
 * @brief Destroy hierarchical recovery context
 *
 * @param ctx HR context
 */
void hr_destroy(hr_context_t* ctx);

/**
 * @brief Get default configuration
 *
 * @return Default configuration
 */
hr_config_t hr_default_config(void);

/**
 * @brief Start hierarchical recovery
 *
 * @param ctx HR context
 * @return true on success
 */
bool hr_start(hr_context_t* ctx);

/**
 * @brief Stop hierarchical recovery
 *
 * @param ctx HR context
 * @return true on success
 */
bool hr_stop(hr_context_t* ctx);

//=============================================================================
// Hierarchy Management
//=============================================================================

/**
 * @brief Add child node
 *
 * @param ctx HR context
 * @param child_id Child node ID
 * @param level Child's level
 * @return true on success
 */
bool hr_add_child(hr_context_t* ctx, uint32_t child_id, hr_level_t level);

/**
 * @brief Remove child node
 *
 * @param ctx HR context
 * @param child_id Child node ID
 * @return true on success
 */
bool hr_remove_child(hr_context_t* ctx, uint32_t child_id);

/**
 * @brief Set parent node
 *
 * @param ctx HR context
 * @param parent_id Parent node ID
 * @return true on success
 */
bool hr_set_parent(hr_context_t* ctx, uint32_t parent_id);

/**
 * @brief Get node context
 *
 * @param ctx HR context
 * @param node_id Node to query
 * @param node_ctx Output node context
 * @return true if found
 */
bool hr_get_node_context(hr_context_t* ctx, uint32_t node_id, hr_node_context_t* node_ctx);

//=============================================================================
// Recovery Operations
//=============================================================================

/**
 * @brief Submit recovery request
 *
 * WHAT: Submit fault for recovery
 * WHY:  Initiate recovery process
 * HOW:  Route to appropriate level, execute recovery
 *
 * @param ctx HR context
 * @param request Recovery request
 * @param response Output response
 * @return Result code
 */
hr_result_t hr_submit_recovery(
    hr_context_t* ctx,
    const hr_recovery_request_t* request,
    hr_recovery_response_t* response
);

/**
 * @brief Escalate recovery to higher level
 *
 * @param ctx HR context
 * @param request_id Request to escalate
 * @param reason Escalation reason
 * @return true if escalated
 */
bool hr_escalate(hr_context_t* ctx, uint32_t request_id, const char* reason);

/**
 * @brief Register recovery handler for level
 *
 * @param ctx HR context
 * @param level Hierarchy level
 * @param handler Recovery handler
 * @param user_data User data for handler
 * @return true on success
 */
bool hr_register_handler(
    hr_context_t* ctx,
    hr_level_t level,
    hr_recovery_handler_t handler,
    void* user_data
);

/**
 * @brief Register recovery completion callback
 *
 * WHAT: Register callback for recovery completion
 * WHY:  Enable immune anti-inflammatory response
 * HOW:  Store callback, invoke after successful recovery
 *
 * @param ctx HR context
 * @param callback Completion callback
 * @param user_data User data
 * @return true on success
 */
bool hr_register_completion_callback(
    hr_context_t* ctx,
    hr_recovery_completion_callback_t callback,
    void* user_data
);

//=============================================================================
// Escalation Policies
//=============================================================================

/**
 * @brief Add escalation policy
 *
 * @param ctx HR context
 * @param policy Escalation policy
 * @return true on success
 */
bool hr_add_policy(hr_context_t* ctx, const hr_escalation_policy_t* policy);

/**
 * @brief Remove escalation policy
 *
 * @param ctx HR context
 * @param policy_name Policy name
 * @return true on success
 */
bool hr_remove_policy(hr_context_t* ctx, const char* policy_name);

/**
 * @brief Enable/disable policy
 *
 * @param ctx HR context
 * @param policy_name Policy name
 * @param enabled Enable state
 * @return true on success
 */
bool hr_set_policy_enabled(hr_context_t* ctx, const char* policy_name, bool enabled);

//=============================================================================
// Circuit Breakers
//=============================================================================

/**
 * @brief Add circuit breaker
 *
 * @param ctx HR context
 * @param config Circuit configuration
 * @return true on success
 */
bool hr_add_circuit_breaker(hr_context_t* ctx, const hr_circuit_config_t* config);

/**
 * @brief Get circuit breaker state
 *
 * @param ctx HR context
 * @param name Circuit name
 * @param breaker Output breaker state
 * @return true if found
 */
bool hr_get_circuit_breaker(hr_context_t* ctx, const char* name, hr_circuit_breaker_t* breaker);

/**
 * @brief Record circuit breaker result
 *
 * @param ctx HR context
 * @param name Circuit name
 * @param success true if operation succeeded
 * @return New circuit state
 */
hr_circuit_state_t hr_record_circuit_result(hr_context_t* ctx, const char* name, bool success);

/**
 * @brief Check if circuit allows operation
 *
 * @param ctx HR context
 * @param name Circuit name
 * @return true if operation allowed
 */
bool hr_circuit_allow(hr_context_t* ctx, const char* name);

/**
 * @brief Reset circuit breaker
 *
 * @param ctx HR context
 * @param name Circuit name
 * @return true on success
 */
bool hr_reset_circuit(hr_context_t* ctx, const char* name);

//=============================================================================
// Cascade Prevention
//=============================================================================

/**
 * @brief Detect potential cascade failure
 *
 * @param ctx HR context
 * @param failed_node Initial failed node
 * @param cascade_info Output cascade information
 * @return true if cascade detected
 */
bool hr_detect_cascade(hr_context_t* ctx, uint32_t failed_node, hr_cascade_info_t* cascade_info);

/**
 * @brief Apply cascade prevention
 *
 * @param ctx HR context
 * @param cascade_info Detected cascade
 * @return true on success
 */
bool hr_prevent_cascade(hr_context_t* ctx, const hr_cascade_info_t* cascade_info);

/**
 * @brief Get cascade prevention status
 *
 * @param ctx HR context
 * @return Number of active cascade preventions
 */
uint32_t hr_get_cascade_prevention_count(hr_context_t* ctx);

//=============================================================================
// Statistics and Diagnostics
//=============================================================================

/**
 * @brief Get HR statistics
 *
 * @param ctx HR context
 * @param stats Output statistics
 * @return true on success
 */
bool hr_get_stats(hr_context_t* ctx, hr_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param ctx HR context
 */
void hr_reset_stats(hr_context_t* ctx);

/**
 * @brief Get recovery latency by level
 *
 * @param ctx HR context
 * @param level Hierarchy level
 * @return Average latency in ms
 */
uint64_t hr_get_latency_by_level(hr_context_t* ctx, hr_level_t level);

//=============================================================================
// String Conversion
//=============================================================================

const char* hr_level_to_string(hr_level_t level);
const char* hr_trigger_to_string(hr_escalation_trigger_t trigger);
const char* hr_circuit_state_to_string(hr_circuit_state_t state);
const char* hr_result_to_string(hr_result_t result);
const char* hr_cascade_strategy_to_string(hr_cascade_strategy_t strategy);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_HIERARCHICAL_RECOVERY_H
