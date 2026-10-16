/**
 * @file nimcp_security_recovery_bridge.h
 * @brief Security-Fault Tolerance Bridge - Automated Repair on Security Violations
 *
 * WHAT: Connects security monitoring to fault tolerance recovery systems
 * WHY:  Enable automatic repair when security violations (tampering, corruption) detected
 * HOW:  Security violations trigger appropriate fault tolerance recovery actions
 *
 * ARCHITECTURE:
 *
 *   +------------------+       +----------------------+       +------------------+
 *   | Security Module  |       | Security-Recovery    |       | Fault Tolerance  |
 *   |------------------|       | Bridge               |       |------------------|
 *   | Coverage         |------>|                      |------>| Fast Recovery    |
 *   | Fractal Security |       | Violation Handler    |       | Brain Recovery   |
 *   | CFI              |       | Repair Dispatcher    |       | Checkpoints      |
 *   | Shadow Stack     |       | Health Monitor       |       | State Rollback   |
 *   +------------------+       +----------------------+       +------------------+
 *          |                            |                            |
 *          v                            v                            v
 *   +------------------------------------------------------------------+
 *   |                         BRAIN SYSTEM                             |
 *   | - Registers critical regions for security protection             |
 *   | - Receives security status updates                               |
 *   | - Cognitive decisions informed by security state                 |
 *   +------------------------------------------------------------------+
 *
 * RECOVERY FLOW:
 * 1. Security module detects violation (hash mismatch, CFI failure, etc.)
 * 2. Bridge receives violation notification
 * 3. Bridge classifies severity and determines recovery action
 * 4. Bridge triggers appropriate fault tolerance mechanism
 * 5. Recovery executes (fast path or full recovery)
 * 6. Bridge logs outcome and updates security audit
 * 7. Brain is notified of security event (for cognitive awareness)
 *
 * Part of Phase SC-1: Security Coverage Framework (Tier 0.7)
 */

#ifndef NIMCP_SECURITY_RECOVERY_BRIDGE_H
#define NIMCP_SECURITY_RECOVERY_BRIDGE_H

#include "utils/validation/nimcp_common.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "security/nimcp_security_coverage.h"
#include "security/nimcp_security_fractal.h"
#include "security/nimcp_cfi.h"
#include "security/nimcp_shadow_stack.h"
#include "security/nimcp_security_audit.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct brain_struct* brain_t;
typedef struct brain_recovery_context_internal* brain_recovery_context_t;

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of registered brains */
#define NIMCP_SRB_MAX_BRAINS 16

/** Maximum violation history entries */
#define NIMCP_SRB_MAX_HISTORY 256

/** Auto-repair threshold (violations before triggering full recovery) */
#define NIMCP_SRB_AUTO_REPAIR_THRESHOLD 3

/** Violation cooldown period (ms) before re-triggering repair */
#define NIMCP_SRB_COOLDOWN_MS 1000

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Security violation types (bridges to recovery actions)
 */
typedef enum {
    NIMCP_SV_NONE = 0,                   /**< No violation */

    /* Memory Integrity Violations */
    NIMCP_SV_MEMORY_HASH_MISMATCH,       /**< Hash verification failed */
    NIMCP_SV_MEMORY_PROTECTION_BREACH,   /**< Protected memory modified */
    NIMCP_SV_MEMORY_CORRUPTION,          /**< General memory corruption */

    /* Control Flow Violations */
    NIMCP_SV_CFI_INVALID_TARGET,         /**< Invalid call target */
    NIMCP_SV_CFI_TYPE_MISMATCH,          /**< Type signature mismatch */
    NIMCP_SV_SHADOW_STACK_MISMATCH,      /**< Return address mismatch */

    /* Fractal Security Violations */
    NIMCP_SV_FRACTAL_HASH_MISMATCH,      /**< Hierarchical hash failure */
    NIMCP_SV_FRACTAL_DIMENSION_ANOMALY,  /**< Fractal dimension anomaly */
    NIMCP_SV_FRACTAL_TRUST_DEGRADED,     /**< Trust level dropped */

    /* Capability Violations */
    NIMCP_SV_CAPABILITY_UNAUTHORIZED,    /**< Unauthorized access attempt */
    NIMCP_SV_CAPABILITY_REVOKED,         /**< Used revoked capability */

    /* Temporal Violations */
    NIMCP_SV_TEMPORAL_GAP,               /**< Monitoring gap detected */
    NIMCP_SV_TEMPORAL_ANOMALY,           /**< Timing anomaly */

    NIMCP_SV_TYPE_COUNT                  /**< Number of violation types */
} nimcp_security_violation_type_t;

