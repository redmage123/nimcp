/**
 * @file nimcp_checkpoints.h
 * @brief Security Enforcement Checkpoints and Macros
 *
 * WHAT: Provides security checkpoint macros for enforcing security policies
 *       at critical points in code execution.
 *
 * WHY:  Security checks must be consistent and cannot be bypassed.
 *       These macros provide uniform enforcement with configurable responses.
 *
 * HOW:  Macros wrap common security checks with logging, metrics, and
 *       configurable failure handling (log, abort, exception).
 *
 * USAGE:
 *   - NIMCP_CHECK_*: Assertion-style checks
 *   - NIMCP_GUARD_*: Guard clauses with early return
 *   - NIMCP_REQUIRE_*: Prerequisites that must be true
 *   - NIMCP_VERIFY_*: Verification with custom action
 *
 * Part of Phase SC-1: Security Coverage Framework (Tier 0.7)
 */

#ifndef NIMCP_CHECKPOINTS_H
#define NIMCP_CHECKPOINTS_H

#include "security/nimcp_security.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Checkpoint behavior on failure
 */
#ifndef NIMCP_CHECKPOINT_ON_FAILURE
#define NIMCP_CHECKPOINT_ON_FAILURE NIMCP_CHECKPOINT_ABORT
#endif

#define NIMCP_CHECKPOINT_LOG       1  /**< Log and continue */
#define NIMCP_CHECKPOINT_ABORT     2  /**< Log and abort */
#define NIMCP_CHECKPOINT_EXCEPTION 3  /**< Throw exception (C++) */

/**
 * @brief Enable/disable checkpoints at compile time
 */
#ifndef NIMCP_CHECKPOINTS_ENABLED
#define NIMCP_CHECKPOINTS_ENABLED 1
#endif

//=============================================================================
// Internal Macros
//=============================================================================

/**
 * @brief Internal: Log checkpoint failure
 */
