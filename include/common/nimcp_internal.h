#ifndef NIMCP_INTERNAL_H
#define NIMCP_INTERNAL_H

/**
 * @file nimcp_internal.h
 * @brief Internal utilities and macros for NIMCP library
 *
 * This file contains internal implementation details that should NOT be
 * used by external consumers of the library. For public API, see the
 * respective module headers (nimcp_neuralnet.h, nimcp_brain.h, etc.).
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

//==============================================================================
// API Boundary Validation Macros
//==============================================================================

/**
 * @defgroup ValidationMacros API Boundary Validation
 * @brief Macros for validating parameters at public API boundaries
 *
 * These macros provide consistent parameter validation across the library,
 * ensuring that all public APIs properly validate inputs before use.
 *
 * @{
 */

/**
 * @brief Validate a parameter and return a value if invalid
 *
 * Checks a condition and returns early with a specified value if the
 * condition is false. Logs an error message for debugging.
 *
 * @param cond The condition to check (should evaluate to true for valid input)
 * @param ret_val The value to return if condition is false
 *
 * Example:
 * @code
 * int my_function(void *data) {
 *     NIMCP_VALIDATE_PARAM(data != NULL, -1);
 *     // ... rest of function ...
 *     return 0;
 * }
 * @endcode
 */
#define NIMCP_VALIDATE_PARAM(cond, ret_val)                                    \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr,                                                    \
                    "[NIMCP ERROR] Invalid parameter '%s' at %s:%d in %s()\n",\
                    #cond, __FILE__, __LINE__, __func__);                      \
            return (ret_val);                                                  \
        }                                                                      \
    } while (0)

/**
 * @brief Validate a parameter and return (void) if invalid
 *
 * Similar to NIMCP_VALIDATE_PARAM but for void functions.
 *
 * @param cond The condition to check
 *
 * Example:
 * @code
 * void my_function(void *data) {
 *     NIMCP_VALIDATE_PARAM_VOID(data != NULL);
 *     // ... rest of function ...
 * }
 * @endcode
 */
#define NIMCP_VALIDATE_PARAM_VOID(cond)                                        \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr,                                                    \
                    "[NIMCP ERROR] Invalid parameter '%s' at %s:%d in %s()\n",\
                    #cond, __FILE__, __LINE__, __func__);                      \
            return;                                                            \
        }                                                                      \
    } while (0)

/**
 * @brief Validate a pointer is non-NULL
 *
 * Convenience macro for the common case of NULL pointer checking.
 *
 * @param ptr The pointer to check
 * @param ret_val The value to return if pointer is NULL
 */
#ifndef NIMCP_CHECK_NULL
#define NIMCP_CHECK_NULL(ptr, ret_val) \
    NIMCP_VALIDATE_PARAM((ptr) != NULL, (ret_val))
#endif

/**
 * @brief Validate a numerical value is within a range
 *
 * @param val The value to check
 * @param min Minimum valid value (inclusive)
 * @param max Maximum valid value (inclusive)
 * @param ret_val The value to return if out of range
 */
#define NIMCP_CHECK_RANGE(val, min, max, ret_val)                              \
    NIMCP_VALIDATE_PARAM(((val) >= (min)) && ((val) <= (max)), (ret_val))

/**
 * @brief Validate a size parameter
 *
 * Ensures a size value is positive and within reasonable bounds.
 *
 * @param size The size to check
 * @param max_size Maximum allowed size
 * @param ret_val The value to return if invalid
 */
#define NIMCP_CHECK_SIZE(size, max_size, ret_val)                              \
    NIMCP_VALIDATE_PARAM(((size) > 0) && ((size) <= (max_size)), (ret_val))

/** @} */ // end of ValidationMacros

//==============================================================================
// Memory Safety Macros
//==============================================================================

/**
 * @defgroup MemorySafety Memory Safety Helpers
 * @brief Macros for safe memory operations
 * @{
 */

/**
 * @brief Safe free - sets pointer to NULL after freeing
 *
 * Prevents use-after-free by nullifying the pointer.
 */
#define NIMCP_SAFE_FREE(ptr)                                                   \
    do {                                                                       \
        if ((ptr) != NULL) {                                                   \
            nimcp_free(ptr);                                                         \
            (ptr) = NULL;                                                      \
        }                                                                      \
    } while (0)

