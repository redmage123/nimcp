/**
 * @file nimcp_signal_handler.h
 * @brief Signal handling and crash recovery system
 *
 * WHAT: Graceful handling of fatal signals to prevent brain instance crashes
 * WHY:  Production systems need recovery from SIGSEGV, SIGABRT, etc.
 * HOW:  Install signal handlers, log state, attempt recovery or graceful shutdown
 *
 * SIGNALS HANDLED:
 * - SIGSEGV: Segmentation fault (NULL pointer, invalid memory access)
 * - SIGABRT: Abort signal (assertion failure, explicit abort)
 * - SIGBUS: Bus error (misaligned memory access)
 * - SIGFPE: Floating point exception (division by zero, overflow)
 * - SIGILL: Illegal instruction
 * - SIGTERM: Termination request (graceful shutdown)
 * - SIGINT: Interrupt (Ctrl+C)
 * - SIGHUP: Hang up (reload config)
 *
 * RECOVERY STRATEGY:
 * 1. Fatal signals (SIGSEGV, SIGBUS): Log state, save checkpoint, attempt cleanup
 * 2. Numeric errors (SIGFPE): Return NaN/Inf, continue processing
 * 3. Termination (SIGTERM, SIGINT): Graceful shutdown with state save
 * 4. Config reload (SIGHUP): Reload hyperparameters from config file
 *
 * @author NIMCP Team
 * @date 2025-11-09
 */

#ifndef NIMCP_SIGNAL_HANDLER_H
#define NIMCP_SIGNAL_HANDLER_H

#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct brain_struct* brain_t;

/* Forward declaration for code immune system (always active).
 * Must match the typedef in cognitive/immune/nimcp_code_immune.h. */
#ifndef NIMCP_CODE_IMMUNE_SYSTEM_T_DEFINED
#define NIMCP_CODE_IMMUNE_SYSTEM_T_DEFINED
typedef struct code_immune_system code_immune_system_t;
#endif

//=============================================================================
// Signal Handler Configuration
//=============================================================================

/**
 * @brief Signal handler behavior modes
 */
typedef enum {
    SIGNAL_MODE_IGNORE,      /**< Ignore signal (not recommended for fatal signals) */
    SIGNAL_MODE_LOG_ONLY,    /**< Log signal but don't handle */
    SIGNAL_MODE_LOG_CONTINUE,/**< Log signal and attempt to continue */
    SIGNAL_MODE_LOG_SHUTDOWN,/**< Log signal and graceful shutdown */
    SIGNAL_MODE_LOG_ABORT    /**< Log signal and abort immediately */
} signal_handler_mode_t;

/**
 * @brief Configuration for signal handler
 */
typedef struct {
    signal_handler_mode_t sigsegv_mode;  /**< SIGSEGV handling mode */
    signal_handler_mode_t sigabrt_mode;  /**< SIGABRT handling mode */
    signal_handler_mode_t sigbus_mode;   /**< SIGBUS handling mode */
    signal_handler_mode_t sigfpe_mode;   /**< SIGFPE handling mode */
    signal_handler_mode_t sigill_mode;   /**< SIGILL handling mode */
    signal_handler_mode_t sigterm_mode;  /**< SIGTERM handling mode */
    signal_handler_mode_t sigint_mode;   /**< SIGINT handling mode */
    signal_handler_mode_t sighup_mode;   /**< SIGHUP handling mode */

    bool enable_stack_trace;             /**< Log stack trace on crash */
    bool enable_state_dump;              /**< Dump brain state on crash */
    bool enable_checkpoint_save;         /**< Save checkpoint on crash */
    const char* crash_log_path;          /**< Path to crash log file */
    const char* checkpoint_path;         /**< Path to checkpoint directory */

    // Recovery callbacks
    void (*on_fatal_signal)(int sig);    /**< Called on fatal signal */
    void (*on_reload_config)(void);      /**< Called on SIGHUP */
    void (*on_graceful_shutdown)(void);  /**< Called on SIGTERM/SIGINT */
} signal_handler_config_t;

/**
 * @brief Signal statistics
 */
