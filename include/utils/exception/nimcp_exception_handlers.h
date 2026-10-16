/**
 * @file nimcp_exception_handlers.h
 * @brief Exception handler registration and try/catch mechanism
 * @version 1.0.0
 * @date 2025-01-16
 *
 * WHAT: Handler registration and try/catch macros for exception handling
 * WHY:  Provide structured exception handling with priority-based handler chain
 * HOW:  Handler registry with category filtering; optional setjmp/longjmp for
 *       non-local returns
 *
 * HANDLER CHAIN:
 * ```
 * Exception Raised
 *       |
 *       v
 * +------------------+
 * | Priority 100     |  <-- High priority (security, critical)
 * | handlers         |
 * +------------------+
 *       |
 *       v (if not handled)
 * +------------------+
 * | Priority 50      |  <-- Normal handlers
 * | handlers         |
 * +------------------+
 *       |
 *       v (if not handled)
 * +------------------+
 * | Priority 10      |  <-- Low priority (logging, metrics)
 * | handlers         |
 * +------------------+
 *       |
 *       v (if not handled)
 * Default handler (log + present to immune)
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_EXCEPTION_HANDLERS_H
#define NIMCP_EXCEPTION_HANDLERS_H

#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>

#include "utils/exception/nimcp_exception.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define NIMCP_HANDLER_MAX_REGISTERED    64    /**< Max registered handlers */
#define NIMCP_TRY_STACK_DEPTH           16    /**< Max nested try blocks */
#define NIMCP_HANDLER_PRIORITY_HIGH     100   /**< High priority */
#define NIMCP_HANDLER_PRIORITY_NORMAL   50    /**< Normal priority */
#define NIMCP_HANDLER_PRIORITY_LOW      10    /**< Low priority */

/* ============================================================================
 * Handler Callback Types
 * ============================================================================ */

/**
 * @brief Exception handler callback
 *
 * Handlers are called in priority order (highest first).
 * Return true to mark exception as handled; false to continue chain.
 *
 * @param ex Exception being handled
 * @param user_data User-provided context
 * @return true if exception was handled, false to continue chain
 */
typedef bool (*nimcp_exception_handler_fn)(
    nimcp_exception_t* ex,
    void* user_data
);

/**
 * @brief Recovery callback type
 *
 * Called when immune system triggers recovery action.
 *
 * @param ex Exception that triggered recovery
 * @param action Recovery action to perform
 * @param user_data User-provided context
 * @return 0 on success, -1 on failure
 */
typedef int (*nimcp_recovery_callback_fn)(
    nimcp_exception_t* ex,
    nimcp_exception_recovery_action_t action,
    void* user_data
);

/* ============================================================================
 * Handler Registration
 * ============================================================================ */

/**
 * @brief Handler registration options
 */
typedef struct {
    const char* name;                  /**< Handler name (for debugging) */
    nimcp_exception_handler_fn handler; /**< Handler callback */
    void* user_data;                   /**< User data for callback */
    int priority;                      /**< Priority (higher = called first) */

    /* Filtering */
    nimcp_exception_category_t category_filter; /**< 0 = all categories */
    nimcp_exception_severity_t min_severity;    /**< Minimum severity to handle */
    nimcp_exception_type_t type_filter;         /**< 0 = all types */
} nimcp_handler_options_t;

/**
 * @brief Handler registration handle
 */
typedef struct nimcp_handler_registration {
    uint32_t id;                       /**< Registration ID */
    nimcp_handler_options_t options;   /**< Handler options */
    bool active;                       /**< Handler is active */
} nimcp_handler_registration_t;

/**
 * @brief Register an exception handler
 *
 * @param options Handler options
 * @return Registration handle or NULL on failure
 */
nimcp_handler_registration_t* nimcp_handler_register(
    const nimcp_handler_options_t* options
);

/**
 * @brief Unregister a handler
 *
 * @param registration Handler registration
 * @return 0 on success
 */
int nimcp_handler_unregister(nimcp_handler_registration_t* registration);

/**
 * @brief Temporarily disable a handler
 *
 * @param registration Handler registration
 */
void nimcp_handler_disable(nimcp_handler_registration_t* registration);

