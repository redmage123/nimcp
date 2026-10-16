/**
 * @file nimcp_api_exception.h
 * @brief Exception integration macros for public API layer
 * @version 1.0.0
 * @date 2025-01-19
 *
 * WHAT: Macros that integrate exception handling with API error returns
 * WHY:  Enable immune system tracking of API-level errors while maintaining
 *       stable status code returns for external callers
 * HOW:  Wrap common error patterns with exception presentation + status return
 *
 * USAGE:
 * ```c
 * // Instead of:
 * if (!brain) {
 *     set_error("Brain is NULL");
 *     return NIMCP_ERROR_NULL_ARG;
 * }
 *
 * // Use:
 * NIMCP_API_CHECK_NULL(brain, NIMCP_ERROR_NULL_ARG, "Brain is NULL");
 * ```
 *
 * KG WIRING:
 * - All API exceptions are presented to the immune system
 * - Enables automatic error pattern detection and recovery
 * - Builds immune memory for recurring API errors
 *
 * INCREMENTAL ADOPTION (Phase 7):
 * This header is part of the Phase 7 System-Wide Exception Handling Audit.
 * Files should be migrated incrementally to use these macros:
 *
 * 1. Include this header in your source file
 * 2. Define NIMCP_API_SET_ERROR before including (if you have a local error setter)
 * 3. Replace manual error checks with the appropriate macro
 * 4. The macros handle logging, exception presentation, and error return
 *
 * PRIORITY ORDER FOR MIGRATION:
 * - Tier 1 (Critical): API layer files, brain core, executive control
 * - Tier 2 (Important): Cognitive subsystems, swarm, async
 * - Tier 3 (Enhancement): Perception, plasticity, utilities
 *
 * See Phase 7 in the resilience plan for the full list of 338 files needing migration.
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_API_EXCEPTION_H
#define NIMCP_API_EXCEPTION_H

#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * API Exception Category
 * ============================================================================ */

/** API-specific exception category for immune system mapping */
#define NIMCP_EXCEPTION_CATEGORY_API  100

/* ============================================================================
 * Error Setting Helper (must be provided by including file)
 * ============================================================================ */

/**
 * @brief Default API error setter macro
 *
 * If the including file has its own set_error function, define
 * NIMCP_API_SET_ERROR before including this header.
 */
#ifndef NIMCP_API_SET_ERROR
#define NIMCP_API_SET_ERROR(fmt, ...) \
    do { /* No-op if not defined */ } while(0)
#endif

/* ============================================================================
 * NULL Check Macros
 * ============================================================================ */

/**
 * @brief Check for NULL and return with exception if NULL
 *
 * @param ptr Pointer to check
 * @param code Error code to return
 * @param msg Error message
 */
#define NIMCP_API_CHECK_NULL(ptr, code, msg) \
    do { \
        if (!(ptr)) { \
            LOG_ERROR("%s", (msg)); \
            NIMCP_THROW_TO_IMMUNE((code), "%s", (msg)); \
            NIMCP_API_SET_ERROR("%s", (msg)); \
            return (code); \
        } \
    } while (0)

/**
 * @brief Check for NULL pointer with formatted message
 */
