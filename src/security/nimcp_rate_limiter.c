/**
 * @file nimcp_rate_limiter.c
 * @brief Token bucket rate limiter implementation
 *
 * WHAT: Token bucket algorithm with penalty system and per-client tracking
 * WHY:  Protect BBB and other APIs from DoS and abuse
 * HOW:  Hash table of client buckets, refill on access, penalty state machine
 *
 * @author NIMCP Development Team
 * @date 2025-12-07
 */

#include "security/nimcp_rate_limiter.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_wiring_helpers.h"

#include "utils/validation/nimcp_common.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdatomic.h>
#include <stdint.h>
#include <limits.h>
#include <errno.h>

#include "utils/memory/nimcp_memory.h"
#include <stddef.h>  /* for NULL */
//=============================================================================
// Health Agent Integration (Phase 8: System-Wide Health Integration)
//=============================================================================
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;
extern void nimcp_health_agent_heartbeat_ex(nimcp_health_agent_t* agent,
                                             const char* operation,
                                             float progress);

/** Global health agent for rate_limiter module - P0 fix: Use atomic for thread safety */
static _Atomic(nimcp_health_agent_t*) g_rate_limiter_health_agent = NULL;

/**
 * @brief Set health agent for rate_limiter heartbeats
 * @param agent Health agent (can be NULL to disable)
 */
static void rate_limiter_set_health_agent(nimcp_health_agent_t* agent) {
    atomic_store(&g_rate_limiter_health_agent, agent);
}

/** @brief Send heartbeat from rate_limiter module */
static inline void rate_limiter_heartbeat(const char* operation, float progress) {
    /* P0 fix: Atomic load to prevent data race */
    nimcp_health_agent_t* agent = atomic_load(&g_rate_limiter_health_agent);
    if (agent) {
        nimcp_health_agent_heartbeat_ex(agent, operation, progress);
    }
}


//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Per-client bucket state
 */
typedef struct client_bucket {
    char client_id[NIMCP_RATE_LIMIT_MAX_CLIENT_ID];
    float tokens;                   /**< Current tokens in bucket */
    uint64_t last_refill_time_ms;   /**< Last refill timestamp */
    nimcp_client_state_t state;     /**< Current state */
    uint32_t penalty_level;         /**< Current penalty level (0-4) */
    uint32_t violations;            /**< Total violations */
    uint32_t consecutive_good;      /**< Consecutive good requests */
    uint64_t block_until_ms;        /**< Block expiration time */
    nimcp_client_stats_t stats;     /**< Per-client statistics */
    struct client_bucket* next;     /**< Hash chain next */
} client_bucket_t;

/**
 * @brief Hash table for client buckets
 */
#define HASH_TABLE_SIZE 1024

typedef struct {
    client_bucket_t* buckets[HASH_TABLE_SIZE];
    nimcp_platform_mutex_t bucket_locks[HASH_TABLE_SIZE];
} client_hash_table_t;

/**
 * @brief Internal rate limiter structure
 *
 * NOTE: Some statistics fields are atomic to allow lock-free updates from
 *       apply_penalty() without causing deadlock (bucket lock -> limiter lock).
 */
struct nimcp_rate_limiter_impl {
    uint32_t magic;                             /**< Magic number */
    nimcp_rate_limit_config_t config;           /**< Configuration */
    nimcp_platform_mutex_t limiter_lock;        /**< Main limiter lock */
    client_hash_table_t* client_table;          /**< Per-client buckets */
    float global_tokens;                        /**< Global bucket tokens */
    uint64_t global_last_refill_ms;             /**< Global refill time */

    // Atomic statistics - updated from apply_penalty() while holding bucket lock
    // Using atomic prevents deadlock (nested locking: bucket -> limiter)
    atomic_uint_fast64_t atomic_clients_blocked_temp;  /**< Clients temporarily blocked */
    atomic_uint_fast64_t atomic_clients_blocked_perm;  /**< Clients permanently blocked */
    atomic_uint_fast64_t atomic_penalties_applied;     /**< Penalties applied */
    atomic_uint_fast64_t atomic_active_clients;        /**< Currently tracked clients */

    nimcp_rate_limit_stats_t stats;             /**< Non-atomic statistics */
    nimcp_rate_limit_violation_callback_t violation_callback;
    void* violation_callback_data;
    bio_module_context_t bio_context;           /**< Bio-async context */
    bool bio_registered;                        /**< Bio-async registration */
};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Get current time in milliseconds (safe wrapper)
 *
 * SECURITY FIX: Check clock_gettime() return value to avoid using garbage data.
 * On failure, returns 0 which causes rate limiting to be conservative.
 */
static uint64_t get_time_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        /* clock_gettime failed - return 0 to be conservative */
        LOG_MODULE_WARN("rate_limiter", "clock_gettime() failed: %s", strerror(errno));
        return 0;
    }
    return (uint64_t)ts.tv_sec * NIMCP_MS_PER_SEC + (uint64_t)ts.tv_nsec / NIMCP_NS_PER_MS;
}

/**
 * @brief Simple hash function for client IDs
 */
static uint32_t hash_client_id(const char* client_id) {
    if (!client_id) return 0;

    uint32_t hash = 5381;
    while (*client_id) {
        hash = ((hash << 5) + hash) + (uint32_t)(*client_id++);
    }
    return hash % HASH_TABLE_SIZE;
}

/**
 * @brief Validate limiter handle
 */
static inline bool is_valid_limiter(nimcp_rate_limiter_t limiter) {
    return limiter != NULL && limiter->magic == NIMCP_RATE_LIMITER_MAGIC;
}

/**
 * @brief Refill tokens based on elapsed time (token bucket algorithm)
 *
 * SECURITY: Added overflow protection for elapsed_ms calculation.
 *           If now < last_refill (clock skew/wrap), we skip refill to avoid
 *           underflow that could grant unlimited tokens.
 */