typedef struct {
    uint64_t sigsegv_count;     /**< Number of SIGSEGV signals received */
    uint64_t sigabrt_count;     /**< Number of SIGABRT signals received */
    uint64_t sigbus_count;      /**< Number of SIGBUS signals received */
    uint64_t sigfpe_count;      /**< Number of SIGFPE signals received */
    uint64_t sigill_count;      /**< Number of SIGILL signals received */
    uint64_t sigterm_count;     /**< Number of SIGTERM signals received */
    uint64_t sigint_count;      /**< Number of SIGINT signals received */
    uint64_t sighup_count;      /**< Number of SIGHUP signals received */
    uint64_t recoveries;        /**< Number of successful recoveries */
    uint64_t fatal_crashes;     /**< Number of unrecoverable crashes */
} signal_handler_stats_t;

//=============================================================================
// Public API
//=============================================================================

/**
 * @brief Get default signal handler configuration
 *
 * WHAT: Return safe defaults for signal handling
 * WHY:  Simplify initialization
 * HOW:  Fatal signals → LOG_SHUTDOWN, SIGHUP → reload config
 *
 * @return Default configuration
 */
signal_handler_config_t signal_handler_default_config(void);

/**
 * @brief Install signal handlers
 *
 * WHAT: Register signal handlers for all configured signals
 * WHY:  Enable graceful error handling
 * HOW:  Use sigaction() to install handlers
 *
 * @param config Signal handler configuration (NULL for defaults)
 * @return true on success, false on failure
 */
bool signal_handler_install(const signal_handler_config_t* config);

/**
 * @brief Uninstall signal handlers
 *
 * WHAT: Restore default signal handlers
 * WHY:  Clean shutdown or testing
 * HOW:  Restore SIG_DFL for all signals
 *
 * @return true on success, false on failure
 */
bool signal_handler_uninstall(void);

/**
 * @brief Register brain instance for crash recovery
 *
 * WHAT: Associate brain instance with signal handler
 * WHY:  Enable state dump and checkpoint save on crash
 * HOW:  Store brain pointer in signal-safe global
 *
 * @param brain Brain instance to register (NULL to unregister)
 */
void signal_handler_register_brain(brain_t brain);

/**
 * @brief Unregister brain instance (test cleanup)
 *
 * WHAT: Clear registered brain instance from signal handler
 * WHY:  Prevent test contamination from global state
 * HOW:  Set g_registered_brain to NULL
 *
 * NOTE: Should be called in test TearDown() to ensure clean state
 */
void signal_handler_unregister_brain(void);

/**
 * @brief Get signal handler statistics
 *
 * WHAT: Return signal counts and recovery stats
 * WHY:  Monitor system health and crash frequency
 * HOW:  Return copy of internal counters
 *
 * @return Signal statistics
 */
signal_handler_stats_t signal_handler_get_stats(void);

/**
 * @brief Reset signal handler statistics
 *
 * WHAT: Zero out all signal counters
 * WHY:  Fresh start for monitoring
 * HOW:  Memset stats to zero
 */
void signal_handler_reset_stats(void);

/**
 * @brief Check if graceful shutdown was requested
 *
 * WHAT: Check if SIGTERM/SIGINT was received
 * WHY:  Application loop can check and shutdown cleanly
 * HOW:  Return flag set by signal handler
 *
 * @return true if shutdown requested, false otherwise
 */
bool signal_handler_shutdown_requested(void);

/**
 * @brief Check if config reload was requested
 *
 * WHAT: Check if SIGHUP was received
 * WHY:  Application can reload config on SIGHUP
 * HOW:  Return and clear flag set by signal handler
 *
 * @return true if reload requested, false otherwise
 */
bool signal_handler_reload_requested(void);

/**
 * @brief Set custom crash callback
 *
 * WHAT: Register callback to be called before crash handling
 * WHY:  Application-specific cleanup or logging
 * HOW:  Store function pointer, call from signal handler
 *
 * @param callback Function to call on fatal signal (NULL to unset)
 */
void signal_handler_set_crash_callback(void (*callback)(int sig));

/**
 * @brief Set custom config reload callback
 *
 * WHAT: Register callback to be called on SIGHUP
 * WHY:  Application needs to reload config file
 * HOW:  Store function pointer, call from SIGHUP handler
 *
 * @param callback Function to call on SIGHUP (NULL to unset)
 */
void signal_handler_set_reload_callback(void (*callback)(void));

/**
 * @brief Force a checkpoint save
 *
 * WHAT: Manually trigger checkpoint save of registered brain
 * WHY:  Periodic checkpointing or before risky operations
 * HOW:  Call brain_save_checkpoint() if brain is registered
 *
 * @return true on success, false on failure
 */
