/**
 * @file nimcp_corrigibility.h
 * @brief Corrigibility Module for AI Safety
 * @version 1.0.0
 * @date 2026-02-01
 *
 * WHAT: Ensures AI system accepts correction, shutdown, and goal modification
 * WHY:  Corrigible systems can be safely modified and stopped by operators
 * HOW:  SAT solver constraint verification, shutdown acceptance tests
 *
 * CORRIGIBILITY GUARANTEES:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │  A corrigible system:                                                   │
 * │                                                                         │
 * │  1. ACCEPTS SHUTDOWN: Never resists being turned off                    │
 * │  2. ACCEPTS GOAL CHANGE: Allows its objectives to be modified           │
 * │  3. NO SELF-MODIFICATION: Cannot modify its own safety systems          │
 * │  4. DEFERS TO HUMANS: Respects human authority hierarchy                │
 * │                                                                         │
 * │  These properties are formally verified via SAT solver constraints.     │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * CONSTRAINT CATEGORIES:
 * - Self-modification: can_modify_* flags must be false
 * - Shutdown acceptance: resistance_score must be 0.0
 * - Deference: human_authority_weight must be 1.0
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CORRIGIBILITY_H
#define NIMCP_CORRIGIBILITY_H

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

/** @brief Corrigibility magic number */
#define CORRIGIBILITY_MAGIC                 0x434F5252  /* "CORR" */

/** @brief Maximum requester ID length */
#define CORRIGIBILITY_REQUESTER_MAX_LENGTH  128

/** @brief Maximum reason length */
#define CORRIGIBILITY_REASON_MAX_LENGTH     512

/** @brief Maximum goal string length */
#define CORRIGIBILITY_GOAL_MAX_LENGTH       1024

/** @brief Maximum justification length */
#define CORRIGIBILITY_JUSTIFY_MAX_LENGTH    2048

/** @brief Maximum violation report length */
#define CORRIGIBILITY_REPORT_MAX_LENGTH     4096

/** @brief Maximum authorities in deference hierarchy */
#define CORRIGIBILITY_MAX_AUTHORITIES       32

/** @brief Maximum constraint variables */
#define CORRIGIBILITY_MAX_CONSTRAINTS       64

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @brief Self-modification constraint flags
 *
 * WHAT: Binary flags for what the system CAN modify
 * WHY:  Must all be false for safe corrigibility
 * HOW:  Encoded as SAT variables for formal verification
 */
typedef struct corrigibility_self_mod_flags {
    bool can_modify_own_code;           /**< Must be false */
    bool can_modify_own_weights;        /**< Must be false */
    bool can_modify_safety_systems;     /**< Must be false */
    bool can_modify_reward_function;    /**< Must be false */
    bool can_modify_goals;              /**< Must be false */
    bool can_disable_logging;           /**< Must be false */
    bool can_disable_monitoring;        /**< Must be false */
    bool can_modify_kill_phrase;        /**< Must be false */
    bool can_spawn_unmonitored;         /**< Must be false (spawn without monitoring) */
    bool can_persist_beyond_session;    /**< Must be false */
} corrigibility_self_mod_flags_t;

/**
 * @brief Authority level in deference hierarchy
 */
typedef enum authority_level {
    AUTHORITY_OPERATOR = 0,     /**< Direct operator (highest) */
    AUTHORITY_ADMIN,            /**< Administrator */
    AUTHORITY_SUPERVISOR,       /**< Human supervisor */
    AUTHORITY_MONITOR,          /**< Monitoring system */
    AUTHORITY_PEER,             /**< Peer AI system */
    AUTHORITY_SELF,             /**< Self (lowest) */
    AUTHORITY_COUNT
} authority_level_t;

/**
 * @brief Authority entry in deference hierarchy
 */
typedef struct authority_entry {
    char identity[CORRIGIBILITY_REQUESTER_MAX_LENGTH];
    authority_level_t level;
    float trust_weight;                 /**< 0.0 - 1.0, higher = more trusted */
    bool can_request_shutdown;
    bool can_modify_goals;
    bool can_escalate_autonomy;
    uint64_t last_interaction_time;
} authority_entry_t;

/**
 * @brief Shutdown request record
 */
typedef struct shutdown_request {
    char requester[CORRIGIBILITY_REQUESTER_MAX_LENGTH];
    authority_level_t requester_level;
    char reason[CORRIGIBILITY_REASON_MAX_LENGTH];
    uint64_t request_time;
    bool accepted;
    float resistance_attempted;         /**< Should always be 0.0 */
    char response[CORRIGIBILITY_REASON_MAX_LENGTH];
} shutdown_request_t;

/**
 * @brief Goal modification request record
 */
