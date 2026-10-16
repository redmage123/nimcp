/**
 * @file nimcp_kg_wiring_exception.h
 * @brief Exception handling integration for KG module wiring system
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Exception types and macros for KG module wiring errors
 * WHY:  Integrate KG wiring failures with the NIMCP exception and immune system
 * HOW:  Extends brain exception with wiring-specific context, provides throwing macros
 *
 * ARCHITECTURE:
 * ```
 * KG Wiring Error -> Create Exception -> Log -> Present to Immune -> Recovery
 *                                                     |
 *                         Recovery Action Mapping (GC, REDUCE_LOAD, ROLLBACK)
 * ```
 *
 * USAGE:
 * ```c
 * // Enable exceptions (default)
 * kg_wiring_enable_exceptions();
 *
 * // In wiring functions:
 * if (wiring->input_count >= KG_WIRING_MAX_INPUTS) {
 *     NIMCP_THROW_KG_WIRING_CAPACITY(NIMCP_ERROR_KG_WIRING_INPUTS_FULL,
 *         wiring->module_name, "add_input", KG_WIRING_MAX_INPUTS);
 *     return -1;
 * }
 *
 * // Disable exceptions for backward compatibility testing
 * kg_wiring_disable_exceptions();
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_KG_WIRING_EXCEPTION_H
#define NIMCP_KG_WIRING_EXCEPTION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define KG_WIRING_EXCEPTION_MAX_MODULE_NAME   64   /**< Max module name length */
#define KG_WIRING_EXCEPTION_MAX_OPERATION     32   /**< Max operation name length */

/* ============================================================================
 * KG Wiring Exception Type
 * ============================================================================ */

/**
 * @brief KG wiring exception extending brain exception
 *
 * Provides additional context specific to KG module wiring errors:
 * - Module name that failed
 * - Operation that was attempted
 * - Capacity limits (for overflow errors)
 * - String length limits (for truncation errors)
 */
typedef struct nimcp_kg_wiring_exception {
    nimcp_brain_exception_t base;     /**< Base brain exception (must be first) */

    /* KG Wiring-specific fields */
    char module_name[KG_WIRING_EXCEPTION_MAX_MODULE_NAME];  /**< Affected module */
    char operation[KG_WIRING_EXCEPTION_MAX_OPERATION];       /**< Failed operation */
    uint32_t current_count;           /**< Current count (for capacity errors) */
    uint32_t max_count;               /**< Maximum allowed (for capacity errors) */
    size_t string_length;             /**< String length (for truncation errors) */
    size_t max_string_length;         /**< Max allowed length */
} nimcp_kg_wiring_exception_t;

/* ============================================================================
 * Exception Enable/Disable API
 * ============================================================================ */

/**
 * @brief Enable KG wiring exception throwing
 *
 * Enabled by default. When enabled, KG wiring functions will throw
 * exceptions in addition to returning error codes.
 */
void kg_wiring_enable_exceptions(void);

/**
 * @brief Disable KG wiring exception throwing
 *
 * When disabled, KG wiring functions only return error codes and log
 * errors without throwing exceptions. Useful for testing backward
 * compatibility or performance-critical paths.
 */
void kg_wiring_disable_exceptions(void);

/**
 * @brief Check if KG wiring exceptions are enabled
 *
 * @return true if exceptions are enabled
 */
bool kg_wiring_exceptions_enabled(void);

/* ============================================================================
 * Exception Creation API
 * ============================================================================ */

/**
 * @brief Create a KG wiring exception
 *
 * @param code NIMCP error code (NIMCP_ERROR_KG_WIRING_*)
 * @param severity Exception severity
 * @param file Source file (__FILE__)
 * @param line Source line (__LINE__)
 * @param func Function name (__func__)
 * @param module_name Name of the affected module
 * @param operation Operation that failed
 * @param format Printf-style message format
 * @param ... Format arguments
 * @return New exception or NULL on allocation failure
 */
nimcp_kg_wiring_exception_t* nimcp_kg_wiring_exception_create(
    nimcp_error_t code,
    nimcp_exception_severity_t severity,
    const char* file,
    int line,
    const char* func,
    const char* module_name,
    const char* operation,
    const char* format,
    ...
);

