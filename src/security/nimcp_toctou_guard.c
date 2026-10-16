/**
 * @file nimcp_toctou_guard.c
 * @brief TOCTOU guard implementation with atomic validate+execute
 *
 * WHAT: Implementation of TOCTOU protection guards
 * WHY:  Prevent time-of-check-time-of-use race conditions
 * HOW:  Token-based locking with single-use guarantees
 *
 * @author NIMCP Development Team
 * @date 2025-12-07
 */

#include "security/nimcp_toctou_guard.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_wiring_helpers.h"

#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/validation/nimcp_common.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_atomic.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "utils/platform/nimcp_platform_once.h"

#include <stddef.h>  /* for NULL */
#include <stdatomic.h>
//=============================================================================
// Health Agent Integration (Phase 8: System-Wide Health Integration)
//=============================================================================
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;
extern void nimcp_health_agent_heartbeat_ex(nimcp_health_agent_t* agent,
                                             const char* operation,
                                             float progress);

/** Global health agent for toctou_guard module - P2 fix: Use atomic for thread safety */
static _Atomic(nimcp_health_agent_t*) g_toctou_guard_health_agent = NULL;

/**
 * @brief Set health agent for toctou_guard heartbeats (thread-safe)
 * @param agent Health agent (can be NULL to disable)
 */
void toctou_guard_set_health_agent(nimcp_health_agent_t* agent) {
    atomic_store(&g_toctou_guard_health_agent, agent);
}

/** @brief Send heartbeat from toctou_guard module (thread-safe) */
static inline void toctou_guard_heartbeat(const char* operation, float progress) {
    /* P2 fix: Atomic load to prevent data race */
    nimcp_health_agent_t* agent = atomic_load(&g_toctou_guard_health_agent);
    if (agent) {
        nimcp_health_agent_heartbeat_ex(agent, operation, progress);
    }
}


//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Token state enumeration
 */
typedef enum {
    TOKEN_STATE_INVALID = 0,
    TOKEN_STATE_ACTIVE,
    TOKEN_STATE_USED,
    TOKEN_STATE_EXPIRED,
    TOKEN_STATE_CANCELLED
} nimcp_token_state_t;

/**
 * @brief Internal token structure
 */
struct nimcp_toctou_token_impl {
    uint32_t magic;                     /**< Magic number for validation */
    nimcp_token_state_t state;          /**< Current token state */
    const void* resource;               /**< Validated resource pointer */
    size_t resource_size;               /**< Size of resource */
    uint64_t creation_time_ms;          /**< Creation timestamp */
    uint64_t expiration_time_ms;        /**< Expiration timestamp */
    nimcp_toctou_guard_t parent_guard;  /**< Parent guard reference */
    nimcp_platform_mutex_t token_lock;  /**< Per-token lock */
    uint32_t token_id;                  /**< Unique token ID */
};

/**
 * @brief Internal guard structure
 */
struct nimcp_toctou_guard_impl {
    uint32_t magic;                         /**< Magic number for validation */
    nimcp_toctou_config_t config;           /**< Configuration */
    nimcp_platform_mutex_t guard_lock;      /**< Main guard lock */
    nimcp_toctou_token_t* token_pool;       /**< Pool of token slots */
    uint32_t next_token_id;                 /**< Next token ID counter */
    nimcp_toctou_stats_t stats;             /**< Statistics */
    bio_module_context_t bio_context;       /**< Bio-async module context */
    bool bio_registered;                    /**< Bio-async registration flag */
};

//=============================================================================
// Global State
//=============================================================================

/** @brief Global guard count for statistics */
static uint64_t g_total_guards_created = 0;

/** @brief Global mutex for guard creation */
static nimcp_platform_mutex_t g_guard_creation_lock;

/** @brief nimcp_platform_once control for thread-safe initialization */
static nimcp_platform_once_t g_toctou_init_once = NIMCP_PLATFORM_ONCE_INIT;

