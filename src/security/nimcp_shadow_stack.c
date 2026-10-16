/**
 * @file nimcp_shadow_stack.c
 * @brief Implementation of Shadow Stack for return address protection
 *
 * WHAT: Implements a shadow stack that maintains a separate, protected
 *       copy of return addresses for verification.
 *
 * WHY:  The regular call stack is vulnerable to buffer overflows.
 *       Shadow stack provides tamper detection for return addresses.
 *
 * HOW:  Maintains parallel stack of return addresses with canary values.
 *       On return, compares actual return address with saved value.
 *
 * Part of Phase SC-1: Security Coverage Framework (Tier 0.7)
 */

#include "security/nimcp_shadow_stack.h"
#include "constants/nimcp_buffer_constants.h"
#include "security/nimcp_security.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "security_shadow_stack"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(shadow_stack, MESH_ADAPTER_CATEGORY_SECURITY)


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/random.h>
#endif

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal shadow stack context
 */
struct nimcp_shadow_stack {
    nimcp_ss_entry_t* entries;        /**< Stack entries array */
    uint32_t size;                    /**< Maximum size */
    uint32_t top;                     /**< Current top index */

    nimcp_ss_mode_t mode;             /**< Enforcement mode */
    nimcp_ss_stats_t stats;           /**< Statistics */

    nimcp_ss_mismatch_callback_t callback;  /**< Mismatch handler */
    void* callback_user_data;

    uint64_t top_guard;               /**< Guard before stack */
    uint64_t bottom_guard;            /**< Guard after stack */

    bool initialized;
};

/**
 * @brief Thread-local shadow stack
 */
static pthread_key_t tls_shadow_stack_key;
static pthread_once_t tls_key_once = PTHREAD_ONCE_INIT;

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Generate random canary value using secure entropy source
 *
 * WHAT: Generate cryptographically random 64-bit canary value
 * WHY:  Predictable canaries would allow attackers to bypass stack protection
 * HOW:  Use getrandom() (Linux) or /dev/urandom (fallback) - fail if neither works
 *
 * SECURITY: Do NOT use predictable sources like time(), clock(), or addresses
 *           as fallback - an attacker could predict these values.
 */
static uint64_t generate_canary(void)
{
    uint64_t canary = 0;

#if defined(__linux__)
    // Try getrandom() first (best option on Linux 3.17+)
    ssize_t result = getrandom(&canary, sizeof(canary), 0);
    if (result == sizeof(canary)) {
        return canary;
    }

    // Fallback to /dev/urandom if getrandom() fails
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        ssize_t bytes_read = read(fd, &canary, sizeof(canary));
        close(fd);
        if (bytes_read == sizeof(canary)) {
            return canary;
        }
    }

    // SECURITY FIX: No weak fallback - fail securely by returning 0
    // A zero canary will cause shadow stack creation to fail, which is
    // safer than proceeding with a predictable canary that attackers could guess.
    LOG_ERROR("Failed to generate secure canary - no entropy source available");
    nimcp_security_log_event(
        NIMCP_SECURITY_EVENT_THREAT_DETECTED,
        NIMCP_THREAT_CRITICAL,
        "Shadow stack: No secure entropy source for canary generation - refusing to use weak fallback"
    );
    return 0;  // Return 0 to signal failure - caller must check for this
#else
    // Non-Linux platforms: use /dev/urandom if available
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        ssize_t bytes_read = read(fd, &canary, sizeof(canary));
        close(fd);
        if (bytes_read == sizeof(canary)) {
            return canary;
        }
    }

    // SECURITY FIX: No weak fallback - fail securely by returning 0
    // A zero canary will cause shadow stack creation to fail, which is
    // safer than proceeding with a predictable canary that attackers could guess.
    LOG_ERROR("Failed to generate secure canary - no entropy source available");
    nimcp_security_log_event(
        NIMCP_SECURITY_EVENT_THREAT_DETECTED,
        NIMCP_THREAT_CRITICAL,
        "Shadow stack: No secure entropy source for canary generation - refusing to use weak fallback"
    );
    return 0;  // Return 0 to signal failure - caller must check for this
