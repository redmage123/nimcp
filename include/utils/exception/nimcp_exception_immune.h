/**
 * @file nimcp_exception_immune.h
 * @brief Exception-to-immune system integration
 * @version 1.0.0
 * @date 2025-01-16
 *
 * WHAT: Bridge between exception handling and brain immune system
 * WHY:  Enable automatic error resolution through immune-style responses
 * HOW:  Map exceptions to antigens, trigger immune response, execute
 *       recovery antibodies
 *
 * INTEGRATION FLOW:
 * ```
 * Exception                    Brain Immune System
 * ─────────────────────────────────────────────────────────────
 * Exception raised       ->    Present as antigen
 * Error code/category    ->    Antigen source type
 * Exception epitope      ->    Immune epitope (64 bytes)
 * Severity level         ->    Antigen severity (1-10)
 *
 * Antigen processed      <-    B cell activation
 * Antibody produced      <-    Recovery action selected
 * Recovery executed      ->    Notify immune of result
 * Success/failure        <-    Memory cell formation
 * ```
 *
 * MAPPING TABLES:
 *
 * Exception Category -> Antigen Source:
 * - MEMORY     -> ANTIGEN_SOURCE_ANOMALY (memory anomaly)
 * - BRAIN      -> ANTIGEN_SOURCE_BBB (brain security)
 * - THREADING  -> ANTIGEN_SOURCE_BFT (byzantine detection)
 * - SECURITY   -> ANTIGEN_SOURCE_BBB (security threat)
 * - I/O        -> ANTIGEN_SOURCE_ANOMALY (system anomaly)
 *
 * Exception Type -> Recovery Strategy:
 * - MEMORY     -> GC + Compact (primary), Quarantine (fallback)
 * - BRAIN      -> Clear NaN + Reduce LR (primary), Rollback (fallback)
 * - THREADING  -> Release locks + Restart thread
 * - SIGNAL     -> Rollback checkpoint + Emergency save
 * - I/O        -> Retry + Switch to backup
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_EXCEPTION_IMMUNE_H
#define NIMCP_EXCEPTION_IMMUNE_H

#include <stdbool.h>
#include <stdint.h>

#include "utils/exception/nimcp_exception.h"

/* Forward declarations to avoid circular includes */
struct brain_immune_system;
typedef struct brain_immune_system brain_immune_system_t;

/* Forward declarations for recovery context types */
/* These are defined with #ifndef guards to avoid conflicts with actual headers */
#ifndef NIMCP_BRAIN_H
typedef struct brain_struct* brain_t;
#endif

#ifndef NIMCP_KG_GC_H
typedef struct kg_gc_context kg_gc_context_t;
#endif

#ifndef NIMCP_BLOOD_BRAIN_BARRIER_H
typedef struct bbb_system_struct* bbb_system_t;
#endif

#ifndef NIMCP_RUNTIME_ADAPTATION_H
typedef struct runtime_adaptation_context_internal* runtime_adaptation_context_t;
#endif

/* Forward declaration for exception-immune integration bridge
 * Used by health_agent for exception handling integration */
typedef struct exception_immune_integration exception_immune_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Minimum severity to auto-present to immune system */
#define NIMCP_EXCEPTION_IMMUNE_MIN_SEVERITY  EXCEPTION_SEVERITY_SEVERE

/** Maximum pending exceptions in async queue */
#define NIMCP_EXCEPTION_IMMUNE_QUEUE_SIZE    256

/* ============================================================================
 * Antigen Source Mapping
 * ============================================================================ */

/**
 * @brief Antigen source type (mirrors brain_antigen_source_t)
 *
 * We redeclare here to avoid pulling in the full brain_immune header,
 * which would create circular dependencies.
 */
typedef enum {
    EX_ANTIGEN_SOURCE_BBB = 0,         /**< Security/brain threat */
    EX_ANTIGEN_SOURCE_BFT = 1,         /**< Byzantine/threading fault */
    EX_ANTIGEN_SOURCE_ANOMALY = 2,     /**< General anomaly */
    EX_ANTIGEN_SOURCE_SWARM = 3,       /**< Swarm-detected */
    EX_ANTIGEN_SOURCE_MANUAL = 4       /**< Manual report */
} exception_antigen_source_t;

/* ============================================================================
 * Recovery Strategy
 * ============================================================================ */

/**
 * @brief Recovery strategy for an exception type
 *
 * NOTE: Named nimcp_exception_recovery_strategy_t to avoid collision with
 * nimcp_recovery_strategy_t enum in fault_tolerance/nimcp_recovery_cache.h
 */
typedef struct {
    nimcp_exception_recovery_action_t primary_action;    /**< Primary recovery action */
    nimcp_exception_recovery_action_t fallback_action;   /**< Fallback if primary fails */
    uint32_t retry_count;                      /**< Max retries for primary */
    uint32_t cooldown_ms;                      /**< Cooldown between retries */
    float severity_threshold;                  /**< Min severity for activation */
} nimcp_exception_recovery_strategy_t;