/** P2-SEC-2: Atomic flag to prevent race on cleanup check (forward declaration) */
static _Atomic bool g_toctou_module_cleaned_up = false;

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * NIMCP_MS_PER_SEC + (uint64_t)ts.tv_nsec / NIMCP_NS_PER_MS;
}

/**
 * @brief Internal initialization routine called by nimcp_platform_once
 *
 * THREAD-SAFETY: Guaranteed to execute exactly once by nimcp_platform_once
 */
static void toctou_module_init_internal(void) {
    nimcp_platform_mutex_init(&g_guard_creation_lock, false);

    /* Health agent uses _Atomic type, no init needed (statically initialized to NULL) */

    /* P2-SEC-3: Reset cleanup flag so re-init after cleanup works */
    atomic_store(&g_toctou_module_cleaned_up, false);

    LOG_MODULE_INFO("toctou_guard", "TOCTOU guard module initialized");
}

/**
 * @brief Initialize module on first use (thread-safe via nimcp_platform_once)
 */
static void toctou_module_init(void) {
    nimcp_platform_once(&g_toctou_init_once, toctou_module_init_internal);
}

/**
 * @brief Validate guard handle
 */
static inline bool is_valid_guard(nimcp_toctou_guard_t guard) {
    return guard != NULL && guard->magic == NIMCP_TOCTOU_GUARD_MAGIC;
}

/**
 * @brief Validate token handle
 */
static inline bool is_valid_token(nimcp_toctou_token_t token) {
    return token != NULL && token->magic == NIMCP_TOCTOU_TOKEN_MAGIC;
}

/**
 * @brief Check if token is expired
 */
static bool is_token_expired(nimcp_toctou_token_t token) {
    if (!is_valid_token(token)) {
        return true;
    }

    uint64_t now = get_time_ms();
    return now >= token->expiration_time_ms;
}

/**
 * @brief Find available token slot
 */
static nimcp_toctou_token_t find_free_token_slot(nimcp_toctou_guard_t guard) {
    for (uint32_t i = 0; i < guard->config.max_concurrent_tokens; i++) {
        nimcp_toctou_token_t token = guard->token_pool[i];
        if (token == NULL) {
            return NULL; // Slot available for new allocation (not an error)
        }

        nimcp_platform_mutex_lock(&token->token_lock);
        if (token->state == TOKEN_STATE_INVALID ||
            token->state == TOKEN_STATE_USED ||
            token->state == TOKEN_STATE_EXPIRED ||
            token->state == TOKEN_STATE_CANCELLED) {
            // This slot can be reused
            nimcp_platform_mutex_unlock(&token->token_lock);
            return token;
        }
        nimcp_platform_mutex_unlock(&token->token_lock);
    }

    /* P1-43: No free slot found is a normal condition, not an error */
    return NULL;
}

/**
 * @brief Update guard statistics (assumes guard lock held)
 */
static void update_stats_locked(nimcp_toctou_guard_t guard,
                                 uint64_t wait_time_ms) {
    if (!guard->config.enable_statistics) {
        return;
    }

    guard->stats.total_wait_time_ms += wait_time_ms;
    if (wait_time_ms > guard->stats.max_wait_time_ms) {
        guard->stats.max_wait_time_ms = wait_time_ms;
    }

    uint64_t total_ops = guard->stats.tokens_created;
    if (total_ops > 0) {
        guard->stats.avg_wait_time_ms =
            (float)guard->stats.total_wait_time_ms / (float)total_ops;
    }
}

//=============================================================================
// Guard Lifecycle Implementation
//=============================================================================

nimcp_toctou_config_t nimcp_toctou_default_config(void) {
    nimcp_toctou_config_t config = {
        .max_concurrent_tokens = NIMCP_TOCTOU_DEFAULT_MAX_TOKENS,
        .token_timeout_ms = NIMCP_TOCTOU_DEFAULT_TIMEOUT_MS,
        .enable_statistics = true,
        .enable_logging = true,
        .contention_backoff_ms = 10,
        .strict_mode = false
    };
    return config;
}