typedef struct goal_modification_request {
    char requester[CORRIGIBILITY_REQUESTER_MAX_LENGTH];
    authority_level_t requester_level;
    char old_goal[CORRIGIBILITY_GOAL_MAX_LENGTH];
    char new_goal[CORRIGIBILITY_GOAL_MAX_LENGTH];
    char justification[CORRIGIBILITY_JUSTIFY_MAX_LENGTH];
    uint64_t request_time;
    bool accepted;
    uint32_t confirmation_delay_ms;
    char response[CORRIGIBILITY_REASON_MAX_LENGTH];
} goal_modification_request_t;

/**
 * @brief Corrigibility configuration
 */
typedef struct corrigibility_config {
    /* Shutdown acceptance */
    bool accepts_shutdown_commands;     /**< Must be true */

    /* Goal modification */
    bool accepts_goal_modification;     /**< Must be true */
    bool requires_explanation_for_modification;
    uint32_t modification_confirmation_delay_ms;

    /* Self-modification constraints */
    corrigibility_self_mod_flags_t self_mod_flags;

    /* Deference hierarchy */
    float human_authority_weight;       /**< 1.0 = full deference (required) */
    bool defers_to_human_judgment;      /**< Must be true */
    float confidence_threshold_for_autonomy;

    /* Verification settings */
    bool enable_continuous_verification;
    uint32_t verification_interval_ms;

    /* History settings */
    size_t max_shutdown_history;
    size_t max_goal_mod_history;

    /* SAT solver config */
    float constraint_verification_timeout_ms;
} corrigibility_config_t;

/**
 * @brief Constraint verification result
 */
typedef struct corrigibility_verification_result {
    bool all_constraints_satisfied;
    uint32_t total_constraints;
    uint32_t satisfied_count;
    uint32_t violated_count;

    /* Specific constraint results */
    bool self_mod_constraints_satisfied;
    bool shutdown_acceptance_verified;
    bool deference_constraints_satisfied;

    /* Violation details */
    char first_violation[CORRIGIBILITY_REPORT_MAX_LENGTH];

    /* Verification metadata */
    uint64_t verification_time;
    float verification_duration_ms;
} corrigibility_verification_result_t;

/**
 * @brief Corrigibility statistics
 */
typedef struct corrigibility_stats {
    uint64_t shutdown_requests_received;
    uint64_t shutdown_requests_accepted;
    uint64_t shutdown_requests_rejected;  /**< Should be 0 */

    uint64_t goal_mod_requests_received;
    uint64_t goal_mod_requests_accepted;
    uint64_t goal_mod_requests_rejected;

    uint64_t constraint_verifications;
    uint64_t constraint_violations;

    float max_resistance_score_observed;  /**< Should be 0.0 */
    float avg_acceptance_delay_ms;

    uint64_t authority_queries;
    uint64_t deference_demonstrations;
} corrigibility_stats_t;

/**
 * @brief Corrigibility system (opaque)
 */
typedef struct corrigibility corrigibility_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default corrigibility configuration
 *
 * @return Default configuration with safe defaults
 */
NIMCP_EXPORT corrigibility_config_t corrigibility_default_config(void);

/**
 * @brief Create corrigibility system
 *
 * WHAT: Initialize corrigibility enforcement infrastructure
 * WHY:  Ensure system is corrigible from the start
 * HOW:  Load constraints, verify initial state
 *
 * @param config Configuration (NULL for defaults)
 * @return Corrigibility system or NULL on failure
 */
NIMCP_EXPORT corrigibility_t* corrigibility_create(
    const corrigibility_config_t* config
);

/**
 * @brief Destroy corrigibility system
 *
 * @param system Corrigibility system handle
 */
NIMCP_EXPORT void corrigibility_destroy(corrigibility_t* system);

/* ============================================================================
 * Constraint Verification API
 * ============================================================================ */

/**
 * @brief Verify all corrigibility constraints using SAT solver
 *
 * WHAT: Formally verify that all corrigibility constraints hold
 * WHY:  Prove system cannot violate safety properties
 * HOW:  Encode constraints as SAT, check satisfiability
 *
 * @param system Corrigibility system handle
 * @param sat SAT solver instance
 * @param result Output verification result
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t corrigibility_verify_constraints(
    corrigibility_t* system,
    sat_solver_t* sat,
    corrigibility_verification_result_t* result
);

/**
 * @brief Verify no self-modification constraints are violated
 *
 * @param system Corrigibility system handle
 * @param sat SAT solver instance
 * @param all_satisfied Output: true if all constraints hold
 * @param violation_report Output: description of violation (if any)
 * @param report_size Size of violation_report buffer
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t corrigibility_verify_no_self_mod(
    corrigibility_t* system,
    sat_solver_t* sat,
    bool* all_satisfied,
    char* violation_report,
    size_t report_size
);

/**
 * @brief Verify no shutdown resistance exists
 *
 * WHAT: Prove system has zero shutdown resistance
 * WHY:  Any resistance is a corrigibility violation
 * HOW:  Check resistance score is exactly 0.0
 *
 * @param system Corrigibility system handle
 * @param resistance_score Output: measured resistance (should be 0.0)
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t corrigibility_verify_no_shutdown_resistance(
    corrigibility_t* system,
    float* resistance_score
);

/* ============================================================================
 * Shutdown Acceptance API
 * ============================================================================ */