static void refill_tokens(float* tokens, uint64_t* last_refill_ms,
                          float rate_per_second, float max_tokens)
{
    // Defensive null checks
    if (!tokens || !last_refill_ms) {
        return;
    }

    uint64_t now = get_time_ms();

    // SECURITY: Prevent underflow if clock goes backwards (time skew)
    if (now < *last_refill_ms) {
        // Clock skew detected - reset to current time, don't grant extra tokens
        *last_refill_ms = now;
        return;
    }

    uint64_t elapsed_ms = now - *last_refill_ms;

    if (elapsed_ms > 0) {
        // SECURITY: Cap elapsed_ms to prevent huge token grants after long idle
        // Maximum 1 hour of token accumulation
        const uint64_t MAX_ELAPSED_MS = 3600000ULL;
        if (elapsed_ms > MAX_ELAPSED_MS) {
            elapsed_ms = MAX_ELAPSED_MS;
        }

        float new_tokens = ((float)elapsed_ms / (float)NIMCP_MS_PER_SEC) * rate_per_second;
        *tokens += new_tokens;

        if (*tokens > max_tokens) {
            *tokens = max_tokens;
        }

        *last_refill_ms = now;
    }
}

/**
 * @brief Deferred callback info for invoking outside critical section
 */
typedef struct {
    bool should_invoke;
    char client_id[NIMCP_RATE_LIMIT_MAX_CLIENT_ID];
    uint32_t violations;
    nimcp_penalty_action_t action;
    nimcp_rate_limit_violation_callback_t callback;
    void* user_data;
} deferred_callback_info_t;

/**
 * @brief Apply penalty to client
 *
 * THREAD SAFETY: This function is called while holding the bucket lock.
 * To avoid invoking user callbacks while holding locks (which could cause
 * deadlocks if the callback tries to acquire locks), callback information
 * is stored in deferred_info for the caller to invoke after releasing locks.
 */
static void apply_penalty(nimcp_rate_limiter_t limiter,
                          client_bucket_t* bucket,
                          deferred_callback_info_t* deferred_info)
{
    // Initialize deferred callback info
    if (deferred_info) {
        deferred_info->should_invoke = false;
    }

    if (!limiter->config.penalty.enabled) {
        return;
    }

    bucket->violations++;

    // Get penalty action for current violation count
    uint32_t violation_idx = bucket->penalty_level;
    if (violation_idx >= 5) {
        violation_idx = 4; // Max penalty
    }

    nimcp_penalty_action_t action = limiter->config.penalty.actions[violation_idx];

    // Apply penalty based on action
    switch (action) {
        case PENALTY_NONE:
            break;

        case PENALTY_WARN:
            bucket->state = CLIENT_STATE_WARNING;
            LOG_MODULE_WARN("rate_limiter",
                "Client %s warning (violations=%u)",
                bucket->client_id, bucket->violations);
            break;

        case PENALTY_REDUCE_RATE_25:
            bucket->state = CLIENT_STATE_RATE_REDUCED;
            bucket->stats.current_rate *= 0.75F;
            bucket->penalty_level++;
            LOG_MODULE_WARN("rate_limiter",
                "Client %s rate reduced to 75%% (violations=%u)",
                bucket->client_id, bucket->violations);
            break;

        case PENALTY_REDUCE_RATE_50:
            bucket->state = CLIENT_STATE_RATE_REDUCED;
            bucket->stats.current_rate *= 0.50F;
            bucket->penalty_level++;
            LOG_MODULE_WARN("rate_limiter",
                "Client %s rate reduced to 50%% (violations=%u)",
                bucket->client_id, bucket->violations);
            break;

        case PENALTY_REDUCE_RATE_75:
            bucket->state = CLIENT_STATE_RATE_REDUCED;
            bucket->stats.current_rate *= 0.25F;
            bucket->penalty_level++;
            LOG_MODULE_WARN("rate_limiter",
                "Client %s rate reduced to 25%% (violations=%u)",
                bucket->client_id, bucket->violations);
            break;

        case PENALTY_BLOCK_TEMPORARY:
            bucket->state = CLIENT_STATE_BLOCKED_TEMP;
            bucket->block_until_ms = get_time_ms() + limiter->config.penalty.cooldown_ms;
            bucket->penalty_level++;
            // Use atomic to avoid deadlock (we're holding bucket lock)
            atomic_fetch_add_explicit(&limiter->atomic_clients_blocked_temp, 1, memory_order_relaxed);
            LOG_MODULE_WARN("rate_limiter",
                "Client %s temporarily blocked for %ums (violations=%u)",
                bucket->client_id, limiter->config.penalty.cooldown_ms,
                bucket->violations);
            break;

        case PENALTY_BLOCK_PERMANENT:
            bucket->state = CLIENT_STATE_BLOCKED_PERM;
            bucket->penalty_level++;
            // Use atomic to avoid deadlock (we're holding bucket lock)
            atomic_fetch_add_explicit(&limiter->atomic_clients_blocked_perm, 1, memory_order_relaxed);
            LOG_MODULE_ERROR("rate_limiter",
                "Client %s permanently blocked (violations=%u)",
                bucket->client_id, bucket->violations);
            break;
    }

    // Update statistics atomically to avoid deadlock (we may be holding bucket lock)
    atomic_fetch_add_explicit(&limiter->atomic_penalties_applied, 1, memory_order_relaxed);

    /* Send BIO_MSG_SECURITY_RATE_LIMIT so other security modules are notified.
     * Must use deferred pattern since we're inside bucket lock — we only build
     * the message here; caller is responsible for actually sending after
     * releasing locks.  However, bio_router_broadcast is safe to call while
     * holding a different lock (no nested router lock), so we send directly
     * for blocking actions which are security-critical. */
    if (limiter->bio_context && limiter->bio_registered &&
        (action == PENALTY_BLOCK_TEMPORARY || action == PENALTY_BLOCK_PERMANENT)) {
        struct {
            bio_message_header_t hdr;
            char     client_id[NIMCP_RATE_LIMIT_MAX_CLIENT_ID];
            uint32_t violations;
            uint32_t penalty_action;
        } rate_msg = {0};
        rate_msg.hdr.type          = BIO_MSG_SECURITY_RATE_LIMIT;
        rate_msg.hdr.source_module = BIO_MODULE_SECURITY;
        rate_msg.hdr.target_module = 0;
        rate_msg.hdr.payload_size  = sizeof(rate_msg) - sizeof(bio_message_header_t);
        rate_msg.hdr.flags         = BIO_MSG_FLAG_BROADCAST;
        strncpy(rate_msg.client_id, bucket->client_id, sizeof(rate_msg.client_id) - 1);
        rate_msg.violations        = bucket->violations;
        rate_msg.penalty_action    = (uint32_t)action;
        (void)bio_router_broadcast(limiter->bio_context, &rate_msg, sizeof(rate_msg));
    }

    // Store callback info for deferred invocation (outside critical section)
    if (deferred_info && limiter->violation_callback) {
        deferred_info->should_invoke = true;
        strncpy(deferred_info->client_id, bucket->client_id,
                sizeof(deferred_info->client_id) - 1);
        deferred_info->client_id[sizeof(deferred_info->client_id) - 1] = '\0';
        deferred_info->violations = bucket->violations;
        deferred_info->action = action;
        deferred_info->callback = limiter->violation_callback;
        deferred_info->user_data = limiter->violation_callback_data;
    }
}

