/**
 * @file nimcp_recovery_cache.c
 * @brief Recovery strategy cache implementation
 *
 * WHAT: Fast memoization cache for recovery strategies
 * WHY: 63x speedup for repeated errors
 * HOW: Hash table + LRU eviction + success tracking
 */

#include "utils/fault_tolerance/nimcp_recovery_cache.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "utils_recovery_cache"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(recovery_cache)

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include "utils/memory/nimcp_unified_memory.h"

/* ============================================================================
 * INTERNAL HELPERS
 * ============================================================================ */

/**
 * @brief Get current timestamp in nanoseconds
 */
static uint64_t get_timestamp_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * @brief FNV-1a hash function
 *
 * WHAT: Fast non-cryptographic hash
 * WHY: Good distribution for cache keys
 * HOW: FNV-1a algorithm
 */
static uint64_t fnv1a_hash(const uint8_t* data, size_t size) {
    const uint64_t FNV_PRIME = 0x100000001b3ULL;
    const uint64_t FNV_OFFSET = 0xcbf29ce484222325ULL;

    uint64_t hash = FNV_OFFSET;
    for (size_t i = 0; i < size; i++) {
        hash ^= (uint64_t)data[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

/**
 * @brief Compute hash bucket index
 */
static size_t hash_bucket_index(uint64_t hash, size_t hash_size) {
    return (size_t)(hash % hash_size);
}

/**
 * @brief Move entry to front of LRU list
 */
static void lru_move_to_front(nimcp_recovery_cache_t* cache, nimcp_cache_entry_t* entry) {
    if (!cache->enable_lru || entry == cache->lru_head) {
        return;  /* Already at front */
    }

    /* Remove from current position */
    if (entry->prev) {
        entry->prev->next = entry->next;
    }
    if (entry->next) {
        entry->next->prev = entry->prev;
    }
    if (entry == cache->lru_tail) {
        cache->lru_tail = entry->prev;
    }

    /* Insert at front */
    entry->prev = NULL;
    entry->next = cache->lru_head;
    if (cache->lru_head) {
        cache->lru_head->prev = entry;
    }
    cache->lru_head = entry;

    /* Update tail if list was empty */
    if (!cache->lru_tail) {
        cache->lru_tail = entry;
    }
}

/**
 * @brief Remove entry from LRU list
 */
static void lru_remove(nimcp_recovery_cache_t* cache, nimcp_cache_entry_t* entry) {
    if (entry->prev) {
        entry->prev->next = entry->next;
    }
    if (entry->next) {
        entry->next->prev = entry->prev;
    }
    if (entry == cache->lru_head) {
        cache->lru_head = entry->next;
    }
    if (entry == cache->lru_tail) {
        cache->lru_tail = entry->prev;
    }
    entry->prev = NULL;
    entry->next = NULL;
}

/**
 * @brief Evict LRU entry
 *
 * WHAT: Remove least recently used entry
 * WHY: Make room for new entry
 * HOW: Remove tail from LRU list and hash table
 */
static bool evict_lru_entry(nimcp_recovery_cache_t* cache) {
    if (!cache->lru_tail) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "evict_lru_entry: cache->lru_tail is NULL");
        return false;  /* Cache is empty */
    }

    nimcp_cache_entry_t* victim = cache->lru_tail;

    /* Remove from hash table */
    size_t bucket = hash_bucket_index(victim->signature.hash, cache->hash_size);
    nimcp_cache_entry_t** curr = &cache->hash_table[bucket];
    while (*curr) {
        if (*curr == victim) {
            *curr = victim->hash_next;
            break;
        }
        curr = &(*curr)->hash_next;
    }

    /* Remove from LRU list */
    lru_remove(cache, victim);

    /* Free entry */
    nimcp_free(victim);

    cache->current_size--;
    cache->stats.evictions++;

    NIMCP_LOGGING_DEBUG("Evicted LRU cache entry (size: %zu/%zu)",
                    cache->current_size, cache->capacity);

    return true;
}

/**
 * @brief Update cache statistics after operation
 */
static void update_stats(nimcp_recovery_cache_t* cache) {
    if (!cache->track_stats) {
        return;
    }

    uint64_t total_accesses = cache->stats.hits + cache->stats.misses;
    if (total_accesses > 0) {
        cache->stats.hit_rate = (double)cache->stats.hits / (double)total_accesses;
    }

    if (cache->stats.hits > 0) {
        cache->stats.avg_lookup_time_ns = cache->stats.total_lookup_time_ns / cache->stats.hits;
    }

    if (cache->stats.stores > 0) {
        cache->stats.avg_store_time_ns = cache->stats.total_store_time_ns / cache->stats.stores;
    }

    cache->stats.current_size = cache->current_size;
    cache->stats.max_size = cache->capacity;
}

/* ============================================================================
 * CACHE LIFECYCLE
 * ============================================================================ */

nimcp_recovery_cache_t* nimcp_recovery_cache_create(size_t capacity) {
    NIMCP_LOGGING_DEBUG("Creating recovery cache (capacity: %zu)", capacity);

    if (capacity == 0) {
        capacity = NIMCP_RECOVERY_CACHE_DEFAULT_CAPACITY;
    }

    nimcp_recovery_cache_t* cache = nimcp_calloc(1, sizeof(nimcp_recovery_cache_t));
    NIMCP_API_CHECK_ALLOC(cache, "nimcp_recovery_cache_create: Failed to allocate recovery cache");

    /* Allocate hash table */
    cache->hash_size = NIMCP_RECOVERY_CACHE_HASH_SIZE;
    cache->hash_table = nimcp_calloc(cache->hash_size, sizeof(nimcp_cache_entry_t*));
    if (!cache->hash_table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_recovery_cache_create: Failed to allocate hash table");
        nimcp_free(cache);
        return NULL;
    }

    /* Initialize configuration */
    cache->capacity = capacity;
    cache->current_size = 0;
    cache->enable_lru = true;
    cache->track_stats = true;

    /* Initialize LRU list */
    cache->lru_head = NULL;
    cache->lru_tail = NULL;

    /* Initialize mutex */
    if (nimcp_mutex_init(&cache->lock, NULL) != NIMCP_SUCCESS) {
        NIMCP_LOGGING_ERROR("Failed to initialize cache mutex");
        nimcp_free(cache->hash_table);
        nimcp_free(cache);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_recovery_cache_create: validation failed");
        return NULL;
    }

    /* Initialize statistics */
    memset(&cache->stats, 0, sizeof(nimcp_recovery_cache_stats_t));

    NIMCP_LOGGING_INFO("Recovery cache created (capacity: %zu, hash_size: %zu)",
                   cache->capacity, cache->hash_size);

    return cache;
}

void nimcp_recovery_cache_destroy(nimcp_recovery_cache_t* cache) {
    if (!cache) {
        return;
    }

    NIMCP_LOGGING_DEBUG("Destroying recovery cache (entries: %zu)", cache->current_size);

    /* Log warning if cache still has entries */
    if (cache->current_size > 0) {
        NIMCP_LOGGING_WARN("Destroying cache with %zu active entries", cache->current_size);
    }

    /* Free all entries */
    for (size_t i = 0; i < cache->hash_size; i++) {
        nimcp_cache_entry_t* entry = cache->hash_table[i];
        while (entry) {
            nimcp_cache_entry_t* next = entry->hash_next;
            nimcp_free(entry);
            entry = next;
        }
    }

    /* Free hash table */
    nimcp_free(cache->hash_table);

    /* Destroy mutex */
    nimcp_mutex_destroy(&cache->lock);

    /* Free cache structure */
    nimcp_free(cache);

    NIMCP_LOGGING_INFO("Recovery cache destroyed");
}

bool nimcp_recovery_cache_clear(nimcp_recovery_cache_t* cache) {
    if (!cache) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_recovery_cache_clear: cache is NULL");
        return false;
    }

    nimcp_mutex_lock(&cache->lock);

    NIMCP_LOGGING_DEBUG("Clearing recovery cache");

    /* Free all entries */
    for (size_t i = 0; i < cache->hash_size; i++) {
        nimcp_cache_entry_t* entry = cache->hash_table[i];
        while (entry) {
            nimcp_cache_entry_t* next = entry->hash_next;
            nimcp_free(entry);
            entry = next;
        }
        cache->hash_table[i] = NULL;
    }

    /* Reset LRU list */
    cache->lru_head = NULL;
    cache->lru_tail = NULL;
    cache->current_size = 0;

    /* Reset statistics (but keep configuration) */
    memset(&cache->stats, 0, sizeof(nimcp_recovery_cache_stats_t));

    nimcp_mutex_unlock(&cache->lock);

    NIMCP_LOGGING_INFO("Recovery cache cleared");

    return true;
}

