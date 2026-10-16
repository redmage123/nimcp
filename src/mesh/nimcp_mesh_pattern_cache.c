/**
 * @file nimcp_mesh_pattern_cache.c
 * @brief Copy-on-Write Pattern Cache Implementation
 *
 * WHAT: High-performance pattern activation cache with CoW semantics
 * WHY:  Pattern matching is expensive, caching avoids recomputation
 * HOW:  Uses nimcp_hash_table for indexing and nimcp_cache for CoW
 *
 * INTEGRATION:
 * - Uses utils/containers/nimcp_hash_table.h for hash-based lookups
 * - Uses utils/cache/nimcp_cache.h for CoW memory semantics
 * - Immune System: Cache failures → antigen presentation
 * - Health Agent: Monitors cache efficiency
 * - Logging: Full audit trail of cache operations
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 2.0.0
 */

#include "mesh/nimcp_mesh_pattern_cache.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/containers/nimcp_hash_table.h"
#include "utils/cache/nimcp_cache.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Hash key for pattern lookup (128-bit pattern hash as uint64 pair)
 */
typedef struct pattern_key {
    uint64_t h1;
    uint64_t h2;
} pattern_key_t;

/**
 * @brief Internal wrapper for cache entries (managed by nimcp_cache)
 */
typedef struct cow_entry_wrapper {
    pattern_cache_entry_t entry;
    bool is_cow_copy;           /**< True if this is a CoW copy */
} cow_entry_wrapper_t;

/**
 * @brief Pattern cache internal state
 */
struct pattern_cache {
    uint32_t magic;
    pattern_cache_config_t config;

    /* Hash table: pattern_hash -> cow_entry_wrapper_t* (managed by nimcp_cache) */
    hash_table_t* table;

    /* LRU tracking (index-based doubly-linked list) */
    struct {
        pattern_hash_t* keys;       /**< Ordered keys for LRU */
        size_t capacity;
        size_t head;                /**< Most recently used */
        size_t tail;                /**< Least recently used */
        size_t count;
    } lru;

    /* Statistics */
    pattern_cache_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;
    nimcp_rwlock_t rwlock;
    bool rwlock_initialized;

    /* Immune integration */
    brain_immune_system_t* immune;

    /* Health monitoring */
    nimcp_health_agent_t* health;
};

/* ============================================================================
 * Hash Functions
 * ============================================================================ */

/**
 * @brief FNV-1a hash for pattern vectors (used internally)
 */
static uint64_t fnv1a_hash(const void* data, size_t len) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint64_t hash = 14695981039346656037ULL;  /* FNV offset basis */

    for (size_t i = 0; i < len; i++) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;  /* FNV prime */
    }

    return hash;
}

/**
 * @brief Custom hash function for hash_table_t
 *
 * Converts pattern_hash_t to uint32 for hash table indexing
 */
static uint32_t pattern_hash_fn(const void* key, size_t key_size) {
    (void)key_size;
    const pattern_hash_t* ph = (const pattern_hash_t*)key;
    uint64_t h;
    memcpy(&h, ph->bytes, 8);
    /* Mix high and low bits */
    return (uint32_t)(h ^ (h >> 32));
}

/**
 * @brief Custom key comparison for hash_table_t
 */
static bool pattern_key_compare(const void* key1, size_t key1_size,
                                 const void* key2, size_t key2_size) {
    (void)key1_size;
    (void)key2_size;
    const pattern_hash_t* a = (const pattern_hash_t*)key1;
    const pattern_hash_t* b = (const pattern_hash_t*)key2;
    return memcmp(a->bytes, b->bytes, PATTERN_HASH_SIZE) == 0;
}

/**
 * @brief Value destructor for hash_table_t - releases nimcp_cache reference
 */
static void cache_entry_destructor(void* value, size_t value_size) {
    (void)value_size;
    cow_entry_wrapper_t** wrapper_ptr = (cow_entry_wrapper_t**)value;
    if (wrapper_ptr && *wrapper_ptr) {
        /* Release the CoW-managed memory */
        nimcp_cache_release(*wrapper_ptr);
    }
}