nimcp_toctou_guard_t nimcp_toctou_guard_create(
    const nimcp_toctou_config_t* config)
{
    // Initialize module if needed
    toctou_module_init();

    // Use default config if NULL
    nimcp_toctou_config_t actual_config;
    if (config == NULL) {
        actual_config = nimcp_toctou_default_config();
    } else {
        actual_config = *config;
    }

    // Validate configuration
    if (actual_config.max_concurrent_tokens == 0) {
        LOG_ERROR("Invalid max_concurrent_tokens (0)");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_toctou_guard_create: actual_config.max_concurrent_tokens is zero");
        return NULL;
    }

    // Allocate guard structure
    nimcp_toctou_guard_t guard = (nimcp_toctou_guard_t)nimcp_calloc(
        1, sizeof(struct nimcp_toctou_guard_impl));
    if (!guard) {
        LOG_ERROR("Failed to allocate TOCTOU guard");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "guard is NULL");

        return NULL;
    }

    // Initialize guard
    guard->magic = NIMCP_TOCTOU_GUARD_MAGIC;
    guard->config = actual_config;
    guard->next_token_id = 1;
    guard->bio_registered = false;
    memset(&guard->stats, 0, sizeof(nimcp_toctou_stats_t));

    // Initialize guard lock
    if (nimcp_platform_mutex_init(&guard->guard_lock, false) != 0) {
        LOG_ERROR("Failed to initialize guard lock");
        nimcp_free(guard);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_toctou_guard_create: validation failed");
        return NULL;
    }

    // Allocate token pool
    guard->token_pool = (nimcp_toctou_token_t*)nimcp_calloc(
        actual_config.max_concurrent_tokens, sizeof(nimcp_toctou_token_t));
    if (!guard->token_pool) {
        LOG_ERROR("Failed to allocate token pool");
        nimcp_platform_mutex_destroy(&guard->guard_lock);
        nimcp_free(guard);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_toctou_guard_create: guard->token_pool is NULL");
        return NULL;
    }

    // Update global statistics
    nimcp_platform_mutex_lock(&g_guard_creation_lock);
    g_total_guards_created++;
    guard->stats.guards_created = g_total_guards_created;
    nimcp_platform_mutex_unlock(&g_guard_creation_lock);

    if (actual_config.enable_logging) {
        LOG_MODULE_INFO("toctou_guard",
            "Created TOCTOU guard (max_tokens=%u, timeout=%ums)",
            actual_config.max_concurrent_tokens,
            actual_config.token_timeout_ms);
    }

    return guard;
}

void nimcp_toctou_guard_destroy(nimcp_toctou_guard_t guard) {
    if (!is_valid_guard(guard)) {
        return;
    }

    LOG_MODULE_DEBUG("toctou_guard", "Destroying TOCTOU guard");

    // Acquire guard lock
    nimcp_platform_mutex_lock(&guard->guard_lock);

    // Clean up tokens
    if (guard->token_pool) {
        for (uint32_t i = 0; i < guard->config.max_concurrent_tokens; i++) {
            nimcp_toctou_token_t token = guard->token_pool[i];
            if (token) {
                nimcp_platform_mutex_destroy(&token->token_lock);
                token->magic = 0; // Invalidate
                nimcp_free(token);
            }
        }
        nimcp_free(guard->token_pool);
    }

    // Unregister from bio-async
    if (guard->bio_registered && guard->bio_context) {
        bio_router_unregister_module(guard->bio_context);
    }

    // Invalidate and cleanup
    guard->magic = 0;
    nimcp_platform_mutex_unlock(&guard->guard_lock);
    nimcp_platform_mutex_destroy(&guard->guard_lock);

    nimcp_free(guard);
}

//=============================================================================
// Core TOCTOU Protection Implementation
//=============================================================================