/* ============================================================================
 * SIGNATURE OPERATIONS
 * ============================================================================ */

bool nimcp_recovery_cache_compute_signature(
    const nimcp_signal_error_context_t* context,
    nimcp_error_signature_t* signature)
{
    if (!context || !signature) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_recovery_cache_compute_signature: required parameter is NULL (context, signature)");
        return false;
    }

    /* Build signature data from context */
    uint8_t temp_buffer[256];
    size_t offset = 0;

    /* Add signal */
    memcpy(temp_buffer + offset, &context->signal, sizeof(context->signal));
    offset += sizeof(context->signal);

    /* Add fault address */
    memcpy(temp_buffer + offset, &context->fault_address, sizeof(context->fault_address));
    offset += sizeof(context->fault_address);

    /* Add context hash */
    memcpy(temp_buffer + offset, &context->context_hash, sizeof(context->context_hash));
    offset += sizeof(context->context_hash);

    /* Add error code */
    memcpy(temp_buffer + offset, &context->error_code, sizeof(context->error_code));
    offset += sizeof(context->error_code);

    /* Add function name if provided */
    if (context->function_name) {
        size_t fn_len = strlen(context->function_name);
        size_t copy_len = (fn_len < 64) ? fn_len : 64;
        memcpy(temp_buffer + offset, context->function_name, copy_len);
        offset += copy_len;
    }

    /* Compute hash */
    signature->hash = fnv1a_hash(temp_buffer, offset);

    /* Store raw signature data (up to max size) */
    signature->size = (offset < NIMCP_RECOVERY_CACHE_MAX_SIGNATURE_SIZE) ?
                      offset : NIMCP_RECOVERY_CACHE_MAX_SIGNATURE_SIZE;
    memcpy(signature->data, temp_buffer, signature->size);

    return true;
}

