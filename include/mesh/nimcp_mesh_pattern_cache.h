/**
 * @file nimcp_mesh_pattern_cache.h
 * @brief Copy-on-Write Pattern Cache for Mesh Routing
 *
 * WHAT: Caches computed pattern similarities and activations with CoW semantics
 * WHY:  Avoid recomputing expensive pattern matching for repeated queries
 * HOW:  Hash-based cache with CoW for concurrent access and versioning
 *
 * BRAIN ANALOGY:
 * ```
 *   Pattern recognition caching in visual cortex:
 *   - First time: Compute from scratch
 *   - Repeated: Instant recognition from cache
 *   - Modified: Copy-on-write for variants
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_PATTERN_CACHE_H
#define NIMCP_MESH_PATTERN_CACHE_H

#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_pattern_routing.h"
#include "mesh/nimcp_mesh_endorsement.h"
#include "utils/error/nimcp_error_codes.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum cache entries */
#define PATTERN_CACHE_MAX_ENTRIES       4096

/** @brief Pattern hash size (128-bit) */
#define PATTERN_HASH_SIZE               16

/** @brief Cache entry TTL in milliseconds */
#define PATTERN_CACHE_DEFAULT_TTL_MS    60000

/** @brief Cache magic number */
#define PATTERN_CACHE_MAGIC             0x50434143  /* "PCAC" */

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @brief Pattern hash (128-bit for collision resistance)
 */
typedef struct pattern_hash {
    uint8_t bytes[PATTERN_HASH_SIZE];
} pattern_hash_t;

/**
 * @brief Cached activation result
 */
typedef struct cached_activation {
    mesh_participant_id_t module_id;
    float activation_level;
    float similarity;
    endorser_role_t role;
    bool should_endorse;
} cached_activation_t;

/**
 * @brief Cache entry
 */
typedef struct pattern_cache_entry {
    pattern_hash_t hash;                /**< Pattern hash key */
    mesh_pattern_t pattern;             /**< Original pattern (for verification) */

    /* Cached results */
    cached_activation_t activations[MESH_MAX_ENDORSERS];
    size_t activation_count;

    /* CoW metadata */
    uint32_t ref_count;                 /**< Reference count */
    uint32_t version;                   /**< Version number */
    uint64_t created_ns;                /**< Creation timestamp */
    uint64_t last_access_ns;            /**< Last access timestamp */
    uint64_t ttl_ns;                    /**< Time-to-live */

    bool valid;                         /**< Entry is valid */
    bool cow_pending;                   /**< Copy-on-write pending */
} pattern_cache_entry_t;

/**
 * @brief Cache statistics
 */
typedef struct pattern_cache_stats {
    uint64_t hits;                      /**< Cache hits */
    uint64_t misses;                    /**< Cache misses */
    uint64_t evictions;                 /**< Evictions (LRU) */
    uint64_t cow_copies;                /**< Copy-on-write operations */
    uint64_t invalidations;             /**< Manual invalidations */
    float hit_rate;                     /**< Hit rate [0, 1] */
    size_t current_entries;             /**< Current entry count */
    size_t peak_entries;                /**< Peak entry count */
} pattern_cache_stats_t;

/**
 * @brief Cache configuration
 */
typedef struct pattern_cache_config {
    size_t max_entries;                 /**< Maximum entries */
    uint64_t default_ttl_ms;            /**< Default TTL in ms */
    bool enable_cow;                    /**< Enable copy-on-write */
    bool enable_lru;                    /**< Enable LRU eviction */
    bool enable_logging;                /**< Enable logging */
    float similarity_tolerance;         /**< Tolerance for hash collisions */
} pattern_cache_config_t;

/**
 * @brief Pattern cache (opaque)
 */
typedef struct pattern_cache pattern_cache_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default cache configuration
 */
nimcp_error_t pattern_cache_default_config(pattern_cache_config_t* config);

/**
 * @brief Create pattern cache
 */
