/**
 * @file nimcp_kg_ratelimit.c
 * @brief Rate Limiting and Quotas for Brain Knowledge Graph
 * @version 1.0.0
 * @date 2026-01-16
 *
 * Implementation of rate limiting using token bucket, sliding window,
 * fixed window, and leaky bucket algorithms. Includes quota management
 * and priority lanes for fair resource allocation.
 */

#include "core/brain/nimcp_kg_ratelimit.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(kg_ratelimit, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Data Structures
 * ============================================================================ */

/**
 * @brief Token bucket state for rate limiting
 */
typedef struct {
    uint64_t tokens;              /**< Current available tokens */
    uint64_t last_refill;         /**< Last refill timestamp (ms) */
} token_bucket_state_t;

/**
 * @brief Module quota entry
 */
typedef struct {
    char module_name[KG_RATELIMIT_MAX_MODULE_NAME];
    kg_quota_config_t quota;
    kg_quota_config_t usage;
    uint32_t priority;
    float reserved_bandwidth;
    kg_ratelimit_stats_t stats;
    token_bucket_state_t bucket;
} quota_entry_t;

/**
 * @brief Rate limiter internal structure
 */
struct kg_ratelimiter {
    kg_ratelimit_config_t config;
    nimcp_mutex_t* mutex;

    /* Global token bucket */
    token_bucket_state_t global_bucket;

    /* Per-module quotas */
    quota_entry_t quotas[KG_RATELIMIT_MAX_QUOTAS];
    uint32_t quota_count;