/**
 * @brief Safe string copy with size check
 *
 * Always null-terminates and prevents buffer overflow.
 *
 * @param dest Destination buffer
 * @param src Source string
 * @param dest_size Size of destination buffer
 */
#define NIMCP_SAFE_STRCPY(dest, src, dest_size)                                \
    do {                                                                       \
        snprintf((dest), (dest_size), "%s", (src));                            \
    } while (0)

/** @} */ // end of MemorySafety

//==============================================================================
// Assertions and Error Handling
//==============================================================================

/**
 * @defgroup ErrorHandling Error Handling
 * @brief Error handling and assertion macros
 * @{
 */

/**
 * @brief Runtime assertion for internal invariants
 *
 * Similar to assert() but remains active in release builds for critical checks.
 * Should be used for internal invariants that must never be violated.
 *
 * @param cond Condition that must be true
 * @param msg Error message to display if assertion fails
 */
#define NIMCP_ASSERT(cond, msg)                                                \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr,                                                    \
                    "[NIMCP ASSERTION FAILED] %s at %s:%d in %s()\n",          \
                    (msg), __FILE__, __LINE__, __func__);                      \
            abort();                                                           \
        }                                                                      \
    } while (0)

/**
 * @brief Log an error message
 * NOTE: Only define if not already defined (nimcp_common.h defines it as an error code)
 */
#ifndef NIMCP_ERROR
#define NIMCP_ERROR(fmt, ...)                                                  \
    fprintf(stderr, "[NIMCP ERROR] " fmt " at %s:%d\n", ##__VA_ARGS__,         \
            __FILE__, __LINE__)
#endif

/**
 * @brief Log a warning message
 */
#define NIMCP_WARN(fmt, ...)                                                   \
    fprintf(stderr, "[NIMCP WARN] " fmt " at %s:%d\n", ##__VA_ARGS__,          \
            __FILE__, __LINE__)

/**
 * @brief Log a debug message (only in debug builds)
 */
#ifdef NDEBUG
#define NIMCP_DEBUG(fmt, ...) ((void)0)
#else
#define NIMCP_DEBUG(fmt, ...)                                                  \
    fprintf(stderr, "[NIMCP DEBUG] " fmt "\n", ##__VA_ARGS__)
#endif

/** @} */ // end of ErrorHandling

//==============================================================================
// Compiler Hints and Attributes
//==============================================================================

/**
 * @defgroup CompilerHints Compiler Hints
 * @brief Compiler-specific attributes for optimization and safety
 * @{
 */

// Mark functions that should never return NULL
#if defined(__GNUC__) || defined(__clang__)
#define NIMCP_NONNULL_RETURN __attribute__((returns_nonnull))
#else
#define NIMCP_NONNULL_RETURN
#endif

// Mark parameters that must not be NULL
#if defined(__GNUC__) || defined(__clang__)
#define NIMCP_NONNULL(...) __attribute__((nonnull(__VA_ARGS__)))
#else
#define NIMCP_NONNULL(...)
#endif

// Warn if return value is unused
#if defined(__GNUC__) || defined(__clang__)
#define NIMCP_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define NIMCP_WARN_UNUSED_RESULT
#endif

// Mark functions that don't modify global state
#if defined(__GNUC__) || defined(__clang__)
#define NIMCP_PURE __attribute__((pure))
#else
#define NIMCP_PURE
#endif

// Mark functions that don't read or modify global state
#if defined(__GNUC__) || defined(__clang__)
#define NIMCP_CONST __attribute__((const))
#else
#define NIMCP_CONST
#endif

/** @} */ // end of CompilerHints

//==============================================================================
// Constants
//==============================================================================

/** Maximum number of neurons in any layer (safety limit) */
#define NIMCP_MAX_NEURONS 4000000

/** Maximum number of layers (safety limit) */
#ifndef NIMCP_MAX_LAYERS
#define NIMCP_MAX_LAYERS 1000
#endif

/** Maximum string length for names/descriptions */
#define NIMCP_MAX_STRING_LENGTH 1024

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_INTERNAL_H