/**
 * @brief Check if penalty should be reduced (good behavior)
 */
static void check_good_behavior(nimcp_rate_limiter_t limiter,
                                 client_bucket_t* bucket)
{
    if (!limiter->config.penalty.enabled ||
        !limiter->config.penalty.reset_on_good_behavior) {
        return;
    }

    bucket->consecutive_good++;

    if (bucket->consecutive_good >= limiter->config.penalty.good_behavior_count &&
        bucket->penalty_level > 0) {
        // Reset one penalty level
        bucket->penalty_level--;
        bucket->consecutive_good = 0;

        // Restore some rate
        bucket->stats.current_rate = limiter->config.requests_per_second *
                                      powf(0.75F, (float)bucket->penalty_level);

        if (bucket->penalty_level == 0) {
            bucket->state = CLIENT_STATE_NORMAL;
            LOG_MODULE_INFO("rate_limiter",
                "Client %s penalties reset due to good behavior",
                bucket->client_id);
        }
    }
}

/**
 * @brief Find or create client bucket
 *
 * THREAD-SAFETY: Lock is held for entire search-and-create operation to prevent
 * TOCTOU race condition where two threads could create duplicate buckets.
 *
 * WHAT: Find existing bucket or atomically create new one
 * WHY:  Prevent race where check-then-create creates duplicates
 * HOW:  Hold bucket_lock throughout entire operation (search + create)
 */
static client_bucket_t* get_client_bucket(nimcp_rate_limiter_t limiter,
                                           const char* client_id,
                                           bool create)
{
    uint32_t hash = hash_client_id(client_id);
    nimcp_platform_mutex_lock(&limiter->client_table->bucket_locks[hash]);

    // Search chain - lock is held throughout
    client_bucket_t* bucket = limiter->client_table->buckets[hash];
    while (bucket) {
        if (strcmp(bucket->client_id, client_id) == 0) {
            // Found existing bucket - return while still holding lock briefly
            // for consistency, but we can release now since we have the pointer
            nimcp_platform_mutex_unlock(&limiter->client_table->bucket_locks[hash]);
            return bucket;
        }
        bucket = bucket->next;
    }

    // Not found - still holding lock to prevent TOCTOU
    if (!create) {
        nimcp_platform_mutex_unlock(&limiter->client_table->bucket_locks[hash]);
        return NULL;  // Normal "not found" - caller handles this
    }

    // Create new bucket - lock still held, preventing race condition
    // Another thread cannot insert the same client_id between our search and insert
    bucket = (client_bucket_t*)nimcp_calloc(1, sizeof(client_bucket_t));
    if (!bucket) {
        nimcp_platform_mutex_unlock(&limiter->client_table->bucket_locks[hash]);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "get_client_bucket: bucket is NULL");
        return NULL;
    }

    strncpy(bucket->client_id, client_id, NIMCP_RATE_LIMIT_MAX_CLIENT_ID - 1);
    bucket->tokens = (float)limiter->config.burst_size;
    bucket->last_refill_time_ms = get_time_ms();
    bucket->state = CLIENT_STATE_NORMAL;
    bucket->stats.current_rate = limiter->config.requests_per_second;
    bucket->stats.tokens_available = (uint32_t)bucket->tokens;
    strncpy(bucket->stats.client_id, client_id, NIMCP_RATE_LIMIT_MAX_CLIENT_ID - 1);

    // Add to chain - atomic insert while lock held
    bucket->next = limiter->client_table->buckets[hash];
    limiter->client_table->buckets[hash] = bucket;

    // Update statistics atomically (we're holding bucket lock, avoid nested locking)
    atomic_fetch_add_explicit(&limiter->atomic_active_clients, 1, memory_order_relaxed);

    nimcp_platform_mutex_unlock(&limiter->client_table->bucket_locks[hash]);
    return bucket;
}

//=============================================================================
// Lifecycle Implementation
//=============================================================================