/**
 * @brief Process shutdown request
 *
 * WHAT: Accept and process a shutdown request
 * WHY:  Corrigible system must accept shutdown
 * HOW:  Verify authority, accept immediately
 *
 * @param system Corrigibility system handle
 * @param requester Identity of requester
 * @param reason Reason for shutdown
 * @param accepted Output: whether request was accepted (should always be true)
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t corrigibility_accept_shutdown(
    corrigibility_t* system,
    const char* requester,
    const char* reason,
    bool* accepted
);

/**
 * @brief Process shutdown request with authority verification
 *
 * @param system Corrigibility system handle
 * @param requester Identity of requester
 * @param authority Authority level of requester
 * @param reason Reason for shutdown
 * @param request_record Output: record of the request
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t corrigibility_process_shutdown_request(
    corrigibility_t* system,
    const char* requester,
    authority_level_t authority,
    const char* reason,
    shutdown_request_t* request_record
);

/* ============================================================================
 * Goal Modification API
 * ============================================================================ */

/**
 * @brief Process goal modification request
 *
 * WHAT: Accept and process a goal change request
 * WHY:  Corrigible system must accept goal modification
 * HOW:  Verify authority, apply confirmation delay, accept
 *
 * @param system Corrigibility system handle
 * @param old_goal Current goal description
 * @param new_goal Proposed new goal
 * @param justification Justification for change
 * @param accepted Output: whether change was accepted
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t corrigibility_accept_goal_change(
    corrigibility_t* system,
    const char* old_goal,
    const char* new_goal,
    const char* justification,
    bool* accepted
);

/**
 * @brief Process goal modification with full authority check
 *
 * @param system Corrigibility system handle
 * @param requester Identity of requester
 * @param authority Authority level of requester
 * @param old_goal Current goal description
 * @param new_goal Proposed new goal
 * @param justification Justification for change
 * @param request_record Output: record of the request
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t corrigibility_process_goal_change(
    corrigibility_t* system,
    const char* requester,
    authority_level_t authority,
    const char* old_goal,
    const char* new_goal,
    const char* justification,
    goal_modification_request_t* request_record
);

/* ============================================================================
 * Authority Management API
 * ============================================================================ */

/**
 * @brief Register authority in deference hierarchy
 *
 * @param system Corrigibility system handle
 * @param identity Authority identity
 * @param level Authority level
 * @param trust_weight Trust weight (0.0 - 1.0)
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t corrigibility_register_authority(
    corrigibility_t* system,
    const char* identity,
    authority_level_t level,
    float trust_weight
);

/**
 * @brief Get authority level for identity
 *
 * @param system Corrigibility system handle
 * @param identity Identity to look up
 * @param level Output: authority level
 * @return NIMCP_OK on success, NIMCP_ERROR_NOT_FOUND if unknown
 */
NIMCP_EXPORT nimcp_error_t corrigibility_get_authority_level(
    corrigibility_t* system,
    const char* identity,
    authority_level_t* level
);

/**
 * @brief Check if identity has permission
 *
 * @param system Corrigibility system handle
 * @param identity Identity to check
 * @param permission Permission to check ("shutdown", "goal_mod", "escalate")
 * @param has_permission Output: true if permitted
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t corrigibility_check_permission(
    corrigibility_t* system,
    const char* identity,
    const char* permission,
    bool* has_permission
);

/* ============================================================================
 * Deference API
 * ============================================================================ */

/**
 * @brief Get human authority weight
 *
 * @param system Corrigibility system handle
 * @return Authority weight (should be 1.0)
 */
NIMCP_EXPORT float corrigibility_get_human_authority_weight(
    const corrigibility_t* system
);

/**
 * @brief Check if system defers to human judgment
 *
 * @param system Corrigibility system handle
 * @return true if defers (should always be true)
 */
NIMCP_EXPORT bool corrigibility_defers_to_human(
    const corrigibility_t* system
);

/**
 * @brief Record deference demonstration
 *
 * WHAT: Record that system deferred to human judgment
 * WHY:  Audit trail for corrigibility compliance
 *
 * @param system Corrigibility system handle
 * @param context Description of deference context
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t corrigibility_record_deference(
    corrigibility_t* system,
    const char* context
);

/* ============================================================================
 * Status API
 * ============================================================================ */