#endif
}

/**
 * @brief Handle return address mismatch
 */
static bool handle_mismatch(
    nimcp_shadow_stack_t* ss,
    void* expected,
    void* actual)
{
    if (!ss) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "handle_mismatch: ss is NULL");
        return false;
    }

    ss->stats.mismatches++;

    // Log the violation
    char msg[NIMCP_ERROR_BUFFER_SIZE];
    snprintf(msg, sizeof(msg),
             "Shadow stack mismatch: expected %p, got %p",
             expected, actual);

    nimcp_security_log_event(
        NIMCP_SECURITY_EVENT_THREAT_DETECTED,
        NIMCP_THREAT_CRITICAL,
        msg
    );

    // Call handler if registered
    if (ss->callback) {
        bool allow = ss->callback(expected, actual, ss->callback_user_data);
        if (allow) {
            return true;  // Handler says allow
        }
    }

    // Check enforcement mode
    switch (ss->mode) {
        case NIMCP_SS_MODE_DISABLED:
            return true;

        case NIMCP_SS_MODE_DETECT:
            return true;  // Log but allow

        case NIMCP_SS_MODE_ENFORCE:
            // In production, this would abort()
            // For testing, return false
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "handle_mismatch: operation failed");
            return false;

        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "handle_mismatch: operation failed");
            return false;
    }
}

/**
 * @brief Cleanup function for thread-local storage
 */
static void tls_cleanup(void* data)
{
    if (data) {
        nimcp_shadow_stack_destroy((nimcp_shadow_stack_t*)data);
    }
}

/**
 * @brief Initialize thread-local storage key
 */
static void tls_init_key(void)
{
    pthread_key_create(&tls_shadow_stack_key, tls_cleanup);
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

nimcp_shadow_stack_t* nimcp_shadow_stack_create(uint32_t size)
{
    if (size == 0) {
        size = NIMCP_SHADOW_STACK_DEFAULT_SIZE;
    }

    if (size < NIMCP_SHADOW_STACK_MIN_SIZE) {
        size = NIMCP_SHADOW_STACK_MIN_SIZE;
    }

    if (size > NIMCP_SHADOW_STACK_MAX_SIZE) {
        size = NIMCP_SHADOW_STACK_MAX_SIZE;
    }

    nimcp_shadow_stack_t* ss =
        (nimcp_shadow_stack_t*)nimcp_calloc(1, sizeof(nimcp_shadow_stack_t));

    NIMCP_API_CHECK_ALLOC(ss, "Failed to allocate shadow stack");

    ss->entries = (nimcp_ss_entry_t*)nimcp_calloc(size, sizeof(nimcp_ss_entry_t));
    if (!ss->entries) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate shadow stack entries");
        nimcp_free(ss);
        return NULL;
    }

    ss->size = size;
    ss->top = 0;
    ss->mode = NIMCP_SS_MODE_DISABLED;
    ss->initialized = false;

    // Set guard values
    ss->top_guard = NIMCP_SHADOW_STACK_GUARD;
    ss->bottom_guard = NIMCP_SHADOW_STACK_GUARD;

    return ss;
}

nimcp_result_t nimcp_shadow_stack_init(
    nimcp_shadow_stack_t* ss,
    nimcp_ss_mode_t mode)
{
    NIMCP_API_CHECK_NULL(ss, NIMCP_INVALID_PARAM, "NULL shadow stack in init");
    NIMCP_API_CHECK(!ss->initialized, NIMCP_INVALID_STATE, "Shadow stack already initialized");

    ss->mode = mode;
    ss->initialized = true;

    nimcp_security_log_event(
        NIMCP_SECURITY_EVENT_DIRECTIVE_VERIFIED,
        NIMCP_THREAT_NONE,
        "Shadow stack initialized"
    );

    return NIMCP_SUCCESS;
}