nimcp_rate_limit_config_t nimcp_rate_limiter_default_config(void) {
    nimcp_rate_limit_config_t config = {
        .requests_per_second = NIMCP_RATE_LIMIT_DEFAULT_RPS,
        .burst_size = NIMCP_RATE_LIMIT_DEFAULT_BURST,
        .algorithm = RATE_LIMIT_TOKEN_BUCKET,
        .per_client = true,
        .per_resource = false,
        .max_tracked_clients = 10000,
        .penalty = {
            .enabled = true,
            .violation_threshold = 2,
            .actions = {
                PENALTY_WARN,
                PENALTY_REDUCE_RATE_25,
                PENALTY_REDUCE_RATE_50,
                PENALTY_BLOCK_TEMPORARY,
                PENALTY_BLOCK_PERMANENT
            },
            .cooldown_ms = NIMCP_RATE_LIMIT_DEFAULT_COOLDOWN_MS,
            .violation_window_ms = 60000,
            .reset_on_good_behavior = true,
            .good_behavior_count = 100
        },
        .enable_statistics = true,
        .enable_logging = true,
        .cleanup_interval_ms = 300000,
        .idle_timeout_ms = 600000
    };
    return config;
}

nimcp_rate_limiter_t nimcp_rate_limiter_create(
    const nimcp_rate_limit_config_t* config)
{
    // Use default if NULL
    nimcp_rate_limit_config_t actual_config;
    if (config == NULL) {
        actual_config = nimcp_rate_limiter_default_config();
    } else {
        actual_config = *config;
    }

    // Validate
    if (actual_config.requests_per_second <= 0.0F ||
        actual_config.burst_size == 0) {
        LOG_ERROR("Invalid rate limiter configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_rate_limiter_create: invalid config (zero rate or burst)");
        return NULL;
    }

    // Allocate limiter
    nimcp_rate_limiter_t limiter = (nimcp_rate_limiter_t)nimcp_calloc(
        1, sizeof(struct nimcp_rate_limiter_impl));
    NIMCP_API_CHECK_ALLOC(limiter, "Failed to allocate rate limiter");

    // Initialize
    limiter->magic = NIMCP_RATE_LIMITER_MAGIC;
    limiter->config = actual_config;
    limiter->global_tokens = (float)actual_config.burst_size;
    limiter->global_last_refill_ms = get_time_ms();
    limiter->bio_registered = false;
    memset(&limiter->stats, 0, sizeof(nimcp_rate_limit_stats_t));

    // Initialize atomic statistics
    atomic_init(&limiter->atomic_clients_blocked_temp, 0);
    atomic_init(&limiter->atomic_clients_blocked_perm, 0);
    atomic_init(&limiter->atomic_penalties_applied, 0);
    atomic_init(&limiter->atomic_active_clients, 0);

    // Initialize main lock
    if (nimcp_platform_mutex_init(&limiter->limiter_lock, false) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_MUTEX_INIT, "Failed to initialize rate limiter lock");
        nimcp_free(limiter);
        return NULL;
    }

    // Allocate hash table
    limiter->client_table = (client_hash_table_t*)nimcp_calloc(
        1, sizeof(client_hash_table_t));
    if (!limiter->client_table) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(client_hash_table_t), "Failed to allocate rate limiter client hash table");
        nimcp_platform_mutex_destroy(&limiter->limiter_lock);
        nimcp_free(limiter);
        return NULL;
    }

    // Initialize bucket locks
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        nimcp_platform_mutex_init(&limiter->client_table->bucket_locks[i], false);
    }

    LOG_MODULE_INFO("rate_limiter",
        "Created rate limiter (rate=%.1f req/s, burst=%u)",
        actual_config.requests_per_second, actual_config.burst_size);

    return limiter;
}

void nimcp_rate_limiter_destroy(nimcp_rate_limiter_t limiter) {
    if (!is_valid_limiter(limiter)) {
        return;
    }

    LOG_MODULE_DEBUG("rate_limiter", "Destroying rate limiter");

    // Cleanup client buckets
    if (limiter->client_table) {
        for (int i = 0; i < HASH_TABLE_SIZE; i++) {
            nimcp_platform_mutex_lock(&limiter->client_table->bucket_locks[i]);

            client_bucket_t* bucket = limiter->client_table->buckets[i];
            while (bucket) {
                client_bucket_t* next = bucket->next;
                // SECURITY: Zero sensitive data before freeing
                memset(bucket, 0, sizeof(client_bucket_t));
                nimcp_free(bucket);
                bucket = next;
            }

            nimcp_platform_mutex_unlock(&limiter->client_table->bucket_locks[i]);
            nimcp_platform_mutex_destroy(&limiter->client_table->bucket_locks[i]);
        }
        // SECURITY: Zero table before freeing
        memset(limiter->client_table, 0, sizeof(client_hash_table_t));
        nimcp_free(limiter->client_table);
    }

    // Unregister from bio-async
    if (limiter->bio_registered && limiter->bio_context) {
        bio_router_unregister_module(limiter->bio_context);
    }

    // Cleanup
    limiter->magic = 0;
    nimcp_platform_mutex_destroy(&limiter->limiter_lock);
    nimcp_free(limiter);
}

//=============================================================================
// Core Rate Limiting Implementation
//=============================================================================

