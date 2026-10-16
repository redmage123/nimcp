/**
 * @file nimcp_toctou_guard.h
 * @brief TOCTOU (Time-of-Check-Time-of-Use) guards for race condition prevention
 *
 * WHAT: Atomic validation+execution guards to prevent race conditions
 * WHY:  TOCTOU vulnerabilities allow attackers to exploit race windows
 * HOW:  Single-use validation tokens + resource locking during validate+execute
 *
 * TOCTOU ATTACK SCENARIO:
 * ┌────────────────────────────────────────────────────────────────┐
 * │  WITHOUT GUARD:                                                │
 * │  1. Thread A: Check file permissions ✓ (authorized)           │
 * │  2. Attacker: Swap file (symbolic link attack)                │
 * │  3. Thread A: Open file ✗ (now unauthorized file)             │
 * │                                                                │
 * │  WITH GUARD:                                                   │
 * │  1. Thread A: token = validate(file) - locks file             │
 * │  2. Attacker: Attempt swap → blocked by lock                  │
 * │  3. Thread A: execute(token, open_file) → success             │
 * │  4. Token expires automatically (single use)                  │
 * └────────────────────────────────────────────────────────────────┘
 *
 * ARCHITECTURE:
 * ┌────────────────────────────────────────────────────────────────┐
 * │                      TOCTOU GUARD                              │
 * ├────────────────────────────────────────────────────────────────┤
 * │  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐    │
 * │  │  Validate    │───▶│  Token Pool  │───▶│   Execute    │    │
 * │  │  (Check)     │    │  (Lock held) │    │   (Use)      │    │
 * │  └──────────────┘    └──────────────┘    └──────────────┘    │
 * │         │                    │                    │           │
 * │         └────────────────────┴────────────────────┘           │
 * │                  Single Atomic Window                         │
 * └────────────────────────────────────────────────────────────────┘
 *
 * USAGE:
 * ```c
 * // Create guard
 * nimcp_toctou_config_t config = {
 *     .max_concurrent_tokens = 1024,
 *     .token_timeout_ms = 5000,
 *     .enable_statistics = true
 * };
 * nimcp_toctou_guard_t guard = nimcp_toctou_guard_create(&config);
 *
 * // Validate resource (locks it)
 * nimcp_toctou_token_t token = nimcp_toctou_validate(
 *     guard, file_path, strlen(file_path));
 * if (!token) {
 *     // Validation failed or guard busy
 *     return NIMCP_ERROR_TIMEOUT;
 * }
 *
 * // Execute action (token consumed automatically)
 * nimcp_error_t err = nimcp_toctou_execute(token, open_file_action, ctx);
 *
 * // Token is now invalid (single-use), lock released
 * nimcp_toctou_guard_destroy(guard);
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-12-07
 * @version 1.0.0
 */

#ifndef NIMCP_TOCTOU_GUARD_H
#define NIMCP_TOCTOU_GUARD_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Default maximum concurrent tokens */
#define NIMCP_TOCTOU_DEFAULT_MAX_TOKENS 1024

/** @brief Default token timeout (milliseconds) */
#define NIMCP_TOCTOU_DEFAULT_TIMEOUT_MS 5000

/** @brief Magic number for guard validation */
#define NIMCP_TOCTOU_GUARD_MAGIC 0xDEADBEEF

/** @brief Magic number for token validation */
#define NIMCP_TOCTOU_TOKEN_MAGIC 0xCAFEBABE

//=============================================================================
// Opaque Types
//=============================================================================

/**
 * @brief TOCTOU guard handle (opaque)
 */
typedef struct nimcp_toctou_guard_impl* nimcp_toctou_guard_t;

/**
 * @brief Validation token handle (opaque, single-use)
 */
typedef struct nimcp_toctou_token_impl* nimcp_toctou_token_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief TOCTOU guard configuration
 */
typedef struct {
    uint32_t max_concurrent_tokens; /**< Maximum concurrent validation tokens */
    uint32_t token_timeout_ms;      /**< Token expiration timeout */
    bool enable_statistics;         /**< Track guard statistics */
    bool enable_logging;            /**< Log security events */
    uint32_t contention_backoff_ms; /**< Backoff time on contention */
    bool strict_mode;               /**< Fail fast on any violation */
} nimcp_toctou_config_t;

/**
 * @brief TOCTOU guard statistics
 */
typedef struct {
    uint64_t guards_created;        /**< Total guards created */
    uint64_t tokens_created;        /**< Total tokens created */
    uint64_t tokens_used;           /**< Total tokens consumed in execute */
    uint64_t tokens_expired;        /**< Tokens expired before use */
    uint64_t tokens_cancelled;      /**< Tokens cancelled explicitly */
    uint64_t contention_events;     /**< Lock contention events */
    uint64_t validation_failures;   /**< Validation failures */
    uint64_t execution_failures;    /**< Execution failures */
    uint32_t active_tokens;         /**< Currently active tokens */
    uint64_t total_wait_time_ms;    /**< Total time spent waiting */
    uint64_t max_wait_time_ms;      /**< Maximum wait time observed */
    float avg_wait_time_ms;         /**< Average wait time */
} nimcp_toctou_stats_t;

