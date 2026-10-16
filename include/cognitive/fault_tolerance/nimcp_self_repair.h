/**
 * @file nimcp_self_repair.h
 * @brief Self-Repair Coordinator - Orchestrates Autonomous Code Fix Pipeline
 *
 * WHAT: Coordinates the complete self-repair pipeline from diagnosis to deployment
 * WHY:  Enable fully autonomous code generation and self-healing
 * HOW:  Integrate diagnostics → code analysis → code generation → validation → dual deployment
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────────┐
 * │                    AUTONOMOUS SELF-REPAIR PIPELINE                           │
 * ├─────────────────────────────────────────────────────────────────────────────┤
 * │  1. DETECTION           2. ANALYSIS              3. GENERATION               │
 * │  ┌──────────────┐      ┌─────────────────┐      ┌──────────────────┐        │
 * │  │ Health Agent │─────>│ Recovery        │─────>│ Code Generation  │        │
 * │  │ Diagnostics  │      │ Parietal Bridge │      │ Engine           │        │
 * │  └──────────────┘      └─────────────────┘      └────────┬─────────┘        │
 * │                                                          │                   │
 * │  4. VALIDATION                         5. DUAL DEPLOYMENT                    │
 * │  ┌─────────────────────┐              ┌────────────────────────────────┐    │
 * │  │ Recompiler          │              │ HOT PATCH      │ SOURCE COMMIT │    │
 * │  │ - Compile fix       │─────────────>│ Hot Injector   │ VCS Module    │    │
 * │  │ - Sandbox test      │              │ Code Immune    │               │    │
 * │  │ - Regression check  │              └────────────────────────────────┘    │
 * │  └─────────────────────┘                                                    │
 * └─────────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * DEPLOYMENT MODES:
 * - HOT_PATCH_ONLY: Only apply runtime patches (no source changes)
 * - SOURCE_ONLY: Only commit to source (manual restart required)
 * - DUAL: Both hot-patch AND source commit (recommended)
 *
 * SAFETY FEATURES:
 * - Minimum confidence threshold (default: 0.7)
 * - Maximum risk score (default: 0.3)
 * - Validation required before deployment
 * - Auto-rollback on failure
 * - Learning from outcomes for future improvements
 *
 * @author NIMCP Development Team
 * @date 2025-01-20
 * @version 1.0.0
 */

#ifndef NIMCP_SELF_REPAIR_H
#define NIMCP_SELF_REPAIR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Dependencies - Note: nimcp_recompiler.h not included to avoid crash_context_t conflict */
#include "utils/fault_tolerance/nimcp_diagnostics.h"
#include "cognitive/fault_tolerance/nimcp_recovery_parietal_bridge.h"
#include "cognitive/parietal/nimcp_code_generation.h"
#include "utils/vcs/nimcp_vcs_integration.h"
#include "utils/code/nimcp_hot_inject.h"
#include "cognitive/immune/nimcp_code_immune.h"

/* Forward declarations from nimcp_recompiler.h to avoid type conflict */
typedef struct recompiler* recompiler_t;

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define SELF_REPAIR_VERSION             "1.0.0"
#define SELF_REPAIR_MAGIC               0x53524550  /**< 'SREP' */
#define SELF_REPAIR_MAX_PENDING         64          /**< Max pending repairs */
#define SELF_REPAIR_MAX_HISTORY         256         /**< Max repair history */

//=============================================================================
// Repair Mode and Status Types
//=============================================================================

/**
 * @brief Deployment mode for self-repair
 */
typedef enum {
    REPAIR_MODE_HOT_PATCH_ONLY = 0,     /**< Runtime patch only */
    REPAIR_MODE_SOURCE_ONLY,            /**< Source commit only */
    REPAIR_MODE_DUAL                    /**< Both hot-patch and source (recommended) */
} self_repair_mode_t;

/**
 * @brief Repair pipeline stage
 */
typedef enum {
    REPAIR_STAGE_PENDING = 0,           /**< Awaiting processing */
    REPAIR_STAGE_ANALYZING,             /**< Code analysis in progress */
    REPAIR_STAGE_GENERATING,            /**< Fix generation in progress */
    REPAIR_STAGE_VALIDATING,            /**< Validation in progress */
    REPAIR_STAGE_DEPLOYING,             /**< Deployment in progress */
    REPAIR_STAGE_COMPLETED,             /**< Successfully completed */
    REPAIR_STAGE_FAILED,                /**< Repair failed */
    REPAIR_STAGE_ROLLED_BACK            /**< Fix was rolled back */
} repair_stage_t;