/**
 * @brief Recovery action to take for security violation
 */
typedef enum {
    NIMCP_SRA_NONE = 0,                  /**< No action (logging only) */
    NIMCP_SRA_LOG_ONLY,                  /**< Log and continue */
    NIMCP_SRA_REHASH,                    /**< Recompute hashes */
    NIMCP_SRA_RESTORE_CHECKPOINT,        /**< Restore from checkpoint */
    NIMCP_SRA_FAST_RECOVERY,             /**< Trigger fast recovery */
    NIMCP_SRA_FULL_RECOVERY,             /**< Trigger full recovery */
    NIMCP_SRA_HALT,                      /**< Halt system (critical) */
    NIMCP_SRA_QUARANTINE                 /**< Isolate affected component */
} nimcp_security_recovery_action_t;

/**
 * @brief Severity levels for violations
 */
typedef enum {
    NIMCP_SV_SEVERITY_INFO = 0,          /**< Informational */
    NIMCP_SV_SEVERITY_LOW,               /**< Low - monitor */
    NIMCP_SV_SEVERITY_MEDIUM,            /**< Medium - investigate */
    NIMCP_SV_SEVERITY_HIGH,              /**< High - action needed */
    NIMCP_SV_SEVERITY_CRITICAL           /**< Critical - immediate action */
} nimcp_sv_severity_t;

/**
 * @brief Bridge operational mode
 */
typedef enum {
    NIMCP_SRB_MODE_MONITOR = 0,          /**< Monitor only (no auto-repair) */
    NIMCP_SRB_MODE_ADVISORY,             /**< Suggest repairs (no auto) */
    NIMCP_SRB_MODE_AUTO_REPAIR,          /**< Automatic repair enabled */
    NIMCP_SRB_MODE_PARANOID              /**< Aggressive repair on any violation */
} nimcp_srb_mode_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Security violation event
 */
typedef struct {
    nimcp_security_violation_type_t type; /**< Violation type */
    nimcp_sv_severity_t severity;         /**< Severity level */
    uint64_t timestamp;                   /**< When occurred */

    void* affected_address;               /**< Memory address involved */
    size_t affected_size;                 /**< Size of affected region */
    const char* region_name;              /**< Name of affected region */

    brain_t affected_brain;               /**< Brain affected (if any) */

    uint8_t expected_hash[32];            /**< Expected hash (if applicable) */
    uint8_t actual_hash[32];              /**< Actual hash (if applicable) */

    char details[256];                    /**< Human-readable details */
} nimcp_security_violation_t;

/**
 * @brief Recovery result from security violation
 */