//=============================================================================
// Callback Types
//=============================================================================

/**
 * @brief Action function executed under TOCTOU protection
 *
 * WHAT: User-defined action executed atomically with validation
 * WHY:  Ensures action runs without race condition window
 * HOW:  Called while resource lock is held
 *
 * @param resource Validated resource pointer
 * @param resource_size Size of resource
 * @param context User context passed to execute
 * @return NIMCP_SUCCESS or error code
 *
 * IMPORTANT: This function must complete quickly to avoid blocking
 */
typedef nimcp_error_t (*nimcp_toctou_action_fn)(
    const void* resource,
    size_t resource_size,
    void* context
);

/**
 * @brief Validation function for custom resource checks
 *
 * WHAT: Optional validation callback for complex checks
 * WHY:  Allow custom validation logic beyond default checks
 * HOW:  Called before token creation, lock held
 *
 * @param resource Resource to validate
 * @param resource_size Size of resource
 * @param context User context
 * @return true if valid, false otherwise
 */
typedef bool (*nimcp_toctou_validator_fn)(
    const void* resource,
    size_t resource_size,
    void* context
);

//=============================================================================
// Guard Lifecycle API
//=============================================================================

/**
 * @brief Create TOCTOU guard with configuration
 *
 * WHAT: Creates guard system for atomic validate+execute operations
 * WHY:  Initialize TOCTOU protection for resources
 * HOW:  Allocates guard, token pool, locks, statistics tracking
 *
 * @param config Configuration (NULL for defaults)
 * @return Guard handle or NULL on failure
 *
 * COMPLEXITY: O(n) where n = max_concurrent_tokens
 * THREAD SAFETY: Thread-safe
 *
 * ERRORS:
 * - Returns NULL if memory allocation fails
 * - Returns NULL if mutex initialization fails
 */
nimcp_toctou_guard_t nimcp_toctou_guard_create(
    const nimcp_toctou_config_t* config
);

/**
 * @brief Destroy TOCTOU guard
 *
 * WHAT: Cleanup guard resources
 * WHY:  Free memory, destroy locks
 * HOW:  Waits for active tokens, destroys guard
 *
 * @param guard Guard handle (NULL safe)
 *
 * COMPLEXITY: O(n) where n = active_tokens (waits for completion)
 * THREAD SAFETY: NOT thread-safe (must be last operation)
 *
 * IMPORTANT: All tokens must be completed or cancelled before destroy
 */
void nimcp_toctou_guard_destroy(nimcp_toctou_guard_t guard);

/**
 * @brief Get default configuration
 *
 * @return Default configuration with sensible values
 */
nimcp_toctou_config_t nimcp_toctou_default_config(void);

//=============================================================================
// Core TOCTOU Protection API
//=============================================================================

/**
 * @brief Validate resource and create single-use token
 *
 * WHAT: Atomically validate resource and lock it
 * WHY:  Prevent TOCTOU race between check and use
 * HOW:  Validates resource, acquires lock, creates token
 *
 * @param guard Guard handle
 * @param resource Pointer to resource to validate
 * @param size Size of resource
 * @return Token handle or NULL on failure
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe
 *
 * BEHAVIOR:
 * - Validates resource exists and size > 0
 * - Acquires lock on resource (blocks if needed)
 * - Creates single-use token with expiration
 * - Returns NULL if validation fails or timeout
 *
 * ERRORS:
 * - NULL if guard is NULL or invalid
 * - NULL if resource is NULL or size is 0
 * - NULL if max concurrent tokens exceeded
 * - NULL if lock acquisition times out
 */
nimcp_toctou_token_t nimcp_toctou_validate(
    nimcp_toctou_guard_t guard,
    const void* resource,
    size_t size
);

/**
 * @brief Validate with custom validator callback
 *
 * @param guard Guard handle
 * @param resource Resource to validate
 * @param size Resource size
 * @param validator Custom validation function
 * @param validator_ctx Context for validator
 * @return Token handle or NULL on failure
 */
nimcp_toctou_token_t nimcp_toctou_validate_custom(
    nimcp_toctou_guard_t guard,
    const void* resource,
    size_t size,
    nimcp_toctou_validator_fn validator,
    void* validator_ctx
);

