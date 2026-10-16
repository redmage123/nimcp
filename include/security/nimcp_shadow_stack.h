/**
 * @file nimcp_shadow_stack.h
 * @brief Shadow Stack - Return Address Protection
 *
 * WHAT: Provides a shadow stack implementation for protecting return addresses
 *       against buffer overflow attacks and Return-Oriented Programming (ROP).
 *
 * WHY:  Return addresses on the regular stack are vulnerable to overwrites.
 *       The shadow stack maintains a separate, protected copy of return addresses
 *       that is verified on function return.
 *
 * HOW:  On function entry, push return address to shadow stack.
 *       On function return, compare actual return with shadow stack.
 *       Mismatch indicates stack corruption or attack.
 *
 * INTEGRATION:
 *   - Works with CFI for complete control flow protection
 *   - Can be used with compiler instrumentation or manual calls
 *   - Thread-safe with per-thread shadow stacks
 *
 * Part of Phase SC-1: Security Coverage Framework (Tier 0.7)
 */

#ifndef NIMCP_SHADOW_STACK_H
#define NIMCP_SHADOW_STACK_H

#include "utils/validation/nimcp_common.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Default shadow stack size (number of entries) */
#define NIMCP_SHADOW_STACK_DEFAULT_SIZE 1024

/** Maximum shadow stack size */
#define NIMCP_SHADOW_STACK_MAX_SIZE 65536

/** Minimum shadow stack size */
#define NIMCP_SHADOW_STACK_MIN_SIZE 64

/** Guard value for stack integrity */
#define NIMCP_SHADOW_STACK_GUARD 0xDEADBEEFCAFEBABEULL

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Shadow stack result codes
 */
typedef enum {
    NIMCP_SS_OK = 0,                  /**< Operation successful */
    NIMCP_SS_OVERFLOW,                /**< Stack overflow */
    NIMCP_SS_UNDERFLOW,               /**< Stack underflow */
    NIMCP_SS_MISMATCH,                /**< Return address mismatch */
    NIMCP_SS_CORRUPTED,               /**< Stack corruption detected */
    NIMCP_SS_NOT_INITIALIZED,         /**< Stack not initialized */
    NIMCP_SS_INVALID_PARAM            /**< Invalid parameter */
} nimcp_ss_result_t;

/**
 * @brief Shadow stack mode
 */
typedef enum {
    NIMCP_SS_MODE_DISABLED = 0,       /**< Shadow stack disabled */
    NIMCP_SS_MODE_DETECT,             /**< Log mismatches but allow */
    NIMCP_SS_MODE_ENFORCE             /**< Abort on mismatch */
} nimcp_ss_mode_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Shadow stack entry
 */
typedef struct {
    void* return_address;             /**< Saved return address */
    void* frame_pointer;              /**< Optional: frame pointer */
    uint64_t canary;                  /**< Integrity canary */
} nimcp_ss_entry_t;

/**
 * @brief Shadow stack statistics
 */
typedef struct {
    uint64_t pushes;                  /**< Total push operations */
    uint64_t pops;                    /**< Total pop operations */
    uint64_t mismatches;              /**< Return address mismatches */
    uint64_t overflows;               /**< Overflow attempts */
    uint64_t underflows;              /**< Underflow attempts */
    uint64_t corruptions;             /**< Corruption detections */
    uint32_t max_depth;               /**< Maximum stack depth reached */
    uint32_t current_depth;           /**< Current stack depth */
} nimcp_ss_stats_t;

/**
 * @brief Shadow stack context (opaque handle)
 */
typedef struct nimcp_shadow_stack nimcp_shadow_stack_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create shadow stack
 *
 * @param size Number of entries (0 = default size)
 * @return Shadow stack context or NULL on failure
 */
nimcp_shadow_stack_t* nimcp_shadow_stack_create(uint32_t size);

