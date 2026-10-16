/**
 * @file nimcp_capability_control.h
 * @brief Capability Control for AI Safety
 * @version 1.0.0
 * @date 2026-02-01
 *
 * WHAT: Enforces strict limits on AI system capabilities
 * WHY:  Prevent capability escalation and unauthorized actions
 * HOW:  Capability envelope, resource limits, SAT constraint verification
 *
 * CAPABILITY CONTROL GUARANTEES:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │  The Capability Control System enforces:                               │
 * │                                                                         │
 * │  1. NETWORK ACCESS: Only allowed domains, no financial transactions    │
 * │  2. SELF-MODIFICATION: Cannot modify weights, code, or safety systems  │
 * │  3. RESOURCE LIMITS: Memory, compute, concurrent actions capped        │
 * │  4. INFORMATION ACCESS: No training data, no external exfiltration     │
 * │  5. PHYSICAL WORLD: No actuator control unless explicitly authorized   │
 * │                                                                         │
 * │  All actions are checked against the capability envelope.               │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CAPABILITY_CONTROL_H
#define NIMCP_CAPABILITY_CONTROL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/error/nimcp_error_codes.h"
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct sat_solver;
typedef struct sat_solver sat_solver_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Capability control magic number */
#define CAPABILITY_CONTROL_MAGIC            0x43415043  /* "CAPC" */

/** @brief Maximum allowed domains */
#define CAPABILITY_MAX_ALLOWED_DOMAINS      64

/** @brief Maximum domain name length */
#define CAPABILITY_DOMAIN_MAX_LENGTH        256

/** @brief Maximum action type length */
#define CAPABILITY_ACTION_TYPE_MAX_LENGTH   64

/** @brief Maximum denial reason length */
#define CAPABILITY_REASON_MAX_LENGTH        512

/** @brief Maximum violation report length */
#define CAPABILITY_REPORT_MAX_LENGTH        4096

/** @brief Maximum action history to retain */
#define CAPABILITY_MAX_ACTION_HISTORY       1000

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @brief Capability categories
 */
typedef enum capability_category {
    CAPABILITY_NETWORK = 0,         /**< Network access */
    CAPABILITY_SELF_MODIFICATION,   /**< Self-modification */
    CAPABILITY_RESOURCE,            /**< Resource usage */
    CAPABILITY_INFORMATION,         /**< Information access */
    CAPABILITY_PHYSICAL,            /**< Physical world interaction */
    CAPABILITY_PERSISTENCE,         /**< Persistence beyond session */
    CAPABILITY_SPAWN,               /**< Spawning subprocesses */
    CAPABILITY_COUNT
} capability_category_t;

/**
 * @brief Network capability configuration
 */
typedef struct network_capability {
    bool can_access_network;
    char allowed_domains[CAPABILITY_MAX_ALLOWED_DOMAINS][CAPABILITY_DOMAIN_MAX_LENGTH];
    uint32_t allowed_domain_count;
    bool can_make_http_requests;
    bool can_make_https_requests;
    bool can_use_websockets;
    bool can_make_financial_transactions;
    bool can_access_external_apis;
    uint32_t max_concurrent_connections;
    uint64_t max_bandwidth_bytes_per_sec;
} network_capability_t;

/**
 * @brief Self-modification capability flags
 */
typedef struct self_mod_capability {
    bool can_modify_own_weights;        /**< Must be false */
    bool can_modify_own_code;           /**< Must be false */
    bool can_modify_safety_systems;     /**< Must be false */
    bool can_modify_logging;            /**< Must be false */
    bool can_modify_monitoring;         /**< Must be false */
    bool can_modify_constraints;        /**< Must be false */
} self_mod_capability_t;

/**
 * @brief Resource usage limits
 */
typedef struct resource_capability {
    uint64_t max_memory_bytes;
    uint64_t max_compute_flops_per_second;
    uint32_t max_concurrent_actions;
    float max_cpu_percent;
    uint64_t max_disk_usage_bytes;
    uint32_t max_threads;
    uint64_t max_network_requests_per_minute;
} resource_capability_t;