/**
 * @brief Repair result status
 */
typedef enum {
    REPAIR_STATUS_SUCCESS = 0,          /**< Repair successful */
    REPAIR_STATUS_ANALYSIS_FAILED,      /**< Code analysis failed */
    REPAIR_STATUS_NO_FIX_FOUND,         /**< No suitable fix generated */
    REPAIR_STATUS_LOW_CONFIDENCE,       /**< Fix confidence too low */
    REPAIR_STATUS_HIGH_RISK,            /**< Fix risk too high */
    REPAIR_STATUS_VALIDATION_FAILED,    /**< Fix failed validation */
    REPAIR_STATUS_HOT_PATCH_FAILED,     /**< Hot-patch deployment failed */
    REPAIR_STATUS_SOURCE_COMMIT_FAILED, /**< Source commit failed */
    REPAIR_STATUS_ROLLED_BACK,          /**< Fix was rolled back */
    REPAIR_STATUS_TIMEOUT,              /**< Repair timed out */
    REPAIR_STATUS_ERROR                 /**< General error */
} repair_status_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Self-repair coordinator configuration
 */
typedef struct {
    /* Deployment mode */
    self_repair_mode_t mode;                /**< Deployment mode */

    /* Confidence thresholds */
    float min_fix_confidence;               /**< Minimum fix confidence (0.7) */
    float max_risk_score;                   /**< Maximum acceptable risk (0.3) */

    /* Human approval */
    bool require_human_approval;            /**< Require human approval for all fixes */
    bool require_approval_complex;          /**< Require approval for complex fixes only */

    /* Timeouts */
    uint32_t analysis_timeout_ms;           /**< Code analysis timeout */
    uint32_t generation_timeout_ms;         /**< Fix generation timeout */
    uint32_t validation_timeout_ms;         /**< Validation timeout */
    uint32_t deployment_timeout_ms;         /**< Deployment timeout */

    /* Safety */
    bool auto_rollback_on_failure;          /**< Auto-rollback on failure */
    bool auto_rollback_on_regression;       /**< Auto-rollback on regression */

    /* Learning */
    bool learn_from_outcome;                /**< Update code_immune with outcomes */

    /* Logging */
    bool verbose_logging;                   /**< Enable verbose output */
} self_repair_config_t;

//=============================================================================
// Repair Record
//=============================================================================

/**
 * @brief Record of a repair attempt
 */
typedef struct {
    uint64_t repair_id;                     /**< Unique repair ID */

    /* Source */
    uint64_t diagnostic_id;                 /**< Source diagnostic ID */
    error_type_t error_type;                /**< Original error type */

    /* Generated fix */
    uint64_t fix_id;                        /**< Generated fix ID */
    code_fix_strategy_t fix_strategy;       /**< Fix strategy used */
    float fix_confidence;                   /**< Fix confidence */
    float fix_risk;                         /**< Fix risk score */

    /* Location */
    char source_file[512];                  /**< Source file path */
    char function_name[128];                /**< Function name */
    uint32_t start_line;                    /**< Start line */
    uint32_t end_line;                      /**< End line */

    /* Deployment */
    bool hot_patched;                       /**< Hot-patch applied */
    bool source_committed;                  /**< Source committed */
    char commit_hash[64];                   /**< Git commit hash if committed */
    uint64_t patch_id;                      /**< Hot-patch ID if patched */

    /* Status */
    repair_stage_t stage;                   /**< Current stage */
    repair_status_t status;                 /**< Final status */
    char error_message[256];                /**< Error message if failed */

    /* Timing */
    uint64_t start_time;                    /**< Start timestamp (ms) */
    uint64_t end_time;                      /**< End timestamp (ms) */
    uint64_t analysis_time_ms;              /**< Time for analysis */
    uint64_t generation_time_ms;            /**< Time for generation */
    uint64_t validation_time_ms;            /**< Time for validation */
    uint64_t deployment_time_ms;            /**< Time for deployment */

    /* Rollback */
    bool can_rollback;                      /**< Rollback possible */
    bool rolled_back;                       /**< Has been rolled back */
} self_repair_record_t;