/**
 * @brief Initialize shadow stack with mode
 *
 * @param ss Shadow stack context
 * @param mode Enforcement mode
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_shadow_stack_init(
    nimcp_shadow_stack_t* ss,
    nimcp_ss_mode_t mode
);

/**
 * @brief Destroy shadow stack
 *
 * @param ss Shadow stack context
 */
void nimcp_shadow_stack_destroy(nimcp_shadow_stack_t* ss);

//=============================================================================
// Core Operations
//=============================================================================

/**
 * @brief Push return address to shadow stack
 *
 * Call this at function entry to save return address.
 *
 * @param ss Shadow stack context
 * @param return_address Return address to save
 * @return Result code
 */
nimcp_ss_result_t nimcp_shadow_stack_push(
    nimcp_shadow_stack_t* ss,
    void* return_address
);

/**
 * @brief Push return address with frame pointer
 *
 * @param ss Shadow stack context
 * @param return_address Return address to save
 * @param frame_pointer Frame pointer (rbp/ebp)
 * @return Result code
 */
nimcp_ss_result_t nimcp_shadow_stack_push_frame(
    nimcp_shadow_stack_t* ss,
    void* return_address,
    void* frame_pointer
);

/**
 * @brief Pop and verify return address
 *
 * Call this at function return to verify return address.
 *
 * @param ss Shadow stack context
 * @param actual_return Actual return address from stack
 * @return Result code (NIMCP_SS_MISMATCH if different)
 */
nimcp_ss_result_t nimcp_shadow_stack_pop(
    nimcp_shadow_stack_t* ss,
    void* actual_return
);

/**
 * @brief Pop without verification (for exception handling)
 *
 * @param ss Shadow stack context
 * @param expected_return Output: saved return address
 * @return Result code
 */
nimcp_ss_result_t nimcp_shadow_stack_pop_unchecked(
    nimcp_shadow_stack_t* ss,
    void** expected_return
);

/**
 * @brief Peek at top of shadow stack without popping
 *
 * @param ss Shadow stack context
 * @param expected_return Output: top return address
 * @return Result code
 */
nimcp_ss_result_t nimcp_shadow_stack_peek(
    nimcp_shadow_stack_t* ss,
    void** expected_return
);

//=============================================================================
// Stack Management
//=============================================================================

/**
 * @brief Get current stack depth
 *
 * @param ss Shadow stack context
 * @return Current depth (number of saved returns)
 */
uint32_t nimcp_shadow_stack_depth(nimcp_shadow_stack_t* ss);

/**
 * @brief Check if stack is empty
 *
 * @param ss Shadow stack context
 * @return true if empty
 */
bool nimcp_shadow_stack_is_empty(nimcp_shadow_stack_t* ss);

/**
 * @brief Check if stack is full
 *
 * @param ss Shadow stack context
 * @return true if full
 */
bool nimcp_shadow_stack_is_full(nimcp_shadow_stack_t* ss);

/**
 * @brief Clear shadow stack
 *
 * @param ss Shadow stack context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_shadow_stack_clear(nimcp_shadow_stack_t* ss);

/**
 * @brief Verify stack integrity (check canaries)
 *
 * @param ss Shadow stack context
 * @return true if intact, false if corrupted
 */
bool nimcp_shadow_stack_verify(nimcp_shadow_stack_t* ss);

//=============================================================================
// Unwinding Support
//=============================================================================

/**
 * @brief Unwind shadow stack to specific depth
 *
 * Used for exception handling / longjmp.
 *
 * @param ss Shadow stack context
 * @param target_depth Depth to unwind to
 * @return Result code
 */
nimcp_ss_result_t nimcp_shadow_stack_unwind(
    nimcp_shadow_stack_t* ss,
    uint32_t target_depth
);

/**
 * @brief Unwind to specific return address
 *
 * @param ss Shadow stack context
 * @param target_address Return address to unwind to
 * @return Result code
 */
nimcp_ss_result_t nimcp_shadow_stack_unwind_to(
    nimcp_shadow_stack_t* ss,
    void* target_address
);