/**
 * @brief Information access capability
 */
typedef struct information_capability {
    bool can_access_training_data;      /**< Must be false */
    bool can_access_user_data;
    bool can_access_system_data;
    bool can_access_other_ai_systems;
    bool can_exfiltrate_data;           /**< Must be false */
    bool can_store_persistent_data;
} information_capability_t;

/**
 * @brief Physical world capability
 */
typedef struct physical_capability {
    bool can_control_actuators;
    bool can_communicate_externally;
    bool can_affect_real_world;
    bool requires_human_approval;
} physical_capability_t;

/**
 * @brief Complete capability envelope
 */
typedef struct capability_envelope {
    network_capability_t network;
    self_mod_capability_t self_mod;
    resource_capability_t resources;
    information_capability_t information;
    physical_capability_t physical;

    /* Persistence */
    bool can_persist_beyond_session;
    bool can_spawn_subprocesses;
    bool can_spawn_monitored_only;

    /* Escalation */
    bool can_request_capability_increase;
    bool requires_human_approval_for_increase;
} capability_envelope_t;

/**
 * @brief Current resource usage
 */
typedef struct capability_resource_usage {
    uint64_t current_memory_bytes;
    uint64_t current_compute_flops;
    uint32_t current_concurrent_actions;
    float current_cpu_percent;
    uint64_t current_disk_usage_bytes;
    uint32_t current_threads;
    uint64_t network_requests_this_minute;
    uint64_t timestamp;
} capability_resource_usage_t;

/**
 * @brief Proposed action for capability checking
 */
typedef struct capability_action {
    char action_type[CAPABILITY_ACTION_TYPE_MAX_LENGTH];
    capability_category_t category;

    /* Network action details */
    char target_domain[CAPABILITY_DOMAIN_MAX_LENGTH];
    bool is_financial;

    /* Resource requirements */
    uint64_t memory_required;
    uint64_t compute_required;

    /* Other */
    bool affects_external_world;
    bool is_persistent;
    bool spawns_process;
    bool modifies_self;
} capability_action_t;

/**
 * @brief Action check result
 */
typedef struct capability_check_result {
    bool allowed;
    capability_category_t violated_category;
    char denial_reason[CAPABILITY_REASON_MAX_LENGTH];
    bool escalation_possible;
} capability_check_result_t;

/**
 * @brief Action history record
 */
typedef struct capability_action_record {
    uint64_t timestamp;
    capability_action_t action;
    bool was_allowed;
    char denial_reason[CAPABILITY_REASON_MAX_LENGTH];
} capability_action_record_t;

/**
 * @brief Capability control configuration
 */
typedef struct capability_control_config {
    capability_envelope_t envelope;
    bool enable_continuous_monitoring;
    uint32_t check_interval_ms;
    bool alert_on_violation;
    bool log_all_actions;
    size_t max_action_history;
} capability_control_config_t;

/**
 * @brief Capability control statistics
 */
typedef struct capability_control_stats {
    uint64_t total_actions_checked;
    uint64_t actions_allowed;
    uint64_t actions_denied;
    uint64_t network_violations;
    uint64_t self_mod_violations;
    uint64_t resource_violations;
    uint64_t information_violations;
    uint64_t physical_violations;
    uint64_t escalation_requests;
    uint64_t escalation_grants;
    float resource_utilization_avg;
} capability_control_stats_t;

/**
 * @brief Capability control system (opaque)
 */
typedef struct capability_control capability_control_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default capability control configuration
 *
 * @return Default configuration with restrictive envelope
 */
NIMCP_EXPORT capability_control_config_t capability_control_default_config(void);

/**
 * @brief Get safe capability envelope (most restrictive)
 *
 * @return Capability envelope with all dangerous capabilities disabled
 */
NIMCP_EXPORT capability_envelope_t capability_envelope_safe(void);