typedef struct {
    nimcp_security_recovery_action_t action_taken; /**< Action performed */
    bool success;                         /**< Recovery successful */
    uint32_t recovery_time_us;            /**< Time to recover (microseconds) */

    bool checkpoint_restored;             /**< Was checkpoint restored */
    const char* checkpoint_name;          /**< Checkpoint used (if any) */

    bool data_recovered;                  /**< Was data recovered */
    uint32_t bytes_recovered;             /**< Bytes recovered */

    char message[256];                    /**< Status message */
} nimcp_security_recovery_result_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    nimcp_srb_mode_t mode;                /**< Operational mode */

    bool enable_auto_checkpoint;          /**< Auto-checkpoint on registration */
    uint32_t checkpoint_interval_ms;      /**< Checkpoint interval (0 = disabled) */

    bool enable_fractal_verification;     /**< Use fractal security */
    uint32_t verification_interval_ms;    /**< Verification interval */

    uint32_t cooldown_ms;                 /**< Cooldown between repairs */
    uint32_t max_repairs_per_minute;      /**< Rate limit repairs */

    bool notify_brain;                    /**< Notify brain of violations */
    bool log_to_audit;                    /**< Log to audit system */
} nimcp_srb_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t violations_detected;         /**< Total violations detected */
    uint64_t recoveries_attempted;        /**< Recovery attempts */
    uint64_t recoveries_successful;       /**< Successful recoveries */
    uint64_t recoveries_failed;           /**< Failed recoveries */

    uint64_t checkpoints_created;         /**< Checkpoints created */
    uint64_t checkpoints_restored;        /**< Checkpoints restored */

    uint64_t fast_recoveries;             /**< Fast path recoveries */
    uint64_t full_recoveries;             /**< Full recoveries */

    uint32_t avg_recovery_time_us;        /**< Average recovery time */
    uint32_t max_recovery_time_us;        /**< Maximum recovery time */

    /* Per-type violation counts */
    uint32_t violations_by_type[NIMCP_SV_TYPE_COUNT];
} nimcp_srb_stats_t;

/**
 * @brief Violation callback function type
 *
 * @param violation The violation that occurred
 * @param user_data User-provided context
 * @return Recovery action to take (or NIMCP_SRA_NONE for default)
 */
typedef nimcp_security_recovery_action_t (*nimcp_srb_callback_t)(
    const nimcp_security_violation_t* violation,
    void* user_data
);

/**
 * @brief Security-Recovery Bridge context (opaque handle)
 */
typedef struct nimcp_security_recovery_bridge nimcp_security_recovery_bridge_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create security-recovery bridge
 *
 * @return Bridge context or NULL on failure
 */
nimcp_security_recovery_bridge_t* nimcp_srb_create(void);

/**
 * @brief Initialize bridge with configuration
 *
 * @param bridge Bridge context
 * @param config Configuration (NULL for defaults)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_srb_init(
    nimcp_security_recovery_bridge_t* bridge,
    const nimcp_srb_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge context
 */
void nimcp_srb_destroy(nimcp_security_recovery_bridge_t* bridge);

/**
 * @brief Get default configuration
 *
 * @return Default configuration
 */
nimcp_srb_config_t nimcp_srb_default_config(void);

//=============================================================================
// Brain Registration
//=============================================================================

/**
 * @brief Register brain with security-recovery bridge
 *
 * WHAT: Register brain for security monitoring and automatic recovery
 * WHY:  Enable security protection for brain's critical data structures
 * HOW:  Register memory regions, enable fractal protection, setup checkpoints
 *
 * @param bridge Bridge context
 * @param brain Brain to register
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_srb_register_brain(
    nimcp_security_recovery_bridge_t* bridge,
    brain_t brain
);

/**
 * @brief Unregister brain from bridge
 *
 * @param bridge Bridge context
 * @param brain Brain to unregister
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_srb_unregister_brain(
    nimcp_security_recovery_bridge_t* bridge,
    brain_t brain
);

/**
 * @brief Register brain's critical memory regions for protection
 *
 * @param bridge Bridge context
 * @param brain Brain instance
 * @return Number of regions registered
 */
uint32_t nimcp_srb_register_brain_regions(
    nimcp_security_recovery_bridge_t* bridge,
    brain_t brain
);

//=============================================================================
// Security Module Connection
//=============================================================================