#define NIMCP_API_CHECK_NULL_FMT(ptr, code, fmt, ...) \
    do { \
        if (!(ptr)) { \
            LOG_ERROR(fmt, ##__VA_ARGS__); \
            NIMCP_THROW_TO_IMMUNE((code), fmt, ##__VA_ARGS__); \
            NIMCP_API_SET_ERROR(fmt, ##__VA_ARGS__); \
            return (code); \
        } \
    } while (0)

/**
 * @brief Check for NULL and return NULL (for pointer-returning functions)
 *
 * @param ptr Pointer to check
 * @param msg Error message
 */
#define NIMCP_API_CHECK_NULL_RET_NULL(ptr, msg) \
    do { \
        if (!(ptr)) { \
            LOG_ERROR("%s", (msg)); \
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "%s", (msg)); \
            NIMCP_API_SET_ERROR("%s", (msg)); \
            return NULL; \
        } \
    } while (0)

/**
 * @brief Check for NULL pointer and return false (for bool-returning functions)
 *
 * @param ptr Pointer to check
 * @param msg Error message
 */
#define NIMCP_API_CHECK_NULL_RET_FALSE(ptr, msg) \
    do { \
        if (!(ptr)) { \
            LOG_ERROR("%s", (msg)); \
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "%s", (msg)); \
            NIMCP_API_SET_ERROR("%s", (msg)); \
            return false; \
        } \
    } while (0)

/**
 * @brief Check condition and return NULL (for pointer-returning functions)
 *
 * @param cond Condition to check
 * @param code Error code for exception
 * @param msg Error message
 */
#define NIMCP_API_CHECK_RET_NULL(cond, code, msg) \
    do { \
        if (!(cond)) { \
            LOG_ERROR("%s", (msg)); \
            NIMCP_THROW_TO_IMMUNE((code), "%s", (msg)); \
            NIMCP_API_SET_ERROR("%s", (msg)); \
            return NULL; \
        } \
    } while (0)

/**
 * @brief Check condition and return false (for bool-returning functions)
 *
 * @param cond Condition to check
 * @param code Error code for exception
 * @param msg Error message
 */
#define NIMCP_API_CHECK_RET_FALSE(cond, code, msg) \
    do { \
        if (!(cond)) { \
            LOG_ERROR("%s", (msg)); \
            NIMCP_THROW_TO_IMMUNE((code), "%s", (msg)); \
            NIMCP_API_SET_ERROR("%s", (msg)); \
            return false; \
        } \
    } while (0)

/* ============================================================================
 * Condition Check Macros
 * ============================================================================ */

/**
 * @brief Check condition and return with exception if false
 *
 * @param cond Condition to check
 * @param code Error code to return
 * @param msg Error message
 */
#define NIMCP_API_CHECK(cond, code, msg) \
    do { \
        if (!(cond)) { \
            LOG_ERROR("%s", (msg)); \
            NIMCP_THROW_TO_IMMUNE((code), "%s", (msg)); \
            NIMCP_API_SET_ERROR("%s", (msg)); \
            return (code); \
        } \
    } while (0)

/**
 * @brief Check condition with formatted message
 */
#define NIMCP_API_CHECK_FMT(cond, code, fmt, ...) \
    do { \
        if (!(cond)) { \
            LOG_ERROR(fmt, ##__VA_ARGS__); \
            NIMCP_THROW_TO_IMMUNE((code), fmt, ##__VA_ARGS__); \
            NIMCP_API_SET_ERROR(fmt, ##__VA_ARGS__); \
            return (code); \
        } \
    } while (0)

/* ============================================================================
 * Allocation Check Macros
 * ============================================================================ */

/**
 * @brief Check allocation result and return NULL with exception if failed
 *
 * For functions that return NULL on error (like create functions).
 *
 * @param ptr Pointer to check (result of allocation)
 * @param msg Error message
 */
#define NIMCP_API_CHECK_ALLOC(ptr, msg) \
    do { \
        if (!(ptr)) { \
            LOG_ERROR("%s", (msg)); \
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, 0, "%s", (msg)); \
            NIMCP_API_SET_ERROR("%s", (msg)); \
            return NULL; \
        } \
    } while (0)

/**
 * @brief Check allocation with size info
 */
#define NIMCP_API_CHECK_ALLOC_SIZE(ptr, size, msg) \
    do { \
        if (!(ptr)) { \
            LOG_ERROR("%s (requested %zu bytes)", (msg), (size_t)(size)); \
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, (size), "%s", (msg)); \
            NIMCP_API_SET_ERROR("%s", (msg)); \
            return NULL; \
        } \
    } while (0)

/* ============================================================================
 * Internal Call Check Macros
 * ============================================================================ */

/**
 * @brief Check internal function result and return on failure
 *
 * For checking calls to internal functions that return status codes.
 *
 * @param call The function call expression
 * @param fallback_code Error code to return if call fails
 * @param msg Error message
 */
#define NIMCP_API_CHECK_CALL(call, fallback_code, msg) \
    do { \
        nimcp_status_t _result = (call); \
        if (_result != NIMCP_OK && _result != NIMCP_SUCCESS) { \
            LOG_ERROR("%s (internal error: %d)", (msg), (int)_result); \
            NIMCP_THROW_TO_IMMUNE((_result != NIMCP_ERROR) ? _result : (fallback_code), \
                                  "%s", (msg)); \
            NIMCP_API_SET_ERROR("%s", (msg)); \
            return (_result != NIMCP_ERROR) ? _result : (fallback_code); \
        } \
    } while (0)

/**
 * @brief Check internal pointer-returning call
 */
#define NIMCP_API_CHECK_CALL_PTR(result, call, msg) \
    do { \
        (result) = (call); \
        if (!(result)) { \
            LOG_ERROR("%s", (msg)); \
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR, "%s", (msg)); \
            NIMCP_API_SET_ERROR("%s", (msg)); \
            return NULL; \
        } \
    } while (0)

/* ============================================================================
 * BBB Validation Check Macros
 * ============================================================================ */

/**
 * @brief Check BBB validation result
 *
 * Used after bbb_validate_* calls.
 *
 * @param validation_passed Boolean result of validation
 * @param result BBB validation result structure
 * @param code Error code to return
 */
#define NIMCP_API_CHECK_BBB(validation_passed, result, code) \
    do { \
        if (!(validation_passed)) { \
            LOG_WARN("BBB rejected input: %s", (result).reason); \
            NIMCP_THROW_SECURITY(NIMCP_ERROR_BBB_REJECTED, (uint32_t)(result).threat, \
                                "BBB rejected input: %s", (result).reason); \
            NIMCP_API_SET_ERROR("BBB rejected: %s", (result).reason); \
            return (code); \
        } \
    } while (0)

/* ============================================================================
 * Return Value Check Macros
 * ============================================================================ */

/**
 * @brief Check if a float return value indicates error
 *
 * Many internal functions return -1.0f on error.
 *
 * @param val Float value to check
 * @param code Error code to return
 * @param msg Error message
 */
#define NIMCP_API_CHECK_FLOAT(val, code, msg) \
    do { \
        if ((val) < 0.0f) { \
            LOG_ERROR("%s", (msg)); \
            NIMCP_THROW_TO_IMMUNE((code), "%s", (msg)); \
            NIMCP_API_SET_ERROR("%s", (msg)); \
            return (code); \
        } \
    } while (0)

/**
 * @brief Check if an int return value indicates error
 *
 * Many internal functions return -1 on error.
 */
#define NIMCP_API_CHECK_INT(val, code, msg) \
    do { \
        if ((val) < 0) { \
            LOG_ERROR("%s", (msg)); \
            NIMCP_THROW_TO_IMMUNE((code), "%s", (msg)); \
            NIMCP_API_SET_ERROR("%s", (msg)); \
            return (code); \
        } \
    } while (0)

/* ============================================================================
 * Brain Exception Macros
 * ============================================================================ */

/**
 * @brief Throw brain-specific exception and return
 *
 * @param brain Brain handle (can be NULL)
 * @param region Region name
 * @param code Error code
 * @param msg Error message
 */
#define NIMCP_API_THROW_BRAIN(brain, region, code, msg) \
    do { \
        const char* _brain_id = ((brain) && (brain)->internal_brain) ? \
            brain_get_name((brain)->internal_brain) : "unknown"; \
        LOG_ERROR("[%s:%s] %s", _brain_id, (region), (msg)); \
        NIMCP_THROW_BRAIN((code), _brain_id, (region), "%s", (msg)); \
        NIMCP_API_SET_ERROR("[%s:%s] %s", _brain_id, (region), (msg)); \
        return (code); \
    } while (0)

/* ============================================================================
 * API Initialization Check
 * ============================================================================ */

/**
 * @brief Check if NIMCP is initialized
 *
 * Use at the start of API functions that require initialization.
 */
#define NIMCP_API_CHECK_INITIALIZED() \
    do { \
        extern nimcp_atomic_bool_t g_initialized; \
        if (!nimcp_atomic_load_bool(&g_initialized, NIMCP_MEMORY_ORDER_ACQUIRE)) { \
            LOG_ERROR("NIMCP not initialized - call nimcp_init() first"); \
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, \
                                  "NIMCP not initialized"); \
            NIMCP_API_SET_ERROR("NIMCP not initialized"); \
            return NIMCP_ERROR_NOT_INITIALIZED; \
        } \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_API_EXCEPTION_H */