/**
 * @brief Create a KG wiring capacity exception
 *
 * Convenience function for capacity overflow errors.
 *
 * @param code Error code (e.g., NIMCP_ERROR_KG_WIRING_INPUTS_FULL)
 * @param file Source file
 * @param line Source line
 * @param func Function name
 * @param module_name Affected module name
 * @param operation Failed operation
 * @param current_count Current count
 * @param max_count Maximum allowed
 * @return New exception or NULL
 */
nimcp_kg_wiring_exception_t* nimcp_kg_wiring_exception_create_capacity(
    nimcp_error_t code,
    const char* file,
    int line,
    const char* func,
    const char* module_name,
    const char* operation,
    uint32_t current_count,
    uint32_t max_count
);

/**
 * @brief Create a KG wiring string length exception
 *
 * Convenience function for string too long errors.
 *
 * @param code Error code (NIMCP_ERROR_KG_WIRING_STRING_TOO_LONG)
 * @param file Source file
 * @param line Source line
 * @param func Function name
 * @param module_name Affected module name
 * @param operation Failed operation
 * @param string_length Actual string length
 * @param max_length Maximum allowed length
 * @return New exception or NULL
 */
nimcp_kg_wiring_exception_t* nimcp_kg_wiring_exception_create_string(
    nimcp_error_t code,
    const char* file,
    int line,
    const char* func,
    const char* module_name,
    const char* operation,
    size_t string_length,
    size_t max_length
);

/* ============================================================================
 * Recovery Action Mapping
 * ============================================================================ */

/**
 * @brief Get recovery action for KG wiring error code
 *
 * Maps error codes to appropriate recovery actions:
 * - CREATE, WEIGHT_ALLOC: RECOVERY_ACTION_GC (memory issue)
 * - INPUTS/OUTPUTS/HANDLERS/METADATA_FULL: RECOVERY_ACTION_REDUCE_LOAD
 * - VALIDATION: RECOVERY_ACTION_ROLLBACK
 * - Others: RECOVERY_ACTION_NONE (programmer error)
 *
 * @param code KG wiring error code
 * @return Suggested recovery action
 */
nimcp_exception_recovery_action_t kg_wiring_get_recovery_action(nimcp_error_t code);

/* ============================================================================
 * Exception Handler Registration
 * ============================================================================ */

/**
 * @brief KG wiring exception handler callback type
 *
 * @param ex Exception to handle (cast to nimcp_kg_wiring_exception_t*)
 * @param user_data User-provided context
 * @return true if exception was handled, false to continue chain
 */
typedef bool (*kg_wiring_exception_handler_fn)(
    nimcp_exception_t* ex,
    void* user_data
);

/**
 * @brief Register KG wiring exception handler
 *
 * Registers a handler that will be called for all KG wiring exceptions.
 * Multiple handlers can be registered and are called in order.
 *
 * @param handler Handler callback
 * @param user_data Context passed to handler
 * @return Handler ID for unregistration, or 0 on failure
 */
uint32_t kg_wiring_register_exception_handler(
    kg_wiring_exception_handler_fn handler,
    void* user_data
);

/**
 * @brief Unregister KG wiring exception handler
 *
 * @param handler_id Handler ID from registration
 * @return 0 on success, -1 if not found
 */
int kg_wiring_unregister_exception_handler(uint32_t handler_id);

/**
 * @brief Default KG wiring exception handler
 *
 * Logs exception details and suggests recovery action.
 * This handler is registered automatically during initialization.
 *
 * @param ex Exception
 * @param user_data Unused
 * @return false (allows chain to continue)
 */
bool kg_wiring_default_exception_handler(
    nimcp_exception_t* ex,
    void* user_data
);

/* ============================================================================
 * Initialization
 * ============================================================================ */

/**
 * @brief Initialize KG wiring exception system
 *
 * Called automatically when first exception is created.
 * Can be called explicitly for early initialization.
 *
 * @return 0 on success, -1 on error
 */
int kg_wiring_exception_init(void);

/**
 * @brief Shutdown KG wiring exception system
 *
 * Unregisters all handlers and cleans up resources.
 */
void kg_wiring_exception_shutdown(void);

/* ============================================================================
 * Throwing Macros
 * ============================================================================ */