/**
 * @brief Execute action with validated token
 *
 * WHAT: Execute action atomically, consuming token
 * WHY:  Complete validate+execute without race window
 * HOW:  Verifies token, calls action, releases lock, invalidates token
 *
 * @param token Validation token (consumed after call)
 * @param action Action function to execute
 * @param context Context passed to action
 * @return NIMCP_SUCCESS or error from action
 *
 * COMPLEXITY: O(1) + action complexity
 * THREAD SAFETY: Thread-safe
 *
 * BEHAVIOR:
 * - Validates token is not expired/cancelled/used
 * - Calls action function with lock held
 * - Releases lock after action completes
 * - Marks token as used (cannot be reused)
 *
 * ERRORS:
 * - NIMCP_ERROR_NULL_POINTER if token or action is NULL
 * - NIMCP_ERROR_INVALID_STATE if token expired/used
 * - NIMCP_ERROR_TIMEOUT if token timed out
 * - Returns error from action function if action fails
 *
 * IMPORTANT: Token is ALWAYS consumed, even on error
 */
nimcp_error_t nimcp_toctou_execute(
    nimcp_toctou_token_t token,
    nimcp_toctou_action_fn action,
    void* context
);

/**
 * @brief Cancel validation token
 *
 * WHAT: Cancel token without executing action
 * WHY:  Abort operation while releasing lock
 * HOW:  Marks token cancelled, releases lock
 *
 * @param token Token to cancel
 * @return NIMCP_SUCCESS or error code
 *
 * THREAD SAFETY: Thread-safe
 */
nimcp_error_t nimcp_toctou_cancel(nimcp_toctou_token_t token);

/**
 * @brief Check if token is still valid
 *
 * @param token Token to check
 * @return true if valid and not expired/used/cancelled
 */
bool nimcp_toctou_token_is_valid(nimcp_toctou_token_t token);

/**
 * @brief Get time remaining before token expires (milliseconds)
 *
 * @param token Token to check
 * @return Milliseconds until expiration, or 0 if expired
 */
uint32_t nimcp_toctou_token_time_remaining(nimcp_toctou_token_t token);

//=============================================================================
// Statistics and Monitoring API
//=============================================================================

/**
 * @brief Get guard statistics
 *
 * WHAT: Retrieve statistics for monitoring and debugging
 * WHY:  Track guard usage, contention, performance
 * HOW:  Copies statistics under lock
 *
 * @param guard Guard handle
 * @param stats Output statistics structure
 * @return NIMCP_SUCCESS or error code
 *
 * THREAD SAFETY: Thread-safe
 */
nimcp_error_t nimcp_toctou_get_stats(
    nimcp_toctou_guard_t guard,
    nimcp_toctou_stats_t* stats
);

/**
 * @brief Reset statistics counters
 *
 * @param guard Guard handle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_toctou_reset_stats(nimcp_toctou_guard_t guard);

/**
 * @brief Get number of active tokens
 *
 * @param guard Guard handle
 * @return Number of currently active tokens
 */
uint32_t nimcp_toctou_get_active_count(nimcp_toctou_guard_t guard);

//=============================================================================
// Bio-Async Integration API
//=============================================================================

/**
 * @brief Register TOCTOU guard with bio-async router
 *
 * WHAT: Register guard as security module for async messaging
 * WHY:  Enable event notification and monitoring
 * HOW:  Registers handlers for security events
 *
 * @param guard Guard handle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_toctou_register_bio_async(nimcp_toctou_guard_t guard);

/**
 * @brief Process bio-async inbox messages
 *
 * WHAT: Process pending messages from bio-router
 * WHY:  Handle security events, monitoring requests
 * HOW:  Dequeues and processes messages
 *
 * @param guard Guard handle
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t nimcp_toctou_process_inbox(
    nimcp_toctou_guard_t guard,
    uint32_t max_messages
);

//=============================================================================
// Module Cleanup API
//=============================================================================

/**
 * @brief Cleanup TOCTOU guard module resources
 *
 * WHAT: Destroy global module resources (guard creation lock)
 * WHY:  Clean resource management on module unload
 * HOW:  Call at program exit or explicit module cleanup
 *
 * THREAD-SAFETY: Should only be called when no other threads are using the module
 *
 * NOTE: This function is idempotent - safe to call multiple times
 */
void nimcp_toctou_module_cleanup(void);

//=============================================================================
// Convenience Macros
//=============================================================================

/**
 * @brief Execute protected action in one call
 *
 * Combines validate + execute into single call for convenience.
 *
 * @param guard Guard handle
 * @param resource Resource to validate
 * @param size Resource size
 * @param action Action function
 * @param context Action context
 * @return NIMCP_SUCCESS or error code
 */
#define NIMCP_TOCTOU_PROTECTED(guard, resource, size, action, context) \
    ({ \
        nimcp_toctou_token_t _token = nimcp_toctou_validate(guard, resource, size); \
        nimcp_error_t _err = NIMCP_ERROR_INVALID_STATE; \
        if (_token) { \
            _err = nimcp_toctou_execute(_token, action, context); \
        } \
        _err; \
    })

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TOCTOU_GUARD_H */