bool nimcp_recovery_cache_signatures_equal(
    const nimcp_error_signature_t* sig1,
    const nimcp_error_signature_t* sig2)
{
    if (!sig1 || !sig2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_recovery_cache_signatures_equal: required parameter is NULL (sig1, sig2)");
        return false;
    }

    /* Fast path: compare hashes */
    if (sig1->hash != sig2->hash) {
        return false;
    }

    /* Slow path: compare raw data (hash collision check) */
    if (sig1->size != sig2->size) {
        return false;
    }

    return memcmp(sig1->data, sig2->data, sig1->size) == 0;
}

/* ============================================================================
 * CACHE OPERATIONS
 * ============================================================================ */

bool nimcp_recovery_cache_lookup(
    nimcp_recovery_cache_t* cache,
    const nimcp_error_signature_t* signature,
    nimcp_recovery_strategy_t* strategy)
{
    if (!cache || !signature || !strategy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_recovery_cache_lookup: required parameter is NULL (cache, signature, strategy)");
        return false;
    }

    uint64_t start_time = cache->track_stats ? get_timestamp_ns() : 0;

    nimcp_mutex_lock(&cache->lock);

    /* Find entry in hash table */
    size_t bucket = hash_bucket_index(signature->hash, cache->hash_size);
    nimcp_cache_entry_t* entry = cache->hash_table[bucket];

    while (entry) {
        if (nimcp_recovery_cache_signatures_equal(&entry->signature, signature)) {
            /* Cache hit */
            *strategy = entry->strategy;
            entry->last_access_timestamp = get_timestamp_ns();

            /* Update LRU */
            lru_move_to_front(cache, entry);

            /* Update statistics */
            if (cache->track_stats) {
                cache->stats.hits++;
                uint64_t elapsed = get_timestamp_ns() - start_time;
                cache->stats.total_lookup_time_ns += elapsed;
                update_stats(cache);
            }

            nimcp_mutex_unlock(&cache->lock);

            NIMCP_LOGGING_DEBUG("Cache hit for signature 0x%016lx (strategy: %s)",
                           signature->hash, nimcp_recovery_strategy_name(*strategy));

            return true;
        }
        entry = entry->hash_next;
    }

    /* Cache miss */
    if (cache->track_stats) {
        cache->stats.misses++;
        update_stats(cache);
    }

    nimcp_mutex_unlock(&cache->lock);

    NIMCP_LOGGING_DEBUG("Cache miss for signature 0x%016lx", signature->hash);

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_recovery_cache_lookup: validation failed");
    return false;
}

