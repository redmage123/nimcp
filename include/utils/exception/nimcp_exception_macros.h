/**
 * @file nimcp_exception_macros.h
 * @brief Convenience macros for exception handling with KG wiring integration
 * @version 1.0.0
 * @date 2025-01-16
 *
 * WHAT: Macros for easy exception creation, throwing, and immune presentation
 * WHY:  Minimize boilerplate while ensuring consistent exception handling
 * HOW:  Wrap creation/dispatch into single-line macros with auto file/line
 *
 * KG WIRING INTEGRATION:
 * ```
 * Exception Module Wiring
 * ─────────────────────────────────────────────────────────────
 * Input:   ERROR_REPORT (from any module)
 * Input:   CRASH_SIGNAL (from signal handlers)
 * Output:  EXCEPTION_RAISED (to loggers, metrics)
 * Output:  ANTIGEN_PRESENTED (to brain immune)
 * Output:  RECOVERY_REQUEST (to fault tolerance)
 * Handler: ERROR_REPORT (priority 200)
 * Handler: CRASH_SIGNAL (priority 300)
 * ```
 *
 * USAGE PATTERNS:
 * ```c
 * // Simple throw (creates and raises exception)
 * if (!ptr) {
 *     NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "ptr is NULL");
 *     return NIMCP_ERROR_NULL_POINTER;
 * }
 *
 * // Throw with immune presentation (severity >= SEVERE)
 * if (memory_low) {
 *     NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Memory allocation failed");
 *     return NIMCP_ERROR_NO_MEMORY;
 * }
 *
 * // Conditional throw
 * NIMCP_THROW_IF(!config, NIMCP_ERROR_INVALID_PARAM, "config is NULL");
 *
 * // Check and return pattern
 * NIMCP_CHECK_THROW(ptr != NULL, NIMCP_ERROR_NULL_POINTER, "NULL ptr");
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_EXCEPTION_MACROS_H
#define NIMCP_EXCEPTION_MACROS_H

#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/signal/nimcp_signal_handler.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * KG Wiring Integration Constants
 * ============================================================================ */

/** KG message type for exception raised events */
#define KG_MSG_EXCEPTION_RAISED         "EXCEPTION_RAISED"

/** KG message type for antigen presentation */
#define KG_MSG_ANTIGEN_PRESENTED        "ANTIGEN_PRESENTED"

/** KG message type for recovery request */
#define KG_MSG_RECOVERY_REQUEST         "RECOVERY_REQUEST"

/** KG message type for recovery result */
#define KG_MSG_RECOVERY_RESULT          "RECOVERY_RESULT"

/** KG message type for exception error reports */
#define KG_MSG_ERROR_REPORT             "ERROR_REPORT"

/** KG message type for crash signals */
#define KG_MSG_CRASH_SIGNAL             "CRASH_SIGNAL"

/** Exception module name for KG wiring */
#define KG_EXCEPTION_MODULE_NAME        "exception_handler"

/** Exception module type for KG wiring */
#define KG_EXCEPTION_MODULE_TYPE        "FAULT_TOLERANCE"

/* ============================================================================
 * Basic Throw Macros
 * ============================================================================ */

/**
 * @brief Throw an exception with message
 *
 * Creates exception and dispatches through handler chain.
 * Does NOT present to immune system (use NIMCP_THROW_TO_IMMUNE for that).
 */
#define NIMCP_THROW(code, fmt, ...) \
    nimcp_exception_throw( \
        (code), \
        __FILE__, \
        __LINE__, \
        __func__, \
        fmt, \
        ##__VA_ARGS__ \
    )

/**
 * @brief Throw exception if condition is true
 *
 * Usage: NIMCP_THROW_IF(!ptr, NIMCP_ERROR_NULL_POINTER, "ptr is NULL");
 *        This throws if ptr is NULL.
 */