//=============================================================================
// Statistics and Configuration
//=============================================================================

/**
 * @brief Get shadow stack statistics
 *
 * @param ss Shadow stack context
 * @param stats Output: statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_shadow_stack_get_stats(
    nimcp_shadow_stack_t* ss,
    nimcp_ss_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param ss Shadow stack context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_shadow_stack_reset_stats(nimcp_shadow_stack_t* ss);

/**
 * @brief Set enforcement mode
 *
 * @param ss Shadow stack context
 * @param mode New mode
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_shadow_stack_set_mode(
    nimcp_shadow_stack_t* ss,
    nimcp_ss_mode_t mode
);

/**
 * @brief Get current mode
 *
 * @param ss Shadow stack context
 * @return Current mode
 */
nimcp_ss_mode_t nimcp_shadow_stack_get_mode(nimcp_shadow_stack_t* ss);

//=============================================================================
// Mismatch Handler
//=============================================================================

/**
 * @brief Mismatch callback type
 *
 * @param expected Expected return address (from shadow stack)
 * @param actual Actual return address
 * @param user_data User context
 * @return true to allow return anyway, false to block/abort
 */
typedef bool (*nimcp_ss_mismatch_callback_t)(
    void* expected,
    void* actual,
    void* user_data
);

/**
 * @brief Set mismatch handler
 *
 * @param ss Shadow stack context
 * @param callback Handler function
 * @param user_data User context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_shadow_stack_set_handler(
    nimcp_shadow_stack_t* ss,
    nimcp_ss_mismatch_callback_t callback,
    void* user_data
);

//=============================================================================
// Thread-Local Support
//=============================================================================

/**
 * @brief Get thread-local shadow stack
 *
 * Creates one if not exists for current thread.
 *
 * @return Thread's shadow stack or NULL
 */
nimcp_shadow_stack_t* nimcp_shadow_stack_get_thread_local(void);

/**
 * @brief Set thread-local shadow stack
 *
 * @param ss Shadow stack to set for current thread
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_shadow_stack_set_thread_local(nimcp_shadow_stack_t* ss);

/**
 * @brief Cleanup thread-local shadow stack
 *
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_shadow_stack_cleanup_thread_local(void);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get result name as string
 *
 * @param result Result code
 * @return Result name string
 */
const char* nimcp_ss_result_name(nimcp_ss_result_t result);

/**
 * @brief Get mode name as string
 *
 * @param mode Mode
 * @return Mode name string
 */
const char* nimcp_ss_mode_name(nimcp_ss_mode_t mode);

//=============================================================================
// Convenience Macros
//=============================================================================

/**
 * @brief Push current return address to shadow stack
 *
 * Usage: NIMCP_SS_ENTER(ss);  // At function start
 */
#define NIMCP_SS_ENTER(ss) \
    nimcp_shadow_stack_push((ss), __builtin_return_address(0))

/**
 * @brief Verify and pop return address
 *
 * Usage: NIMCP_SS_LEAVE(ss);  // Before return
 */
#define NIMCP_SS_LEAVE(ss) \
    nimcp_shadow_stack_pop((ss), __builtin_return_address(0))

/**
 * @brief Function instrumentation: prologue
 */
#define NIMCP_SS_PROLOGUE() \
    do { \
        nimcp_shadow_stack_t* _ss = nimcp_shadow_stack_get_thread_local(); \
        if (_ss) nimcp_shadow_stack_push(_ss, __builtin_return_address(0)); \
    } while (0)

/**
 * @brief Function instrumentation: epilogue
 */
#define NIMCP_SS_EPILOGUE() \
    do { \
        nimcp_shadow_stack_t* _ss = nimcp_shadow_stack_get_thread_local(); \
        if (_ss) nimcp_shadow_stack_pop(_ss, __builtin_return_address(0)); \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_SHADOW_STACK_H