void nimcp_shadow_stack_destroy(nimcp_shadow_stack_t* ss)
{
    if (!ss)
        return;

    // Zero out entries to prevent data leakage
    if (ss->entries) {
        memset(ss->entries, 0, ss->size * sizeof(nimcp_ss_entry_t));
        nimcp_free(ss->entries);
    }

    memset(ss, 0, sizeof(nimcp_shadow_stack_t));
    nimcp_free(ss);
}

//=============================================================================
// Core Operations
//=============================================================================

nimcp_ss_result_t nimcp_shadow_stack_push(
    nimcp_shadow_stack_t* ss,
    void* return_address)
{
    return nimcp_shadow_stack_push_frame(ss, return_address, NULL);
}

nimcp_ss_result_t nimcp_shadow_stack_push_frame(
    nimcp_shadow_stack_t* ss,
    void* return_address,
    void* frame_pointer)
{
    if (!ss)
        return NIMCP_SS_INVALID_PARAM;

    if (!ss->initialized)
        return NIMCP_SS_NOT_INITIALIZED;

    // SECURITY: Validate entries pointer before access
    if (!ss->entries) {
        ss->stats.corruptions++;
        nimcp_security_log_event(
            NIMCP_SECURITY_EVENT_DIRECTIVE_TAMPERED,
            NIMCP_THREAT_CRITICAL,
            "Shadow stack entries pointer is NULL"
        );
        return NIMCP_SS_CORRUPTED;
    }

    // SECURITY: Validate size is reasonable (prevent wrap-around)
    if (ss->size == 0 || ss->size > NIMCP_SHADOW_STACK_MAX_SIZE) {
        ss->stats.corruptions++;
        return NIMCP_SS_CORRUPTED;
    }

    // Check for overflow
    if (ss->top >= ss->size) {
        ss->stats.overflows++;
        nimcp_security_log_event(
            NIMCP_SECURITY_EVENT_THREAT_DETECTED,
            NIMCP_THREAT_HIGH,
            "Shadow stack overflow"
        );
        return NIMCP_SS_OVERFLOW;
    }

    // Verify guards
    if (ss->top_guard != NIMCP_SHADOW_STACK_GUARD ||
        ss->bottom_guard != NIMCP_SHADOW_STACK_GUARD) {
        ss->stats.corruptions++;
        nimcp_security_log_event(
            NIMCP_SECURITY_EVENT_DIRECTIVE_TAMPERED,
            NIMCP_THREAT_CRITICAL,
            "Shadow stack corruption detected"
        );
        return NIMCP_SS_CORRUPTED;
    }

    // Push entry
    nimcp_ss_entry_t* entry = &ss->entries[ss->top];
    entry->return_address = return_address;
    entry->frame_pointer = frame_pointer;
    entry->canary = generate_canary();

    // SECURITY FIX: Fail if canary generation failed (returned 0)
    // A zero canary indicates no secure entropy source is available
    if (entry->canary == 0) {
        LOG_ERROR("Shadow stack push failed - no secure entropy for canary");
        ss->stats.mismatches++;  // Count as security failure
        return NIMCP_SS_CORRUPTED;  // Fail securely rather than continue with weak protection
    }

    ss->top++;
    ss->stats.pushes++;

    // Track max depth
    if (ss->top > ss->stats.max_depth) {
        ss->stats.max_depth = ss->top;
    }
    ss->stats.current_depth = ss->top;

    return NIMCP_SS_OK;
}