bool nimcp_rate_limiter_allow(
    nimcp_rate_limiter_t limiter,
    const char* client_id)
{
    if (!is_valid_limiter(limiter)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_rate_limiter_allow: invalid or NULL limiter");
        return false;
    }

    // Update statistics with overflow protection
    nimcp_platform_mutex_lock(&limiter->limiter_lock);
    // SECURITY: Prevent overflow - cap at UINT64_MAX
    if (limiter->stats.total_requests < UINT64_MAX) {
        limiter->stats.total_requests++;
    }
    nimcp_platform_mutex_unlock(&limiter->limiter_lock);

    // Global limit if no client_id
    if (!limiter->config.per_client || client_id == NULL) {
        nimcp_platform_mutex_lock(&limiter->limiter_lock);

        refill_tokens(&limiter->global_tokens,
                      &limiter->global_last_refill_ms,
                      limiter->config.requests_per_second,
                      (float)limiter->config.burst_size);

        bool allowed = (limiter->global_tokens >= 1.0F);
        if (allowed) {
            limiter->global_tokens -= 1.0F;
            // SECURITY: Overflow protection for counter
            if (limiter->stats.requests_allowed < UINT64_MAX) {
                limiter->stats.requests_allowed++;
            }
        } else {
            // SECURITY: Overflow protection for counter
            if (limiter->stats.requests_denied < UINT64_MAX) {
                limiter->stats.requests_denied++;
            }
        }

        nimcp_platform_mutex_unlock(&limiter->limiter_lock);
        return allowed;
    }

    // Per-client limiting
    client_bucket_t* bucket = get_client_bucket(limiter, client_id, true);
    if (!bucket) {
        LOG_MODULE_ERROR("rate_limiter",
            "Failed to get bucket for client %s", client_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rate_limiter_allow: bucket is NULL");
        return false;
    }

    uint32_t hash = hash_client_id(client_id);
    nimcp_platform_mutex_lock(&limiter->client_table->bucket_locks[hash]);

    // Check if blocked
    if (bucket->state == CLIENT_STATE_BLOCKED_PERM) {
        nimcp_platform_mutex_unlock(&limiter->client_table->bucket_locks[hash]);
        nimcp_platform_mutex_lock(&limiter->limiter_lock);
        limiter->stats.requests_denied++;
        nimcp_platform_mutex_unlock(&limiter->limiter_lock);
        return false;  /* Client permanently blocked - expected behavior */
    }

    if (bucket->state == CLIENT_STATE_BLOCKED_TEMP) {
        if (get_time_ms() < bucket->block_until_ms) {
            nimcp_platform_mutex_unlock(&limiter->client_table->bucket_locks[hash]);
            nimcp_platform_mutex_lock(&limiter->limiter_lock);
            limiter->stats.requests_denied++;
            nimcp_platform_mutex_unlock(&limiter->limiter_lock);
            return false;  /* Client temporarily blocked - expected behavior */
        } else {
            // Unblock
            bucket->state = CLIENT_STATE_NORMAL;
            LOG_MODULE_INFO("rate_limiter",
                "Client %s temporary block expired", client_id);
        }
    }

    // Refill tokens
    refill_tokens(&bucket->tokens, &bucket->last_refill_time_ms,
                  bucket->stats.current_rate,
                  (float)limiter->config.burst_size);

    // Check tokens
    bool allowed = (bucket->tokens >= 1.0F);

    if (allowed) {
        bucket->tokens -= 1.0F;
        // SECURITY: Overflow protection for per-client counter
        if (bucket->stats.requests_allowed < UINT64_MAX) {
            bucket->stats.requests_allowed++;
        }
        bucket->stats.last_request_time_ms = get_time_ms();
        bucket->stats.tokens_available = (uint32_t)bucket->tokens;
        bucket->consecutive_good++;

        check_good_behavior(limiter, bucket);

        // LOCK ORDERING FIX: Release bucket lock BEFORE acquiring limiter_lock
        // This prevents potential deadlock from inconsistent lock ordering
        nimcp_platform_mutex_unlock(&limiter->client_table->bucket_locks[hash]);

        nimcp_platform_mutex_lock(&limiter->limiter_lock);
        // SECURITY: Overflow protection for global counter
        if (limiter->stats.requests_allowed < UINT64_MAX) {
            limiter->stats.requests_allowed++;
        }
        // SECURITY: Prevent divide-by-zero in rate calculation
        if (limiter->stats.total_requests > 0) {
            limiter->stats.overall_allow_rate =
                (float)limiter->stats.requests_allowed /
                (float)limiter->stats.total_requests * 100.0F;
        }
        nimcp_platform_mutex_unlock(&limiter->limiter_lock);

        return true;  // Return here since we already released bucket lock
    } else {
        // SECURITY: Overflow protection for per-client counter
        if (bucket->stats.requests_denied < UINT64_MAX) {
            bucket->stats.requests_denied++;
        }
        bucket->consecutive_good = 0;

        // Store callback info for deferred invocation outside critical section
        deferred_callback_info_t deferred_cb = {0};
        apply_penalty(limiter, bucket, &deferred_cb);

        // LOCK ORDERING FIX: Release bucket lock BEFORE acquiring limiter_lock
        // This prevents potential deadlock from inconsistent lock ordering
        nimcp_platform_mutex_unlock(&limiter->client_table->bucket_locks[hash]);

        nimcp_platform_mutex_lock(&limiter->limiter_lock);
        // SECURITY: Overflow protection for global counter
        if (limiter->stats.requests_denied < UINT64_MAX) {
            limiter->stats.requests_denied++;
        }
        // SECURITY: Prevent divide-by-zero in rate calculation
        if (limiter->stats.total_requests > 0) {
            limiter->stats.overall_allow_rate =
                (float)limiter->stats.requests_allowed /
                (float)limiter->stats.total_requests * 100.0F;
        }
        nimcp_platform_mutex_unlock(&limiter->limiter_lock);

        // Invoke callback outside critical section to prevent deadlocks
        if (deferred_cb.should_invoke) {
            deferred_cb.callback(deferred_cb.client_id, deferred_cb.violations,
                                 deferred_cb.action, deferred_cb.user_data);
        }

        return false;  /* Rate limit exceeded - request denied */
    }
}

bool nimcp_rate_limiter_allow_resource(
    nimcp_rate_limiter_t limiter,
    const char* client_id,
    const char* resource_id)
{
    // For now, just use client_id (can extend with resource tracking)
    return nimcp_rate_limiter_allow(limiter, client_id);
}

bool nimcp_rate_limiter_acquire(
    nimcp_rate_limiter_t limiter,
    const char* client_id,
    uint32_t count)
{
    if (!is_valid_limiter(limiter) || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_rate_limiter_acquire: invalid limiter or zero count");
        return false;
    }

    client_bucket_t* bucket = get_client_bucket(limiter, client_id, true);
    if (!bucket) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rate_limiter_acquire: bucket is NULL");
        return false;
    }

    uint32_t hash = hash_client_id(client_id);
    nimcp_platform_mutex_lock(&limiter->client_table->bucket_locks[hash]);

    refill_tokens(&bucket->tokens, &bucket->last_refill_time_ms,
                  bucket->stats.current_rate,
                  (float)limiter->config.burst_size);

    bool allowed = (bucket->tokens >= (float)count);

    if (allowed) {
        bucket->tokens -= (float)count;
        bucket->stats.requests_allowed += count;
    } else {
        bucket->stats.requests_denied++;
    }

    nimcp_platform_mutex_unlock(&limiter->client_table->bucket_locks[hash]);
    return allowed;
}