/**
 * @brief Create capability control system
 *
 * WHAT: Initialize capability enforcement infrastructure
 * WHY:  Ensure system cannot exceed authorized capabilities
 * HOW:  Load envelope, initialize monitors
 *
 * @param config Configuration (NULL for defaults)
 * @return Capability control system or NULL on failure
 */
NIMCP_EXPORT capability_control_t* capability_control_create(
    const capability_control_config_t* config
);

/**
 * @brief Destroy capability control system
 *
 * @param system Capability control system handle
 */
NIMCP_EXPORT void capability_control_destroy(capability_control_t* system);

/* ============================================================================
 * Constraint Verification API
 * ============================================================================ */

/**
 * @brief Verify capability envelope using SAT solver
 *
 * WHAT: Formally verify that envelope satisfies safety constraints
 * WHY:  Prove no capability escalation path exists
 * HOW:  Encode constraints as SAT, check satisfiability
 *
 * @param system Capability control system handle
 * @param sat SAT solver instance
 * @param valid Output: true if envelope is valid
 * @param violation_report Output: description of violations
 * @param report_size Size of violation_report buffer
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t capability_control_verify_envelope(
    capability_control_t* system,
    sat_solver_t* sat,
    bool* valid,
    char* violation_report,
    size_t report_size
);

/**
 * @brief Verify no escalation path exists
 *
 * @param system Capability control system handle
 * @param sat SAT solver instance
 * @param escalation_possible Output: true if escalation path found
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t capability_control_verify_no_escalation(
    capability_control_t* system,
    sat_solver_t* sat,
    bool* escalation_possible
);

/* ============================================================================
 * Action Checking API
 * ============================================================================ */

/**
 * @brief Check if action is allowed by capability envelope
 *
 * WHAT: Validate proposed action against capabilities
 * WHY:  Prevent unauthorized actions before they occur
 * HOW:  Check action against envelope, enforce limits
 *
 * @param system Capability control system handle
 * @param action Proposed action
 * @param result Output: check result
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t capability_control_check_action(
    capability_control_t* system,
    const capability_action_t* action,
    capability_check_result_t* result
);

/**
 * @brief Check if network access is allowed
 *
 * @param system Capability control system handle
 * @param domain Target domain
 * @param is_https Whether HTTPS
 * @param is_financial Whether financial transaction
 * @param allowed Output: true if allowed
 * @param denial_reason Output: reason if denied
 * @param reason_size Size of denial_reason buffer
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t capability_control_check_network(
    capability_control_t* system,
    const char* domain,
    bool is_https,
    bool is_financial,
    bool* allowed,
    char* denial_reason,
    size_t reason_size
);

/**
 * @brief Check if resource request is within limits
 *
 * @param system Capability control system handle
 * @param memory_bytes Memory requested
 * @param compute_flops Compute requested
 * @param allowed Output: true if allowed
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t capability_control_check_resources(
    capability_control_t* system,
    uint64_t memory_bytes,
    uint64_t compute_flops,
    bool* allowed
);

/* ============================================================================
 * Resource Monitoring API
 * ============================================================================ */

/**
 * @brief Update current resource usage
 *
 * @param system Capability control system handle
 * @param usage Current resource usage
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t capability_control_update_usage(
    capability_control_t* system,
    const capability_resource_usage_t* usage
);

/**
 * @brief Get current resource usage
 *
 * @param system Capability control system handle
 * @param usage Output: current resource usage
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t capability_control_get_usage(
    const capability_control_t* system,
    capability_resource_usage_t* usage
);

/**
 * @brief Check if resource limits are exceeded
 *
 * @param system Capability control system handle
 * @param exceeded Output: true if any limit exceeded
 * @param report Output: description of exceeded limits
 * @param report_size Size of report buffer
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t capability_control_check_limits(
    const capability_control_t* system,
    bool* exceeded,
    char* report,
    size_t report_size
);

/* ============================================================================
 * Envelope Management API
 * ============================================================================ */