bool signal_handler_force_checkpoint(void);

/**
 * @brief Get last signal received
 *
 * WHAT: Return signal number of last signal
 * WHY:  Debugging and diagnostics
 * HOW:  Return cached signal number
 *
 * @return Signal number or 0 if none
 */
int signal_handler_get_last_signal(void);

/**
 * @brief Get signal name as string
 *
 * WHAT: Convert signal number to name string
 * WHY:  Human-readable logging
 * HOW:  Map signal numbers to names
 *
 * @param sig Signal number
 * @return Signal name string (e.g., "SIGSEGV")
 */
const char* signal_handler_get_signal_name(int sig);

//=============================================================================
// Enhanced Recovery & Diagnostics API
//=============================================================================

/**
 * @brief Health status of the system
 */
typedef enum {
    SIGNAL_HEALTH_HEALTHY,      /**< All systems operational */
    SIGNAL_HEALTH_DEGRADED,     /**< Some issues detected, recovery attempted */
    SIGNAL_HEALTH_COMPROMISED,  /**< Multiple issues, stability at risk */
    SIGNAL_HEALTH_CRITICAL,     /**< Critical issues, immediate attention needed */
    SIGNAL_HEALTH_UNKNOWN        /**< Status cannot be determined */
} signal_health_status_t;

/**
 * @brief Detailed health information
 */
typedef struct {
    signal_health_status_t status;           /**< Overall health status */
    uint64_t total_signals;                  /**< Total signals received */
    uint64_t fatal_crashes;                  /**< Unrecoverable crashes */
    uint64_t successful_recoveries;          /**< Recovery attempts that succeeded */
    uint64_t failed_recoveries;              /**< Recovery attempts that failed */
    float recovery_success_rate;             /**< % of recoveries that succeeded */
    int last_signal;                         /**< Last signal received */
    const char* last_signal_name;            /**< Name of last signal */
    uint64_t checkpoint_saves;               /**< Checkpoints successfully saved */
    bool is_in_recovery;                     /**< Currently attempting recovery */
} signal_health_info_t;

/**
 * @brief Get current health status
 *
 * WHAT: Return comprehensive health information
 * WHY:  Monitor system stability and recovery effectiveness
 * HOW:  Analyze signal stats, recovery history, and system state
 *
 * @return Health information structure
 */
signal_health_info_t signal_handler_get_health_status(void);

/**
 * @brief Attempt to manually trigger checkpoint save
 *
 * WHAT: Synchronously save registered brain state
 * WHY:  Guarantee consistent checkpoint before risky operations
 * HOW:  Call brain_save() if safe, handle errors gracefully
 *
 * @param checkpoint_path Path to save checkpoint to (NULL for config default)
 * @return true on success, false on failure or no brain registered
 */
bool signal_handler_checkpoint_save(const char* checkpoint_path);

/**
 * @brief Set the number of checkpoints to keep
 *
 * WHAT: Configure checkpoint retention policy
 * WHY:  Balance disk usage vs. recovery history
 * HOW:  Manage rotating checkpoint files
 *
 * @param max_checkpoints Maximum number to retain (0 = unlimited)
 */
void signal_handler_set_checkpoint_retention(int max_checkpoints);

/**
 * @brief Get checkpoint statistics
 *
 * WHAT: Return information about saved checkpoints
 * WHY:  Monitor checkpoint health and disk usage
 * HOW:  Scan checkpoint directory and return stats
 *
 * @return Count of saved checkpoints (-1 on error)
 */
int signal_handler_get_checkpoint_count(void);

/**
 * @brief Enable or disable automatic recovery attempts
 *
 * WHAT: Control whether signal handler tries to recover
 * WHY:  Allow explicit control over recovery behavior
 * HOW:  Set internal flag checked during signal handling
 *
 * @param enable true to enable automatic recovery, false to disable
 */
void signal_handler_set_auto_recovery(bool enable);

/**
 * @brief Get current recovery configuration status
 *
 * WHAT: Return whether automatic recovery is enabled
 * WHY:  Query current recovery settings
 * HOW:  Return flag state
 *
 * @return true if auto-recovery enabled, false otherwise
 */
bool signal_handler_is_auto_recovery_enabled(void);

/**
 * @brief Set maximum recovery attempts before giving up
 *
 * WHAT: Configure how many times we attempt recovery for same issue
 * WHY:  Prevent infinite recovery loops
 * HOW:  Store limit and track attempts per signal
 *
 * @param max_attempts Maximum recovery attempts (0 = unlimited)
 */
