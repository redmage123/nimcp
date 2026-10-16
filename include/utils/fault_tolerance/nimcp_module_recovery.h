/**
 * @file nimcp_module_recovery.h
 * @brief Module-Specific Recovery Actions for Fault Tolerance
 *
 * WHAT: Graduated recovery strategies for individual modules
 * WHY:  Different modules need different recovery approaches
 * HOW:  Register module-specific recovery callbacks with recovery system
 *
 * PHASE 8: System-Wide Health Integration
 * Part of the resilience infrastructure for module-specific recovery.
 *
 * GRADUATED RECOVERY STRATEGY:
 * 1. Light reset: Clear traces/caches, preserve learned state
 * 2. Partial reset: Reset recent changes, reload last checkpoint
 * 3. Full reset: Reinitialize module to defaults
 * 4. Isolation: Disable module, mark for manual intervention
 *
 * INTEGRATION POINTS:
 * - State manager: Uses state ops for checkpoint/restore
 * - Health agent: Triggers recovery on anomalies
 * - Recovery system: Provides unified recovery API
 *
 * @author NIMCP Team
 * @date 2026-01-20
 * @version 1.0.0
 */

#ifndef NIMCP_MODULE_RECOVERY_H
#define NIMCP_MODULE_RECOVERY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of recoverable modules */
#define NIMCP_MODULE_RECOVERY_MAX_MODULES 64

/** Maximum module name length */
#define NIMCP_MODULE_RECOVERY_MAX_NAME_LEN 64

/** Module recovery magic for validation */
#define NIMCP_MODULE_RECOVERY_MAGIC 0x4D52454D  /* "MREM" */

//=============================================================================
// Recovery Level Enumeration
//=============================================================================

/**
 * @brief Module recovery level
 *
 * WHAT: Severity of recovery action
 * WHY:  Enable graduated response to failures
 * HOW:  Start with light recovery, escalate if needed
 */
typedef enum nimcp_module_recovery_level {
    NIMCP_MODULE_RECOVERY_NONE = 0,      /**< No recovery needed */
    NIMCP_MODULE_RECOVERY_LIGHT,         /**< Clear traces/caches only */
    NIMCP_MODULE_RECOVERY_PARTIAL,       /**< Reset recent state, preserve core */
    NIMCP_MODULE_RECOVERY_FULL,          /**< Full reset to defaults */
    NIMCP_MODULE_RECOVERY_ISOLATE        /**< Disable module entirely */
} nimcp_module_recovery_level_t;

/**
 * @brief Module recovery result
 */
typedef enum nimcp_module_recovery_result {
    NIMCP_MODULE_RECOVERY_SUCCESS = 0,   /**< Recovery successful */
    NIMCP_MODULE_RECOVERY_PARTIAL_SUCCESS, /**< Partial recovery */
    NIMCP_MODULE_RECOVERY_FAILED,        /**< Recovery failed */
    NIMCP_MODULE_RECOVERY_ESCALATE       /**< Escalate to next level */
} nimcp_module_recovery_result_t;

//=============================================================================
// Module Recovery Callbacks Interface
//=============================================================================

/**
 * @brief Module recovery callback type
 *
 * @param module_state Pointer to module's state context
 * @param level Recovery level to attempt
 * @param user_data Optional user data
 * @return Recovery result
 */
typedef nimcp_module_recovery_result_t (*nimcp_module_recovery_fn)(
    void* module_state,
    nimcp_module_recovery_level_t level,
    void* user_data
);

/**
 * @brief Module health check callback type
 *
 * @param module_state Pointer to module's state context
 * @param out_health Output health score (0.0-1.0, 1.0 = healthy)
 * @return 0 on success, negative on error
 */
typedef int (*nimcp_module_health_check_fn)(
    void* module_state,
    float* out_health
);

/**
 * @brief Module recovery operations interface
 */
typedef struct nimcp_module_recovery_ops {
    /**
     * @brief Perform recovery at specified level
     * @param module_state Module state context
     * @param level Recovery level
     * @param user_data Optional user data
     * @return Recovery result
     */
    nimcp_module_recovery_fn recover;

    /**
     * @brief Check module health
     * @param module_state Module state context
     * @param out_health Output health score
     * @return 0 on success, negative on error
     */
    nimcp_module_health_check_fn health_check;

    /**
     * @brief User data passed to callbacks
     */
    void* user_data;

} nimcp_module_recovery_ops_t;

//=============================================================================
// Module Recovery Entry
//=============================================================================

/**
 * @brief Registered recoverable module entry
 */