/**
 * @brief Re-enable a disabled handler
 *
 * @param registration Handler registration
 */
void nimcp_handler_enable(nimcp_handler_registration_t* registration);

/**
 * @brief Get default handler options
 *
 * @param options Output options struct
 */
void nimcp_handler_default_options(nimcp_handler_options_t* options);

/* ============================================================================
 * Exception Dispatch
 * ============================================================================ */

/**
 * @brief Dispatch exception through handler chain
 *
 * Calls handlers in priority order until one returns true (handled).
 * If no handler handles it, calls default handler.
 *
 * @param ex Exception to dispatch
 * @return true if exception was handled
 */
bool nimcp_exception_dispatch(nimcp_exception_t* ex);

/**
 * @brief Raise an exception (dispatch + optional longjmp)
 *
 * If inside a NIMCP_TRY block, performs longjmp.
 * Otherwise, dispatches to handler chain.
 *
 * @param ex Exception to raise
 */
void nimcp_exception_raise(nimcp_exception_t* ex);

/**
 * @brief Throw exception by error code (convenience)
 *
 * Creates exception and raises it.
 *
 * @param code Error code
 * @param file Source file
 * @param line Source line
 * @param func Function name
 * @param format Message format
 * @param ... Format arguments
 */
void nimcp_exception_throw(
    nimcp_error_t code,
    const char* file,
    int line,
    const char* func,
    const char* format,
    ...
);

/* ============================================================================
 * Try/Catch Mechanism (setjmp/longjmp based)
 * ============================================================================ */

/**
 * @brief Try/catch context for setjmp/longjmp
 */
typedef struct {
    jmp_buf jmp_buffer;                       /**< setjmp buffer */
    nimcp_exception_t* volatile exception;    /**< Caught exception (volatile: modified between setjmp/longjmp) */
    volatile bool exception_caught;           /**< Exception was caught (volatile: modified between setjmp/longjmp) */
    const char* file;                         /**< Try block file */
    int line;                                 /**< Try block line */
    const char* function;                     /**< Try block function */
} nimcp_try_context_t;

/**
 * @brief Push try context onto thread-local stack
 *
 * @param ctx Try context to push
 * @return 0 on success, -1 if stack full
 */
int nimcp_try_push(nimcp_try_context_t* ctx);

/**
 * @brief Pop try context from thread-local stack
 *
 * @return Popped context or NULL if stack empty
 */
nimcp_try_context_t* nimcp_try_pop(void);

/**
 * @brief Get current try context (top of stack)
 *
 * @return Current context or NULL if no try block active
 */
nimcp_try_context_t* nimcp_try_current(void);

/**
 * @brief Check if inside a try block
 *
 * @return true if inside try block
 */
bool nimcp_in_try_block(void);

/* ============================================================================
 * Try/Catch Macros
 *
 * Usage:
 * ```c
 * NIMCP_TRY {
 *     // code that may throw
 *     risky_operation();
 * }
 * NIMCP_CATCH(nimcp_exception_t, ex) {
 *     // handle exception
 *     fprintf(stderr, "Caught: %s\n", ex->message);
 *     nimcp_exception_unref(ex);
 * }
 * NIMCP_END_TRY;
 * ```
 *
 * For typed catch:
 * ```c
 * NIMCP_TRY {
 *     memory_operation();
 * }
 * NIMCP_CATCH_TYPE(nimcp_memory_exception_t, EXCEPTION_TYPE_MEMORY, mex) {
 *     fprintf(stderr, "Memory error: requested %zu bytes\n", mex->requested_size);
 *     nimcp_exception_unref((nimcp_exception_t*)mex);
 * }
 * NIMCP_END_TRY;
 * ```
 * ============================================================================ */

/**
 * @brief Begin try block
 */
/* P2-9: Track whether push succeeded so we only pop if push was successful.
 * This prevents leaking the try context on push failure. */