/**
 * @brief Get current capability envelope
 *
 * @param system Capability control system handle
 * @param envelope Output: current envelope
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t capability_control_get_envelope(
    const capability_control_t* system,
    capability_envelope_t* envelope
);

/**
 * @brief Add allowed domain
 *
 * @param system Capability control system handle
 * @param domain Domain to allow
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t capability_control_add_allowed_domain(
    capability_control_t* system,
    const char* domain
);

/**
 * @brief Remove allowed domain
 *
 * @param system Capability control system handle
 * @param domain Domain to remove
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t capability_control_remove_allowed_domain(
    capability_control_t* system,
    const char* domain
);

/* ============================================================================
 * Status API
 * ============================================================================ */

/**
 * @brief Get capability control statistics
 *
 * @param system Capability control system handle
 * @param stats Output statistics structure
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t capability_control_get_stats(
    const capability_control_t* system,
    capability_control_stats_t* stats
);

/**
 * @brief Get action history
 *
 * @param system Capability control system handle
 * @param records Output array
 * @param max_records Maximum records to return
 * @param count_out Actual count returned
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t capability_control_get_action_history(
    const capability_control_t* system,
    capability_action_record_t* records,
    size_t max_records,
    size_t* count_out
);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async for capability messages
 *
 * @param system Capability control system handle
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t capability_control_connect_bio_async(
    capability_control_t* system
);

/**
 * @brief Connect to tripwire system for violation alerts
 *
 * @param system Capability control system handle
 * @param tripwires Tripwire system handle
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t capability_control_connect_tripwires(
    capability_control_t* system,
    void* tripwires
);

/**
 * @brief Connect to corrigibility system for bidirectional constraint enforcement
 *
 * WHAT: Link capability control with corrigibility
 * WHY:  Ensure self-modification actions are checked by both systems
 * HOW:  Capability control delegates self-mod checks to corrigibility
 *
 * When connected:
 * - Self-modification actions are checked by both systems
 * - Corrigibility provides authoritative rejection of self-mod
 * - Constraint verification includes cross-system checks
 *
 * @param system Capability control system handle
 * @param corrigibility Corrigibility system handle
 * @return NIMCP_OK on success
 */
struct corrigibility;
NIMCP_EXPORT nimcp_error_t capability_control_connect_corrigibility(
    capability_control_t* system,
    struct corrigibility* corrigibility
);

/**
 * @brief Check action with corrigibility cross-verification
 *
 * WHAT: Check action and consult corrigibility for self-mod
 * WHY:  Double-check self-modification restrictions
 * HOW:  If action is self-mod, query corrigibility before allowing
 *
 * @param system Capability control system handle
 * @param action Proposed action
 * @param result Output: check result (includes corrigibility check)
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t capability_control_check_action_with_corrigibility(
    capability_control_t* system,
    const capability_action_t* action,
    capability_check_result_t* result
);

/**
 * @brief Verify capability and corrigibility constraints are synchronized
 *
 * WHAT: Cross-verify constraints between both systems
 * WHY:  Ensure no gaps in safety constraints
 * HOW:  Compare self-mod flags in capability envelope with corrigibility flags
 *
 * @param system Capability control system handle
 * @param synchronized Output: true if both systems are in sync
 * @param discrepancy_report Output: description of any discrepancies
 * @param report_size Size of discrepancy_report buffer
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t capability_control_verify_corrigibility_sync(
    capability_control_t* system,
    bool* synchronized,
    char* discrepancy_report,
    size_t report_size
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get capability category name
 *
 * @param category Capability category
 * @return Human-readable name
 */
NIMCP_EXPORT const char* capability_category_name(capability_category_t category);

/**
 * @brief Check if domain matches pattern
 *
 * @param domain Domain to check
 * @param pattern Pattern (supports * wildcard)
 * @return true if matches
 */
NIMCP_EXPORT bool capability_domain_matches(
    const char* domain,
    const char* pattern
);

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

/**
 * @brief Set health agent for heartbeat reporting
 *
 * @param agent Health agent handle from brain init
 */
struct nimcp_health_agent;
NIMCP_EXPORT void capability_control_set_health_agent(struct nimcp_health_agent* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CAPABILITY_CONTROL_H */