nimcp_error_t pattern_cache_hash(
    const mesh_pattern_t* pattern,
    pattern_hash_t* hash_out
) {
    if (!pattern || !hash_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_pattern_cache: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Hash the pattern vector using two FNV-1a passes for 128-bit result */
    uint64_t h1 = fnv1a_hash(pattern->vector, sizeof(pattern->vector));
    uint64_t h2 = fnv1a_hash(&pattern->magnitude, sizeof(pattern->magnitude));
    h2 = h2 * 31 + pattern->active_dims;

    /* Combine into 128-bit hash */
    memcpy(hash_out->bytes, &h1, 8);
    memcpy(hash_out->bytes + 8, &h2, 8);

    return NIMCP_SUCCESS;
}

bool pattern_hash_equals(const pattern_hash_t* a, const pattern_hash_t* b) {
    if (!a || !b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_hash_equals: required parameter is NULL (a, b)");
        return false;
    }
    return memcmp(a->bytes, b->bytes, PATTERN_HASH_SIZE) == 0;
}

void pattern_hash_to_string(const pattern_hash_t* hash, char* buf, size_t buf_size) {
    if (!hash || !buf || buf_size < 33) {
        if (buf && buf_size > 0) buf[0] = '\0';
        return;
    }

    for (size_t i = 0; i < PATTERN_HASH_SIZE && (i * 2 + 2) < buf_size; i++) {
        snprintf(buf + i * 2, 3, "%02x", hash->bytes[i]);
    }
}

/**
 * @brief Convert pattern hash to key string for hash table
 */
static void hash_to_key_string(const pattern_hash_t* hash, char* key, size_t key_size) {
    pattern_hash_to_string(hash, key, key_size);
}

/* ============================================================================
 * Pattern Comparison
 * ============================================================================ */

static bool patterns_similar(
    const mesh_pattern_t* a,
    const mesh_pattern_t* b,
    float tolerance
) {
    if (!a || !b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "patterns_similar: required parameter is NULL (a, b)");
        return false;
    }

    float diff_sum = 0.0f;
    for (int i = 0; i < MESH_PATTERN_DIM; i++) {
        float d = a->vector[i] - b->vector[i];
        diff_sum += d * d;
    }

    float dist = sqrtf(diff_sum);
    return dist <= tolerance;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

nimcp_error_t pattern_cache_default_config(pattern_cache_config_t* config) {
    if (!config) return NIMCP_ERROR_NULL_POINTER;

    memset(config, 0, sizeof(*config));
    config->max_entries = PATTERN_CACHE_MAX_ENTRIES;
    config->default_ttl_ms = PATTERN_CACHE_DEFAULT_TTL_MS;
    config->enable_cow = true;
    config->enable_lru = true;
    config->enable_logging = true;
    config->similarity_tolerance = 0.001f;

    return NIMCP_SUCCESS;
}

pattern_cache_t* pattern_cache_create(const pattern_cache_config_t* config) {
    /* Initialize nimcp_cache system (idempotent) */
    nimcp_cache_init();

    pattern_cache_t* cache = nimcp_calloc(1, sizeof(pattern_cache_t));
    if (!cache) {
        LOG_ERROR("Failed to allocate pattern cache");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pattern_cache_create: cache is NULL");
        return NULL;
    }

    cache->magic = PATTERN_CACHE_MAGIC;

    if (config) {
        cache->config = *config;
    } else {
        pattern_cache_default_config(&cache->config);
    }

    /* Create hash table with custom hash/compare for pattern_hash_t keys */
    hash_table_config_t ht_config = {
        .initial_buckets = cache->config.max_entries / 4,
        .key_type = HASH_KEY_CUSTOM,
        .hash_algorithm = HASH_ALG_CUSTOM,
        .custom_hash_fn = pattern_hash_fn,
        .custom_compare_fn = pattern_key_compare,
        .value_destructor = cache_entry_destructor,
        .case_insensitive = false,
        .thread_safe = false  /* We handle our own locking */
    };

    if (ht_config.initial_buckets < 16) {
        ht_config.initial_buckets = 16;
    }

    cache->table = hash_table_create(&ht_config);
    if (!cache->table) {
        LOG_ERROR("Failed to create pattern cache hash table");
        nimcp_free(cache);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pattern_cache_create: cache->table is NULL");
        return NULL;
    }

    /* Initialize LRU tracking */
    cache->lru.capacity = cache->config.max_entries;
    cache->lru.keys = nimcp_calloc(cache->lru.capacity, sizeof(pattern_hash_t));
    if (!cache->lru.keys) {
        hash_table_destroy(cache->table);
        nimcp_free(cache);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pattern_cache_create: cache->lru is NULL");
        return NULL;
    }
    cache->lru.head = 0;
    cache->lru.tail = 0;
    cache->lru.count = 0;

    /* Create mutex */
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_RECURSIVE;
    cache->mutex = nimcp_mutex_create(&attr);
    if (!cache->mutex) {
        nimcp_free(cache->lru.keys);
        hash_table_destroy(cache->table);
        nimcp_free(cache);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pattern_cache_create: cache->mutex is NULL");
        return NULL;
    }

    /* Initialize rwlock for concurrent reads */
    if (nimcp_rwlock_init(&cache->rwlock) != 0) {
        nimcp_mutex_destroy(cache->mutex);
        nimcp_free(cache->lru.keys);
        hash_table_destroy(cache->table);
        nimcp_free(cache);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "pattern_cache_create: validation failed");
        return NULL;
    }
    cache->rwlock_initialized = true;

    if (cache->config.enable_logging) {
        LOG_INFO("Created pattern cache using nimcp_hash_table + nimcp_cache "
                 "(max=%zu, ttl=%lums, cow=%s)",
                 cache->config.max_entries,
                 (unsigned long)cache->config.default_ttl_ms,
                 cache->config.enable_cow ? "on" : "off");
    }

    return cache;
}