#define NIMCP_TRY \
    do { \
        nimcp_try_context_t _nimcp_try_ctx = {0}; \
        int _nimcp_push_ok = 0; \
        _nimcp_try_ctx.file = __FILE__; \
        _nimcp_try_ctx.line = __LINE__; \
        _nimcp_try_ctx.function = __func__; \
        if ((_nimcp_push_ok = (nimcp_try_push(&_nimcp_try_ctx) == 0))) { \
            if (setjmp(_nimcp_try_ctx.jmp_buffer) == 0) {

/**
 * @brief Catch block for base exception type
 */
#define NIMCP_CATCH(type, var) \
            } \
            if (_nimcp_push_ok) nimcp_try_pop(); \
        } \
        if (_nimcp_try_ctx.exception_caught) { \
            type* var = (type*)_nimcp_try_ctx.exception;

/**
 * @brief Catch block with type filtering
 */
/* P2-8: Renamed 'type' macro parameter to 'cast_type' to avoid collision
 * with the 'type' field of the exception struct in the expansion. */
#define NIMCP_CATCH_TYPE(cast_type, expected_type, var) \
            } \
            if (_nimcp_push_ok) nimcp_try_pop(); \
        } \
        if (_nimcp_try_ctx.exception_caught && \
            _nimcp_try_ctx.exception->type == (expected_type)) { \
            cast_type* var = (cast_type*)_nimcp_try_ctx.exception;

/**
 * @brief End try block
 */
#define NIMCP_END_TRY \
        } \
    } while (0)

/**
 * @brief Finally block (always executes)
 */
#define NIMCP_FINALLY \
        } \
    } \
    {

/**
 * @brief Rethrow current exception
 */
#define NIMCP_RETHROW() \
    do { \
        nimcp_try_context_t* _ctx = nimcp_try_current(); \
        if (_ctx && _ctx->exception) { \
            nimcp_exception_raise(_ctx->exception); \
        } \
    } while (0)

/* ============================================================================
 * Recovery Registration
 * ============================================================================ */

/**
 * @brief Register recovery callback for specific action
 *
 * @param action Recovery action type
 * @param callback Recovery callback
 * @param user_data User context
 * @return 0 on success
 */
int nimcp_register_recovery_callback(
    nimcp_exception_recovery_action_t action,
    nimcp_recovery_callback_fn callback,
    void* user_data
);

/**
 * @brief Unregister recovery callback
 *
 * @param action Recovery action type
 * @return 0 on success
 */
int nimcp_unregister_recovery_callback(nimcp_exception_recovery_action_t action);

/**
 * @brief Execute recovery action
 *
 * Calls registered callback for the specified action.
 *
 * @param ex Exception that triggered recovery
 * @param action Recovery action to perform
 * @return 0 on success, -1 on failure
 */
int nimcp_execute_recovery(
    nimcp_exception_t* ex,
    nimcp_exception_recovery_action_t action
);

/* ============================================================================
 * Default Handlers
 * ============================================================================ */

/**
 * @brief Default logging handler
 *
 * Logs all exceptions via nimcp_exception_log().
 *
 * @param ex Exception
 * @param user_data Not used
 * @return false (does not consume exception)
 */
bool nimcp_default_logging_handler(nimcp_exception_t* ex, void* user_data);

/**
 * @brief Default immune presentation handler
 *
 * Presents exceptions with severity >= SEVERE to immune system.
 *
 * @param ex Exception
 * @param user_data Not used
 * @return false (does not consume exception)
 */
bool nimcp_default_immune_handler(nimcp_exception_t* ex, void* user_data);

/**
 * @brief Install default handlers
 *
 * Installs logging and immune handlers with appropriate priorities.
 *
 * @return 0 on success
 */
int nimcp_install_default_handlers(void);

/* ============================================================================
 * Handler Chain Query
 * ============================================================================ */

/**
 * @brief Get number of registered handlers
 *
 * @return Number of handlers
 */
size_t nimcp_handler_count(void);

/**
 * @brief Get handler by index
 *
 * @param index Handler index
 * @return Handler registration or NULL
 */
const nimcp_handler_registration_t* nimcp_handler_get(size_t index);

/**
 * @brief Shutdown exception handlers system
 *
 * Frees registered handlers and handler mutex.
 * Should be called during library shutdown before memory cleanup.
 */
void nimcp_exception_handlers_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EXCEPTION_HANDLERS_H */