typedef struct nimcp_module_recovery_entry {
    char name[NIMCP_MODULE_RECOVERY_MAX_NAME_LEN];  /**< Module name */
    nimcp_module_recovery_ops_t ops;                 /**< Recovery operations */
    void* state;                                     /**< Module state context */
    bool enabled;                                    /**< Is recovery enabled */
    bool isolated;                                   /**< Is module isolated */

    /* Statistics */
    uint32_t recovery_attempts;                      /**< Total recovery attempts */
    uint32_t recovery_successes;                     /**< Successful recoveries */
    uint32_t recovery_failures;                      /**< Failed recoveries */
    uint32_t escalations;                            /**< Times escalated to next level */
    nimcp_module_recovery_level_t last_level;        /**< Last recovery level used */
    uint64_t last_recovery_time;                     /**< Timestamp of last recovery */
    float last_health_score;                         /**< Last health check score */

} nimcp_module_recovery_entry_t;

//=============================================================================
// Module Recovery Manager
//=============================================================================

/**
 * @brief Module recovery manager
 */
typedef struct nimcp_module_recovery_manager {
    uint32_t magic;                                                     /**< Validation magic */
    nimcp_module_recovery_entry_t modules[NIMCP_MODULE_RECOVERY_MAX_MODULES];
    uint32_t module_count;                                              /**< Number of registered modules */
    bool initialized;                                                   /**< Is manager initialized */

    /* Configuration */
    float health_threshold;                                             /**< Threshold for triggering recovery */
    bool auto_escalate;                                                 /**< Auto-escalate on failure */
    uint32_t max_escalation_level;                                      /**< Max escalation level */

    /* Statistics */
    uint64_t total_recoveries;                                          /**< Total recovery operations */
    uint64_t total_health_checks;                                       /**< Total health checks */

    /* Thread safety */
    void* mutex;                                                        /**< Mutex for thread safety */

} nimcp_module_recovery_manager_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create module recovery manager
 * @return Pointer to manager, NULL on failure
 */
nimcp_module_recovery_manager_t* nimcp_module_recovery_manager_create(void);

/**
 * @brief Destroy module recovery manager
 * @param manager Manager to destroy
 */
void nimcp_module_recovery_manager_destroy(nimcp_module_recovery_manager_t* manager);

//=============================================================================
// Module Registration API
//=============================================================================

/**
 * @brief Register a module for recovery
 * @param manager Recovery manager
 * @param name Module name (must be unique)
 * @param ops Recovery operations
 * @param state Module state context
 * @return 0 on success, negative on error
 */
int nimcp_module_recovery_register(
    nimcp_module_recovery_manager_t* manager,
    const char* name,
    const nimcp_module_recovery_ops_t* ops,
    void* state
);

/**
 * @brief Unregister a module
 * @param manager Recovery manager
 * @param name Module name
 * @return 0 on success, negative on error
 */
int nimcp_module_recovery_unregister(
    nimcp_module_recovery_manager_t* manager,
    const char* name
);

/**
 * @brief Enable/disable recovery for a module
 * @param manager Recovery manager
 * @param name Module name
 * @param enabled True to enable, false to disable
 * @return 0 on success, negative on error
 */
int nimcp_module_recovery_set_enabled(
    nimcp_module_recovery_manager_t* manager,
    const char* name,
    bool enabled
);

//=============================================================================
// Recovery Execution API
//=============================================================================

/**
 * @brief Attempt recovery for a specific module
 *
 * GRADUATED RECOVERY:
 * 1. Try at specified level
 * 2. If failed and auto_escalate, try next level
 * 3. Continue until success or max level reached
 *
 * @param manager Recovery manager
 * @param name Module name
 * @param level Initial recovery level
 * @return Recovery result
 */
nimcp_module_recovery_result_t nimcp_module_recovery_attempt(
    nimcp_module_recovery_manager_t* manager,
    const char* name,
    nimcp_module_recovery_level_t level
);

/**
 * @brief Attempt recovery for all modules below health threshold
 * @param manager Recovery manager
 * @return Number of modules recovered (negative on error)
 */
int nimcp_module_recovery_attempt_all_unhealthy(
    nimcp_module_recovery_manager_t* manager
);

/**
 * @brief Check health of all modules
 * @param manager Recovery manager
 * @return Average health score (0.0-1.0)
 */
float nimcp_module_recovery_check_all_health(
    nimcp_module_recovery_manager_t* manager
);

/**
 * @brief Check health of a specific module
 * @param manager Recovery manager
 * @param name Module name
 * @param out_health Output health score
 * @return 0 on success, negative on error
 */