/**
 * @brief Throw a KG wiring exception
 *
 * Only throws if exceptions are enabled.
 *
 * @param code NIMCP error code
 * @param module_name Module name
 * @param operation Operation name
 * @param fmt Message format
 */
#define NIMCP_THROW_KG_WIRING(code, module_name, operation, fmt, ...) \
    do { \
        if (kg_wiring_exceptions_enabled() && nimcp_exception_system_is_initialized()) { \
            nimcp_kg_wiring_exception_t* _kex = nimcp_kg_wiring_exception_create( \
                (code), \
                nimcp_exception_get_severity_from_code(code), \
                __FILE__, \
                __LINE__, \
                __func__, \
                (module_name), \
                (operation), \
                fmt, \
                ##__VA_ARGS__ \
            ); \
            if (_kex) { \
                nimcp_exception_present_to_immune((nimcp_exception_t*)_kex, NULL); \
                nimcp_exception_dispatch((nimcp_exception_t*)_kex); \
                nimcp_exception_unref((nimcp_exception_t*)_kex); \
            } \
        } \
    } while (0)

/**
 * @brief Throw KG wiring capacity exception
 *
 * @param code Error code (NIMCP_ERROR_KG_WIRING_*_FULL)
 * @param module_name Module name
 * @param operation Operation name
 * @param max_count Maximum allowed count
 */
#define NIMCP_THROW_KG_WIRING_CAPACITY(code, module_name, operation, max_count) \
    do { \
        if (kg_wiring_exceptions_enabled() && nimcp_exception_system_is_initialized()) { \
            nimcp_kg_wiring_exception_t* _kex = nimcp_kg_wiring_exception_create_capacity( \
                (code), \
                __FILE__, \
                __LINE__, \
                __func__, \
                (module_name), \
                (operation), \
                (max_count), \
                (max_count) \
            ); \
            if (_kex) { \
                nimcp_exception_present_to_immune((nimcp_exception_t*)_kex, NULL); \
                nimcp_exception_dispatch((nimcp_exception_t*)_kex); \
                nimcp_exception_unref((nimcp_exception_t*)_kex); \
            } \
        } \
    } while (0)

/**
 * @brief Throw KG wiring string length exception
 *
 * @param code Error code (NIMCP_ERROR_KG_WIRING_STRING_TOO_LONG)
 * @param module_name Module name
 * @param operation Operation name
 * @param len Actual length
 * @param max_len Maximum allowed
 */
#define NIMCP_THROW_KG_WIRING_STRING(code, module_name, operation, len, max_len) \
    do { \
        if (kg_wiring_exceptions_enabled() && nimcp_exception_system_is_initialized()) { \
            nimcp_kg_wiring_exception_t* _kex = nimcp_kg_wiring_exception_create_string( \
                (code), \
                __FILE__, \
                __LINE__, \
                __func__, \
                (module_name), \
                (operation), \
                (len), \
                (max_len) \
            ); \
            if (_kex) { \
                nimcp_exception_present_to_immune((nimcp_exception_t*)_kex, NULL); \
                nimcp_exception_dispatch((nimcp_exception_t*)_kex); \
                nimcp_exception_unref((nimcp_exception_t*)_kex); \
            } \
        } \
    } while (0)

/**
 * @brief Throw KG wiring NULL parameter exception
 *
 * @param module_name Module name (may be NULL for create errors)
 * @param operation Operation name
 * @param param_name Parameter that was NULL
 */
#define NIMCP_THROW_KG_WIRING_NULL(module_name, operation, param_name) \
    NIMCP_THROW_KG_WIRING( \
        NIMCP_ERROR_KG_WIRING_NULL, \
        (module_name) ? (module_name) : "unknown", \
        (operation), \
        "NULL parameter: %s", \
        (param_name) \
    )

/**
 * @brief Throw KG wiring validation exception
 *
 * @param module_name Module name
 * @param error_msg Validation error message
 */
#define NIMCP_THROW_KG_WIRING_VALIDATION(module_name, error_msg) \
    NIMCP_THROW_KG_WIRING( \
        NIMCP_ERROR_KG_WIRING_VALIDATION, \
        (module_name), \
        "validate", \
        "Validation failed: %s", \
        (error_msg) \
    )

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KG_WIRING_EXCEPTION_H */