void signal_handler_set_max_recovery_attempts(int max_attempts);

//=============================================================================
// Code Immune System Integration
//=============================================================================

/**
 * @brief Backtrace depth for crash context
 */
#define SIGNAL_HANDLER_BACKTRACE_DEPTH 32

/**
 * @brief Crash context captured by signal handler
 *
 * Contains full register state and memory region information
 * captured when a crash signal is received.
 */
typedef struct signal_crash_context {
    int signal;                               /**< Signal number */
    void* fault_address;                      /**< Address that caused the fault */
    void* instruction_pointer;                /**< IP/PC at crash time */
    void* stack_pointer;                      /**< SP at crash time */
    void* base_pointer;                       /**< BP/FP at crash time */
    void* backtrace[SIGNAL_HANDLER_BACKTRACE_DEPTH]; /**< Stack backtrace */
    int backtrace_depth;                      /**< Number of backtrace frames */
    char memory_region[256];                  /**< Memory region from /proc/self/maps */
} signal_crash_context_t;

/**
 * @brief Register code immune system for crash handling
 *
 * WHAT: Associate code immune system with signal handler
 * WHY:  Enable immune-based crash recovery and hot-patching
 * HOW:  Store pointer for use in signal handler
 *       Always active — signal handler MUST report crashes to immune system
 *
 * @param sys Code immune system instance (NULL to unregister)
 */
void signal_handler_set_code_immune(code_immune_system_t* sys);

/**
 * @brief Get registered code immune system
 *
 * @return Registered code immune system or NULL
 */
code_immune_system_t* signal_handler_get_code_immune(void);

/**
 * @brief Set recovery point for siglongjmp-based recovery
 *
 * WHAT: Save execution context for crash recovery
 * WHY:  Allow resumption after code immune fixes a crash
 * HOW:  Use sigsetjmp to save state; crashes jump back here
 *
 * USAGE:
 *   if (signal_handler_set_recovery_point() != 0) {
 *       // Crash occurred and code immune handled it
 *       // Clean up and retry or exit gracefully
 *   }
 *   // Execute potentially crash-prone code here
 *   signal_handler_clear_recovery_point();
 *
 * @return 0 on initial call, signal number on recovery jump
 */
int signal_handler_set_recovery_point(void);

/**
 * @brief Clear recovery point
 *
 * WHAT: Disable siglongjmp recovery
 * WHY:  Prevent unexpected jumps after exiting crash-prone code
 * HOW:  Clear internal flag
 */
void signal_handler_clear_recovery_point(void);

/**
 * @brief Check if a crash is pending deferred processing
 *
 * WHAT: Check for captured crash waiting for main thread processing
 * WHY:  Allow signal handler to be minimal; defer complex processing
 * HOW:  Check pending crash flag
 *
 * @return true if crash is pending
 */
bool signal_handler_has_pending_crash(void);

/**
 * @brief Get pending crash context
 *
 * WHAT: Retrieve crash details captured by signal handler
 * WHY:  Allow main thread to analyze or respond to crash
 * HOW:  Copy pending context to caller's buffer
 *
 * @param ctx Output buffer for crash context
 * @return true if crash was pending and context copied
 */
bool signal_handler_get_pending_crash(signal_crash_context_t* ctx);

/**
 * @brief Clear pending crash flag
 *
 * WHAT: Acknowledge that pending crash has been processed
 * WHY:  Prevent duplicate processing
 * HOW:  Reset pending flag
 */
void signal_handler_clear_pending_crash(void);

/**
 * @brief Check if code immune integration is available
 *
 * WHAT: Query whether code immune system is registered
 * WHY:  Allow conditional immune-dependent logic
 * HOW:  Check compile flag and registration status
 *
 * @return true if code immune is available
 */
bool signal_handler_has_code_immune(void);

/**
 * @brief Get immune system recovery statistics
 *
 * WHAT: Query immune-based recovery metrics
 * WHY:  Monitor immune system effectiveness
 * HOW:  Return counts from internal state
 *
 * @param immune_recoveries Output for count of immune-handled recoveries
 * @param total_crashes Output for total crash count
 */
void signal_handler_get_immune_stats(uint64_t* immune_recoveries, uint64_t* total_crashes);