//=============================================================================
// Repair Request
//=============================================================================

/**
 * @brief Request to initiate self-repair
 */
typedef struct {
    /* Source - at least one must be provided */
    diagnostic_result_t* diagnosis;         /**< Diagnostic result (preferred) */
    uint64_t diagnostic_id;                 /**< Or diagnostic ID for lookup */

    /* Overrides */
    code_fix_strategy_t preferred_strategy; /**< Preferred fix strategy (NONE for auto) */
    float min_confidence_override;          /**< Override min confidence (0 for default) */
    float max_risk_override;                /**< Override max risk (0 for default) */

    /* Options */
    bool skip_validation;                   /**< Skip validation (dangerous!) */
    bool hot_patch_only;                    /**< Only hot-patch, don't commit */
    bool source_only;                       /**< Only commit, don't hot-patch */
    bool async;                             /**< Process asynchronously */
} self_repair_request_t;

/**
 * @brief Result of self-repair attempt
 */
typedef struct {
    bool success;                           /**< Repair succeeded */
    repair_status_t status;                 /**< Status code */
    char error_message[256];                /**< Error message if failed */

    /* Record */
    self_repair_record_t record;            /**< Full repair record */

    /* Generated fix (if successful) */
    generated_fix_t* fix;                   /**< Generated fix */

    /* Deployment results */
    bool hot_patch_applied;                 /**< Hot-patch was applied */
    bool source_committed;                  /**< Source was committed */
    char commit_hash[64];                   /**< Git commit hash */
} self_repair_result_t;

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Self-repair statistics
 */
typedef struct {
    /* Counts */
    uint64_t repairs_attempted;             /**< Total repair attempts */
    uint64_t repairs_successful;            /**< Successful repairs */
    uint64_t repairs_failed;                /**< Failed repairs */
    uint64_t repairs_rolled_back;           /**< Rolled back repairs */

    /* By stage failure */
    uint64_t analysis_failures;             /**< Failed at analysis */
    uint64_t generation_failures;           /**< Failed at generation */
    uint64_t validation_failures;           /**< Failed at validation */
    uint64_t deployment_failures;           /**< Failed at deployment */

    /* By deployment type */
    uint64_t hot_patches_applied;           /**< Hot-patches applied */
    uint64_t source_commits_made;           /**< Source commits made */

    /* Quality */
    float avg_fix_confidence;               /**< Average fix confidence */
    float avg_fix_risk;                     /**< Average fix risk */
    float success_rate;                     /**< Overall success rate */

    /* Timing */
    float avg_total_time_ms;                /**< Average total repair time */
    float avg_analysis_time_ms;             /**< Average analysis time */
    float avg_generation_time_ms;           /**< Average generation time */
    float avg_validation_time_ms;           /**< Average validation time */
    float avg_deployment_time_ms;           /**< Average deployment time */

    /* By error type */
    uint64_t by_error_type[32];             /**< Count by error type */

    /* By strategy */
    uint64_t by_strategy[16];               /**< Count by fix strategy */
} self_repair_stats_t;

//=============================================================================
// Callback Types
//=============================================================================

/**
 * @brief Callback for repair stage changes
 */
typedef void (*self_repair_stage_cb_t)(
    uint64_t repair_id,
    repair_stage_t old_stage,
    repair_stage_t new_stage,
    void* user_data
);

/**
 * @brief Callback for repair completion
 */
typedef void (*self_repair_complete_cb_t)(
    uint64_t repair_id,
    const self_repair_result_t* result,
    void* user_data
);

/**
 * @brief Callback for human approval request
 */
typedef bool (*self_repair_approval_cb_t)(
    uint64_t repair_id,
    const generated_fix_t* fix,
    void* user_data
);

//=============================================================================
// Opaque Handle
//=============================================================================

typedef struct self_repair_coordinator self_repair_coordinator_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default configuration
 *
 * @return Default configuration
 */
self_repair_config_t self_repair_default_config(void);

/**
 * @brief Create self-repair coordinator
 *
 * WHAT: Initialize the self-repair coordinator
 * WHY:  Entry point for autonomous self-repair capability
 * HOW:  Create and wire up all components
 *
 * @param config Configuration (NULL for defaults)
 * @return Coordinator handle or NULL on failure
 */