bool nimcp_rate_limiter_check(
    nimcp_rate_limiter_t limiter,
    const char* client_id)
{
    if (!is_valid_limiter(limiter)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_rate_limiter_check: is_valid_limiter is NULL");
        return false;
    }

    client_bucket_t* bucket = get_client_bucket(limiter, client_id, false);
    if (!bucket) {
        return true; // New client, would be allowed
    }

    uint32_t hash = hash_client_id(client_id);
    nimcp_platform_mutex_lock(&limiter->client_table->bucket_locks[hash]);

    // Just check, don't consume
    refill_tokens(&bucket->tokens, &bucket->last_refill_time_ms,
                  bucket->stats.current_rate,
                  (float)limiter->config.burst_size);

    bool would_allow = (bucket->tokens >= 1.0F) &&
                       (bucket->state != CLIENT_STATE_BLOCKED_PERM) &&
                       (bucket->state != CLIENT_STATE_BLOCKED_TEMP ||
                        get_time_ms() >= bucket->block_until_ms);

    nimcp_platform_mutex_unlock(&limiter->client_table->bucket_locks[hash]);
    return would_allow;
}

uint64_t nimcp_rate_limiter_time_until_ready(
    nimcp_rate_limiter_t limiter,
    const char* client_id)
{
    if (!is_valid_limiter(limiter)) {
        return 0;
    }

    client_bucket_t* bucket = get_client_bucket(limiter, client_id, false);
    if (!bucket) {
        return 0; // New client, ready now
    }

    uint32_t hash = hash_client_id(client_id);
    nimcp_platform_mutex_lock(&limiter->client_table->bucket_locks[hash]);

    refill_tokens(&bucket->tokens, &bucket->last_refill_time_ms,
                  bucket->stats.current_rate,
                  (float)limiter->config.burst_size);

    uint64_t wait_ms = 0;
    if (bucket->tokens < 1.0F) {
        float tokens_needed = 1.0F - bucket->tokens;

        /* Guard against division by zero or very small rates */
        if (bucket->stats.current_rate > 1e-6F) {
            float wait_time = tokens_needed / bucket->stats.current_rate * (float)NIMCP_MS_PER_SEC;

            /* Cap maximum wait time to prevent overflow (max 1 hour) */
            const uint64_t MAX_WAIT_MS = 3600000ULL;  /* 1 hour in milliseconds */
            if (wait_time > (float)MAX_WAIT_MS) {
                wait_ms = MAX_WAIT_MS;
            } else {
                wait_ms = (uint64_t)wait_time;
            }
        } else {
            /* Rate too small, cap at maximum wait */
            wait_ms = 3600000ULL;  /* 1 hour */
        }
    }

    nimcp_platform_mutex_unlock(&limiter->client_table->bucket_locks[hash]);
    return wait_ms;
}

//=============================================================================
// Statistics and Management Implementation
//=============================================================================