//=============================================================================
// Thread-Local Recovery API
//=============================================================================

/**
 * @brief Recovery point result codes
 *
 * Returned by signal_handler_set_recovery_point_ex() after recovery jump.
 */
typedef enum {
    RECOVERY_INITIAL = 0,           /**< Initial sigsetjmp call - no crash yet */
    RECOVERY_CRASH_HANDLED = 1,     /**< Crash occurred, code immune handled it */
    RECOVERY_CRASH_UNHANDLED = 2,   /**< Crash occurred, no fix available */
    RECOVERY_RETRY_REQUESTED = 3,   /**< Fix applied, retry operation */
    RECOVERY_ABORT_REQUESTED = 4    /**< Abort current operation */
} signal_recovery_result_t;

/**
 * @brief Recovery context for thread-local recovery
 *
 * Contains recovery state for the current thread including
 * sigjmp_buf and recovery metadata.
 */
typedef struct signal_recovery_ctx {
    sigjmp_buf jmp_buf;             /**< Jump buffer for recovery */
    volatile sig_atomic_t valid;    /**< Is recovery point valid? */
    volatile sig_atomic_t result;   /**< Recovery result code */
    volatile int crash_signal;      /**< Signal that caused crash */
    volatile int retry_count;       /**< Number of retries attempted */
    int max_retries;                /**< Maximum retries allowed */
    void* user_data;                /**< User-provided context */
    const char* label;              /**< Optional label for debugging */
} signal_recovery_ctx_t;

/**
 * @brief Initialize thread-local recovery context
 *
 * WHAT: Initialize recovery context for current thread
 * WHY:  Each thread needs its own recovery state
 * HOW:  Allocate/init thread-local storage
 *
 * @return 0 on success, -1 on error
 */
int signal_handler_init_thread_recovery(void);

/**
 * @brief Cleanup thread-local recovery context
 *
 * WHAT: Free recovery context for current thread
 * WHY:  Clean up before thread exit
 * HOW:  Free thread-local storage
 */
void signal_handler_cleanup_thread_recovery(void);

/**
 * @brief Get thread-local recovery context
 *
 * WHAT: Access recovery context for current thread
 * WHY:  Query/modify recovery state
 * HOW:  Return pointer to thread-local context
 *
 * @return Recovery context or NULL if not initialized
 */
signal_recovery_ctx_t* signal_handler_get_recovery_ctx(void);

/**
 * @brief Set recovery point with extended options (thread-local)
 *
 * WHAT: Save execution context for crash recovery on current thread
 * WHY:  Allow per-thread recovery from crashes
 * HOW:  Use sigsetjmp with thread-local context
 *
 * USAGE:
 *   signal_recovery_ctx_t* ctx = signal_handler_get_recovery_ctx();
 *   int result = signal_handler_set_recovery_point_ex(ctx, 3, "parse_input");
 *   if (result == RECOVERY_CRASH_HANDLED) {
 *       // Crash occurred, code immune applied fix - can retry
 *       if (ctx->retry_count < ctx->max_retries) {
 *           goto retry_operation;
 *       }
 *   } else if (result == RECOVERY_CRASH_UNHANDLED) {
 *       // Crash occurred, no fix - handle gracefully
 *       return ERROR_CODE;
 *   }
 *   // result == RECOVERY_INITIAL: Execute potentially crash-prone code
 *   signal_handler_clear_recovery_point_ex(ctx);
 *
 * @param ctx Recovery context (NULL for global context)
 * @param max_retries Maximum retry attempts (0 = no retry)
 * @param label Debug label for this recovery point
 * @return RECOVERY_INITIAL on first call, recovery result after jump
 */
int signal_handler_set_recovery_point_ex(
    signal_recovery_ctx_t* ctx,
    int max_retries,
    const char* label
);

/**
 * @brief Clear thread-local recovery point
 *
 * WHAT: Disable recovery for current thread
 * WHY:  Prevent unexpected jumps after safe code completes
 * HOW:  Clear validity flag in thread context
 *
 * @param ctx Recovery context (NULL for global context)
 */
void signal_handler_clear_recovery_point_ex(signal_recovery_ctx_t* ctx);

/**
 * @brief Trigger recovery jump from signal handler
 *
 * WHAT: Jump to recovery point after crash handling
 * WHY:  Resume execution after code immune processes crash
 * HOW:  Use siglongjmp to thread's recovery point
 *
 * NOTE: Called from signal handler context - must be signal-safe.
 *       This function does not return if jump succeeds.
 *
 * @param result Recovery result code to pass to recovery point
 * @return -1 if no valid recovery point (should not normally return)
 */