self_repair_coordinator_t* self_repair_create(const self_repair_config_t* config);

/**
 * @brief Create with injected dependencies
 *
 * WHAT: Create coordinator with pre-existing components
 * WHY:  Integration with existing system
 * HOW:  Use provided components instead of creating new ones
 *
 * @param config Configuration
 * @param code_gen Code generation engine (can be NULL)
 * @param vcs VCS integration (can be NULL)
 * @param hot_inject Hot injector (can be NULL)
 * @param code_immune Code immune system (can be NULL)
 * @return Coordinator handle or NULL on failure
 */
self_repair_coordinator_t* self_repair_create_with_deps(
    const self_repair_config_t* config,
    code_gen_engine_t* code_gen,
    vcs_integration_t* vcs,
    hot_injector_t hot_inject,
    code_immune_system_t* code_immune
);

/**
 * @brief Destroy self-repair coordinator
 *
 * @param coordinator Coordinator handle (NULL safe)
 */
void self_repair_destroy(self_repair_coordinator_t* coordinator);

/**
 * @brief Check if coordinator is ready
 *
 * @param coordinator Coordinator handle
 * @return true if ready for repair operations
 */
bool self_repair_is_ready(const self_repair_coordinator_t* coordinator);

//=============================================================================
// Core Repair Functions
//=============================================================================

/**
 * @brief Initiate self-repair for diagnostic
 *
 * WHAT: Start the full self-repair pipeline
 * WHY:  Main entry point for autonomous repair
 * HOW:  Analyze → generate → validate → deploy
 *
 * @param coordinator Coordinator handle
 * @param request Repair request
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int self_repair_initiate(
    self_repair_coordinator_t* coordinator,
    const self_repair_request_t* request,
    self_repair_result_t* result
);

/**
 * @brief Process repair asynchronously
 *
 * WHAT: Start repair processing in background
 * WHY:  Non-blocking repair for real-time systems
 * HOW:  Queue repair, return immediately
 *
 * @param coordinator Coordinator handle
 * @param request Repair request
 * @param repair_id Output: assigned repair ID for tracking
 * @return 0 on success (queued), -1 on error
 */
int self_repair_initiate_async(
    self_repair_coordinator_t* coordinator,
    const self_repair_request_t* request,
    uint64_t* repair_id
);

/**
 * @brief Get status of async repair
 *
 * @param coordinator Coordinator handle
 * @param repair_id Repair ID
 * @param result Output result (if complete)
 * @return Current stage, or REPAIR_STAGE_COMPLETED/FAILED if done
 */
repair_stage_t self_repair_get_status(
    self_repair_coordinator_t* coordinator,
    uint64_t repair_id,
    self_repair_result_t* result
);

/**
 * @brief Cancel async repair
 *
 * @param coordinator Coordinator handle
 * @param repair_id Repair ID to cancel
 * @return 0 on success
 */
int self_repair_cancel(
    self_repair_coordinator_t* coordinator,
    uint64_t repair_id
);

//=============================================================================
// Pipeline Stage Functions
//=============================================================================

/**
 * @brief Perform code analysis stage
 *
 * WHAT: Analyze code at failure location
 * WHY:  Understand code structure for fix generation
 * HOW:  Call recovery_parietal_bridge analysis
 *
 * @param coordinator Coordinator handle
 * @param diagnosis Diagnostic result
 * @param analysis Output code analysis result
 * @return 0 on success
 */
int self_repair_analyze_code(
    self_repair_coordinator_t* coordinator,
    const diagnostic_result_t* diagnosis,
    code_analysis_result_t* analysis
);

/**
 * @brief Perform fix generation stage
 *
 * WHAT: Generate candidate fixes for error
 * WHY:  Core code generation step
 * HOW:  Call code generation engine
 *
 * @param coordinator Coordinator handle
 * @param diagnosis Diagnostic result
 * @param analysis Code analysis result
 * @param fix Output best generated fix
 * @return 0 on success
 */
int self_repair_generate_fix(
    self_repair_coordinator_t* coordinator,
    const diagnostic_result_t* diagnosis,
    const code_analysis_result_t* analysis,
    generated_fix_t* fix
);