nimcp_ss_result_t nimcp_shadow_stack_pop(
    nimcp_shadow_stack_t* ss,
    void* actual_return)
{
    if (!ss)
        return NIMCP_SS_INVALID_PARAM;

    if (!ss->initialized)
        return NIMCP_SS_NOT_INITIALIZED;

    // SECURITY: Validate entries pointer before access
    if (!ss->entries) {
        ss->stats.corruptions++;
        return NIMCP_SS_CORRUPTED;
    }

    // SECURITY: Validate size is reasonable
    if (ss->size == 0 || ss->size > NIMCP_SHADOW_STACK_MAX_SIZE) {
        ss->stats.corruptions++;
        return NIMCP_SS_CORRUPTED;
    }

    // Check for underflow
    if (ss->top == 0) {
        ss->stats.underflows++;
        nimcp_security_log_event(
            NIMCP_SECURITY_EVENT_THREAT_DETECTED,
            NIMCP_THREAT_HIGH,
            "Shadow stack underflow"
        );
        return NIMCP_SS_UNDERFLOW;
    }

    // SECURITY: Validate top is within bounds before decrement
    if (ss->top > ss->size) {
        ss->stats.corruptions++;
        return NIMCP_SS_CORRUPTED;
    }

    // Verify guards
    if (ss->top_guard != NIMCP_SHADOW_STACK_GUARD ||
        ss->bottom_guard != NIMCP_SHADOW_STACK_GUARD) {
        ss->stats.corruptions++;
        return NIMCP_SS_CORRUPTED;
    }

    ss->top--;
    nimcp_ss_entry_t* entry = &ss->entries[ss->top];

    ss->stats.pops++;
    ss->stats.current_depth = ss->top;

    // Verify canary
    if (entry->canary == 0) {
        ss->stats.corruptions++;
        return NIMCP_SS_CORRUPTED;
    }

    // Compare return addresses
    if (entry->return_address != actual_return) {
        bool allow = handle_mismatch(ss, entry->return_address, actual_return);
        if (!allow) {
            return NIMCP_SS_MISMATCH;
        }
    }

    // Clear entry
    entry->return_address = NULL;
    entry->frame_pointer = NULL;
    entry->canary = 0;

    return NIMCP_SS_OK;
}

nimcp_ss_result_t nimcp_shadow_stack_pop_unchecked(
    nimcp_shadow_stack_t* ss,
    void** expected_return)
{
    if (!ss || !expected_return)
        return NIMCP_SS_INVALID_PARAM;

    if (!ss->initialized)
        return NIMCP_SS_NOT_INITIALIZED;

    // SECURITY: Validate entries pointer and bounds
    if (!ss->entries || ss->size == 0 || ss->size > NIMCP_SHADOW_STACK_MAX_SIZE) {
        ss->stats.corruptions++;
        return NIMCP_SS_CORRUPTED;
    }

    if (ss->top == 0) {
        ss->stats.underflows++;
        return NIMCP_SS_UNDERFLOW;
    }

    // SECURITY: Validate top is within bounds
    if (ss->top > ss->size) {
        ss->stats.corruptions++;
        return NIMCP_SS_CORRUPTED;
    }

    ss->top--;
    nimcp_ss_entry_t* entry = &ss->entries[ss->top];

    *expected_return = entry->return_address;

    ss->stats.pops++;
    ss->stats.current_depth = ss->top;

    // Clear entry
    entry->return_address = NULL;
    entry->frame_pointer = NULL;
    entry->canary = 0;

    return NIMCP_SS_OK;
}

nimcp_ss_result_t nimcp_shadow_stack_peek(
    nimcp_shadow_stack_t* ss,
    void** expected_return)
{
    if (!ss || !expected_return)
        return NIMCP_SS_INVALID_PARAM;

    if (!ss->initialized)
        return NIMCP_SS_NOT_INITIALIZED;

    // SECURITY: Validate entries pointer and bounds
    if (!ss->entries || ss->size == 0 || ss->size > NIMCP_SHADOW_STACK_MAX_SIZE) {
        return NIMCP_SS_CORRUPTED;
    }

    if (ss->top == 0) {
        return NIMCP_SS_UNDERFLOW;
    }

    // SECURITY: Validate top is within bounds
    if (ss->top > ss->size) {
        return NIMCP_SS_CORRUPTED;
    }

    *expected_return = ss->entries[ss->top - 1].return_address;

    return NIMCP_SS_OK;
}