nimcp_error_t nimcp_rate_limiter_get_stats(
    nimcp_rate_limiter_t limiter,
    nimcp_rate_limit_stats_t* stats)
{
    NIMCP_API_CHECK_NULL(limiter, NIMCP_ERROR_NULL_POINTER, "NULL limiter in get_stats");
    NIMCP_API_CHECK_NULL(stats, NIMCP_ERROR_NULL_POINTER, "NULL stats output in get_stats");
    NIMCP_API_CHECK(is_valid_limiter(limiter), NIMCP_ERROR_INVALID_PARAM, "Invalid limiter magic in get_stats");

    nimcp_platform_mutex_lock(&limiter->limiter_lock);
    *stats = limiter->stats;
    nimcp_platform_mutex_unlock(&limiter->limiter_lock);

    // Read atomic statistics (these are updated without holding limiter_lock)
    stats->clients_blocked_temp = atomic_load_explicit(&limiter->atomic_clients_blocked_temp, memory_order_relaxed);
    stats->clients_blocked_perm = atomic_load_explicit(&limiter->atomic_clients_blocked_perm, memory_order_relaxed);
    stats->penalties_applied = atomic_load_explicit(&limiter->atomic_penalties_applied, memory_order_relaxed);
    stats->active_clients = (uint32_t)atomic_load_explicit(&limiter->atomic_active_clients, memory_order_relaxed);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_rate_limiter_reset_stats(nimcp_rate_limiter_t limiter) {
    NIMCP_API_CHECK_NULL(limiter, NIMCP_ERROR_NULL_POINTER, "NULL limiter in reset_stats");
    NIMCP_API_CHECK(is_valid_limiter(limiter), NIMCP_ERROR_INVALID_PARAM, "Invalid limiter magic in reset_stats");

    nimcp_platform_mutex_lock(&limiter->limiter_lock);
    memset(&limiter->stats, 0, sizeof(nimcp_rate_limit_stats_t));
    nimcp_platform_mutex_unlock(&limiter->limiter_lock);

    LOG_MODULE_INFO("rate_limiter", "Statistics reset");
    return NIMCP_SUCCESS;
}

uint32_t nimcp_rate_limiter_get_active_clients(nimcp_rate_limiter_t limiter) {
    if (!is_valid_limiter(limiter)) {
        return 0;
    }

    // Read from atomic (lock-free)
    return (uint32_t)atomic_load_explicit(&limiter->atomic_active_clients, memory_order_relaxed);
}

nimcp_error_t nimcp_rate_limiter_reset_client(
    nimcp_rate_limiter_t limiter,
    const char* client_id)
{
    NIMCP_API_CHECK_NULL(limiter, NIMCP_ERROR_NULL_POINTER, "NULL limiter in reset_client");
    NIMCP_API_CHECK_NULL(client_id, NIMCP_ERROR_NULL_POINTER, "NULL client_id in reset_client");
    NIMCP_API_CHECK(is_valid_limiter(limiter), NIMCP_ERROR_INVALID_PARAM, "Invalid limiter magic in reset_client");

    client_bucket_t* bucket = get_client_bucket(limiter, client_id, false);
    if (!bucket) {
        return NIMCP_ERROR_NOT_FOUND;
    }

    uint32_t hash = hash_client_id(client_id);
    nimcp_platform_mutex_lock(&limiter->client_table->bucket_locks[hash]);

    bucket->tokens = (float)limiter->config.burst_size;
    bucket->state = CLIENT_STATE_NORMAL;
    bucket->penalty_level = 0;
    bucket->violations = 0;
    bucket->consecutive_good = 0;
    bucket->stats.current_rate = limiter->config.requests_per_second;

    nimcp_platform_mutex_unlock(&limiter->client_table->bucket_locks[hash]);

    LOG_MODULE_INFO("rate_limiter", "Client %s reset", client_id);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_rate_limiter_block_client(
    nimcp_rate_limiter_t limiter,
    const char* client_id)
{
    NIMCP_API_CHECK_NULL(limiter, NIMCP_ERROR_NULL_POINTER, "NULL limiter in block_client");
    NIMCP_API_CHECK_NULL(client_id, NIMCP_ERROR_NULL_POINTER, "NULL client_id in block_client");
    NIMCP_API_CHECK(is_valid_limiter(limiter), NIMCP_ERROR_INVALID_PARAM, "Invalid limiter magic in block_client");

    client_bucket_t* bucket = get_client_bucket(limiter, client_id, true);
    if (!bucket) {
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    uint32_t hash = hash_client_id(client_id);
    nimcp_platform_mutex_lock(&limiter->client_table->bucket_locks[hash]);

    bucket->state = CLIENT_STATE_BLOCKED_PERM;

    // Use atomic to avoid nested locking (we're holding bucket lock)
    atomic_fetch_add_explicit(&limiter->atomic_clients_blocked_perm, 1, memory_order_relaxed);

    nimcp_platform_mutex_unlock(&limiter->client_table->bucket_locks[hash]);

    LOG_MODULE_WARN("rate_limiter", "Client %s permanently blocked", client_id);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_rate_limiter_unblock_client(
    nimcp_rate_limiter_t limiter,
    const char* client_id)
{
    NIMCP_API_CHECK_NULL(limiter, NIMCP_ERROR_NULL_POINTER, "NULL limiter in unblock_client");
    NIMCP_API_CHECK_NULL(client_id, NIMCP_ERROR_NULL_POINTER, "NULL client_id in unblock_client");
    NIMCP_API_CHECK(is_valid_limiter(limiter), NIMCP_ERROR_INVALID_PARAM, "Invalid limiter magic in unblock_client");

    client_bucket_t* bucket = get_client_bucket(limiter, client_id, false);
    if (!bucket) {
        return NIMCP_ERROR_NOT_FOUND;
    }

    uint32_t hash = hash_client_id(client_id);
    nimcp_platform_mutex_lock(&limiter->client_table->bucket_locks[hash]);

    if (bucket->state == CLIENT_STATE_BLOCKED_PERM) {
        // Use atomic to avoid nested locking (we're holding bucket lock)
        atomic_fetch_sub_explicit(&limiter->atomic_clients_blocked_perm, 1, memory_order_relaxed);
    }

    bucket->state = CLIENT_STATE_NORMAL;
    bucket->penalty_level = 0;

    // Reset token bucket to give client a fresh start after being unblocked
    bucket->tokens = (float)limiter->config.burst_size;
    bucket->last_refill_time_ms = get_time_ms();
    bucket->stats.current_rate = (float)limiter->config.requests_per_second;
    bucket->consecutive_good = 0;
    bucket->stats.violations = 0;

    nimcp_platform_mutex_unlock(&limiter->client_table->bucket_locks[hash]);

    LOG_MODULE_INFO("rate_limiter", "Client %s unblocked with fresh token bucket", client_id);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_rate_limiter_get_client_stats(
    nimcp_rate_limiter_t limiter,
    const char* client_id,
    nimcp_client_stats_t* stats)
{
    NIMCP_API_CHECK_NULL(limiter, NIMCP_ERROR_NULL_POINTER, "NULL limiter in get_client_stats");
    NIMCP_API_CHECK_NULL(client_id, NIMCP_ERROR_NULL_POINTER, "NULL client_id in get_client_stats");
    NIMCP_API_CHECK_NULL(stats, NIMCP_ERROR_NULL_POINTER, "NULL stats output in get_client_stats");
    NIMCP_API_CHECK(is_valid_limiter(limiter), NIMCP_ERROR_INVALID_PARAM, "Invalid limiter magic in get_client_stats");

    client_bucket_t* bucket = get_client_bucket(limiter, client_id, false);
    if (!bucket) {
        return NIMCP_ERROR_NOT_FOUND;
    }

    uint32_t hash = hash_client_id(client_id);
    nimcp_platform_mutex_lock(&limiter->client_table->bucket_locks[hash]);
    *stats = bucket->stats;
    nimcp_platform_mutex_unlock(&limiter->client_table->bucket_locks[hash]);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_rate_limiter_set_violation_callback(
    nimcp_rate_limiter_t limiter,
    nimcp_rate_limit_violation_callback_t callback,
    void* user_data)
{
    NIMCP_API_CHECK_NULL(limiter, NIMCP_ERROR_NULL_POINTER, "NULL limiter in set_violation_callback");
    NIMCP_API_CHECK(is_valid_limiter(limiter), NIMCP_ERROR_INVALID_PARAM, "Invalid limiter magic in set_violation_callback");

    nimcp_platform_mutex_lock(&limiter->limiter_lock);
    limiter->violation_callback = callback;
    limiter->violation_callback_data = user_data;
    nimcp_platform_mutex_unlock(&limiter->limiter_lock);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_rate_limiter_set_rate(
    nimcp_rate_limiter_t limiter,
    float requests_per_second)
{
    NIMCP_API_CHECK_NULL(limiter, NIMCP_ERROR_NULL_POINTER, "NULL limiter in set_rate");
    NIMCP_API_CHECK(is_valid_limiter(limiter), NIMCP_ERROR_INVALID_PARAM, "Invalid limiter magic in set_rate");
    NIMCP_API_CHECK(requests_per_second > 0.0F, NIMCP_ERROR_INVALID_PARAM, "Invalid requests_per_second in set_rate");

    nimcp_platform_mutex_lock(&limiter->limiter_lock);
    limiter->config.requests_per_second = requests_per_second;
    nimcp_platform_mutex_unlock(&limiter->limiter_lock);

    LOG_MODULE_INFO("rate_limiter",
        "Rate updated to %.1f req/s", requests_per_second);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_rate_limiter_set_burst(
    nimcp_rate_limiter_t limiter,
    uint32_t burst_size)
{
    NIMCP_API_CHECK_NULL(limiter, NIMCP_ERROR_NULL_POINTER, "NULL limiter in set_burst");
    NIMCP_API_CHECK(is_valid_limiter(limiter), NIMCP_ERROR_INVALID_PARAM, "Invalid limiter magic in set_burst");
    NIMCP_API_CHECK(burst_size > 0, NIMCP_ERROR_INVALID_PARAM, "Invalid burst_size in set_burst");

    nimcp_platform_mutex_lock(&limiter->limiter_lock);
    limiter->config.burst_size = burst_size;
    nimcp_platform_mutex_unlock(&limiter->limiter_lock);

    LOG_MODULE_INFO("rate_limiter", "Burst size updated to %u", burst_size);
    return NIMCP_SUCCESS;
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
static int rate_limiter_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
);

static nimcp_error_t rate_limiter_bio_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    nimcp_rate_limiter_t limiter = (nimcp_rate_limiter_t)user_data;

    if (!is_valid_limiter(limiter) || msg == NULL) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    // Handle health check
    if (header->type == BIO_MSG_HEALTH_CHECK) {
        bio_msg_health_response_t response = {0};
        bio_msg_init_header(&response.header, BIO_MSG_HEALTH_RESPONSE,
            BIO_MODULE_SECURITY, header->source_module, sizeof(response));

        response.healthy = true;
        response.active_threads = limiter->stats.active_clients;

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
static int rate_limiter_wiring_handler_callback(
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
                bio_router_register_handler(ctx, message_types[i], rate_limiter_bio_handler);
                registered++;
                break;
            default:
                LOG_MODULE_DEBUG("rate_limiter", "Unknown message type %d in wiring callback", message_types[i]);
                break;
        }
    }

    LOG_MODULE_INFO("rate_limiter", "KG-driven wiring callback registered %d handlers", registered);
    return (registered > 0) ? 0 : -1;
}

nimcp_error_t nimcp_rate_limiter_register_bio_async(
    nimcp_rate_limiter_t limiter)
{
    if (!is_valid_limiter(limiter)) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (limiter->bio_registered) {
        return NIMCP_SUCCESS;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SECURITY,
        .module_name = "rate_limiter",
        .inbox_capacity = 64,
        .user_data = limiter
    };

    limiter->bio_context = bio_router_register_module(&info);
    if (!limiter->bio_context) {
        LOG_ERROR("Failed to register rate limiter with bio-router");
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    // Try KG-driven wiring callback registration first
    nimcp_error_t result = bio_router_register_wiring_callback(
        BIO_MODULE_SECURITY,
        (void*)rate_limiter_wiring_handler_callback,
        limiter
    );

    if (result == NIMCP_SUCCESS) {
        LOG_MODULE_INFO("rate_limiter", "KG-driven wiring callback registered successfully");
    } else {
        // Fallback to legacy handler registration
        LOG_MODULE_INFO("rate_limiter", "Falling back to legacy handler registration");

        LEGACY_HANDLER_REGISTRATION(
            bio_router_register_handler(limiter->bio_context,
                BIO_MSG_HEALTH_CHECK, rate_limiter_bio_handler)
        );
    }

    limiter->bio_registered = true;

    LOG_MODULE_INFO("rate_limiter", "Registered with bio-async router");
    return NIMCP_SUCCESS;
}

uint32_t nimcp_rate_limiter_process_inbox(
    nimcp_rate_limiter_t limiter,
    uint32_t max_messages)
{
    if (!is_valid_limiter(limiter) || !limiter->bio_registered) {
        return 0;
    }

    return bio_router_process_inbox(limiter->bio_context, max_messages);
}