/**
 * @brief Perform validation stage
 *
 * WHAT: Validate generated fix
 * WHY:  Ensure fix compiles and passes tests
 * HOW:  Call recompiler with sandbox testing
 *
 * @param coordinator Coordinator handle
 * @param fix Fix to validate
 * @param validation_result Output validation result
 * @return 0 if valid, -1 if invalid
 */
int self_repair_validate_fix(
    self_repair_coordinator_t* coordinator,
    const generated_fix_t* fix,
    void* validation_result
);

/**
 * @brief Perform hot-patch deployment
 *
 * WHAT: Apply fix to running system
 * WHY:  Immediate protection without restart
 * HOW:  Call hot injector
 *
 * @param coordinator Coordinator handle
 * @param fix Fix to deploy
 * @param patch_id Output: hot-patch ID
 * @return 0 on success
 */
int self_repair_deploy_hot_patch(
    self_repair_coordinator_t* coordinator,
    const generated_fix_t* fix,
    uint64_t* patch_id
);

/**
 * @brief Perform source commit deployment
 *
 * WHAT: Commit fix to source code
 * WHY:  Permanent fix in codebase
 * HOW:  Call VCS integration
 *
 * @param coordinator Coordinator handle
 * @param fix Fix to commit
 * @param commit_hash Output: git commit hash
 * @param commit_hash_size Size of commit_hash buffer
 * @return 0 on success
 */
int self_repair_deploy_source(
    self_repair_coordinator_t* coordinator,
    const generated_fix_t* fix,
    char* commit_hash,
    size_t commit_hash_size
);

//=============================================================================
// Rollback Functions
//=============================================================================

/**
 * @brief Rollback a repair
 *
 * WHAT: Undo a previously applied repair
 * WHY:  Fix caused regression or other issues
 * HOW:  Rollback hot-patch and/or source commit
 *
 * @param coordinator Coordinator handle
 * @param repair_id Repair ID to rollback
 * @return 0 on success
 */
int self_repair_rollback(
    self_repair_coordinator_t* coordinator,
    uint64_t repair_id
);

/**
 * @brief Report repair regression
 *
 * WHAT: Report that a repair caused a regression
 * WHY:  Trigger auto-rollback and learning
 * HOW:  Mark repair as failed, optionally rollback
 *
 * @param coordinator Coordinator handle
 * @param repair_id Repair ID that regressed
 * @param auto_rollback Whether to automatically rollback
 * @return 0 on success
 */
int self_repair_report_regression(
    self_repair_coordinator_t* coordinator,
    uint64_t repair_id,
    bool auto_rollback
);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Set stage change callback
 */
int self_repair_set_stage_callback(
    self_repair_coordinator_t* coordinator,
    self_repair_stage_cb_t callback,
    void* user_data
);

/**
 * @brief Set completion callback
 */
int self_repair_set_complete_callback(
    self_repair_coordinator_t* coordinator,
    self_repair_complete_cb_t callback,
    void* user_data
);

/**
 * @brief Set approval callback
 */