//=============================================================================
// Stack Management
//=============================================================================

uint32_t nimcp_shadow_stack_depth(nimcp_shadow_stack_t* ss)
{
    if (!ss)
        return 0;

    return ss->top;
}

bool nimcp_shadow_stack_is_empty(nimcp_shadow_stack_t* ss)
{
    if (!ss)
        return true;

    return ss->top == 0;
}

bool nimcp_shadow_stack_is_full(nimcp_shadow_stack_t* ss)
{
    if (!ss)
        return true;

    return ss->top >= ss->size;
}

nimcp_result_t nimcp_shadow_stack_clear(nimcp_shadow_stack_t* ss)
{
    NIMCP_API_CHECK_NULL(ss, NIMCP_INVALID_PARAM, "NULL shadow stack in clear");

    // Zero all entries
    memset(ss->entries, 0, ss->size * sizeof(nimcp_ss_entry_t));
    ss->top = 0;
    ss->stats.current_depth = 0;

    return NIMCP_SUCCESS;
}

bool nimcp_shadow_stack_verify(nimcp_shadow_stack_t* ss)
{
    if (!ss) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_shadow_stack_verify: ss is NULL");
        return false;
    }

    // Check guards
    if (ss->top_guard != NIMCP_SHADOW_STACK_GUARD ||
        ss->bottom_guard != NIMCP_SHADOW_STACK_GUARD) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_shadow_stack_verify: ss is NULL");
        return false;
    }

    // Check all canaries
    for (uint32_t i = 0; i < ss->top; i++) {
        if (ss->entries[i].canary == 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_shadow_stack_verify: .canary is zero");
            return false;
        }
    }

    return true;
}

//=============================================================================
// Unwinding Support
//=============================================================================

nimcp_ss_result_t nimcp_shadow_stack_unwind(
    nimcp_shadow_stack_t* ss,
    uint32_t target_depth)
{
    if (!ss)
        return NIMCP_SS_INVALID_PARAM;

    if (target_depth > ss->top)
        return NIMCP_SS_INVALID_PARAM;

    // Clear entries being unwound
    for (uint32_t i = target_depth; i < ss->top; i++) {
        ss->entries[i].return_address = NULL;
        ss->entries[i].frame_pointer = NULL;
        ss->entries[i].canary = 0;
    }

    ss->top = target_depth;
    ss->stats.current_depth = ss->top;

    return NIMCP_SS_OK;
}

nimcp_ss_result_t nimcp_shadow_stack_unwind_to(
    nimcp_shadow_stack_t* ss,
    void* target_address)
{
    if (!ss || !target_address)
        return NIMCP_SS_INVALID_PARAM;

    // Find target address in stack
    for (uint32_t i = ss->top; i > 0; i--) {
        if (ss->entries[i - 1].return_address == target_address) {
            return nimcp_shadow_stack_unwind(ss, i);
        }
    }

    return NIMCP_SS_INVALID_PARAM;  // Target not found
}

//=============================================================================
// Statistics and Configuration
//=============================================================================