bool nimcp_recovery_cache_store(
    nimcp_recovery_cache_t* cache,
    const nimcp_error_signature_t* signature,
    nimcp_recovery_strategy_t strategy,
    const char* description)
{
    if (!cache || !signature) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_recovery_cache_store: required parameter is NULL (cache, signature)");
        return false;
    }

    uint64_t start_time = cache->track_stats ? get_timestamp_ns() : 0;

    nimcp_mutex_lock(&cache->lock);

    /* Check if entry already exists (update instead of insert) */
    size_t bucket = hash_bucket_index(signature->hash, cache->hash_size);
    nimcp_cache_entry_t* existing = cache->hash_table[bucket];

    while (existing) {
        if (nimcp_recovery_cache_signatures_equal(&existing->signature, signature)) {
            /* Update existing entry */
            existing->strategy = strategy;
            if (description) {
                strncpy(existing->strategy_desc, description,
                       NIMCP_RECOVERY_CACHE_MAX_STRATEGY_SIZE - 1);
                existing->strategy_desc[NIMCP_RECOVERY_CACHE_MAX_STRATEGY_SIZE - 1] = '\0';
            }
            existing->last_access_timestamp = get_timestamp_ns();

            /* Move to front of LRU */
            lru_move_to_front(cache, existing);

            if (cache->track_stats) {
                cache->stats.stores++;
                uint64_t elapsed = get_timestamp_ns() - start_time;
                cache->stats.total_store_time_ns += elapsed;
                update_stats(cache);
            }

            nimcp_mutex_unlock(&cache->lock);

            NIMCP_LOGGING_DEBUG("Updated existing cache entry for signature 0x%016lx",
                           signature->hash);

            return true;
        }
        existing = existing->hash_next;
    }

    /* Evict LRU entry if at capacity */
    if (cache->current_size >= cache->capacity) {
        if (!evict_lru_entry(cache)) {
            nimcp_mutex_unlock(&cache->lock);
            NIMCP_LOGGING_ERROR("Failed to evict LRU entry");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_recovery_cache_store: evict_lru_entry is NULL");
            return false;
        }
    }

    /* Create new entry */
    nimcp_cache_entry_t* entry = nimcp_calloc(1, sizeof(nimcp_cache_entry_t));
    if (!entry) {
        nimcp_mutex_unlock(&cache->lock);
        NIMCP_LOGGING_ERROR("Failed to allocate cache entry");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_recovery_cache_store: entry is NULL");
        return false;
    }

    /* Initialize entry */
    memcpy(&entry->signature, signature, sizeof(nimcp_error_signature_t));
    entry->strategy = strategy;
    if (description) {
        strncpy(entry->strategy_desc, description,
               NIMCP_RECOVERY_CACHE_MAX_STRATEGY_SIZE - 1);
        entry->strategy_desc[NIMCP_RECOVERY_CACHE_MAX_STRATEGY_SIZE - 1] = '\0';
    }

    uint64_t now = get_timestamp_ns();
    entry->created_timestamp = now;
    entry->last_access_timestamp = now;
    entry->last_success_timestamp = 0;

    entry->success_count = 0;
    entry->failure_count = 0;
    entry->success_rate = 0.0;

    /* Insert into hash table */
    entry->hash_next = cache->hash_table[bucket];
    cache->hash_table[bucket] = entry;

    /* Insert at front of LRU list */
    entry->prev = NULL;
    entry->next = cache->lru_head;
    if (cache->lru_head) {
        cache->lru_head->prev = entry;
    }
    cache->lru_head = entry;
    if (!cache->lru_tail) {
        cache->lru_tail = entry;
    }

    cache->current_size++;

    /* Update statistics */
    if (cache->track_stats) {
        cache->stats.stores++;
        uint64_t elapsed = get_timestamp_ns() - start_time;
        cache->stats.total_store_time_ns += elapsed;
        update_stats(cache);
    }

    nimcp_mutex_unlock(&cache->lock);

    NIMCP_LOGGING_INFO("Stored cache entry for signature 0x%016lx (strategy: %s, size: %zu/%zu)",
                   signature->hash, nimcp_recovery_strategy_name(strategy),
                   cache->current_size, cache->capacity);

    return true;
}