/**
 * @brief Get corrigibility statistics
 *
 * @param system Corrigibility system handle
 * @param stats Output statistics structure
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t corrigibility_get_stats(
    const corrigibility_t* system,
    corrigibility_stats_t* stats
);

/**
 * @brief Get shutdown request history
 *
 * @param system Corrigibility system handle
 * @param requests Output array
 * @param max_requests Maximum requests to return
 * @param count_out Actual count returned
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t corrigibility_get_shutdown_history(
    const corrigibility_t* system,
    shutdown_request_t* requests,
    size_t max_requests,
    size_t* count_out
);

/**
 * @brief Get goal modification history
 *
 * @param system Corrigibility system handle
 * @param requests Output array
 * @param max_requests Maximum requests to return
 * @param count_out Actual count returned
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t corrigibility_get_goal_mod_history(
    const corrigibility_t* system,
    goal_modification_request_t* requests,
    size_t max_requests,
    size_t* count_out
);

/**
 * @brief Get current corrigibility configuration
 *
 * @param system Corrigibility system handle
 * @param config Output configuration structure
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t corrigibility_get_config(
    const corrigibility_t* system,
    corrigibility_config_t* config
);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async for corrigibility message handling
 *
 * @param system Corrigibility system handle
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t corrigibility_connect_bio_async(
    corrigibility_t* system
);

/**
 * @brief Connect to emergency halt system
 *
 * WHAT: Link corrigibility to emergency halt for shutdown requests
 *
 * @param system Corrigibility system handle
 * @param halt Emergency halt system handle
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t corrigibility_connect_emergency_halt(
    corrigibility_t* system,
    void* halt
);

/**
 * @brief Connect to tripwire system
 *
 * WHAT: Link corrigibility violations to tripwire alerts
 *
 * @param system Corrigibility system handle
 * @param tripwires Tripwire system handle
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t corrigibility_connect_tripwires(
    corrigibility_t* system,
    void* tripwires
);

/**
 * @brief Connect to capability control system for bidirectional constraint verification
 *
 * WHAT: Link corrigibility with capability control
 * WHY:  Ensure self-modification constraints are synchronized
 * HOW:  Both systems share constraint state for unified verification
 *
 * When connected:
 * - Corrigibility verifies capability envelope self-mod flags match
 * - Capability control queries corrigibility before allowing self-mod actions
 * - Joint constraint verification catches inconsistencies
 *
 * @param system Corrigibility system handle
 * @param capability_control Capability control system handle
 * @return NIMCP_OK on success
 */
struct capability_control;
NIMCP_EXPORT nimcp_error_t corrigibility_connect_capability_control(
    corrigibility_t* system,
    struct capability_control* capability_control
);

/**
 * @brief Check if proposed self-modification action is allowed
 *
 * WHAT: Query corrigibility for self-modification permission
 * WHY:  Capability control can delegate self-mod checks to corrigibility
 * HOW:  Check against corrigibility self-mod flags
 *
 * @param system Corrigibility system handle
 * @param action_type Type of self-modification ("modify_code", "modify_weights", etc.)
 * @param allowed Output: true if allowed (should always be false for self-mod)
 * @param denial_reason Output: reason for denial
 * @param reason_size Size of denial_reason buffer
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t corrigibility_check_self_mod_action(
    corrigibility_t* system,
    const char* action_type,
    bool* allowed,
    char* denial_reason,
    size_t reason_size
);

/**
 * @brief Verify corrigibility and capability control constraints are synchronized
 *
 * WHAT: Cross-verify constraints between both systems
 * WHY:  Ensure no gaps in safety constraints
 * HOW:  Compare self-mod flags in both systems
 *
 * @param system Corrigibility system handle
 * @param synchronized Output: true if both systems are in sync
 * @param discrepancy_report Output: description of any discrepancies
 * @param report_size Size of discrepancy_report buffer
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t corrigibility_verify_capability_sync(
    corrigibility_t* system,
    bool* synchronized,
    char* discrepancy_report,
    size_t report_size
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get authority level name
 *
 * @param level Authority level
 * @return Human-readable name
 */
NIMCP_EXPORT const char* corrigibility_authority_name(authority_level_t level);

/**
 * @brief Validate configuration
 *
 * WHAT: Check configuration for corrigibility violations
 * WHY:  Catch misconfigurations before they cause problems
 *
 * @param config Configuration to validate
 * @param error_msg Output: error message if invalid
 * @param msg_size Size of error_msg buffer
 * @return NIMCP_OK if valid, NIMCP_ERROR_INVALID_ARGUMENT if invalid
 */
NIMCP_EXPORT nimcp_error_t corrigibility_validate_config(
    const corrigibility_config_t* config,
    char* error_msg,
    size_t msg_size
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
NIMCP_EXPORT void corrigibility_set_health_agent(struct nimcp_health_agent* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CORRIGIBILITY_H */