#define _NIMCP_CHECKPOINT_LOG(check, file, line, func) \
    do { \
        char _msg[512]; \
        snprintf(_msg, sizeof(_msg), \
                 "Security checkpoint failed: %s at %s:%d in %s()", \
                 #check, file, line, func); \
        nimcp_security_log_event( \
            NIMCP_SECURITY_EVENT_THREAT_DETECTED, \
            NIMCP_THREAT_CRITICAL, \
            _msg \
        ); \
    } while (0)

/**
 * @brief Internal: Handle checkpoint failure
 */
#if NIMCP_CHECKPOINT_ON_FAILURE == NIMCP_CHECKPOINT_ABORT
#define _NIMCP_CHECKPOINT_FAIL(check, file, line, func) \
    do { \
        _NIMCP_CHECKPOINT_LOG(check, file, line, func); \
        abort(); \
    } while (0)
#elif NIMCP_CHECKPOINT_ON_FAILURE == NIMCP_CHECKPOINT_LOG
#define _NIMCP_CHECKPOINT_FAIL(check, file, line, func) \
    _NIMCP_CHECKPOINT_LOG(check, file, line, func)
#else
#define _NIMCP_CHECKPOINT_FAIL(check, file, line, func) \
    do { \
        _NIMCP_CHECKPOINT_LOG(check, file, line, func); \
        abort(); \
    } while (0)
#endif

//=============================================================================
// Basic Checkpoint Macros
//=============================================================================

#if NIMCP_CHECKPOINTS_ENABLED

/**
 * @brief Assert a security condition
 *
 * Usage: NIMCP_CHECK(ptr != NULL);
 */
#define NIMCP_CHECK(condition) \
    do { \
        if (!(condition)) { \
            _NIMCP_CHECKPOINT_FAIL(condition, __FILE__, __LINE__, __func__); \
        } \
    } while (0)

/**
 * @brief Check with custom message
 */
#define NIMCP_CHECK_MSG(condition, msg) \
    do { \
        if (!(condition)) { \
            nimcp_security_log_event( \
                NIMCP_SECURITY_EVENT_THREAT_DETECTED, \
                NIMCP_THREAT_CRITICAL, \
                msg \
            ); \
            abort(); \
        } \
    } while (0)

/**
 * @brief Check that returns on failure
 */
#define NIMCP_CHECK_RETURN(condition, retval) \
    do { \
        if (!(condition)) { \
            _NIMCP_CHECKPOINT_LOG(condition, __FILE__, __LINE__, __func__); \
            return (retval); \
        } \
    } while (0)

/**
 * @brief Check that returns void on failure
 */
#define NIMCP_CHECK_RETURN_VOID(condition) \
    do { \
        if (!(condition)) { \
            _NIMCP_CHECKPOINT_LOG(condition, __FILE__, __LINE__, __func__); \
            return; \
        } \
    } while (0)

#else  // NIMCP_CHECKPOINTS_ENABLED

#define NIMCP_CHECK(condition) ((void)0)
#define NIMCP_CHECK_MSG(condition, msg) ((void)0)
#define NIMCP_CHECK_RETURN(condition, retval) ((void)0)
#define NIMCP_CHECK_RETURN_VOID(condition) ((void)0)

#endif  // NIMCP_CHECKPOINTS_ENABLED

//=============================================================================
// Guard Clause Macros
//=============================================================================

/**
 * @brief Guard clause - return if condition fails
 *
 * Usage: NIMCP_GUARD(ptr != NULL, NULL);
 */
#define NIMCP_GUARD(condition, retval) \
    do { \
        if (!(condition)) { \
            return (retval); \
        } \
    } while (0)

/**
 * @brief Guard clause with logging
 */
#define NIMCP_GUARD_LOG(condition, retval) \
    NIMCP_CHECK_RETURN(condition, retval)

/**
 * @brief Guard for NULL pointers
 */
#define NIMCP_GUARD_NULL(ptr, retval) \
    NIMCP_GUARD((ptr) != NULL, retval)

/**
 * @brief Guard for NULL with logging
 */
#define NIMCP_GUARD_NULL_LOG(ptr, retval) \
    NIMCP_GUARD_LOG((ptr) != NULL, retval)

/**
 * @brief Guard for invalid state
 */
#define NIMCP_GUARD_STATE(ctx, field, expected, retval) \
    NIMCP_GUARD((ctx)->field == (expected), retval)

//=============================================================================
// Requirement Macros
//=============================================================================

/**
 * @brief Require precondition (aborts on failure)
 *
 * Usage: NIMCP_REQUIRE(size > 0);
 */
#define NIMCP_REQUIRE(condition) \
    NIMCP_CHECK(condition)

/**
 * @brief Require non-NULL pointer
 */
#define NIMCP_REQUIRE_NOT_NULL(ptr) \
    NIMCP_CHECK((ptr) != NULL)

/**
 * @brief Require pointer is NULL
 */
#define NIMCP_REQUIRE_NULL(ptr) \
    NIMCP_CHECK((ptr) == NULL)

/**
 * @brief Require initialized state
 */
#define NIMCP_REQUIRE_INITIALIZED(ctx) \
    NIMCP_CHECK((ctx) != NULL && (ctx)->initialized)

/**
 * @brief Require range
 */
#define NIMCP_REQUIRE_RANGE(val, min, max) \
    NIMCP_CHECK((val) >= (min) && (val) <= (max))

/**
 * @brief Require positive value
 */
#define NIMCP_REQUIRE_POSITIVE(val) \
    NIMCP_CHECK((val) > 0)

//=============================================================================
// Verification Macros
//=============================================================================

/**
 * @brief Verify with custom action on failure
 *
 * Usage: NIMCP_VERIFY(ptr != NULL, { cleanup(); return NULL; });
 */
#define NIMCP_VERIFY(condition, action) \
    do { \
        if (!(condition)) { \
            _NIMCP_CHECKPOINT_LOG(condition, __FILE__, __LINE__, __func__); \
            action; \
        } \
    } while (0)

/**
 * @brief Verify and log without action
 */
#define NIMCP_VERIFY_LOG(condition) \
    do { \
        if (!(condition)) { \
            _NIMCP_CHECKPOINT_LOG(condition, __FILE__, __LINE__, __func__); \
        } \
    } while (0)

/**
 * @brief Verify memory operation result
 */
#define NIMCP_VERIFY_ALLOC(ptr) \
    NIMCP_VERIFY((ptr) != NULL, { return NULL; })

/**
 * @brief Verify NIMCP result code
 */
#define NIMCP_VERIFY_RESULT(result) \
    NIMCP_VERIFY((result) == NIMCP_SUCCESS, { return (result); })

//=============================================================================
// Security-Specific Checkpoints
//=============================================================================

/**
 * @brief Check memory region bounds
 */
#define NIMCP_CHECK_BOUNDS(ptr, size, base, limit) \
    NIMCP_CHECK( \
        (uintptr_t)(ptr) >= (uintptr_t)(base) && \
        (uintptr_t)(ptr) + (size) <= (uintptr_t)(limit) \
    )

/**
 * @brief Check pointer alignment
 */
#define NIMCP_CHECK_ALIGNED(ptr, alignment) \
    NIMCP_CHECK(((uintptr_t)(ptr) & ((alignment) - 1)) == 0)

/**
 * @brief Check buffer size
 */
#define NIMCP_CHECK_BUFFER_SIZE(size, min_size) \
    NIMCP_CHECK((size) >= (min_size))

/**
 * @brief Check array index
 */
#define NIMCP_CHECK_INDEX(index, array_size) \
    NIMCP_CHECK((index) < (array_size))

/**
 * @brief Check capability
 */
#define NIMCP_CHECK_CAPABILITY(ctx, required_cap) \
    NIMCP_CHECK(((ctx)->capabilities & (required_cap)) == (required_cap))

/**
 * @brief Check threat level
 */
#define NIMCP_CHECK_THREAT_LEVEL(level, max_allowed) \
    NIMCP_CHECK((level) <= (max_allowed))

//=============================================================================
// CFI Integration Checkpoints
//=============================================================================

/**
 * @brief Check CFI before indirect call
 */
#define NIMCP_CHECK_CFI(cfi, target, type_id) \
    do { \
        if ((cfi) && !nimcp_cfi_validate_ptr((cfi), (void*)(target), (type_id))) { \
            _NIMCP_CHECKPOINT_FAIL( \
                nimcp_cfi_validate_ptr(cfi, target, type_id), \
                __FILE__, __LINE__, __func__ \
            ); \
        } \
    } while (0)

/**
 * @brief Check shadow stack on return
 */
#define NIMCP_CHECK_SHADOW_STACK(ss) \
    do { \
        if ((ss)) { \
            nimcp_ss_result_t _res = nimcp_shadow_stack_pop( \
                (ss), __builtin_return_address(0) \
            ); \
            if (_res != NIMCP_SS_OK) { \
                _NIMCP_CHECKPOINT_FAIL( \
                    shadow_stack_ok, __FILE__, __LINE__, __func__ \
                ); \
            } \
        } \
    } while (0)

//=============================================================================
// Debug/Development Checkpoints
//=============================================================================

#ifdef NIMCP_DEBUG

/**
 * @brief Debug-only checkpoint
 */
#define NIMCP_DEBUG_CHECK(condition) \
    NIMCP_CHECK(condition)

/**
 * @brief Debug-only assertion with message
 */
#define NIMCP_DEBUG_ASSERT(condition, msg) \
    NIMCP_CHECK_MSG(condition, msg)

#else

#define NIMCP_DEBUG_CHECK(condition) ((void)0)
#define NIMCP_DEBUG_ASSERT(condition, msg) ((void)0)

#endif  // NIMCP_DEBUG

//=============================================================================
// Compile-Time Checks
//=============================================================================

/**
 * @brief Static assertion (compile-time)
 */
#define NIMCP_STATIC_CHECK(condition, msg) \
    _Static_assert(condition, msg)

/**
 * @brief Verify struct size at compile time
 */
#define NIMCP_CHECK_STRUCT_SIZE(type, expected_size) \
    NIMCP_STATIC_CHECK(sizeof(type) == (expected_size), \
                       "Unexpected struct size for " #type)

/**
 * @brief Verify alignment at compile time
 */
#define NIMCP_CHECK_ALIGNMENT(type, expected_align) \
    NIMCP_STATIC_CHECK(_Alignof(type) == (expected_align), \
                       "Unexpected alignment for " #type)

//=============================================================================
// Function Entry/Exit Checkpoints
//=============================================================================

/**
 * @brief Function entry checkpoint
 */
#define NIMCP_FUNCTION_ENTRY() \
    nimcp_security_log_event( \
        NIMCP_SECURITY_EVENT_DIRECTIVE_VERIFIED, \
        NIMCP_THREAT_NONE, \
        __func__ \
    )

/**
 * @brief Security-critical function entry
 */
#define NIMCP_SECURE_FUNCTION_ENTRY(ss) \
    do { \
        if ((ss)) nimcp_shadow_stack_push((ss), __builtin_return_address(0)); \
    } while (0)

/**
 * @brief Security-critical function exit
 */
#define NIMCP_SECURE_FUNCTION_EXIT(ss) \
    NIMCP_CHECK_SHADOW_STACK(ss)

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_CHECKPOINTS_H