void pattern_cache_destroy(pattern_cache_t* cache) {
    if (!cache || cache->magic != PATTERN_CACHE_MAGIC) return;

    if (cache->rwlock_initialized) {
        nimcp_rwlock_destroy(&cache->rwlock);
    }
    if (cache->mutex) {
        nimcp_mutex_destroy(cache->mutex);
    }
    if (cache->lru.keys) {
        nimcp_free(cache->lru.keys);
    }
    if (cache->table) {
        /* hash_table_destroy calls value_destructor which releases nimcp_cache refs */
        hash_table_destroy(cache->table);
    }

    cache->magic = 0;
    nimcp_free(cache);

    LOG_DEBUG("Destroyed pattern cache");
}

nimcp_error_t pattern_cache_clear(pattern_cache_t* cache) {
    if (!cache || cache->magic != PATTERN_CACHE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pattern_cache_clear: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(cache->mutex);

    /* Clear hash table (calls value destructors to release nimcp_cache refs) */
    hash_table_clear(cache->table);

    /* Reset LRU */
    cache->lru.head = 0;
    cache->lru.tail = 0;
    cache->lru.count = 0;

    /* Reset stats */
    cache->stats.invalidations += cache->stats.current_entries;
    cache->stats.current_entries = 0;

    nimcp_mutex_unlock(cache->mutex);

    if (cache->config.enable_logging) {
        LOG_DEBUG("Cleared pattern cache");
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Cache Operations Implementation
 * ============================================================================ */

/**
 * @brief Find entry in cache using hash table lookup
 */
static cow_entry_wrapper_t* find_entry_wrapper(
    pattern_cache_t* cache,
    const pattern_hash_t* hash,
    const mesh_pattern_t* pattern
) {
    /* Use string key for hash table (32 hex chars) */
    char key_str[33];
    hash_to_key_string(hash, key_str, sizeof(key_str));

    cow_entry_wrapper_t** wrapper_ptr = hash_table_lookup_string(cache->table, key_str);
    if (!wrapper_ptr || !*wrapper_ptr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_entry_wrapper: wrapper_ptr is NULL");
        return NULL;
    }

    cow_entry_wrapper_t* wrapper = *wrapper_ptr;
    if (!wrapper->entry.valid) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_entry_wrapper: wrapper->entry is NULL");
        return NULL;
    }

    /* Verify with actual pattern comparison to handle hash collisions */
    if (!patterns_similar(&wrapper->entry.pattern, pattern,
                          cache->config.similarity_tolerance)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_entry_wrapper: wrapper->entry is NULL");
        return NULL;
    }

    return wrapper;
}

/**
 * @brief Allocate a new entry using nimcp_cache for CoW semantics
 */
static cow_entry_wrapper_t* allocate_cow_entry(pattern_cache_t* cache) {
    /* Check if we need to evict */
    if (cache->lru.count >= cache->config.max_entries) {
        if (cache->config.enable_lru) {
            pattern_cache_evict_lru(cache, 1);
        } else {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "allocate_cow_entry: validation failed");
            return NULL;
        }
    }

    /* Allocate using nimcp_cache for CoW support */
    cow_entry_wrapper_t* wrapper = nimcp_cache_calloc(1, sizeof(cow_entry_wrapper_t));
    if (!wrapper) {
        LOG_ERROR("Failed to allocate CoW entry wrapper");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "allocate_cow_entry: wrapper is NULL");
        return NULL;
    }

    wrapper->is_cow_copy = false;
    return wrapper;
}