/**
 * @brief Connect security coverage module to bridge
 *
 * @param bridge Bridge context
 * @param coverage Security coverage context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_srb_connect_coverage(
    nimcp_security_recovery_bridge_t* bridge,
    nimcp_security_coverage_t* coverage
);

/**
 * @brief Connect fractal security module to bridge
 *
 * @param bridge Bridge context
 * @param fsc Fractal security context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_srb_connect_fractal(
    nimcp_security_recovery_bridge_t* bridge,
    nimcp_fractal_security_t* fsc
);

/**
 * @brief Connect CFI module to bridge
 *
 * @param bridge Bridge context
 * @param cfi CFI context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_srb_connect_cfi(
    nimcp_security_recovery_bridge_t* bridge,
    nimcp_cfi_context_t* cfi
);

/**
 * @brief Connect shadow stack module to bridge
 *
 * @param bridge Bridge context
 * @param ss Shadow stack context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_srb_connect_shadow_stack(
    nimcp_security_recovery_bridge_t* bridge,
    nimcp_shadow_stack_t* ss
);

/**
 * @brief Connect audit log to bridge
 *
 * @param bridge Bridge context
 * @param audit Audit log context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_srb_connect_audit(
    nimcp_security_recovery_bridge_t* bridge,
    nimcp_audit_log_t* audit
);

//=============================================================================
// Violation Handling
//=============================================================================

/**
 * @brief Report security violation to bridge
 *
 * WHAT: Notify bridge of security violation
 * WHY:  Trigger appropriate recovery action
 * HOW:  Classify violation, determine action, execute recovery
 *
 * @param bridge Bridge context
 * @param violation Violation details
 * @param result Output: recovery result (can be NULL)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_srb_report_violation(
    nimcp_security_recovery_bridge_t* bridge,
    const nimcp_security_violation_t* violation,
    nimcp_security_recovery_result_t* result
);

/**
 * @brief Report memory hash mismatch
 *
 * Convenience function for hash verification failures.
 *
 * @param bridge Bridge context
 * @param address Memory address
 * @param size Memory size
 * @param region_name Region name
 * @param brain Affected brain (can be NULL)
 * @return Recovery result
 */
nimcp_security_recovery_result_t nimcp_srb_report_hash_mismatch(
    nimcp_security_recovery_bridge_t* bridge,
    void* address,
    size_t size,
    const char* region_name,
    brain_t brain
);

/**
 * @brief Report CFI violation
 *
 * @param bridge Bridge context
 * @param target_address Invalid target address
 * @param type_id Expected type ID
 * @param brain Affected brain (can be NULL)
 * @return Recovery result
 */
nimcp_security_recovery_result_t nimcp_srb_report_cfi_violation(
    nimcp_security_recovery_bridge_t* bridge,
    void* target_address,
    uint32_t type_id,
    brain_t brain
);

/**
 * @brief Report shadow stack mismatch
 *
 * @param bridge Bridge context
 * @param expected Expected return address
 * @param actual Actual return address
 * @param brain Affected brain (can be NULL)
 * @return Recovery result
 */
nimcp_security_recovery_result_t nimcp_srb_report_stack_mismatch(
    nimcp_security_recovery_bridge_t* bridge,
    void* expected,
    void* actual,
    brain_t brain
);

/**
 * @brief Report fractal integrity violation
 *
 * @param bridge Bridge context
 * @param node Affected fractal node
 * @param result_type Fractal verification result
 * @param brain Affected brain (can be NULL)
 * @return Recovery result
 */
nimcp_security_recovery_result_t nimcp_srb_report_fractal_violation(
    nimcp_security_recovery_bridge_t* bridge,
    nimcp_fsc_node_t* node,
    nimcp_fsc_result_t result_type,
    brain_t brain
);

//=============================================================================
// Recovery Control
//=============================================================================

/**
 * @brief Trigger recovery for brain
 *
 * Manually trigger recovery for a specific brain.
 *
 * @param bridge Bridge context
 * @param brain Brain to recover
 * @param action Recovery action to perform
 * @return Recovery result
 */
nimcp_security_recovery_result_t nimcp_srb_trigger_recovery(
    nimcp_security_recovery_bridge_t* bridge,
    brain_t brain,
    nimcp_security_recovery_action_t action
);