int nimcp_module_recovery_check_health(
    nimcp_module_recovery_manager_t* manager,
    const char* name,
    float* out_health
);

//=============================================================================
// Isolation API
//=============================================================================

/**
 * @brief Isolate a failed module
 * @param manager Recovery manager
 * @param name Module name
 * @return 0 on success, negative on error
 */
int nimcp_module_recovery_isolate(
    nimcp_module_recovery_manager_t* manager,
    const char* name
);

/**
 * @brief Restore an isolated module
 * @param manager Recovery manager
 * @param name Module name
 * @return 0 on success, negative on error
 */
int nimcp_module_recovery_restore(
    nimcp_module_recovery_manager_t* manager,
    const char* name
);

/**
 * @brief Check if module is isolated
 * @param manager Recovery manager
 * @param name Module name
 * @return true if isolated, false otherwise
 */
bool nimcp_module_recovery_is_isolated(
    nimcp_module_recovery_manager_t* manager,
    const char* name
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Module recovery statistics
 */
typedef struct nimcp_module_recovery_stats {
    uint32_t module_count;
    uint32_t enabled_modules;
    uint32_t isolated_modules;
    uint64_t total_recoveries;
    uint64_t total_successes;
    uint64_t total_failures;
    uint64_t total_escalations;
    float average_health;
} nimcp_module_recovery_stats_t;

/**
 * @brief Get recovery statistics
 * @param manager Recovery manager
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int nimcp_module_recovery_get_stats(
    nimcp_module_recovery_manager_t* manager,
    nimcp_module_recovery_stats_t* stats
);

//=============================================================================
// Built-in Module Recovery Functions
//=============================================================================

/**
 * @brief STDP synapse recovery function
 *
 * GRADUATED RECOVERY:
 * - LIGHT: Reset traces (pre_trace, post_trace) to zero
 * - PARTIAL: Reset statistics, restore from last checkpoint if available
 * - FULL: Reset weight to 0.5, all parameters to defaults
 * - ISOLATE: Mark synapse as non-functional
 *
 * @param module_state Pointer to stdp_synapse_t
 * @param level Recovery level
 * @param user_data Optional user data (state manager for checkpoint)
 * @return Recovery result
 */
nimcp_module_recovery_result_t nimcp_stdp_recovery(
    void* module_state,
    nimcp_module_recovery_level_t level,
    void* user_data
);

/**
 * @brief STDP synapse health check function
 *
 * HEALTH FACTORS:
 * - Weight in valid range (0.0-1.0): 40%
 * - Traces finite and reasonable: 30%
 * - No excessive saturation events: 20%
 * - Parameters valid: 10%
 *
 * @param module_state Pointer to stdp_synapse_t
 * @param out_health Output health score (0.0-1.0)
 * @return 0 on success, negative on error
 */
int nimcp_stdp_health_check(void* module_state, float* out_health);

/**
 * @brief Astrocyte network recovery function
 *
 * GRADUATED RECOVERY:
 * - LIGHT: Reset calcium to baseline, flush pools
 * - PARTIAL: Reset all astrocytes, preserve topology
 * - FULL: Full network reset, rebuild topology
 * - ISOLATE: Disable network updates
 *
 * @param module_state Pointer to astrocyte_network_t
 * @param level Recovery level
 * @param user_data Optional user data (state manager for checkpoint)
 * @return Recovery result
 */
nimcp_module_recovery_result_t nimcp_astrocyte_recovery(
    void* module_state,
    nimcp_module_recovery_level_t level,
    void* user_data
);

/**
 * @brief Astrocyte network health check function
 *
 * HEALTH FACTORS:
 * - Calcium levels in valid range: 40%
 * - No NaN/Inf values: 30%
 * - Homeostatic regulation active: 20%
 * - Topology intact: 10%
 *
 * @param module_state Pointer to astrocyte_network_t
 * @param out_health Output health score (0.0-1.0)
 * @return 0 on success, negative on error
 */
int nimcp_astrocyte_health_check(void* module_state, float* out_health);

/**
 * @brief Get STDP recovery operations
 * @return Pointer to static recovery ops structure
 */
const nimcp_module_recovery_ops_t* nimcp_stdp_get_recovery_ops(void);

/**
 * @brief Get astrocyte network recovery operations
 * @return Pointer to static recovery ops structure
 */
const nimcp_module_recovery_ops_t* nimcp_astrocyte_get_recovery_ops(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MODULE_RECOVERY_H */