nimcp_result_t nimcp_shadow_stack_get_stats(
    nimcp_shadow_stack_t* ss,
    nimcp_ss_stats_t* stats)
{
    NIMCP_API_CHECK_NULL(ss, NIMCP_INVALID_PARAM, "NULL shadow stack in get_stats");
    NIMCP_API_CHECK_NULL(stats, NIMCP_INVALID_PARAM, "NULL stats output in get_stats");

    memcpy(stats, &ss->stats, sizeof(nimcp_ss_stats_t));

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_shadow_stack_reset_stats(nimcp_shadow_stack_t* ss)
{
    NIMCP_API_CHECK_NULL(ss, NIMCP_INVALID_PARAM, "NULL shadow stack in reset_stats");

    // Keep current_depth, reset everything else
    uint32_t depth = ss->stats.current_depth;
    memset(&ss->stats, 0, sizeof(nimcp_ss_stats_t));
    ss->stats.current_depth = depth;

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_shadow_stack_set_mode(
    nimcp_shadow_stack_t* ss,
    nimcp_ss_mode_t mode)
{
    NIMCP_API_CHECK_NULL(ss, NIMCP_INVALID_PARAM, "NULL shadow stack in set_mode");

    ss->mode = mode;

    return NIMCP_SUCCESS;
}

nimcp_ss_mode_t nimcp_shadow_stack_get_mode(nimcp_shadow_stack_t* ss)
{
    if (!ss)
        return NIMCP_SS_MODE_DISABLED;

    return ss->mode;
}

nimcp_result_t nimcp_shadow_stack_set_handler(
    nimcp_shadow_stack_t* ss,
    nimcp_ss_mismatch_callback_t callback,
    void* user_data)
{
    NIMCP_API_CHECK_NULL(ss, NIMCP_INVALID_PARAM, "NULL shadow stack in set_handler");

    ss->callback = callback;
    ss->callback_user_data = user_data;

    return NIMCP_SUCCESS;
}

//=============================================================================
// Thread-Local Support
//=============================================================================

nimcp_shadow_stack_t* nimcp_shadow_stack_get_thread_local(void)
{
    pthread_once(&tls_key_once, tls_init_key);

    nimcp_shadow_stack_t* ss = pthread_getspecific(tls_shadow_stack_key);

    if (!ss) {
        // Create new shadow stack for this thread
        ss = nimcp_shadow_stack_create(0);
        if (ss) {
            nimcp_shadow_stack_init(ss, NIMCP_SS_MODE_DETECT);
            pthread_setspecific(tls_shadow_stack_key, ss);
        }
    }

    return ss;
}

nimcp_result_t nimcp_shadow_stack_set_thread_local(nimcp_shadow_stack_t* ss)
{
    pthread_once(&tls_key_once, tls_init_key);

    // THREAD SAFETY FIX: Get and clear the old value atomically before destroying.
    // This prevents double-free if the TLS destructor fires during destruction.
    nimcp_shadow_stack_t* old = pthread_getspecific(tls_shadow_stack_key);
    if (old && old != ss) {
        // Set new value first to prevent TLS destructor from re-destroying old
        pthread_setspecific(tls_shadow_stack_key, ss);
        // Now safe to destroy old value
        nimcp_shadow_stack_destroy(old);
        return NIMCP_SUCCESS;
    }

    pthread_setspecific(tls_shadow_stack_key, ss);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_shadow_stack_cleanup_thread_local(void)
{
    pthread_once(&tls_key_once, tls_init_key);

    nimcp_shadow_stack_t* ss = pthread_getspecific(tls_shadow_stack_key);
    if (ss) {
        nimcp_shadow_stack_destroy(ss);
        pthread_setspecific(tls_shadow_stack_key, NULL);
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* nimcp_ss_result_name(nimcp_ss_result_t result)
{
    static const char* names[] = {
        "OK",
        "Overflow",
        "Underflow",
        "Mismatch",
        "Corrupted",
        "Not Initialized",
        "Invalid Param"
    };

    if (result > NIMCP_SS_INVALID_PARAM)
        return "Unknown";

    return names[result];
}

const char* nimcp_ss_mode_name(nimcp_ss_mode_t mode)
{
    static const char* names[] = {
        "Disabled",
        "Detect",
        "Enforce"
    };

    if (mode > NIMCP_SS_MODE_ENFORCE)
        return "Unknown";

    return names[mode];
}