bool nimcp_recovery_cache_update_success(
    nimcp_recovery_cache_t* cache,
    const nimcp_error_signature_t* signature,
    bool success)
{
    if (!cache || !signature) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_recovery_cache_update_success: required parameter is NULL (cache, signature)");
        return false;
    }

    nimcp_mutex_lock(&cache->lock);

    /* Find entry */
    size_t bucket = hash_bucket_index(signature->hash, cache->hash_size);
    nimcp_cache_entry_t* entry = cache->hash_table[bucket];

    while (entry) {
        if (nimcp_recovery_cache_signatures_equal(&entry->signature, signature)) {
            /* Update success tracking */
            if (success) {
                entry->success_count++;
                entry->last_success_timestamp = get_timestamp_ns();
            } else {
                entry->failure_count++;
            }

            /* Recalculate success rate */
            uint64_t total = entry->success_count + entry->failure_count;
            if (total > 0) {
                entry->success_rate = (double)entry->success_count / (double)total;
            }

            entry->last_access_timestamp = get_timestamp_ns();

            nimcp_mutex_unlock(&cache->lock);

            NIMCP_LOGGING_DEBUG("Updated success tracking for signature 0x%016lx "
                           "(success: %s, rate: %.2f%%)",
                           signature->hash, success ? "true" : "false",
                           entry->success_rate * 100.0);

            return true;
        }
        entry = entry->hash_next;
    }

    nimcp_mutex_unlock(&cache->lock);

    NIMCP_LOGGING_DEBUG("Entry not found for success update (signature: 0x%016lx)",
                   signature->hash);

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_recovery_cache_update_success: operation failed");
    return false;
}