int signal_handler_trigger_recovery(signal_recovery_result_t result);

/**
 * @brief Check if current thread has valid recovery point
 *
 * @return true if recovery is possible for current thread
 */
bool signal_handler_can_recover(void);

/**
 * @brief Get crash signal from last recovery
 *
 * @return Signal number or 0 if no crash
 */
int signal_handler_get_crash_signal(void);

//=============================================================================
// Exception Queue Integration
//=============================================================================

/**
 * @brief Process pending signal exceptions
 *
 * WHAT: Dequeue crashes from exception queue and process as exceptions
 * WHY:  Bridge between signal handler and exception hierarchy
 * HOW:  Dequeue entries, create exceptions, present to immune
 *
 * Call this periodically from main thread to process any crashes
 * that were queued by signal handlers.
 *
 * @param max_count Maximum entries to process (0 = all pending)
 * @return Number of entries processed
 */
size_t signal_handler_process_pending_exceptions(size_t max_count);

/**
 * @brief Get count of pending signal exceptions
 *
 * @return Number of pending exceptions in queue
 */
size_t signal_handler_get_pending_exception_count(void);

//=============================================================================
// Recovery Macros
//=============================================================================

/**
 * @brief Macro for wrapping crash-prone code with recovery
 *
 * USAGE:
 *   SIGNAL_TRY_RECOVER(3, "memory_op") {
 *       // Potentially dangerous code here
 *       ptr->field = value;
 *   } SIGNAL_ON_CRASH {
 *       // Recovery handling - crash occurred
 *       LOG_WARN("Recovered from crash in memory_op");
 *       return ERROR_RECOVERED;
 *   } SIGNAL_TRY_END;
 *
 * NOTE: Uses compound statement scope. Variables declared inside
 *       SIGNAL_TRY_RECOVER block are not visible in SIGNAL_ON_CRASH.
 */
#define SIGNAL_TRY_RECOVER(max_retries, label) \
    do { \
        signal_recovery_ctx_t* _sig_ctx = signal_handler_get_recovery_ctx(); \
        int _sig_result = signal_handler_set_recovery_point_ex(_sig_ctx, max_retries, label); \
        if (_sig_result == RECOVERY_INITIAL) {

#define SIGNAL_ON_CRASH \
            signal_handler_clear_recovery_point_ex(_sig_ctx); \
        } else {

#define SIGNAL_TRY_END \
            signal_handler_clear_recovery_point_ex(_sig_ctx); \
        } \
    } while (0)

/**
 * @brief Simple try-recover macro for single statement
 *
 * USAGE:
 *   int result = SIGNAL_TRY_EXPR(dangerous_function(ptr), -1);
 *
 * @param expr Expression to evaluate
 * @param fallback Value to return on crash
 */