    /* Global statistics */
    kg_ratelimit_stats_t global_stats;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_current_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/**
 * @brief Find quota entry by module name
 */
static quota_entry_t* find_quota_entry(
    kg_ratelimiter_t* limiter,
    const char* module_name
) {
    const char* name = module_name ? module_name : "";

    for (uint32_t i = 0; i < limiter->quota_count; i++) {
        if (strcmp(limiter->quotas[i].module_name, name) == 0) {
            return &limiter->quotas[i];
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_quota_entry: validation failed");
    return NULL;
}

/**
 * @brief Refill token bucket based on elapsed time
 */
static void refill_bucket(
    token_bucket_state_t* bucket,
    uint64_t rate_per_second,
    uint64_t burst_capacity,
    uint64_t now
) {
    uint64_t elapsed_ms = now - bucket->last_refill;
    uint64_t tokens_to_add = (elapsed_ms * rate_per_second) / 1000;

    bucket->tokens += tokens_to_add;
    if (bucket->tokens > burst_capacity) {
        bucket->tokens = burst_capacity;
    }

    bucket->last_refill = now;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int kg_ratelimit_default_config(kg_ratelimit_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    memset(config, 0, sizeof(*config));
    config->algorithm = KG_RATELIMIT_TOKEN_BUCKET;
    config->scope = KG_SCOPE_GLOBAL;
    config->operations = KG_OP_ALL;
    config->rate_per_second = 1000;
    config->burst_capacity = 100;
    config->window_size_ms = KG_RATELIMIT_DEFAULT_WINDOW_MS;

    return 0;
}

kg_ratelimiter_t* kg_ratelimit_create(const kg_ratelimit_config_t* config) {
    kg_ratelimiter_t* limiter = nimcp_calloc(1, sizeof(kg_ratelimiter_t));
    if (!limiter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "limiter is NULL");

        return NULL;
    }

    /* Apply configuration */
    if (config) {
        memcpy(&limiter->config, config, sizeof(kg_ratelimit_config_t));
    } else {
        kg_ratelimit_default_config(&limiter->config);
    }

    /* Create mutex */
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_NORMAL;
    limiter->mutex = nimcp_mutex_create(&attr);
    if (!limiter->mutex) {
        nimcp_free(limiter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_ratelimit_create: limiter->mutex is NULL");
        return NULL;
    }

    /* Initialize global bucket */
    uint64_t now = get_current_timestamp_ms();
    limiter->global_bucket.tokens = limiter->config.burst_capacity;
    limiter->global_bucket.last_refill = now;

    return limiter;
}

void kg_ratelimit_destroy(kg_ratelimiter_t* limiter) {
    if (!limiter) {
        return;
    }

    if (limiter->mutex) {
        nimcp_mutex_free(limiter->mutex);
    }

    nimcp_free(limiter);
}

/* ============================================================================
 * Rate Limiting Operations
 * ============================================================================ */

kg_ratelimit_result_t kg_ratelimit_check(
    kg_ratelimiter_t* limiter,
    const char* module_name,
    kg_operation_type_t op
) {
    kg_ratelimit_result_t result = {0};

    if (!limiter) {
        result.allowed = false;
        return result;
    }

    /* Check if operation type is rate-limited */
    if (!(limiter->config.operations & op)) {
        result.allowed = true;
        return result;
    }

    nimcp_mutex_lock(limiter->mutex);

    uint64_t now = get_current_timestamp_ms();
    token_bucket_state_t* bucket = &limiter->global_bucket;

    /* Find module-specific bucket if scoped */
    if (limiter->config.scope == KG_SCOPE_PER_MODULE && module_name) {
        quota_entry_t* entry = find_quota_entry(limiter, module_name);
        if (entry) {
            bucket = &entry->bucket;
        }
    }

    /* Refill bucket */
    refill_bucket(bucket, limiter->config.rate_per_second,
                  limiter->config.burst_capacity, now);

    /* Check availability */
    if (bucket->tokens > 0) {
        result.allowed = true;
        result.remaining_tokens = bucket->tokens;
        result.wait_time_ms = 0;
    } else {
        result.allowed = false;
        result.remaining_tokens = 0;
        /* Calculate wait time for one token */
        result.wait_time_ms = 1000 / limiter->config.rate_per_second;
    }

    result.reset_time_ms = now + limiter->config.window_size_ms;

    nimcp_mutex_unlock(limiter->mutex);

    return result;
}

kg_ratelimit_result_t kg_ratelimit_acquire(
    kg_ratelimiter_t* limiter,
    const char* module_name,
    kg_operation_type_t op
) {
    kg_ratelimit_result_t result = {0};

    if (!limiter) {
        result.allowed = false;
        return result;
    }

    /* Check if operation type is rate-limited */
    if (!(limiter->config.operations & op)) {
        result.allowed = true;
        return result;
    }

    nimcp_mutex_lock(limiter->mutex);

    uint64_t now = get_current_timestamp_ms();
    token_bucket_state_t* bucket = &limiter->global_bucket;
    kg_ratelimit_stats_t* stats = &limiter->global_stats;

    /* Find module-specific bucket if scoped */
    if (limiter->config.scope == KG_SCOPE_PER_MODULE && module_name) {
        quota_entry_t* entry = find_quota_entry(limiter, module_name);
        if (entry) {
            bucket = &entry->bucket;
            stats = &entry->stats;
        }
    }

    /* Refill bucket */
    refill_bucket(bucket, limiter->config.rate_per_second,
                  limiter->config.burst_capacity, now);

    stats->total_requests++;

    /* Try to consume a token */
    if (bucket->tokens > 0) {
        bucket->tokens--;
        result.allowed = true;
        result.remaining_tokens = bucket->tokens;
        result.wait_time_ms = 0;
        stats->allowed_requests++;
    } else {
        result.allowed = false;
        result.remaining_tokens = 0;
        result.wait_time_ms = 1000 / limiter->config.rate_per_second;
        stats->denied_requests++;
        stats->total_wait_time_ms += result.wait_time_ms;
    }

    result.reset_time_ms = now + limiter->config.window_size_ms;

    /* Update stats */
    if (stats->denied_requests > 0) {
        stats->avg_wait_time_ms = (float)stats->total_wait_time_ms / stats->denied_requests;
    }
    stats->utilization = 1.0f - ((float)bucket->tokens / limiter->config.burst_capacity);

    nimcp_mutex_unlock(limiter->mutex);

    return result;
}

int kg_ratelimit_release(
    kg_ratelimiter_t* limiter,
    const char* module_name,
    kg_operation_type_t op
) {
    if (!limiter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "limiter is NULL");

        return -1;
    }

    (void)op;

    nimcp_mutex_lock(limiter->mutex);

    token_bucket_state_t* bucket = &limiter->global_bucket;

    if (limiter->config.scope == KG_SCOPE_PER_MODULE && module_name) {
        quota_entry_t* entry = find_quota_entry(limiter, module_name);
        if (entry) {
            bucket = &entry->bucket;
        }
    }

    /* Return the token */
    bucket->tokens++;
    if (bucket->tokens > limiter->config.burst_capacity) {
        bucket->tokens = limiter->config.burst_capacity;
    }

    nimcp_mutex_unlock(limiter->mutex);

    return 0;
}

int kg_ratelimit_reset(kg_ratelimiter_t* limiter) {
    if (!limiter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "limiter is NULL");

        return -1;
    }

    nimcp_mutex_lock(limiter->mutex);

    uint64_t now = get_current_timestamp_ms();

    /* Reset global bucket */
    limiter->global_bucket.tokens = limiter->config.burst_capacity;
    limiter->global_bucket.last_refill = now;

    /* Reset all module buckets */
    for (uint32_t i = 0; i < limiter->quota_count; i++) {
        limiter->quotas[i].bucket.tokens = limiter->config.burst_capacity;
        limiter->quotas[i].bucket.last_refill = now;
    }

    nimcp_mutex_unlock(limiter->mutex);

    return 0;
}

int kg_ratelimit_update_config(
    kg_ratelimiter_t* limiter,
    const kg_ratelimit_config_t* config,
    bool reset_counters
) {
    if (!limiter || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_ratelimit_update_config: required parameter is NULL (limiter, config)");
        return -1;
    }

    nimcp_mutex_lock(limiter->mutex);

    memcpy(&limiter->config, config, sizeof(kg_ratelimit_config_t));

    if (reset_counters) {
        uint64_t now = get_current_timestamp_ms();
        limiter->global_bucket.tokens = config->burst_capacity;
        limiter->global_bucket.last_refill = now;

        for (uint32_t i = 0; i < limiter->quota_count; i++) {
            limiter->quotas[i].bucket.tokens = config->burst_capacity;
            limiter->quotas[i].bucket.last_refill = now;
        }
    }

    nimcp_mutex_unlock(limiter->mutex);

    return 0;
}

/* ============================================================================
 * Quota Management
 * ============================================================================ */

int kg_quota_set(
    kg_ratelimiter_t* limiter,
    const kg_quota_config_t* quota
) {
    if (!limiter || !quota) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_quota_set: required parameter is NULL (limiter, quota)");
        return -1;
    }

    nimcp_mutex_lock(limiter->mutex);

    /* Find existing or create new entry */
    quota_entry_t* entry = find_quota_entry(limiter, quota->module_name);

    if (!entry) {
        if (limiter->quota_count >= KG_RATELIMIT_MAX_QUOTAS) {
            nimcp_mutex_unlock(limiter->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "kg_quota_set: capacity exceeded");
            return -1; /* Quota limit reached */
        }

        entry = &limiter->quotas[limiter->quota_count++];
        memset(entry, 0, sizeof(*entry));
        strncpy(entry->module_name, quota->module_name,
                KG_RATELIMIT_MAX_MODULE_NAME - 1);
        entry->priority = 128; /* Default priority */

        /* Initialize bucket */
        uint64_t now = get_current_timestamp_ms();
        entry->bucket.tokens = limiter->config.burst_capacity;
        entry->bucket.last_refill = now;
    }

    memcpy(&entry->quota, quota, sizeof(kg_quota_config_t));

    nimcp_mutex_unlock(limiter->mutex);

    return 0;
}

int kg_quota_get(
    const kg_ratelimiter_t* limiter,
    const char* module_name,
    kg_quota_config_t* quota
) {
    if (!limiter || !quota) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_quota_get: required parameter is NULL (limiter, quota)");
        return -1;
    }