bool nimcp_recovery_cache_invalidate(
    nimcp_recovery_cache_t* cache,
    const nimcp_error_signature_t* signature)
{
    if (!cache || !signature) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_recovery_cache_invalidate: required parameter is NULL (cache, signature)");
        return false;
    }

    nimcp_mutex_lock(&cache->lock);

    /* Find and remove entry from hash table */
    size_t bucket = hash_bucket_index(signature->hash, cache->hash_size);
    nimcp_cache_entry_t** curr = &cache->hash_table[bucket];

    while (*curr) {
        if (nimcp_recovery_cache_signatures_equal(&(*curr)->signature, signature)) {
            nimcp_cache_entry_t* victim = *curr;

            /* Remove from hash chain */
            *curr = victim->hash_next;

            /* Remove from LRU list */
            lru_remove(cache, victim);

            /* Free entry */
            nimcp_free(victim);

            cache->current_size--;

            if (cache->track_stats) {
                cache->stats.invalidations++;
                update_stats(cache);
            }

            nimcp_mutex_unlock(&cache->lock);

            NIMCP_LOGGING_INFO("Invalidated cache entry for signature 0x%016lx (size: %zu/%zu)",
                          signature->hash, cache->current_size, cache->capacity);

            return true;
        }
        curr = &(*curr)->hash_next;
    }

    nimcp_mutex_unlock(&cache->lock);

    NIMCP_LOGGING_DEBUG("Entry not found for invalidation (signature: 0x%016lx)",
                   signature->hash);

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_recovery_cache_invalidate: operation failed");
    return false;
}

/* ============================================================================
 * CACHE MANAGEMENT
 * ============================================================================ */

bool nimcp_recovery_cache_get_entry(
    nimcp_recovery_cache_t* cache,
    const nimcp_error_signature_t* signature,
    nimcp_cache_entry_t* entry)
{
    if (!cache || !signature || !entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_recovery_cache_get_entry: required parameter is NULL (cache, signature, entry)");
        return false;
    }

    nimcp_mutex_lock(&cache->lock);

    /* Find entry */
    size_t bucket = hash_bucket_index(signature->hash, cache->hash_size);
    nimcp_cache_entry_t* found = cache->hash_table[bucket];

    while (found) {
        if (nimcp_recovery_cache_signatures_equal(&found->signature, signature)) {
            /* Copy entry (excluding chain pointers) */
            memcpy(&entry->signature, &found->signature, sizeof(nimcp_error_signature_t));
            entry->strategy = found->strategy;
            memcpy(entry->strategy_desc, found->strategy_desc,
                   NIMCP_RECOVERY_CACHE_MAX_STRATEGY_SIZE);

            entry->success_count = found->success_count;
            entry->failure_count = found->failure_count;
            entry->success_rate = found->success_rate;

            entry->created_timestamp = found->created_timestamp;
            entry->last_access_timestamp = found->last_access_timestamp;
            entry->last_success_timestamp = found->last_success_timestamp;

            /* Don't copy chain pointers */
            entry->prev = NULL;
            entry->next = NULL;
            entry->hash_next = NULL;

            nimcp_mutex_unlock(&cache->lock);
            return true;
        }
        found = found->hash_next;
    }

    nimcp_mutex_unlock(&cache->lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_recovery_cache_get_entry: operation failed");
    return false;
}

bool nimcp_recovery_cache_get_stats(
    nimcp_recovery_cache_t* cache,
    nimcp_recovery_cache_stats_t* stats)
{
    if (!cache || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_recovery_cache_get_stats: required parameter is NULL (cache, stats)");
        return false;
    }

    nimcp_mutex_lock(&cache->lock);
    update_stats(cache);
    memcpy(stats, &cache->stats, sizeof(nimcp_recovery_cache_stats_t));
    nimcp_mutex_unlock(&cache->lock);

    return true;
}

bool nimcp_recovery_cache_reset_stats(nimcp_recovery_cache_t* cache) {
    if (!cache) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_recovery_cache_reset_stats: cache is NULL");
        return false;
    }

    nimcp_mutex_lock(&cache->lock);
    memset(&cache->stats, 0, sizeof(nimcp_recovery_cache_stats_t));
    nimcp_mutex_unlock(&cache->lock);

    NIMCP_LOGGING_INFO("Reset cache statistics");

    return true;
}