#define NIMCP_THROW_IF(cond, code, fmt, ...) \
    do { \
        if (cond) { \
            NIMCP_THROW((code), fmt, ##__VA_ARGS__); \
        } \
    } while (0)

/**
 * @brief Check condition, throw and return if false
 */
#define NIMCP_CHECK_THROW(cond, code, fmt, ...) \
    do { \
        if (!(cond)) { \
            NIMCP_THROW((code), fmt, ##__VA_ARGS__); \
            return (code); \
        } \
    } while (0)

/**
 * @brief Alias for NIMCP_CHECK_THROW with explicit MSG suffix
 *
 * Functionally identical to NIMCP_CHECK_THROW - provided for code clarity
 * when the format message is the primary focus of the check.
 */
#define NIMCP_CHECK_THROW_MSG(cond, code, fmt, ...) \
    NIMCP_CHECK_THROW((cond), (code), fmt, ##__VA_ARGS__)

/* ============================================================================
 * Immune-Integrated Throw Macros
 * ============================================================================ */

/**
 * @brief Throw exception and present to immune system
 *
 * Creates exception, presents to immune system, and dispatches.
 * Use this for recoverable errors that the immune system should learn from.
 */
#define NIMCP_THROW_TO_IMMUNE(code, fmt, ...) \
    do { \
        nimcp_exception_t* _ex = nimcp_exception_create( \
            (code), \
            nimcp_exception_get_severity_from_code(code), \
            __FILE__, \
            __LINE__, \
            __func__, \
            fmt, \
            ##__VA_ARGS__ \
        ); \
        if (_ex) { \
            nimcp_exception_present_to_immune(_ex, NULL); \
            nimcp_exception_dispatch(_ex); \
            nimcp_exception_unref(_ex); \
        } \
    } while (0)

/**
 * @brief Throw to immune if condition is false
 */
#define NIMCP_THROW_TO_IMMUNE_IF(cond, code, fmt, ...) \
    do { \
        if (!(cond)) { \
            NIMCP_THROW_TO_IMMUNE((code), fmt, ##__VA_ARGS__); \
        } \
    } while (0)

/**
 * @brief Check, throw to immune, and return if false
 */
#define NIMCP_CHECK_THROW_IMMUNE(cond, code, fmt, ...) \
    do { \
        if (!(cond)) { \
            NIMCP_THROW_TO_IMMUNE((code), fmt, ##__VA_ARGS__); \
            return (code); \
        } \
    } while (0)

/* ============================================================================
 * FEP Bridge Macros (return -1 for errors, 0 for success)
 * ============================================================================ */

/**
 * @brief Check condition, throw, and return -1 if false.
 *
 * FEP bridge functions return int (0/-1), not NIMCP_ERROR_* codes.
 * Use this macro in FEP bridge files instead of NIMCP_CHECK_THROW.
 */
#define NIMCP_FEP_CHECK_THROW(cond, code, fmt, ...) \
    do { \
        if (!(cond)) { \
            NIMCP_THROW((code), fmt, ##__VA_ARGS__); \
            return -1; \
        } \
    } while (0)

/**
 * @brief Check condition, throw to immune, and return -1 if false.
 *
 * FEP bridge variant of NIMCP_CHECK_THROW_IMMUNE.
 */
#define NIMCP_FEP_CHECK_THROW_IMMUNE(cond, code, fmt, ...) \
    do { \
        if (!(cond)) { \
            NIMCP_THROW_TO_IMMUNE((code), fmt, ##__VA_ARGS__); \
            return -1; \
        } \
    } while (0)

/* ============================================================================
 * Full Immune Recovery Macros
 * ============================================================================ */

/**
 * @brief Throw exception with full immune processing and auto-recovery
 *
 * Creates exception, presents to immune system, captures response,
 * executes recommended recovery action, and notifies result.
 * Use this for full fault-tolerance integration.
 *
 * Flow:
 * 1. Create exception with file/line/function metadata
 * 2. Present to immune system and capture response
 * 3. If recovery was recommended, execute the action
 * 4. Notify immune system of recovery result
 * 5. Dispatch through handler chain
 */
#define NIMCP_THROW_IMMUNE_RECOVER(code, fmt, ...) \
    do { \
        nimcp_exception_t* _ex = nimcp_exception_create( \
            (code), \
            nimcp_exception_get_severity_from_code(code), \
            __FILE__, \
            __LINE__, \
            __func__, \
            fmt, \
            ##__VA_ARGS__ \
        ); \
        if (_ex) { \
            nimcp_immune_response_t _response = {0}; \
            nimcp_exception_present_to_immune(_ex, &_response); \
            if (_response.recovery_attempted && _response.action_taken != EXCEPTION_RECOVERY_NONE) { \
                int _rc = nimcp_exception_execute_recovery(_ex, _response.action_taken); \
                nimcp_exception_notify_recovery_result(_ex, _response.action_taken, (_rc == 0)); \
            } \
            nimcp_exception_dispatch(_ex); \
            nimcp_exception_unref(_ex); \
        } \
    } while (0)

/**
 * @brief Throw to immune with recovery if condition is false
 */
#define NIMCP_THROW_IMMUNE_RECOVER_IF(cond, code, fmt, ...) \
    do { \
        if (!(cond)) { \
            NIMCP_THROW_IMMUNE_RECOVER((code), fmt, ##__VA_ARGS__); \
        } \
    } while (0)

/**
 * @brief Check, throw to immune with recovery, and return if false
 */
#define NIMCP_CHECK_THROW_IMMUNE_RECOVER(cond, code, fmt, ...) \
    do { \
        if (!(cond)) { \
            NIMCP_THROW_IMMUNE_RECOVER((code), fmt, ##__VA_ARGS__); \
            return (code); \
        } \
    } while (0)

/* ============================================================================
 * Async/Non-Blocking Throw Macros
 * ============================================================================ */

/**
 * @brief Throw exception with async immune presentation
 *
 * Creates exception, queues for async immune presentation, dispatches.
 * Non-blocking - useful in performance-critical code paths.
 */
#define NIMCP_THROW_ASYNC(code, fmt, ...) \
    do { \
        nimcp_exception_t* _ex = nimcp_exception_create( \
            (code), \
            nimcp_exception_get_severity_from_code(code), \
            __FILE__, \
            __LINE__, \
            __func__, \
            fmt, \
            ##__VA_ARGS__ \
        ); \
        if (_ex) { \
            nimcp_exception_present_async(_ex); \
            nimcp_exception_dispatch(_ex); \
            nimcp_exception_unref(_ex); \
        } \
    } while (0)

/* ============================================================================
 * Typed Exception Macros
 * ============================================================================ */

/**
 * @brief Throw a memory exception
 */
#define NIMCP_THROW_MEMORY(code, requested_size, fmt, ...) \
    do { \
        nimcp_memory_exception_t* _mex = nimcp_memory_exception_create( \
            (code), \
            nimcp_exception_get_severity_from_code(code), \
            __FILE__, \
            __LINE__, \
            __func__, \
            (requested_size), \
            fmt, \
            ##__VA_ARGS__ \
        ); \
        if (_mex) { \
            nimcp_exception_present_to_immune((nimcp_exception_t*)_mex, NULL); \
            nimcp_exception_dispatch((nimcp_exception_t*)_mex); \
            nimcp_exception_unref((nimcp_exception_t*)_mex); \
        } \
    } while (0)

/**
 * @brief Throw a brain/neural exception
 */
#define NIMCP_THROW_BRAIN(code, brain_id, region_name, fmt, ...) \
    do { \
        nimcp_brain_exception_t* _bex = nimcp_brain_exception_create( \
            (code), \
            nimcp_exception_get_severity_from_code(code), \
            __FILE__, \
            __LINE__, \
            __func__, \
            (brain_id), \
            (region_name), \
            fmt, \
            ##__VA_ARGS__ \
        ); \
        if (_bex) { \
            nimcp_exception_present_to_immune((nimcp_exception_t*)_bex, NULL); \
            nimcp_exception_dispatch((nimcp_exception_t*)_bex); \
            nimcp_exception_unref((nimcp_exception_t*)_bex); \
        } \
    } while (0)

/**
 * @brief Throw an I/O exception
 */
#define NIMCP_THROW_IO(code, path, fmt, ...) \
    do { \
        nimcp_io_exception_t* _iex = nimcp_io_exception_create( \
            (code), \
            nimcp_exception_get_severity_from_code(code), \
            __FILE__, \
            __LINE__, \
            __func__, \
            (path), \
            fmt, \
            ##__VA_ARGS__ \
        ); \
        if (_iex) { \
            nimcp_exception_present_to_immune((nimcp_exception_t*)_iex, NULL); \
            nimcp_exception_dispatch((nimcp_exception_t*)_iex); \
            nimcp_exception_unref((nimcp_exception_t*)_iex); \
        } \
    } while (0)

/**
 * @brief Throw a threading exception
 */
#define NIMCP_THROW_THREADING(code, thread_id, fmt, ...) \
    do { \
        nimcp_threading_exception_t* _tex = nimcp_threading_exception_create( \
            (code), \
            nimcp_exception_get_severity_from_code(code), \
            __FILE__, \
            __LINE__, \
            __func__, \
            (thread_id), \
            fmt, \
            ##__VA_ARGS__ \
        ); \
        if (_tex) { \
            nimcp_exception_present_to_immune((nimcp_exception_t*)_tex, NULL); \
            nimcp_exception_dispatch((nimcp_exception_t*)_tex); \
            nimcp_exception_unref((nimcp_exception_t*)_tex); \
        } \
    } while (0)

/**
 * @brief Throw a security exception
 */
#define NIMCP_THROW_SECURITY(code, threat_type, fmt, ...) \
    do { \
        nimcp_security_exception_t* _sex = nimcp_security_exception_create( \
            (code), \
            EXCEPTION_SEVERITY_CRITICAL, \
            __FILE__, \
            __LINE__, \
            __func__, \
            (threat_type), \
            fmt, \
            ##__VA_ARGS__ \
        ); \
        if (_sex) { \
            nimcp_exception_present_to_immune((nimcp_exception_t*)_sex, NULL); \
            nimcp_exception_dispatch((nimcp_exception_t*)_sex); \
            nimcp_exception_unref((nimcp_exception_t*)_sex); \
        } \
    } while (0)

/**
 * @brief Throw a GPU exception
 */
#define NIMCP_THROW_GPU(code, device_id, cuda_err, fmt, ...) \
    do { \
        nimcp_gpu_exception_t* _gex = nimcp_gpu_exception_create( \
            (code), \
            nimcp_exception_get_severity_from_code(code), \
            __FILE__, \
            __LINE__, \
            __func__, \
            (device_id), \
            (cuda_err), \
            fmt, \
            ##__VA_ARGS__ \
        ); \
        if (_gex) { \
            nimcp_exception_present_to_immune((nimcp_exception_t*)_gex, NULL); \
            nimcp_exception_dispatch((nimcp_exception_t*)_gex); \
            nimcp_exception_unref((nimcp_exception_t*)_gex); \
        } \
    } while (0)

/* ============================================================================
 * Recovery Macros
 * ============================================================================ */

/**
 * @brief Execute recovery action for current exception
 */
#define NIMCP_RECOVER(action) \
    do { \
        nimcp_exception_t* _curr = nimcp_exception_get_current(); \
        if (_curr) { \
            nimcp_execute_recovery(_curr, (action)); \
        } \
    } while (0)

/**
 * @brief Throw and attempt recovery
 */
#define NIMCP_THROW_AND_RECOVER(code, action, fmt, ...) \
    do { \
        nimcp_exception_t* _ex = nimcp_exception_create( \
            (code), \
            nimcp_exception_get_severity_from_code(code), \
            __FILE__, \
            __LINE__, \
            __func__, \
            fmt, \
            ##__VA_ARGS__ \
        ); \
        if (_ex) { \
            nimcp_exception_present_to_immune(_ex, NULL); \
            nimcp_execute_recovery(_ex, (action)); \
            nimcp_exception_dispatch(_ex); \
            nimcp_exception_unref(_ex); \
        } \
    } while (0)

/* ============================================================================
 * KG Wiring Integration Macros
 * ============================================================================ */

/**
 * @brief Create exception module wiring descriptor
 *
 * Call this during exception system initialization to register
 * the exception handler module with the KG wiring system.
 *
 * Usage:
 * ```c
 * kg_module_wiring_t* wiring = NIMCP_EXCEPTION_CREATE_WIRING();
 * // ... additional customization ...
 * brain_register_module_wiring(wiring);
 * ```
 */
#define NIMCP_EXCEPTION_CREATE_WIRING() \
    nimcp_exception_create_kg_wiring()

/*
 * KG Wiring creation function - only available if KG module wiring is included.
 * Include "core/brain/nimcp_kg_module_wiring.h" before this header to enable.
 */
#ifdef NIMCP_KG_MODULE_WIRING_H
/**
 * @brief Create KG wiring descriptor for exception module
 *
 * Creates a complete wiring descriptor declaring:
 * - Inputs: ERROR_REPORT, CRASH_SIGNAL
 * - Outputs: EXCEPTION_RAISED, ANTIGEN_PRESENTED, RECOVERY_REQUEST, RECOVERY_RESULT
 * - Handlers: ERROR_REPORT, CRASH_SIGNAL
 *
 * @return New wiring descriptor or NULL on error
 */
kg_module_wiring_t* nimcp_exception_create_kg_wiring(void);
#endif /* NIMCP_KG_MODULE_WIRING_H */

/* ============================================================================
 * Signal Exception Macros
 * ============================================================================ */

/**
 * @brief Process and throw signal exception after recovery
 *
 * Call this after siglongjmp recovery to convert the pending crash
 * context into an exception and process it through the exception hierarchy.
 *
 * USAGE (inside SIGNAL_ON_CRASH block):
 * ```c
 * SIGNAL_TRY_RECOVER(3, "risky_op") {
 *     dangerous_operation();
 * } SIGNAL_ON_CRASH {
 *     NIMCP_THROW_SIGNAL_RECOVERED();
 *     return NIMCP_ERROR_CRASH_RECOVERY;
 * } SIGNAL_TRY_END;
 * ```
 *
 * This macro:
 * 1. Gets the pending crash context from the signal handler
 * 2. Creates a nimcp_signal_exception_t from the context
 * 3. Marks siglongjmp_executed = true
 * 4. Presents to immune system
 * 5. Dispatches through handler chain
 * 6. Clears the pending crash
 */
#define NIMCP_THROW_SIGNAL_RECOVERED() \
    do { \
        signal_crash_context_t _sig_ctx; \
        if (signal_handler_get_pending_crash(&_sig_ctx)) { \
            nimcp_signal_exception_t* _sex = \
                nimcp_signal_exception_create_from_context(&_sig_ctx); \
            if (_sex) { \
                _sex->siglongjmp_executed = true; \
                nimcp_exception_present_to_immune((nimcp_exception_t*)_sex, NULL); \
                nimcp_exception_dispatch((nimcp_exception_t*)_sex); \
                nimcp_exception_unref((nimcp_exception_t*)_sex); \
            } \
            signal_handler_clear_pending_crash(); \
        } \
    } while (0)

/**
 * @brief Process all pending signal exceptions from queue
 *
 * Call this periodically in the main thread to process any crashes
 * that were queued by signal handlers.
 *
 * USAGE:
 * ```c
 * // In main loop or periodic callback:
 * NIMCP_PROCESS_SIGNAL_EXCEPTIONS();
 * ```
 *
 * @param max_count Maximum to process (0 = all pending)
 */
#define NIMCP_PROCESS_SIGNAL_EXCEPTIONS() \
    signal_handler_process_pending_exceptions(0)

#define NIMCP_PROCESS_SIGNAL_EXCEPTIONS_N(max_count) \
    signal_handler_process_pending_exceptions(max_count)

/**
 * @brief Throw signal exception for a specific signal
 *
 * Use when you want to manually create a signal exception
 * (e.g., in a test or when converting from another error type).
 *
 * @param sig Signal number (SIGSEGV, SIGFPE, etc.)
 * @param fault_addr Fault address (may be NULL)
 * @param fmt Message format string
 */
#define NIMCP_THROW_SIGNAL(sig, fault_addr, fmt, ...) \
    do { \
        nimcp_signal_exception_t* _sex = nimcp_signal_exception_create( \
            (sig), \
            (fault_addr), \
            __FILE__, \
            __LINE__, \
            __func__, \
            fmt, \
            ##__VA_ARGS__ \
        ); \
        if (_sex) { \
            nimcp_exception_present_to_immune((nimcp_exception_t*)_sex, NULL); \
            nimcp_exception_dispatch((nimcp_exception_t*)_sex); \
            nimcp_exception_unref((nimcp_exception_t*)_sex); \
        } \
    } while (0)

/* ============================================================================
 * Legacy Compatibility Macros
 * ============================================================================ */

/**
 * @brief Legacy error setting that also throws exception
 *
 * Combines NIMCP_SET_ERROR with exception creation.
 * Use for gradual migration from error-only to exception handling.
 */
#define NIMCP_SET_ERROR_EX(code, fmt, ...) \
    do { \
        nimcp_set_error_ex((code), __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); \
        NIMCP_THROW((code), fmt, ##__VA_ARGS__); \
    } while (0)

/* ============================================================================
 * Severity Override Macros
 * ============================================================================ */

/**
 * @brief Throw with explicit severity
 */
#define NIMCP_THROW_SEVERITY(code, severity, fmt, ...) \
    do { \
        nimcp_exception_t* _ex = nimcp_exception_create( \
            (code), \
            (severity), \
            __FILE__, \
            __LINE__, \
            __func__, \
            fmt, \
            ##__VA_ARGS__ \
        ); \
        if (_ex) { \
            if ((severity) >= EXCEPTION_SEVERITY_SEVERE) { \
                nimcp_exception_present_to_immune(_ex, NULL); \
            } \
            nimcp_exception_dispatch(_ex); \
            nimcp_exception_unref(_ex); \
        } \
    } while (0)

/**
 * @brief Throw critical exception (always presents to immune)
 */
#define NIMCP_THROW_CRITICAL(code, fmt, ...) \
    NIMCP_THROW_SEVERITY((code), EXCEPTION_SEVERITY_CRITICAL, fmt, ##__VA_ARGS__)

/**
 * @brief Throw fatal exception (triggers emergency response)
 */
#define NIMCP_THROW_FATAL(code, fmt, ...) \
    NIMCP_THROW_SEVERITY((code), EXCEPTION_SEVERITY_FATAL, fmt, ##__VA_ARGS__)

/* ============================================================================
 * Debug Macros (compile-time disabled in release)
 * ============================================================================ */

#ifdef NDEBUG
#define NIMCP_ASSERT_THROW(cond, code, fmt, ...) ((void)0)
#else
/**
 * @brief Assert with exception throw on failure
 *
 * Only active in debug builds.
 */
#define NIMCP_ASSERT_THROW(cond, code, fmt, ...) \
    do { \
        if (!(cond)) { \
            NIMCP_THROW_CRITICAL((code), fmt, ##__VA_ARGS__); \
        } \
    } while (0)
#endif

/* ============================================================================
 * Compatibility Macros for module-prefixed error handling
 * ============================================================================ */

/**
 * @brief Compatibility macro for 3-argument form (module_id, code, message)
 *
 * Some modules (VAE, etc.) use a different calling convention that includes
 * a module ID. This macro provides compatibility.
 *
 * Usage: NIMCP_THROW_TO_IMMUNE_MODULE(0x1234, NIMCP_ERROR_INVALID_PARAM, "message");
 */
#define NIMCP_THROW_TO_IMMUNE_MODULE(module_id, code, msg) \
    NIMCP_THROW_TO_IMMUNE((code), "[0x%04X] %s", (unsigned int)(module_id), (msg))

/* Argument count helper for overloading NIMCP_THROW_TO_IMMUNE */
#define _NIMCP_THROW_NARG(...) _NIMCP_THROW_NARG_(__VA_ARGS__, _NIMCP_THROW_RSEQ())
#define _NIMCP_THROW_NARG_(...) _NIMCP_THROW_ARG_N(__VA_ARGS__)
#define _NIMCP_THROW_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, N, ...) N
#define _NIMCP_THROW_RSEQ() 8, 7, 6, 5, 4, 3, 3, 2

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EXCEPTION_MACROS_H */