/**
 * @brief Update LRU tracking for an entry
 */
static void update_lru(pattern_cache_t* cache, const pattern_hash_t* hash) {
    /* Simple LRU: we track access times in the entries themselves */
    /* For full LRU list implementation, we'd maintain a doubly-linked list */
    (void)cache;
    (void)hash;
    /* Access time is updated directly in the entry */
}

nimcp_error_t pattern_cache_lookup(
    pattern_cache_t* cache,
    const mesh_pattern_t* pattern,
    cached_activation_t* activations_out,
    size_t max_activations,
    size_t* count_out
) {
    if (!cache || !pattern || !activations_out || !count_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_pattern_cache: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    *count_out = 0;

    pattern_hash_t hash;
    pattern_cache_hash(pattern, &hash);

    nimcp_rwlock_rdlock(&cache->rwlock);

    cow_entry_wrapper_t* wrapper = find_entry_wrapper(cache, &hash, pattern);

    if (!wrapper) {
        nimcp_rwlock_unlock(&cache->rwlock);
        cache->stats.misses++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_pattern_cache: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    pattern_cache_entry_t* entry = &wrapper->entry;

    /* Check TTL */
    uint64_t now = nimcp_time_now_ns();
    if (entry->ttl_ns > 0 && now > entry->created_ns + entry->ttl_ns) {
        nimcp_rwlock_unlock(&cache->rwlock);
        cache->stats.misses++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_pattern_cache: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Copy results */
    size_t copy_count = entry->activation_count;
    if (copy_count > max_activations) copy_count = max_activations;

    memcpy(activations_out, entry->activations,
           copy_count * sizeof(cached_activation_t));
    *count_out = copy_count;

    nimcp_rwlock_unlock(&cache->rwlock);

    /* Update LRU and access time (requires write lock) */
    nimcp_mutex_lock(cache->mutex);
    entry->last_access_ns = nimcp_time_now_ns();
    update_lru(cache, &hash);
    cache->stats.hits++;
    nimcp_mutex_unlock(cache->mutex);

    /* Update hit rate */
    uint64_t total = cache->stats.hits + cache->stats.misses;
    if (total > 0) {
        cache->stats.hit_rate = (float)cache->stats.hits / (float)total;
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t pattern_cache_store(
    pattern_cache_t* cache,
    const mesh_pattern_t* pattern,
    const cached_activation_t* activations,
    size_t count,
    uint64_t ttl_ms
) {
    if (!cache || !pattern || !activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_pattern_cache: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (count > MESH_MAX_ENDORSERS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_pattern_cache: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    pattern_hash_t hash;
    pattern_cache_hash(pattern, &hash);

    char key_str[33];
    hash_to_key_string(&hash, key_str, sizeof(key_str));

    nimcp_mutex_lock(cache->mutex);

    /* Check if already exists */
    cow_entry_wrapper_t* wrapper = find_entry_wrapper(cache, &hash, pattern);
    pattern_cache_entry_t* entry;
    bool is_new = false;

    if (!wrapper) {
        /* Allocate new entry using nimcp_cache */
        wrapper = allocate_cow_entry(cache);
        if (!wrapper) {
            nimcp_mutex_unlock(cache->mutex);
            LOG_WARN("Pattern cache full, cannot store");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_pattern_cache: memory allocation failed");
            return NIMCP_ERROR_NO_MEMORY;
        }
        is_new = true;
    } else {
        /* Entry exists - if CoW is enabled and shared, make writable copy */
        if (cache->config.enable_cow && nimcp_cache_is_shared(wrapper)) {
            wrapper = nimcp_cache_make_writable(wrapper);
            if (!wrapper) {
                nimcp_mutex_unlock(cache->mutex);
                LOG_ERROR("Failed to make CoW copy for update");
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_pattern_cache: memory allocation failed");
                return NIMCP_ERROR_NO_MEMORY;
            }
            cache->stats.cow_copies++;
        }
    }

    entry = &wrapper->entry;

    /* Fill entry */
    entry->hash = hash;
    entry->pattern = *pattern;
    memcpy(entry->activations, activations, count * sizeof(cached_activation_t));
    entry->activation_count = count;
    entry->ref_count = 0;
    entry->version++;
    entry->created_ns = nimcp_time_now_ns();
    entry->last_access_ns = entry->created_ns;
    entry->ttl_ns = (ttl_ms > 0 ? ttl_ms : cache->config.default_ttl_ms) * 1000000ULL;
    entry->valid = true;
    entry->cow_pending = false;

    if (is_new) {
        /* Insert into hash table (stores pointer to CoW-managed wrapper) */
        if (!hash_table_insert_string(cache->table, key_str, &wrapper, sizeof(wrapper))) {
            nimcp_cache_release(wrapper);
            nimcp_mutex_unlock(cache->mutex);
            LOG_ERROR("Failed to insert into pattern cache hash table");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_pattern_cache: memory allocation failed");
            return NIMCP_ERROR_NO_MEMORY;
        }

        cache->lru.count++;
        cache->stats.current_entries++;
        if (cache->stats.current_entries > cache->stats.peak_entries) {
            cache->stats.peak_entries = cache->stats.current_entries;
        }

        /* Record the reference in nimcp_cache stats */
        nimcp_cache_record_reference(sizeof(cow_entry_wrapper_t));
    }

    nimcp_mutex_unlock(cache->mutex);

    if (cache->config.enable_logging) {
        LOG_DEBUG("Cached pattern %s with %zu activations", key_str, count);
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t pattern_cache_invalidate(
    pattern_cache_t* cache,
    const mesh_pattern_t* pattern
) {
    if (!cache || !pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_pattern_cache: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    pattern_hash_t hash;
    pattern_cache_hash(pattern, &hash);

    char key_str[33];
    hash_to_key_string(&hash, key_str, sizeof(key_str));

    nimcp_mutex_lock(cache->mutex);

    cow_entry_wrapper_t* wrapper = find_entry_wrapper(cache, &hash, pattern);

    if (wrapper) {
        wrapper->entry.valid = false;
        cache->stats.invalidations++;
        cache->stats.current_entries--;

        /* Remove from hash table if no references */
        if (wrapper->entry.ref_count == 0) {
            hash_table_remove_string(cache->table, key_str);
            cache->lru.count--;
        }
    }

    nimcp_mutex_unlock(cache->mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Iterator context for module invalidation
 */
typedef struct module_invalidate_ctx {
    pattern_cache_t* cache;
    mesh_participant_id_t module_id;
    char** keys_to_remove;
    size_t remove_count;
    size_t remove_capacity;
} module_invalidate_ctx_t;

/**
 * @brief Iterator callback to find entries referencing a module
 */
static bool find_module_entries(const void* key, size_t key_size,
                                  void* value, size_t value_size,
                                  void* user_data) {
    (void)key_size;
    (void)value_size;

    module_invalidate_ctx_t* ctx = (module_invalidate_ctx_t*)user_data;
    const char* key_str = (const char*)key;
    cow_entry_wrapper_t** wrapper_ptr = (cow_entry_wrapper_t**)value;

    if (!wrapper_ptr || !*wrapper_ptr) return true;

    cow_entry_wrapper_t* wrapper = *wrapper_ptr;
    pattern_cache_entry_t* entry = &wrapper->entry;

    if (!entry->valid) return true;

    /* Check if any activation references this module */
    for (size_t j = 0; j < entry->activation_count; j++) {
        if (entry->activations[j].module_id == ctx->module_id) {
            /* Mark for removal (can't remove during iteration) */
            if (ctx->remove_count < ctx->remove_capacity) {
                size_t len = strlen(key_str) + 1;
                ctx->keys_to_remove[ctx->remove_count] = nimcp_calloc(1, len);
                if (ctx->keys_to_remove[ctx->remove_count]) {
                    memcpy(ctx->keys_to_remove[ctx->remove_count], key_str, len);
                    ctx->remove_count++;
                }
            }
            break;
        }
    }

    return true;  /* Continue iteration */
}

nimcp_error_t pattern_cache_invalidate_module(
    pattern_cache_t* cache,
    mesh_participant_id_t module_id
) {
    if (!cache) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(cache->mutex);

    /* Prepare context for iteration */
    module_invalidate_ctx_t ctx = {
        .cache = cache,
        .module_id = module_id,
        .remove_count = 0,
        .remove_capacity = 256  /* Max entries to invalidate in one pass */
    };

    ctx.keys_to_remove = nimcp_calloc(ctx.remove_capacity, sizeof(char*));
    if (!ctx.keys_to_remove) {
        nimcp_mutex_unlock(cache->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_pattern_cache: memory allocation failed");
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Find all entries referencing this module */
    hash_table_iterate(cache->table, find_module_entries, &ctx);

    /* Remove found entries */
    for (size_t i = 0; i < ctx.remove_count; i++) {
        if (ctx.keys_to_remove[i]) {
            hash_table_remove_string(cache->table, ctx.keys_to_remove[i]);
            nimcp_free(ctx.keys_to_remove[i]);
            cache->stats.invalidations++;
            cache->stats.current_entries--;
            cache->lru.count--;
        }
    }

    size_t invalidated = ctx.remove_count;
    nimcp_free(ctx.keys_to_remove);

    nimcp_mutex_unlock(cache->mutex);

    if (cache->config.enable_logging && invalidated > 0) {
        LOG_DEBUG("Invalidated %zu cache entries for module 0x%lx",
                  invalidated, (unsigned long)module_id);
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Copy-on-Write Implementation (using nimcp_cache)
 * ============================================================================ */

const pattern_cache_entry_t* pattern_cache_acquire(
    pattern_cache_t* cache,
    const mesh_pattern_t* pattern
) {
    if (!cache || !pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_cache_acquire: required parameter is NULL (cache, pattern)");
        return NULL;
    }

    pattern_hash_t hash;
    pattern_cache_hash(pattern, &hash);

    nimcp_mutex_lock(cache->mutex);

    cow_entry_wrapper_t* wrapper = find_entry_wrapper(cache, &hash, pattern);

    if (wrapper && wrapper->entry.valid) {
        /* Use nimcp_cache_reference for CoW reference counting */
        wrapper = nimcp_cache_reference(wrapper);
        if (wrapper) {
            wrapper->entry.ref_count++;
            wrapper->entry.last_access_ns = nimcp_time_now_ns();
            update_lru(cache, &hash);
        }
    } else {
        wrapper = NULL;
    }

    nimcp_mutex_unlock(cache->mutex);

    return wrapper ? &wrapper->entry : NULL;
}

void pattern_cache_release(
    pattern_cache_t* cache,
    const pattern_cache_entry_t* entry
) {
    if (!cache || !entry) return;

    /* Get the wrapper from the entry pointer */
    cow_entry_wrapper_t* wrapper = (cow_entry_wrapper_t*)
        ((char*)entry - offsetof(cow_entry_wrapper_t, entry));

    nimcp_mutex_lock(cache->mutex);

    if (wrapper->entry.ref_count > 0) {
        wrapper->entry.ref_count--;
    }

    /* If invalidated and no more refs, let hash_table removal handle cleanup */
    if (!wrapper->entry.valid && wrapper->entry.ref_count == 0) {
        char key_str[33];
        hash_to_key_string(&wrapper->entry.hash, key_str, sizeof(key_str));
        hash_table_remove_string(cache->table, key_str);
        cache->lru.count--;
    }

    nimcp_mutex_unlock(cache->mutex);

    /* Release nimcp_cache reference */
    nimcp_cache_release(wrapper);
}

nimcp_error_t pattern_cache_cow_copy(
    pattern_cache_t* cache,
    const mesh_pattern_t* original,
    const mesh_pattern_t* variant,
    pattern_cache_entry_t** new_entry_out
) {
    if (!cache || !original || !variant || !new_entry_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_pattern_cache: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!cache->config.enable_cow) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_SUPPORTED, "mesh_pattern_cache: error condition");
        return NIMCP_ERROR_NOT_SUPPORTED;
    }

    pattern_hash_t orig_hash;
    pattern_cache_hash(original, &orig_hash);

    nimcp_mutex_lock(cache->mutex);

    cow_entry_wrapper_t* orig_wrapper = find_entry_wrapper(cache, &orig_hash, original);

    if (!orig_wrapper || !orig_wrapper->entry.valid) {
        nimcp_mutex_unlock(cache->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_pattern_cache: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Use nimcp_cache_force_copy to create independent CoW copy */
    cow_entry_wrapper_t* new_wrapper = nimcp_cache_force_copy(orig_wrapper);
    if (!new_wrapper) {
        nimcp_mutex_unlock(cache->mutex);
        LOG_ERROR("Failed to create CoW copy via nimcp_cache");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_pattern_cache: memory allocation failed");
        return NIMCP_ERROR_NO_MEMORY;
    }

    new_wrapper->is_cow_copy = true;
    pattern_cache_entry_t* new_entry = &new_wrapper->entry;

    /* Update for variant */
    pattern_cache_hash(variant, &new_entry->hash);
    new_entry->pattern = *variant;
    new_entry->ref_count = 0;
    new_entry->version = 1;
    new_entry->created_ns = nimcp_time_now_ns();
    new_entry->last_access_ns = new_entry->created_ns;
    new_entry->cow_pending = false;

    /* Add to hash table with new key */
    char key_str[33];
    hash_to_key_string(&new_entry->hash, key_str, sizeof(key_str));

    if (!hash_table_insert_string(cache->table, key_str, &new_wrapper, sizeof(new_wrapper))) {
        nimcp_cache_release(new_wrapper);
        nimcp_mutex_unlock(cache->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_pattern_cache: memory allocation failed");
        return NIMCP_ERROR_NO_MEMORY;
    }

    cache->lru.count++;
    cache->stats.current_entries++;
    cache->stats.cow_copies++;

    *new_entry_out = new_entry;

    nimcp_mutex_unlock(cache->mutex);

    if (cache->config.enable_logging) {
        LOG_DEBUG("Created CoW copy of pattern cache entry via nimcp_cache");
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Maintenance Implementation
 * ============================================================================ */

/**
 * @brief Iterator context for expired entry collection
 */
typedef struct expired_ctx {
    pattern_cache_t* cache;
    uint64_t now_ns;
    char** keys_to_remove;
    size_t remove_count;
    size_t remove_capacity;
} expired_ctx_t;

/**
 * @brief Iterator callback to find expired entries
 */
static bool find_expired_entries(const void* key, size_t key_size,
                                   void* value, size_t value_size,
                                   void* user_data) {
    (void)key_size;
    (void)value_size;

    expired_ctx_t* ctx = (expired_ctx_t*)user_data;
    const char* key_str = (const char*)key;
    cow_entry_wrapper_t** wrapper_ptr = (cow_entry_wrapper_t**)value;

    if (!wrapper_ptr || !*wrapper_ptr) return true;

    cow_entry_wrapper_t* wrapper = *wrapper_ptr;
    pattern_cache_entry_t* entry = &wrapper->entry;

    if (!entry->valid) return true;
    if (entry->ref_count > 0) return true;  /* Don't evict referenced entries */

    if (entry->ttl_ns > 0 && ctx->now_ns > entry->created_ns + entry->ttl_ns) {
        /* Mark for removal */
        if (ctx->remove_count < ctx->remove_capacity) {
            size_t len = strlen(key_str) + 1;
            ctx->keys_to_remove[ctx->remove_count] = nimcp_calloc(1, len);
            if (ctx->keys_to_remove[ctx->remove_count]) {
                memcpy(ctx->keys_to_remove[ctx->remove_count], key_str, len);
                ctx->remove_count++;
            }
        }
    }

    return true;
}

nimcp_error_t pattern_cache_evict_expired(pattern_cache_t* cache) {
    if (!cache) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(cache->mutex);

    expired_ctx_t ctx = {
        .cache = cache,
        .now_ns = nimcp_time_now_ns(),
        .remove_count = 0,
        .remove_capacity = 256
    };

    ctx.keys_to_remove = nimcp_calloc(ctx.remove_capacity, sizeof(char*));
    if (!ctx.keys_to_remove) {
        nimcp_mutex_unlock(cache->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pattern_cache_evict_expired: memory allocation failed");
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Find all expired entries */
    hash_table_iterate(cache->table, find_expired_entries, &ctx);

    /* Remove found entries */
    for (size_t i = 0; i < ctx.remove_count; i++) {
        if (ctx.keys_to_remove[i]) {
            hash_table_remove_string(cache->table, ctx.keys_to_remove[i]);
            nimcp_free(ctx.keys_to_remove[i]);
            cache->stats.evictions++;
            cache->stats.current_entries--;
            cache->lru.count--;
        }
    }

    size_t evicted = ctx.remove_count;
    nimcp_free(ctx.keys_to_remove);

    nimcp_mutex_unlock(cache->mutex);

    if (cache->config.enable_logging && evicted > 0) {
        LOG_DEBUG("Evicted %zu expired cache entries", evicted);
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Iterator context for LRU eviction
 */
typedef struct lru_ctx {
    uint64_t oldest_time;
    char oldest_key[33];
    bool found;
} lru_ctx_t;

/**
 * @brief Iterator callback to find oldest entry
 */
static bool find_oldest_entry(const void* key, size_t key_size,
                                void* value, size_t value_size,
                                void* user_data) {
    (void)key_size;
    (void)value_size;

    lru_ctx_t* ctx = (lru_ctx_t*)user_data;
    const char* key_str = (const char*)key;
    cow_entry_wrapper_t** wrapper_ptr = (cow_entry_wrapper_t**)value;

    if (!wrapper_ptr || !*wrapper_ptr) return true;

    cow_entry_wrapper_t* wrapper = *wrapper_ptr;
    pattern_cache_entry_t* entry = &wrapper->entry;

    if (!entry->valid) return true;
    if (entry->ref_count > 0) return true;  /* Don't evict referenced entries */

    if (entry->last_access_ns < ctx->oldest_time) {
        ctx->oldest_time = entry->last_access_ns;
        strncpy(ctx->oldest_key, key_str, sizeof(ctx->oldest_key) - 1);
        ctx->oldest_key[sizeof(ctx->oldest_key) - 1] = '\0';
        ctx->found = true;
    }

    return true;
}

nimcp_error_t pattern_cache_evict_lru(pattern_cache_t* cache, size_t count) {
    if (!cache) return NIMCP_ERROR_NULL_POINTER;
    if (count == 0) return NIMCP_SUCCESS;

    nimcp_mutex_lock(cache->mutex);

    size_t evicted = 0;

    /* Simple LRU: find and evict oldest entries one by one */
    while (evicted < count && cache->stats.current_entries > 0) {
        lru_ctx_t ctx = {
            .oldest_time = UINT64_MAX,
            .found = false
        };
        ctx.oldest_key[0] = '\0';

        hash_table_iterate(cache->table, find_oldest_entry, &ctx);

        if (!ctx.found) break;  /* No evictable entries */

        hash_table_remove_string(cache->table, ctx.oldest_key);
        cache->stats.evictions++;
        cache->stats.current_entries--;
        cache->lru.count--;
        evicted++;
    }

    nimcp_mutex_unlock(cache->mutex);

    if (cache->config.enable_logging && evicted > 0) {
        LOG_DEBUG("LRU evicted %zu cache entries", evicted);
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t pattern_cache_get_stats(
    pattern_cache_t* cache,
    pattern_cache_stats_t* stats
) {
    if (!cache || !stats) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(cache->mutex);
    *stats = cache->stats;
    nimcp_mutex_unlock(cache->mutex);

    return NIMCP_SUCCESS;
}

void pattern_cache_print(const pattern_cache_t* cache) {
    if (!cache) {
        printf("Pattern Cache: NULL\n");
        return;
    }

    /* Also print nimcp_cache stats for CoW tracking */
    nimcp_cache_stats_t cow_stats;
    nimcp_cache_get_stats(&cow_stats);

    printf("Pattern Cache (using nimcp_hash_table + nimcp_cache):\n");
    printf("  Entries: %zu / %zu\n",
           cache->stats.current_entries, cache->config.max_entries);
    printf("  Hash Table Size: %zu\n", hash_table_size(cache->table));
    printf("  Hits:    %lu\n", (unsigned long)cache->stats.hits);
    printf("  Misses:  %lu\n", (unsigned long)cache->stats.misses);
    printf("  Hit Rate: %.1f%%\n", cache->stats.hit_rate * 100.0f);
    printf("  Evictions: %lu\n", (unsigned long)cache->stats.evictions);
    printf("  CoW Copies: %lu\n", (unsigned long)cache->stats.cow_copies);
    printf("  nimcp_cache Memory Saved: %zu bytes\n", cow_stats.memory_saved);
}