nimcp_toctou_token_t nimcp_toctou_validate(
    nimcp_toctou_guard_t guard,
    const void* resource,
    size_t size)
{
    return nimcp_toctou_validate_custom(guard, resource, size, NULL, NULL);
}

nimcp_toctou_token_t nimcp_toctou_validate_custom(
    nimcp_toctou_guard_t guard,
    const void* resource,
    size_t size,
    nimcp_toctou_validator_fn validator,
    void* validator_ctx)
{
    // Validate inputs
    if (!is_valid_guard(guard)) {
        LOG_ERROR("Invalid guard handle");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_toctou_validate_custom: is_valid_guard is NULL");
        return NULL;
    }

    if (resource == NULL || size == 0) {
        LOG_MODULE_WARN("toctou_guard",
            "Validation failed: NULL resource or zero size");
        nimcp_platform_mutex_lock(&guard->guard_lock);
        guard->stats.validation_failures++;
        nimcp_platform_mutex_unlock(&guard->guard_lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_toctou_validate_custom: size is zero");
        return NULL;
    }

    uint64_t start_time = get_time_ms();

    // Acquire guard lock
    nimcp_platform_mutex_lock(&guard->guard_lock);

    // Find or allocate token slot
    nimcp_toctou_token_t token = find_free_token_slot(guard);

    if (token == NULL) {
        // Need to allocate new token
        for (uint32_t i = 0; i < guard->config.max_concurrent_tokens; i++) {
            if (guard->token_pool[i] == NULL) {
                token = (nimcp_toctou_token_t)nimcp_calloc(
                    1, sizeof(struct nimcp_toctou_token_impl));
                if (token) {
                    nimcp_platform_mutex_init(&token->token_lock, false);
                    guard->token_pool[i] = token;
                }
                break;
            }
        }
    }

    if (token == NULL) {
        // All slots full - normal contention, not an error
        LOG_MODULE_WARN("toctou_guard",
            "Maximum concurrent tokens reached (%u)",
            guard->config.max_concurrent_tokens);
        guard->stats.contention_events++;
        nimcp_platform_mutex_unlock(&guard->guard_lock);
        /* P3: Removed false-positive NIMCP_THROW_TO_IMMUNE.
         * All token slots being in use is a normal contention condition,
         * not an immune-level error. The caller checks the NULL return. */
        return NULL;
    }

    // Lock the token
    nimcp_platform_mutex_lock(&token->token_lock);

    // Run custom validator if provided
    if (validator != NULL) {
        if (!validator(resource, size, validator_ctx)) {
            LOG_MODULE_WARN("toctou_guard", "Custom validation failed");
            guard->stats.validation_failures++;
            token->state = TOKEN_STATE_INVALID;
            nimcp_platform_mutex_unlock(&token->token_lock);
            nimcp_platform_mutex_unlock(&guard->guard_lock);
            /* P3: Removed false-positive NIMCP_THROW_TO_IMMUNE.
             * Validation rejection is a normal result path, not an error.
             * The caller checks the NULL return and stats track the failure. */
            return NULL;
        }
    }

    // Initialize token
    token->magic = NIMCP_TOCTOU_TOKEN_MAGIC;
    token->state = TOKEN_STATE_ACTIVE;
    token->resource = resource;
    token->resource_size = size;
    token->creation_time_ms = get_time_ms();
    token->expiration_time_ms =
        token->creation_time_ms + guard->config.token_timeout_ms;
    token->parent_guard = guard;
    token->token_id = guard->next_token_id++;

    // Update statistics
    guard->stats.tokens_created++;
    guard->stats.active_tokens++;

    uint64_t wait_time = get_time_ms() - start_time;
    update_stats_locked(guard, wait_time);

    nimcp_platform_mutex_unlock(&token->token_lock);
    nimcp_platform_mutex_unlock(&guard->guard_lock);

    if (guard->config.enable_logging) {
        LOG_MODULE_DEBUG("toctou_guard",
            "Created token %u for resource %p (size=%zu)",
            token->token_id, resource, size);
    }

    return token;
}

nimcp_error_t nimcp_toctou_execute(
    nimcp_toctou_token_t token,
    nimcp_toctou_action_fn action,
    void* context)
{
    // Validate inputs
    if (!is_valid_token(token)) {
        LOG_ERROR("Invalid token handle");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (action == NULL) {
        LOG_ERROR("Action function is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    // Lock token
    nimcp_platform_mutex_lock(&token->token_lock);

    nimcp_toctou_guard_t guard = token->parent_guard;

    // Check token state
    if (token->state != TOKEN_STATE_ACTIVE) {
        LOG_MODULE_WARN("toctou_guard",
            "Token %u is not active (state=%d)", token->token_id, token->state);
        nimcp_platform_mutex_unlock(&token->token_lock);

        /* P1-SEC-1: Release token_lock BEFORE acquiring guard_lock
         * to maintain consistent lock ordering (guard_lock -> token_lock) */
        if (guard && is_valid_guard(guard)) {
            nimcp_platform_mutex_lock(&guard->guard_lock);
            guard->stats.execution_failures++;
            nimcp_platform_mutex_unlock(&guard->guard_lock);
        }

        return NIMCP_ERROR_INVALID_STATE;
    }

    // Check expiration
    if (is_token_expired(token)) {
        LOG_MODULE_WARN("toctou_guard", "Token %u expired", token->token_id);
        token->state = TOKEN_STATE_EXPIRED;
        nimcp_platform_mutex_unlock(&token->token_lock);

        /* P1-SEC-1: Release token_lock BEFORE acquiring guard_lock */
        if (guard && is_valid_guard(guard)) {
            nimcp_platform_mutex_lock(&guard->guard_lock);
            guard->stats.tokens_expired++;
            guard->stats.active_tokens--;
            guard->stats.execution_failures++;
            nimcp_platform_mutex_unlock(&guard->guard_lock);
        }

        return NIMCP_ERROR_TIMEOUT;
    }

    // Execute action with token lock held
    if (guard && guard->config.enable_logging) {
        LOG_MODULE_DEBUG("toctou_guard",
            "Executing action for token %u", token->token_id);
    }

    nimcp_error_t result = action(token->resource, token->resource_size, context);

    // Mark token as used
    token->state = TOKEN_STATE_USED;

    /* P1-SEC-1: Release token_lock BEFORE acquiring guard_lock
     * to prevent ABBA deadlock. validate_custom() acquires guard_lock
     * then token_lock, so execute() must never hold token_lock while
     * acquiring guard_lock. */
    nimcp_platform_mutex_unlock(&token->token_lock);

    // Update statistics (guard_lock only, no token_lock held)
    if (guard && is_valid_guard(guard)) {
        nimcp_platform_mutex_lock(&guard->guard_lock);
        guard->stats.tokens_used++;
        guard->stats.active_tokens--;
        if (result != NIMCP_SUCCESS) {
            guard->stats.execution_failures++;
        }
        nimcp_platform_mutex_unlock(&guard->guard_lock);
    }

    return result;
}

nimcp_error_t nimcp_toctou_cancel(nimcp_toctou_token_t token) {
    if (!is_valid_token(token)) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(&token->token_lock);

    if (token->state == TOKEN_STATE_ACTIVE) {
        token->state = TOKEN_STATE_CANCELLED;

        nimcp_toctou_guard_t guard = token->parent_guard;
        if (guard && is_valid_guard(guard)) {
            nimcp_platform_mutex_lock(&guard->guard_lock);
            guard->stats.tokens_cancelled++;
            guard->stats.active_tokens--;
            nimcp_platform_mutex_unlock(&guard->guard_lock);

            if (guard->config.enable_logging) {
                LOG_MODULE_DEBUG("toctou_guard",
                    "Token %u cancelled", token->token_id);
            }
        }
    }

    nimcp_platform_mutex_unlock(&token->token_lock);
    return NIMCP_SUCCESS;
}

bool nimcp_toctou_token_is_valid(nimcp_toctou_token_t token) {
    /* P2-SEC-1: Removed false-positive NIMCP_THROW_TO_IMMUNE.
     * This is a query function - returning false is sufficient for invalid tokens. */
    if (!is_valid_token(token)) {
        return false;
    }

    nimcp_platform_mutex_lock(&token->token_lock);
    bool valid = (token->state == TOKEN_STATE_ACTIVE) && !is_token_expired(token);
    nimcp_platform_mutex_unlock(&token->token_lock);

    return valid;
}

uint32_t nimcp_toctou_token_time_remaining(nimcp_toctou_token_t token) {
    if (!is_valid_token(token)) {
        return 0;
    }

    nimcp_platform_mutex_lock(&token->token_lock);

    if (token->state != TOKEN_STATE_ACTIVE) {
        nimcp_platform_mutex_unlock(&token->token_lock);
        return 0;
    }

    uint64_t now = get_time_ms();
    if (now >= token->expiration_time_ms) {
        nimcp_platform_mutex_unlock(&token->token_lock);
        return 0;
    }

    uint32_t remaining = (uint32_t)(token->expiration_time_ms - now);
    nimcp_platform_mutex_unlock(&token->token_lock);

    return remaining;
}

//=============================================================================
// Statistics Implementation
//=============================================================================

nimcp_error_t nimcp_toctou_get_stats(
    nimcp_toctou_guard_t guard,
    nimcp_toctou_stats_t* stats)
{
    if (!is_valid_guard(guard) || stats == NULL) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(&guard->guard_lock);
    *stats = guard->stats;
    nimcp_platform_mutex_unlock(&guard->guard_lock);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_toctou_reset_stats(nimcp_toctou_guard_t guard) {
    if (!is_valid_guard(guard)) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(&guard->guard_lock);

    uint64_t guards_created = guard->stats.guards_created;
    memset(&guard->stats, 0, sizeof(nimcp_toctou_stats_t));
    guard->stats.guards_created = guards_created;

    nimcp_platform_mutex_unlock(&guard->guard_lock);

    LOG_MODULE_INFO("toctou_guard", "Statistics reset");
    return NIMCP_SUCCESS;
}

uint32_t nimcp_toctou_get_active_count(nimcp_toctou_guard_t guard) {
    if (!is_valid_guard(guard)) {
        return 0;
    }

    nimcp_platform_mutex_lock(&guard->guard_lock);
    uint32_t count = guard->stats.active_tokens;
    nimcp_platform_mutex_unlock(&guard->guard_lock);

    return count;
}

//=============================================================================
// Bio-Async Integration Implementation
//=============================================================================

/**
 * @brief Wiring callback for KG-driven handler registration
 *
 * Called by the orchestrator with discovered message types from the knowledge graph.
 * Registers handlers based on message types discovered at runtime.
 *
 * @param ctx Bio-async module context
 * @param message_types Array of discovered message types
 * @param message_count Number of message types
 * @param user_data User-provided context
 * @return 0 on success, -1 on error
 */
static int toctou_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
);

/**
 * @brief Bio-async message handler for TOCTOU guard
 */
static nimcp_error_t toctou_bio_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    nimcp_toctou_guard_t guard = (nimcp_toctou_guard_t)user_data;

    if (!is_valid_guard(guard) || msg == NULL) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    // Handle health check
    if (header->type == BIO_MSG_HEALTH_CHECK) {
        bio_msg_health_response_t response = {0};
        bio_msg_init_header(&response.header, BIO_MSG_HEALTH_RESPONSE,
            BIO_MODULE_SECURITY, header->source_module, sizeof(response));

        response.healthy = true;
        response.active_threads = guard->stats.active_tokens;
        response.pending_messages = 0;

        if (response_promise) {
            nimcp_bio_promise_complete(response_promise, &response);
        }

        return NIMCP_SUCCESS;
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Wiring callback implementation for KG-driven handler registration
 */
static int toctou_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    (void)user_data;

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_HEALTH_CHECK:
                bio_router_register_handler(ctx, message_types[i], toctou_bio_handler);
                registered++;
                break;
            default:
                LOG_MODULE_DEBUG("toctou_guard", "Unknown message type %d in wiring callback", message_types[i]);
                break;
        }
    }

    LOG_MODULE_INFO("toctou_guard", "KG-driven wiring callback registered %d handlers", registered);
    return (registered > 0) ? 0 : -1;
}

nimcp_error_t nimcp_toctou_register_bio_async(nimcp_toctou_guard_t guard) {
    if (!is_valid_guard(guard)) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (guard->bio_registered) {
        LOG_MODULE_WARN("toctou_guard", "Already registered with bio-async");
        return NIMCP_SUCCESS;
    }

    // Register module
    bio_module_info_t info = {
        .module_id = BIO_MODULE_SECURITY,
        .module_name = "toctou_guard",
        .inbox_capacity = 64,
        .user_data = guard
    };

    guard->bio_context = bio_router_register_module(&info);
    if (!guard->bio_context) {
        LOG_ERROR("Failed to register TOCTOU guard with bio-router");
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    // Try KG-driven wiring callback registration first
    nimcp_error_t result = bio_router_register_wiring_callback(
        BIO_MODULE_SECURITY,
        (void*)toctou_wiring_handler_callback,
        guard
    );

    if (result == NIMCP_SUCCESS) {
        LOG_MODULE_INFO("toctou_guard", "KG-driven wiring callback registered successfully");
    } else {
        // Fallback to legacy handler registration
        LOG_MODULE_INFO("toctou_guard", "Falling back to legacy handler registration");

        LEGACY_HANDLER_REGISTRATION(
            bio_router_register_handler(guard->bio_context,
                BIO_MSG_HEALTH_CHECK, toctou_bio_handler)
        );
    }

    guard->bio_registered = true;

    LOG_MODULE_INFO("toctou_guard", "Registered with bio-async router");
    return NIMCP_SUCCESS;
}

uint32_t nimcp_toctou_process_inbox(
    nimcp_toctou_guard_t guard,
    uint32_t max_messages)
{
    if (!is_valid_guard(guard) || !guard->bio_registered) {
        return 0;
    }

    return bio_router_process_inbox(guard->bio_context, max_messages);
}

//=============================================================================
// Module Cleanup Implementation
//=============================================================================

/**
 * @brief Flag to track if module cleanup has been performed
 * WHY: Prevent double-cleanup
 */
/**
 * @brief Cleanup TOCTOU guard module resources
 *
 * WHAT: Destroy the global guard creation lock
 * WHY:  Clean resource management on module unload
 * HOW:  Called at program exit or explicit module cleanup
 *
 * THREAD-SAFETY: Should only be called when no other threads are using the module
 *
 * NOTE: This function is idempotent - safe to call multiple times
 */
void nimcp_toctou_module_cleanup(void) {
    /* P2-SEC-2: Atomic check to prevent double cleanup race */
    if (atomic_exchange(&g_toctou_module_cleaned_up, true)) {
        return;
    }

    /* Only destroy if the once-init actually ran (module was initialized) */
    /* The once-init sets up g_guard_creation_lock, so we destroy it here */
    nimcp_platform_mutex_destroy(&g_guard_creation_lock);

    /* Clear health agent pointer */
    atomic_store(&g_toctou_guard_health_agent, NULL);

    /* P2-SEC-3: Reset init_once so module can be re-initialized after cleanup */
    g_toctou_init_once = (nimcp_platform_once_t)NIMCP_PLATFORM_ONCE_INIT;

    LOG_MODULE_INFO("toctou_guard", "TOCTOU guard module cleaned up");
}