pattern_cache_t* pattern_cache_create(const pattern_cache_config_t* config);

/**
 * @brief Destroy pattern cache
 */
void pattern_cache_destroy(pattern_cache_t* cache);

/**
 * @brief Clear all cache entries
 */
nimcp_error_t pattern_cache_clear(pattern_cache_t* cache);

/* ============================================================================
 * Cache Operations
 * ============================================================================ */

/**
 * @brief Compute hash for pattern
 */
nimcp_error_t pattern_cache_hash(
    const mesh_pattern_t* pattern,
    pattern_hash_t* hash_out
);

/**
 * @brief Look up cached activations
 *
 * @param cache Pattern cache
 * @param pattern Pattern to look up
 * @param activations_out Output: cached activations
 * @param max_activations Maximum activations to return
 * @param count_out Actual count returned
 * @return NIMCP_SUCCESS if found, NIMCP_ERROR_NOT_FOUND if miss
 */
nimcp_error_t pattern_cache_lookup(
    pattern_cache_t* cache,
    const mesh_pattern_t* pattern,
    cached_activation_t* activations_out,
    size_t max_activations,
    size_t* count_out
);

/**
 * @brief Store activations in cache
 *
 * @param cache Pattern cache
 * @param pattern Pattern key
 * @param activations Activations to cache
 * @param count Number of activations
 * @param ttl_ms Time-to-live (0 for default)
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t pattern_cache_store(
    pattern_cache_t* cache,
    const mesh_pattern_t* pattern,
    const cached_activation_t* activations,
    size_t count,
    uint64_t ttl_ms
);

/**
 * @brief Invalidate cache entry
 */
nimcp_error_t pattern_cache_invalidate(
    pattern_cache_t* cache,
    const mesh_pattern_t* pattern
);

/**
 * @brief Invalidate entries for specific module
 *
 * Called when a module's receptive field changes
 */
nimcp_error_t pattern_cache_invalidate_module(
    pattern_cache_t* cache,
    mesh_participant_id_t module_id
);

/* ============================================================================
 * Copy-on-Write Operations
 * ============================================================================ */

/**
 * @brief Acquire read reference to cache entry
 *
 * Increments ref count, returns pointer valid until release
 */
const pattern_cache_entry_t* pattern_cache_acquire(
    pattern_cache_t* cache,
    const mesh_pattern_t* pattern
);

/**
 * @brief Release reference to cache entry
 */
void pattern_cache_release(
    pattern_cache_t* cache,
    const pattern_cache_entry_t* entry
);

/**
 * @brief Create copy-on-write variant
 *
 * Creates a copy of the entry with modifications
 */
nimcp_error_t pattern_cache_cow_copy(
    pattern_cache_t* cache,
    const mesh_pattern_t* original,
    const mesh_pattern_t* variant,
    pattern_cache_entry_t** new_entry_out
);

/* ============================================================================
 * Maintenance
 * ============================================================================ */

/**
 * @brief Evict expired entries
 */
nimcp_error_t pattern_cache_evict_expired(pattern_cache_t* cache);

/**
 * @brief Evict LRU entries to make room
 */
nimcp_error_t pattern_cache_evict_lru(pattern_cache_t* cache, size_t count);

/**
 * @brief Get cache statistics
 */
nimcp_error_t pattern_cache_get_stats(
    pattern_cache_t* cache,
    pattern_cache_stats_t* stats
);

/**
 * @brief Print cache state
 */
void pattern_cache_print(const pattern_cache_t* cache);

/* ============================================================================
 * Hash Utilities
 * ============================================================================ */

/**
 * @brief Compare two pattern hashes
 */
bool pattern_hash_equals(const pattern_hash_t* a, const pattern_hash_t* b);

/**
 * @brief Convert hash to hex string
 */
void pattern_hash_to_string(const pattern_hash_t* hash, char* buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_PATTERN_CACHE_H */