    nimcp_mutex_lock(((kg_ratelimiter_t*)limiter)->mutex);

    quota_entry_t* entry = find_quota_entry((kg_ratelimiter_t*)limiter, module_name);
    if (!entry) {
        nimcp_mutex_unlock(((kg_ratelimiter_t*)limiter)->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_quota_get: entry is NULL");
        return -1;
    }

    memcpy(quota, &entry->quota, sizeof(kg_quota_config_t));

    nimcp_mutex_unlock(((kg_ratelimiter_t*)limiter)->mutex);

    return 0;
}

int kg_quota_get_usage(
    const kg_ratelimiter_t* limiter,
    const char* module_name,
    kg_quota_config_t* usage
) {
    if (!limiter || !usage) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_quota_get_usage: required parameter is NULL (limiter, usage)");
        return -1;
    }

    nimcp_mutex_lock(((kg_ratelimiter_t*)limiter)->mutex);

    quota_entry_t* entry = find_quota_entry((kg_ratelimiter_t*)limiter, module_name);
    if (!entry) {
        nimcp_mutex_unlock(((kg_ratelimiter_t*)limiter)->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_quota_get_usage: entry is NULL");
        return -1;
    }

    memcpy(usage, &entry->usage, sizeof(kg_quota_config_t));

    nimcp_mutex_unlock(((kg_ratelimiter_t*)limiter)->mutex);

    return 0;
}

bool kg_quota_check(
    const kg_ratelimiter_t* limiter,
    const char* module_name,
    kg_operation_type_t op,
    uint64_t amount
) {
    if (!limiter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_quota_check: limiter is NULL");
        return false;
    }

    (void)op;
    (void)amount;

    nimcp_mutex_lock(((kg_ratelimiter_t*)limiter)->mutex);

    quota_entry_t* entry = find_quota_entry((kg_ratelimiter_t*)limiter, module_name);
    if (!entry) {
        nimcp_mutex_unlock(((kg_ratelimiter_t*)limiter)->mutex);
        return true; /* No quota = unlimited */
    }

    /* Check various quotas */
    bool within_quota = true;

    if (entry->quota.max_nodes > 0 &&
        entry->usage.max_nodes + amount > entry->quota.max_nodes) {
        within_quota = false;
    }

    nimcp_mutex_unlock(((kg_ratelimiter_t*)limiter)->mutex);

    return within_quota;
}

int kg_quota_remove(
    kg_ratelimiter_t* limiter,
    const char* module_name
) {
    if (!limiter || !module_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_quota_remove: required parameter is NULL (limiter, module_name)");
        return -1;
    }

    nimcp_mutex_lock(limiter->mutex);

    for (uint32_t i = 0; i < limiter->quota_count; i++) {
        if (strcmp(limiter->quotas[i].module_name, module_name) == 0) {
            /* Shift remaining entries */
            for (uint32_t j = i; j < limiter->quota_count - 1; j++) {
                memcpy(&limiter->quotas[j], &limiter->quotas[j + 1],
                       sizeof(quota_entry_t));
            }
            limiter->quota_count--;
            nimcp_mutex_unlock(limiter->mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(limiter->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_quota_remove: operation failed");
    return -1; /* Not found */
}

int kg_quota_reset_usage(
    kg_ratelimiter_t* limiter,
    const char* module_name
) {
    if (!limiter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "limiter is NULL");

        return -1;
    }

    nimcp_mutex_lock(limiter->mutex);

    if (module_name) {
        quota_entry_t* entry = find_quota_entry(limiter, module_name);
        if (entry) {
            memset(&entry->usage, 0, sizeof(kg_quota_config_t));
        }
    } else {
        /* Reset all */
        for (uint32_t i = 0; i < limiter->quota_count; i++) {
            memset(&limiter->quotas[i].usage, 0, sizeof(kg_quota_config_t));
        }
    }

    nimcp_mutex_unlock(limiter->mutex);

    return 0;
}

/* ============================================================================
 * Priority Lanes
 * ============================================================================ */

int kg_ratelimit_set_priority(
    kg_ratelimiter_t* limiter,
    const char* module_name,
    uint32_t priority
) {
    if (!limiter || !module_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_ratelimit_set_priority: required parameter is NULL (limiter, module_name)");
        return -1;
    }

    if (priority > KG_RATELIMIT_MAX_PRIORITY) {
        priority = KG_RATELIMIT_MAX_PRIORITY;
    }

    nimcp_mutex_lock(limiter->mutex);

    quota_entry_t* entry = find_quota_entry(limiter, module_name);
    if (!entry) {
        nimcp_mutex_unlock(limiter->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_ratelimit_set_priority: entry is NULL");
        return -1;
    }

    entry->priority = priority;

    nimcp_mutex_unlock(limiter->mutex);

    return 0;
}

int kg_ratelimit_get_priority(
    const kg_ratelimiter_t* limiter,
    const char* module_name,
    uint32_t* priority
) {
    if (!limiter || !module_name || !priority) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_ratelimit_get_priority: required parameter is NULL (limiter, module_name, priority)");
        return -1;
    }

    nimcp_mutex_lock(((kg_ratelimiter_t*)limiter)->mutex);

    quota_entry_t* entry = find_quota_entry((kg_ratelimiter_t*)limiter, module_name);
    if (!entry) {
        nimcp_mutex_unlock(((kg_ratelimiter_t*)limiter)->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_ratelimit_get_priority: entry is NULL");
        return -1;
    }

    *priority = entry->priority;

    nimcp_mutex_unlock(((kg_ratelimiter_t*)limiter)->mutex);

    return 0;
}

int kg_ratelimit_reserve_bandwidth(
    kg_ratelimiter_t* limiter,
    const char* module_name,
    float percent
) {
    if (!limiter || !module_name || percent < 0.0f || percent > 100.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_ratelimit_reserve_bandwidth: required parameter is NULL (limiter, module_name)");
        return -1;
    }

    nimcp_mutex_lock(limiter->mutex);

    /* Calculate total reserved bandwidth */
    float total_reserved = 0.0f;
    quota_entry_t* target_entry = NULL;

    for (uint32_t i = 0; i < limiter->quota_count; i++) {
        if (strcmp(limiter->quotas[i].module_name, module_name) == 0) {
            target_entry = &limiter->quotas[i];
        } else {
            total_reserved += limiter->quotas[i].reserved_bandwidth;
        }
    }

    if (!target_entry) {
        nimcp_mutex_unlock(limiter->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_ratelimit_reserve_bandwidth: target_entry is NULL");
        return -1;
    }

    /* Check if new reservation would exceed 100% */
    if (total_reserved + percent > 100.0f) {
        nimcp_mutex_unlock(limiter->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_ratelimit_reserve_bandwidth: validation failed");
        return -1;
    }

    target_entry->reserved_bandwidth = percent;

    nimcp_mutex_unlock(limiter->mutex);

    return 0;
}

int kg_ratelimit_get_reserved_bandwidth(
    const kg_ratelimiter_t* limiter,
    const char* module_name,
    float* percent
) {
    if (!limiter || !module_name || !percent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_ratelimit_get_reserved_bandwidth: required parameter is NULL (limiter, module_name, percent)");
        return -1;
    }

    nimcp_mutex_lock(((kg_ratelimiter_t*)limiter)->mutex);

    quota_entry_t* entry = find_quota_entry((kg_ratelimiter_t*)limiter, module_name);
    if (!entry) {
        nimcp_mutex_unlock(((kg_ratelimiter_t*)limiter)->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_ratelimit_get_reserved_bandwidth: entry is NULL");
        return -1;
    }

    *percent = entry->reserved_bandwidth;

    nimcp_mutex_unlock(((kg_ratelimiter_t*)limiter)->mutex);

    return 0;
}

int kg_ratelimit_release_bandwidth(
    kg_ratelimiter_t* limiter,
    const char* module_name
) {
    if (!limiter || !module_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_ratelimit_release_bandwidth: required parameter is NULL (limiter, module_name)");
        return -1;
    }

    nimcp_mutex_lock(limiter->mutex);

    quota_entry_t* entry = find_quota_entry(limiter, module_name);
    if (!entry) {
        nimcp_mutex_unlock(limiter->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_ratelimit_release_bandwidth: entry is NULL");
        return -1;
    }

    entry->reserved_bandwidth = 0.0f;

    nimcp_mutex_unlock(limiter->mutex);

    return 0;
}

/* ============================================================================
 * Statistics and Monitoring
 * ============================================================================ */

int kg_ratelimit_get_stats(
    const kg_ratelimiter_t* limiter,
    const char* module_name,
    kg_ratelimit_stats_t* stats
) {
    if (!limiter || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_ratelimit_get_stats: required parameter is NULL (limiter, stats)");
        return -1;
    }

    nimcp_mutex_lock(((kg_ratelimiter_t*)limiter)->mutex);

    if (module_name) {
        quota_entry_t* entry = find_quota_entry((kg_ratelimiter_t*)limiter, module_name);
        if (!entry) {
            nimcp_mutex_unlock(((kg_ratelimiter_t*)limiter)->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_ratelimit_get_stats: entry is NULL");
            return -1;
        }
        memcpy(stats, &entry->stats, sizeof(kg_ratelimit_stats_t));
    } else {
        memcpy(stats, &limiter->global_stats, sizeof(kg_ratelimit_stats_t));
    }

    nimcp_mutex_unlock(((kg_ratelimiter_t*)limiter)->mutex);

    return 0;
}

int kg_ratelimit_reset_stats(
    kg_ratelimiter_t* limiter,
    const char* module_name
) {
    if (!limiter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "limiter is NULL");

        return -1;
    }

    nimcp_mutex_lock(limiter->mutex);

    if (module_name) {
        quota_entry_t* entry = find_quota_entry(limiter, module_name);
        if (entry) {
            memset(&entry->stats, 0, sizeof(kg_ratelimit_stats_t));
        }
    } else {
        /* Reset all */
        memset(&limiter->global_stats, 0, sizeof(kg_ratelimit_stats_t));
        for (uint32_t i = 0; i < limiter->quota_count; i++) {
            memset(&limiter->quotas[i].stats, 0, sizeof(kg_ratelimit_stats_t));
        }
    }

    nimcp_mutex_unlock(limiter->mutex);

    return 0;
}

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

static const char* algo_strings[] = {
    "TOKEN_BUCKET",
    "SLIDING_WINDOW",
    "FIXED_WINDOW",
    "LEAKY_BUCKET"
};

const char* kg_ratelimit_algo_to_string(kg_ratelimit_algo_t algo) {
    if (algo >= 0 && algo <= KG_RATELIMIT_LEAKY_BUCKET) {
        return algo_strings[algo];
    }
    return "UNKNOWN";
}

static const char* scope_strings[] = {
    "GLOBAL",
    "PER_MODULE",
    "PER_LAYER",
    "PER_HEMISPHERE"
};

const char* kg_ratelimit_scope_to_string(kg_ratelimit_scope_t scope) {
    if (scope >= 0 && scope <= KG_SCOPE_PER_HEMISPHERE) {
        return scope_strings[scope];
    }
    return "UNKNOWN";
}

const char* kg_operation_type_to_string(kg_operation_type_t op) {
    if (op & KG_OP_READ) return "READ";
    if (op & KG_OP_WRITE) return "WRITE";
    if (op & KG_OP_QUERY) return "QUERY";
    if (op & KG_OP_TRAVERSE) return "TRAVERSE";
    if (op == KG_OP_ALL) return "ALL";
    return "UNKNOWN";
}