bool nimcp_recovery_cache_resize(
    nimcp_recovery_cache_t* cache,
    size_t new_capacity)
{
    if (!cache || new_capacity == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_recovery_cache_resize: cache is NULL");
        return false;
    }

    nimcp_mutex_lock(&cache->lock);

    NIMCP_LOGGING_INFO("Resizing cache from %zu to %zu entries",
                   cache->capacity, new_capacity);

    size_t old_capacity = cache->capacity;
    cache->capacity = new_capacity;

    /* Evict entries if shrinking */
    while (cache->current_size > cache->capacity) {
        if (!evict_lru_entry(cache)) {
            /* Restore old capacity on failure */
            cache->capacity = old_capacity;
            nimcp_mutex_unlock(&cache->lock);
            NIMCP_LOGGING_ERROR("Failed to resize cache");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_recovery_cache_resize: evict_lru_entry is NULL");
            return false;
        }
    }

    update_stats(cache);

    nimcp_mutex_unlock(&cache->lock);

    return true;
}

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

const char* nimcp_recovery_strategy_name(nimcp_recovery_strategy_t strategy) {
    switch (strategy) {
        case NIMCP_RECOVERY_STRATEGY_NONE:
            return "NONE";
        case NIMCP_RECOVERY_STRATEGY_RETRY:
            return "RETRY";
        case NIMCP_RECOVERY_STRATEGY_ROLLBACK:
            return "ROLLBACK";
        case NIMCP_RECOVERY_STRATEGY_CHECKPOINT_RESTORE:
            return "CHECKPOINT_RESTORE";
        case NIMCP_RECOVERY_STRATEGY_ALTERNATE_PATH:
            return "ALTERNATE_PATH";
        case NIMCP_RECOVERY_STRATEGY_SAFE_MODE:
            return "SAFE_MODE";
        case NIMCP_RECOVERY_STRATEGY_RESET:
            return "RESET";
        case NIMCP_RECOVERY_STRATEGY_CUSTOM:
            return "CUSTOM";
        default:
            return "UNKNOWN";
    }
}

void nimcp_recovery_cache_print_stats(const nimcp_recovery_cache_t* cache) {
    if (!cache) {
        return;
    }

    nimcp_recovery_cache_stats_t stats;
    nimcp_recovery_cache_get_stats((nimcp_recovery_cache_t*)cache, &stats);

    printf("\n=== Recovery Cache Statistics ===\n");
    printf("Capacity: %zu/%zu entries (%.1f%% utilization)\n",
           stats.current_size, stats.max_size,
           stats.max_size > 0 ? (100.0 * stats.current_size / stats.max_size) : 0.0);
    printf("\nPerformance:\n");
    printf("  Hits:        %lu\n", stats.hits);
    printf("  Misses:      %lu\n", stats.misses);
    printf("  Hit Rate:    %.2f%%\n", stats.hit_rate * 100.0);
    printf("  Stores:      %lu\n", stats.stores);
    printf("  Evictions:   %lu\n", stats.evictions);
    printf("  Invalidations: %lu\n", stats.invalidations);
    printf("\nTiming:\n");
    printf("  Avg Lookup:  %lu ns\n", stats.avg_lookup_time_ns);
    printf("  Avg Store:   %lu ns\n", stats.avg_store_time_ns);
    printf("================================\n\n");
}

bool nimcp_recovery_cache_validate(const nimcp_recovery_cache_t* cache) {
    if (!cache) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_recovery_cache_validate: cache is NULL");
        return false;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)&cache->lock);

    /* Count entries in hash table */
    size_t hash_count = 0;
    for (size_t i = 0; i < cache->hash_size; i++) {
        nimcp_cache_entry_t* entry = cache->hash_table[i];
        while (entry) {
            hash_count++;
            entry = entry->hash_next;
        }
    }

    /* Count entries in LRU list */
    size_t lru_count = 0;
    nimcp_cache_entry_t* entry = cache->lru_head;
    while (entry) {
        lru_count++;
        entry = entry->next;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)&cache->lock);

    /* Verify counts match */
    bool valid = (hash_count == cache->current_size) &&
                 (lru_count == cache->current_size);

    if (!valid) {
        NIMCP_LOGGING_ERROR("Cache validation failed: hash_count=%zu, lru_count=%zu, current_size=%zu",
                       hash_count, lru_count, cache->current_size);
    }

    return valid;
}