/* ============================================================================
 * Immune Response Result
 * ============================================================================ */

/**
 * @brief Result of immune response to exception
 */
typedef struct {
    uint32_t antigen_id;               /**< Assigned antigen ID */
    uint32_t antibody_id;              /**< Produced antibody ID (if any) */
    nimcp_exception_recovery_action_t action_taken; /**< Recovery action executed */
    bool recovery_attempted;           /**< Recovery was attempted */
    bool recovery_succeeded;           /**< Recovery succeeded */
    uint64_t response_time_us;         /**< Time to respond (microseconds) */
    bool memory_formed;                /**< Memory cell was formed */
} nimcp_immune_response_t;

/* ============================================================================
 * Immune Integration Configuration
 * ============================================================================ */

/**
 * @brief Exception-immune integration configuration
 */
typedef struct {
    bool enable_auto_present;          /**< Auto-present above threshold */
    nimcp_exception_severity_t min_present_severity; /**< Min severity for auto-present */
    bool enable_auto_recovery;         /**< Auto-execute recovery actions */
    bool enable_memory_formation;      /**< Form immune memory for patterns */
    bool async_presentation;           /**< Present async (non-blocking) */
    uint32_t max_pending_exceptions;   /**< Max queued exceptions */
    uint32_t response_timeout_ms;      /**< Timeout for immune response */
} nimcp_exception_immune_config_t;

/* ============================================================================
 * Immune Integration API
 * ============================================================================ */

/**
 * @brief Initialize exception-immune integration
 *
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_exception_immune_init(const nimcp_exception_immune_config_t* config);

/**
 * @brief Shutdown exception-immune integration
 */
void nimcp_exception_immune_shutdown(void);

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 */
void nimcp_exception_immune_default_config(nimcp_exception_immune_config_t* config);

/**
 * @brief Connect to brain immune system
 *
 * Must be called before presenting exceptions.
 *
 * @param immune_system Brain immune system instance
 * @return 0 on success, -1 on error
 */
int nimcp_exception_immune_connect(brain_immune_system_t* immune_system);

/**
 * @brief Disconnect from brain immune system
 *
 * @return 0 on success
 */
int nimcp_exception_immune_disconnect(void);

/**
 * @brief Check if connected to immune system
 *
 * @return true if connected
 */
bool nimcp_exception_immune_is_connected(void);

/* ============================================================================
 * Exception Presentation
 * ============================================================================ */

/**
 * @brief Present exception to immune system as antigen
 *
 * WHAT: Convert exception to immune antigen and present
 * WHY:  Enable immune-mediated recovery
 * HOW:  Generate epitope, map to antigen source, present to brain immune
 *
 * @param ex Exception to present
 * @param response Output response (may be NULL)
 * @return 0 on success, -1 on error
 */
int nimcp_exception_present_to_immune(
    nimcp_exception_t* ex,
    nimcp_immune_response_t* response
);

/**
 * @brief Present exception asynchronously
 *
 * Queues exception for async presentation. Non-blocking.
 *
 * @param ex Exception to present
 * @return 0 on success, -1 on error (queue full)
 */
int nimcp_exception_present_async(nimcp_exception_t* ex);

/**
 * @brief Process pending async presentations
 *
 * Call periodically to process queued exceptions.
 *
 * @param max_count Maximum number to process (0 = all)
 * @return Number of exceptions processed
 */
size_t nimcp_exception_immune_process_pending(size_t max_count);

/* ============================================================================
 * Mapping Functions
 * ============================================================================ */

/**
 * @brief Map exception category to antigen source
 *
 * @param category Exception category
 * @return Antigen source type
 */
exception_antigen_source_t nimcp_exception_to_antigen_source(
    nimcp_exception_category_t category
);

/**
 * @brief Map exception severity to immune severity (1-10)
 *
 * @param severity Exception severity
 * @return Immune severity (1-10)
 */
uint32_t nimcp_exception_to_immune_severity(
    nimcp_exception_severity_t severity
);

/**
 * @brief Get recovery strategy for exception
 *
 * @param ex Exception
 * @param strategy Output strategy
 */
void nimcp_exception_get_recovery_strategy(
    const nimcp_exception_t* ex,
    nimcp_exception_recovery_strategy_t* strategy
);

/* ============================================================================
 * Epitope Generation
 * ============================================================================ */

/**
 * @brief Generate epitope (fingerprint) for exception
 *
 * Creates a 64-byte fingerprint based on:
 * - Error code
 * - Category
 * - File/function hash
 * - Stack trace hash
 *
 * Used for immune pattern matching and memory formation.
 *
 * @param ex Exception
 * @param epitope Output epitope buffer (64 bytes)
 * @param epitope_size Buffer size (should be 64)
 * @return Length of generated epitope
 */