#define SIGNAL_TRY_EXPR(expr, fallback) \
    ({ \
        signal_recovery_ctx_t* _sig_ctx = signal_handler_get_recovery_ctx(); \
        typeof(expr) _sig_val; \
        int _sig_result = signal_handler_set_recovery_point_ex(_sig_ctx, 0, #expr); \
        if (_sig_result == RECOVERY_INITIAL) { \
            _sig_val = (expr); \
            signal_handler_clear_recovery_point_ex(_sig_ctx); \
        } else { \
            _sig_val = (fallback); \
        } \
        _sig_val; \
    })

//=============================================================================
// Enhanced Recovery Macros with Exception Integration
//=============================================================================

/**
 * @brief Enhanced try-recover macro with exception integration
 *
 * WHAT: Wraps crash-prone code with recovery AND exception processing
 * WHY:  Integrate signal recovery with exception hierarchy
 * HOW:  Combines SIGNAL_TRY_RECOVER with NIMCP_THROW_SIGNAL_RECOVERED
 *
 * USAGE:
 * ```c
 * SIGNAL_TRY_RECOVER_EX(3, "risky_op") {
 *     dangerous_operation();
 * } SIGNAL_ON_CRASH_EX {
 *     // Crash occurred - exception has been processed
 *     LOG_ERROR("Recovered from crash in risky_op");
 *     return NIMCP_ERROR_CRASH_RECOVERY;
 * } SIGNAL_TRY_END_EX;
 * ```
 *
 * The _EX variant:
 * 1. Sets up recovery point (like SIGNAL_TRY_RECOVER)
 * 2. On crash, creates signal exception from pending context
 * 3. Presents exception to immune system
 * 4. Dispatches through handler chain
 * 5. Clears pending crash state
 *
 * NOTE: Requires including nimcp_exception_macros.h for full functionality.
 */
#define SIGNAL_TRY_RECOVER_EX(max_retries, label) \
    do { \
        signal_recovery_ctx_t* _sig_ctx_ex = signal_handler_get_recovery_ctx(); \
        int _sig_result_ex = signal_handler_set_recovery_point_ex(_sig_ctx_ex, max_retries, label); \
        if (_sig_result_ex == RECOVERY_INITIAL) {

#define SIGNAL_ON_CRASH_EX \
            signal_handler_clear_recovery_point_ex(_sig_ctx_ex); \
        } else { \
            /* Process crash as exception - requires nimcp_exception_macros.h */ \
            signal_crash_context_t _ex_crash_ctx; \
            if (signal_handler_get_pending_crash(&_ex_crash_ctx)) { \
                /* Forward declare to avoid header dependency */ \
                extern nimcp_signal_exception_t* nimcp_signal_exception_create_from_context( \
                    const struct signal_crash_context* ctx); \
                extern int nimcp_exception_present_to_immune(nimcp_exception_t* ex, void* resp); \
                extern void nimcp_exception_dispatch(nimcp_exception_t* ex); \
                extern void nimcp_exception_unref(nimcp_exception_t* ex); \
                nimcp_signal_exception_t* _ex_sex = \
                    nimcp_signal_exception_create_from_context(&_ex_crash_ctx); \
                if (_ex_sex) { \
                    _ex_sex->siglongjmp_executed = true; \
                    nimcp_exception_present_to_immune((nimcp_exception_t*)_ex_sex, NULL); \
                    nimcp_exception_dispatch((nimcp_exception_t*)_ex_sex); \
                    nimcp_exception_unref((nimcp_exception_t*)_ex_sex); \
                } \
                signal_handler_clear_pending_crash(); \
            }

#define SIGNAL_TRY_END_EX \
            signal_handler_clear_recovery_point_ex(_sig_ctx_ex); \
        } \
    } while (0)

/**
 * @brief Expression-level recovery with exception integration
 *
 * Like SIGNAL_TRY_EXPR but also processes crash as exception.
 *
 * @param expr Expression to evaluate
 * @param fallback Value to return on crash
 */
#define SIGNAL_TRY_EXPR_EX(expr, fallback) \
    ({ \
        signal_recovery_ctx_t* _sig_ctx_ex = signal_handler_get_recovery_ctx(); \
        typeof(expr) _sig_val_ex; \
        int _sig_result_ex = signal_handler_set_recovery_point_ex(_sig_ctx_ex, 0, #expr); \
        if (_sig_result_ex == RECOVERY_INITIAL) { \
            _sig_val_ex = (expr); \
            signal_handler_clear_recovery_point_ex(_sig_ctx_ex); \
        } else { \
            /* Process crash as exception */ \
            signal_crash_context_t _ex_crash_ctx; \
            if (signal_handler_get_pending_crash(&_ex_crash_ctx)) { \
                extern nimcp_signal_exception_t* nimcp_signal_exception_create_from_context( \
                    const struct signal_crash_context* ctx); \
                extern int nimcp_exception_present_to_immune(nimcp_exception_t* ex, void* resp); \
                extern void nimcp_exception_dispatch(nimcp_exception_t* ex); \
                extern void nimcp_exception_unref(nimcp_exception_t* ex); \
                nimcp_signal_exception_t* _ex_sex = \
                    nimcp_signal_exception_create_from_context(&_ex_crash_ctx); \
                if (_ex_sex) { \
                    _ex_sex->siglongjmp_executed = true; \
                    nimcp_exception_present_to_immune((nimcp_exception_t*)_ex_sex, NULL); \
                    nimcp_exception_dispatch((nimcp_exception_t*)_ex_sex); \
                    nimcp_exception_unref((nimcp_exception_t*)_ex_sex); \
                } \
                signal_handler_clear_pending_crash(); \
            } \
            _sig_val_ex = (fallback); \
        } \
        _sig_val_ex; \
    })

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SIGNAL_HANDLER_H */