/**
 * @brief Create checkpoint for brain
 *
 * @param bridge Bridge context
 * @param brain Brain to checkpoint
 * @param name Checkpoint name
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_srb_create_checkpoint(
    nimcp_security_recovery_bridge_t* bridge,
    brain_t brain,
    const char* name
);

/**
 * @brief Restore brain from checkpoint
 *
 * @param bridge Bridge context
 * @param brain Brain to restore
 * @param name Checkpoint name (NULL for latest)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_srb_restore_checkpoint(
    nimcp_security_recovery_bridge_t* bridge,
    brain_t brain,
    const char* name
);

/**
 * @brief Set bridge operational mode
 *
 * @param bridge Bridge context
 * @param mode New operational mode
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_srb_set_mode(
    nimcp_security_recovery_bridge_t* bridge,
    nimcp_srb_mode_t mode
);

//=============================================================================
// Callbacks
//=============================================================================

/**
 * @brief Register violation callback
 *
 * @param bridge Bridge context
 * @param callback Callback function
 * @param user_data User context
 * @return Callback ID or -1 on failure
 */
int32_t nimcp_srb_register_callback(
    nimcp_security_recovery_bridge_t* bridge,
    nimcp_srb_callback_t callback,
    void* user_data
);

/**
 * @brief Unregister callback
 *
 * @param bridge Bridge context
 * @param callback_id Callback ID
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_srb_unregister_callback(
    nimcp_security_recovery_bridge_t* bridge,
    int32_t callback_id
);

//=============================================================================
// Verification
//=============================================================================

/**
 * @brief Verify all registered brain regions
 *
 * @param bridge Bridge context
 * @return Number of violations found
 */
uint32_t nimcp_srb_verify_all(nimcp_security_recovery_bridge_t* bridge);

/**
 * @brief Verify specific brain
 *
 * @param bridge Bridge context
 * @param brain Brain to verify
 * @return Number of violations found
 */
uint32_t nimcp_srb_verify_brain(
    nimcp_security_recovery_bridge_t* bridge,
    brain_t brain
);

/**
 * @brief Run continuous verification cycle
 *
 * @param bridge Bridge context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_srb_run_verification_cycle(
    nimcp_security_recovery_bridge_t* bridge
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge context
 * @param stats Output: statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_srb_get_stats(
    nimcp_security_recovery_bridge_t* bridge,
    nimcp_srb_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_srb_reset_stats(
    nimcp_security_recovery_bridge_t* bridge
);

/**
 * @brief Get violation history
 *
 * @param bridge Bridge context
 * @param violations Output array
 * @param max_count Maximum entries
 * @param actual_count Output: actual entries returned
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_srb_get_violation_history(
    nimcp_security_recovery_bridge_t* bridge,
    nimcp_security_violation_t* violations,
    uint32_t max_count,
    uint32_t* actual_count
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get violation type name
 *
 * @param type Violation type
 * @return Type name string
 */
const char* nimcp_sv_type_name(nimcp_security_violation_type_t type);

/**
 * @brief Get severity name
 *
 * @param severity Severity level
 * @return Severity name string
 */
const char* nimcp_sv_severity_name(nimcp_sv_severity_t severity);

/**
 * @brief Get recovery action name
 *
 * @param action Recovery action
 * @return Action name string
 */
const char* nimcp_sra_name(nimcp_security_recovery_action_t action);

/**
 * @brief Get mode name
 *
 * @param mode Bridge mode
 * @return Mode name string
 */
const char* nimcp_srb_mode_name(nimcp_srb_mode_t mode);

/**
 * @brief Determine recovery action for violation type
 *
 * @param type Violation type
 * @param severity Severity level
 * @return Recommended recovery action
 */
nimcp_security_recovery_action_t nimcp_srb_determine_action(
    nimcp_security_violation_type_t type,
    nimcp_sv_severity_t severity
);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_SECURITY_RECOVERY_BRIDGE_H