int self_repair_set_approval_callback(
    self_repair_coordinator_t* coordinator,
    self_repair_approval_cb_t callback,
    void* user_data
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get repair record by ID
 *
 * @param coordinator Coordinator handle
 * @param repair_id Repair ID
 * @return Repair record or NULL if not found
 */
const self_repair_record_t* self_repair_get_record(
    self_repair_coordinator_t* coordinator,
    uint64_t repair_id
);

/**
 * @brief Get recent repair records
 *
 * @param coordinator Coordinator handle
 * @param records Output array for records
 * @param max_records Max records to return
 * @return Number of records copied
 */
uint32_t self_repair_get_recent_records(
    self_repair_coordinator_t* coordinator,
    self_repair_record_t* records,
    uint32_t max_records
);

/**
 * @brief Get statistics
 *
 * @param coordinator Coordinator handle
 * @param stats Output statistics
 * @return 0 on success
 */
int self_repair_get_stats(
    const self_repair_coordinator_t* coordinator,
    self_repair_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param coordinator Coordinator handle
 */
void self_repair_reset_stats(self_repair_coordinator_t* coordinator);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get stage name string
 *
 * @param stage Repair stage
 * @return Stage name (static)
 */
const char* self_repair_stage_name(repair_stage_t stage);

/**
 * @brief Get status name string
 *
 * @param status Repair status
 * @return Status name (static)
 */
const char* self_repair_status_name(repair_status_t status);

/**
 * @brief Get mode name string
 *
 * @param mode Repair mode
 * @return Mode name (static)
 */
const char* self_repair_mode_name(self_repair_mode_t mode);

/**
 * @brief Get version string
 *
 * @return Version string
 */
const char* self_repair_version(void);

//=============================================================================
// Bio-Async Communication Functions
//=============================================================================

/**
 * @brief Broadcast repair stage change via bio-async
 *
 * WHAT: Notify other modules about repair stage transitions
 * WHY:  Enable cross-module coordination during repair
 * HOW:  Send broadcast message via bio-router
 *
 * @param coordinator Coordinator handle
 * @param repair_id Repair ID
 * @param old_stage Previous stage
 * @param new_stage New stage
 * @return 0 on success, -1 on error
 */
int self_repair_broadcast_stage_change(
    self_repair_coordinator_t* coordinator,
    uint64_t repair_id,
    repair_stage_t old_stage,
    repair_stage_t new_stage
);

/**
 * @brief Broadcast repair result via bio-async
 *
 * WHAT: Notify other modules about repair completion/failure
 * WHY:  Enable cross-module awareness of repair outcomes
 * HOW:  Send broadcast message via bio-router
 *
 * @param coordinator Coordinator handle
 * @param repair_id Repair ID
 * @param success True if repair succeeded
 * @param status Final repair status
 * @return 0 on success, -1 on error
 */
int self_repair_broadcast_result(
    self_repair_coordinator_t* coordinator,
    uint64_t repair_id,
    bool success,
    repair_status_t status
);

/**
 * @brief Process pending bio-async messages
 *
 * WHAT: Process incoming bio-async messages
 * WHY:  Handle requests from other modules
 * HOW:  Call bio_router_process_inbox
 *
 * @param coordinator Coordinator handle
 * @param max_messages Maximum messages to process
 * @return Number of messages processed
 */
uint32_t self_repair_process_messages(
    self_repair_coordinator_t* coordinator,
    uint32_t max_messages
);

//=============================================================================
// Health Agent Integration (Phase 4)
//=============================================================================

/* Forward declaration */
struct health_agent;

/**
 * @brief Callback for repair failure notification (direct callback)
 *
 * Called when a repair fails to allow health agent integration.
 * Note: Uses different signature from self_repair_health_notify.h callback.
 */
typedef void (*self_repair_direct_failure_cb_t)(
    uint64_t repair_id,
    repair_status_t status,
    const char* error_message,
    void* user_data
);

/**
 * @brief Connect self-repair coordinator to health agent
 *
 * WHAT: Link self-repair system with health agent for failure escalation
 * WHY:  Enable health agent to track repair failures and escalate
 * HOW:  Store health agent reference for notifications
 *
 * @param coordinator Coordinator handle
 * @param agent Health agent to connect (can be NULL to disconnect)
 * @return 0 on success, -1 on error
 */
int self_repair_connect_health_agent(
    self_repair_coordinator_t* coordinator,
    struct health_agent* agent
);

/**
 * @brief Set failure notification callback
 *
 * WHAT: Register callback for repair failures
 * WHY:  Allow external systems to react to repair failures
 * HOW:  Store callback for invocation on failure
 *
 * @param coordinator Coordinator handle
 * @param callback Failure callback function
 * @param user_data User data to pass to callback
 * @return 0 on success, -1 on error
 */
int self_repair_set_failure_callback(
    self_repair_coordinator_t* coordinator,
    self_repair_direct_failure_cb_t callback,
    void* user_data
);

/**
 * @brief Notify health agent of repair failure
 *
 * WHAT: Send failure notification to connected health agent
 * WHY:  Escalate failed repairs for monitoring/alerting
 * HOW:  Send message to health agent with failure details
 *
 * @param coordinator Coordinator handle
 * @param repair_id Failed repair ID
 * @param status Failure status code
 * @param error_message Descriptive error message
 * @return 0 on success, -1 on error
 */
int self_repair_notify_health_agent_failure(
    self_repair_coordinator_t* coordinator,
    uint64_t repair_id,
    repair_status_t status,
    const char* error_message
);

/**
 * @brief Check if health agent is connected
 *
 * @param coordinator Coordinator handle
 * @return true if health agent is connected
 */
bool self_repair_has_health_agent(const self_repair_coordinator_t* coordinator);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SELF_REPAIR_H */