size_t nimcp_exception_compute_epitope(
    const nimcp_exception_t* ex,
    uint8_t* epitope,
    size_t epitope_size
);

/* ============================================================================
 * Recovery Execution
 * ============================================================================ */

/**
 * @brief Execute recovery action for exception
 *
 * Runs the appropriate recovery callback based on action type.
 *
 * @param ex Exception that triggered recovery
 * @param action Recovery action to execute
 * @return 0 on success, -1 on failure
 */
int nimcp_exception_execute_recovery(
    nimcp_exception_t* ex,
    nimcp_exception_recovery_action_t action
);

/**
 * @brief Notify immune system of recovery result
 *
 * Called after recovery attempt to inform immune system of outcome.
 *
 * @param ex Exception that was recovered
 * @param action Action that was taken
 * @param success Whether recovery succeeded
 * @return 0 on success
 */
int nimcp_exception_notify_recovery_result(
    nimcp_exception_t* ex,
    nimcp_exception_recovery_action_t action,
    bool success
);

/* ============================================================================
 * Recovery Callbacks for Common Actions
 * ============================================================================ */

/**
 * @brief Default GC recovery callback
 *
 * Triggers garbage collection and memory compaction.
 */
int nimcp_recovery_gc(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data);

/**
 * @brief Default retry recovery callback
 *
 * Retries the failed operation (requires context in exception).
 */
int nimcp_recovery_retry(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data);

/**
 * @brief Default rollback recovery callback
 *
 * Rolls back to last checkpoint.
 */
int nimcp_recovery_rollback(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data);

/**
 * @brief Default thread restart recovery callback
 *
 * Restarts the affected thread.
 */
int nimcp_recovery_restart_thread(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data);

/**
 * @brief Default quarantine recovery callback
 *
 * Quarantines affected memory region or component.
 */
int nimcp_recovery_quarantine(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data);

/**
 * @brief Default emergency save recovery callback
 *
 * Performs emergency state save.
 */
int nimcp_recovery_emergency_save(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data);

/**
 * @brief Default load reduction recovery callback
 *
 * Reduces system load by adjusting batch size, disabling features.
 */
int nimcp_recovery_reduce_load(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data);

/**
 * @brief Default cache clear recovery callback
 *
 * Clears all caches to free memory.
 */
int nimcp_recovery_clear_cache(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data);

/**
 * @brief Default memory compaction recovery callback
 *
 * Performs memory compaction to reduce fragmentation.
 */
int nimcp_recovery_compact(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data);

/**
 * @brief Default component restart recovery callback
 *
 * Restarts the affected component/subsystem.
 */
int nimcp_recovery_restart_component(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data);

/**
 * @brief Default graceful shutdown recovery callback
 *
 * Initiates graceful system shutdown when unrecoverable.
 */
int nimcp_recovery_graceful_shutdown(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data);

/**
 * @brief Install default recovery callbacks
 *
 * @return 0 on success
 */
int nimcp_exception_install_default_recovery_callbacks(void);

/* ============================================================================
 * Recovery Context Configuration
 * ============================================================================ */

/**
 * @brief Configure recovery context with system references
 *
 * WHAT: Set up recovery callbacks with access to brain subsystems
 * WHY:  Recovery actions need actual system references to operate
 * HOW:  Store references for use by recovery callbacks
 *
 * Call this function after initializing the brain and its subsystems
 * to enable full recovery functionality.
 *
 * @param brain Brain instance for recovery operations
 * @param gc_context GC context for garbage collection (NULL if unavailable)
 * @param bbb_system BBB system for quarantine actions (NULL if unavailable)
 * @param ra_ctx Runtime adaptation context for load reduction (NULL if unavailable)
 * @param checkpoint_dir Directory for checkpoint files (NULL if unavailable)
 * @return 0 on success
 */
int nimcp_recovery_set_context(
    brain_t brain,
    kg_gc_context_t* gc_context,
    bbb_system_t bbb_system,
    runtime_adaptation_context_t ra_ctx,
    const char* checkpoint_dir
);

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Exception-immune integration statistics
 */
typedef struct {
    uint64_t exceptions_presented;     /**< Total presented to immune */
    uint64_t exceptions_pending;       /**< Currently pending */
    uint64_t recoveries_attempted;     /**< Recovery attempts */
    uint64_t recoveries_succeeded;     /**< Successful recoveries */
    uint64_t memories_formed;          /**< Immune memories formed */
    float avg_response_time_us;        /**< Average response time */
    uint64_t queue_overflows;          /**< Queue overflow count */
} nimcp_exception_immune_stats_t;

/**
 * @brief Get exception-immune statistics
 *
 * @param stats Output statistics
 */
void nimcp_exception_immune_get_stats(nimcp_exception_immune_stats_t* stats);

/**
 * @brief Reset statistics
 */
void nimcp_exception_immune_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EXCEPTION_IMMUNE_H */
