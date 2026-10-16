#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_config_hash.c - High-Performance Hash Table Implementation
//=============================================================================
/**
 * @file nimcp_config_hash.c
 * @brief Thread-safe hash table implementation for config storage
 *
 * IMPLEMENTATION DETAILS:
 *
 * HASHING: FNV-1a Algorithm
 * - Fast: ~1 cycle/byte on modern CPUs
 * - Good distribution: avalanche effect
 * - Simple: no complex operations
 * - Formula: hash = (hash ^ byte) * FNV_PRIME
 *
 * COLLISION RESOLUTION: Linear Probing
 * - Cache-friendly: sequential access pattern
 * - Simple: just increment index
 * - Requires: load factor < 0.75 for performance
 * - Formula: index = (hash + probe) % capacity
 *
 * DELETION: Tombstone Method
 * - Marks deleted entries as tombstones
 * - Preserves probe chains
 * - Can be reused for new insertions
 * - Triggers resize when tombstones accumulate
 *
 * RESIZING: Doubling Strategy
 * - Trigger: load_factor > 0.75
 * - New capacity: capacity * 2
 * - Rehash: all entries moved to new table
 * - Lock: exclusive lock during resize
 *
 * THREAD SAFETY: Reader-Writer Lock
 * - Multiple concurrent readers (shared lock)
 * - Single writer (exclusive lock)
 * - Lock-free reads when no resize needed
 *
 * @version 1.0.0
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "utils/config/nimcp_config_hash.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_rwlock.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(config_hash)

//=============================================================================
// FNV-1a Hash Constants
//=============================================================================

/**
 * @brief FNV-1a offset basis (64-bit)
 *
 * WHAT: Initial hash value for FNV-1a algorithm
 * WHY:  Provides good initial distribution
 * HOW:  Magic constant from FNV specification
 */
#define FNV_OFFSET_BASIS_64 0xcbf29ce484222325ULL

/**
 * @brief FNV-1a prime (64-bit)
 *
 * WHAT: Multiplication constant for FNV-1a
 * WHY:  Prime number provides good avalanche effect
 * HOW:  Magic constant from FNV specification
 */
#define FNV_PRIME_64 0x100000001b3ULL

//=============================================================================
// Internal Data Structures
//=============================================================================

/**
 * @brief Hash table entry
 *
 * WHAT: Single bucket in the hash table
 * WHY:  Store key-value pair with metadata
 * HOW:  Fixed-size struct with value union for space efficiency
 */
typedef struct {
    char* key;                  /**< Key string (heap-allocated) */
    config_value_t value;       /**< Value union */
    config_value_type_t type;   /**< Value type */
    bool occupied;              /**< Bucket is occupied */
    bool deleted;               /**< Bucket is tombstone */
} config_hash_entry_t;

/**
 * @brief Hash table structure
 *
 * WHAT: Main hash table data structure
 * WHY:  Encapsulate all table state and metadata
 * HOW:  Opaque struct with buckets array and synchronization
 */
struct config_hash_table {
    uint32_t magic;                     /**< Magic value for validation */
    config_hash_entry_t* buckets;       /**< Buckets array */
    size_t capacity;                    /**< Current capacity (power of 2) */
    size_t size;                        /**< Number of entries */
    size_t tombstones;                  /**< Number of deleted entries */
    nimcp_platform_rwlock_t lock;              /**< Reader-writer lock */
    unified_mem_manager_t mem_manager;  /**< Unified memory manager */
    unified_mem_handle_t buckets_handle; /**< Handle for buckets array */
    uint32_t security_module_id;        /**< Security module ID */
    bool security_registered;           /**< Security registration status */
};

//=============================================================================
// FNV-1a Hash Function Implementation
//=============================================================================

/**
 * @brief Compute FNV-1a hash of string
 *
 * WHAT: Fast non-cryptographic hash function
 * WHY:  Good distribution with minimal collision
 * HOW:  Iteratively XOR and multiply by prime
 *
 * @param key String to hash
 * @return 64-bit hash value
 *
 * COMPLEXITY: O(m) where m = key length
 *
 * ALGORITHM:
 * ```
 * hash = FNV_OFFSET_BASIS
 * for each byte in key:
 *     hash = (hash XOR byte) * FNV_PRIME
 * return hash
 * ```
 */
static uint64_t fnv1a_hash(const char* key) {
    uint64_t hash = FNV_OFFSET_BASIS_64;

    while (*key) {
        hash ^= (uint64_t)(unsigned char)(*key);
        hash *= FNV_PRIME_64;
        key++;
    }

    return hash;
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Find entry index for key
 *
 * WHAT: Locates bucket for key using linear probing
 * WHY:  Core operation for get/set/remove
 * HOW:  Hash key, probe sequentially until found or empty
 *
 * @param table Hash table
 * @param key Key to find
 * @param for_insert If true, return tombstone slots as available
 * @return Bucket index or -1 if not found
 *
 * COMPLEXITY: O(1) average, O(n) worst case
 *
 * LINEAR PROBING:
 * - Start at hash % capacity
 * - If occupied and key matches: found
 * - If empty: not found
 * - If tombstone: continue if searching, return if inserting
 * - Else: increment and wrap around
 */
static ssize_t find_entry(
    config_hash_table_t table,
    const char* key,
    bool for_insert
) {
    uint64_t hash = fnv1a_hash(key);
    size_t index = hash % table->capacity;
    size_t start_index = index;
    ssize_t first_tombstone = -1;

    do {
        config_hash_entry_t* entry = &table->buckets[index];

        // Found exact match
        if (entry->occupied && !entry->deleted &&
            strcmp(entry->key, key) == 0) {
            return (ssize_t)index;
        }

        // Empty slot (key not in table)
        if (!entry->occupied && !entry->deleted) {
            // If inserting and found tombstone earlier, use that
            if (for_insert && first_tombstone >= 0) {
                return first_tombstone;
            }
            return for_insert ? (ssize_t)index : -1;
        }

        // Tombstone (deleted entry)
        if (entry->deleted && first_tombstone < 0 && for_insert) {
            first_tombstone = (ssize_t)index;
        }

        // Linear probe to next slot
        index = (index + 1) % table->capacity;

    } while (index != start_index);

    // Table is full or wrapped around
    if (for_insert && first_tombstone >= 0) {
        return first_tombstone;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_entry: capacity exceeded");
    return -1;
}

/**
 * @brief Resize hash table to new capacity
 *
 * WHAT: Allocates new buckets array and rehashes all entries
 * WHY:  Maintain load factor < 0.75 for O(1) performance
 * HOW:  Allocate new array, rehash all entries, free old array
 *
 * @param table Hash table
 * @param new_capacity New capacity (must be power of 2)
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(n)
 *
 * RESIZE ALGORITHM:
 * 1. Allocate new buckets array
 * 2. For each occupied entry in old array:
 *    a. Compute new hash % new_capacity
 *    b. Linear probe for empty slot
 *    c. Insert entry
 * 3. Free old buckets array
 * 4. Update table metadata
 */
static bool resize_table(config_hash_table_t table, size_t new_capacity) {
    LOG_DEBUG("Resizing config hash table from %zu to %zu (size=%zu, load=%.2f)",
              table->capacity, new_capacity, table->size,
              (double)table->size / table->capacity);

    // Allocate new buckets array
    unified_mem_request_t req = unified_mem_request_direct(
        new_capacity * sizeof(config_hash_entry_t)
    );
    unified_mem_handle_t new_buckets_handle =
        unified_mem_alloc(table->mem_manager, &req);

    if (!new_buckets_handle) {
        LOG_ERROR("Failed to allocate new buckets array for resize");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "resize_table: new_buckets_handle is NULL");
        return false;
    }

    config_hash_entry_t* new_buckets =
        (config_hash_entry_t*)unified_mem_write(new_buckets_handle);

    // Initialize new buckets
    memset(new_buckets, 0, new_capacity * sizeof(config_hash_entry_t));

    // Rehash all entries
    size_t rehashed = 0;
    for (size_t i = 0; i < table->capacity; i++) {
        config_hash_entry_t* old_entry = &table->buckets[i];

        // Skip empty and deleted entries
        if (!old_entry->occupied || old_entry->deleted) {
            continue;
        }

        // Find new slot in resized table
        uint64_t hash = fnv1a_hash(old_entry->key);
        size_t index = hash % new_capacity;

        // Linear probe for empty slot
        while (new_buckets[index].occupied) {
            index = (index + 1) % new_capacity;
        }

        // Move entry to new slot (shallow copy - transfer ownership)
        new_buckets[index] = *old_entry;
        rehashed++;
    }

    LOG_DEBUG("Rehashed %zu entries (expected %zu)", rehashed, table->size);

    // Free old buckets
    if (table->buckets_handle) {
        unified_mem_free(table->buckets_handle);
    }

    // Update table
    table->buckets = new_buckets;
    table->buckets_handle = new_buckets_handle;
    table->capacity = new_capacity;
    table->tombstones = 0;

    LOG_INFO("Config hash table resized to %zu (load=%.2f)",
             new_capacity, (double)table->size / new_capacity);

    return true;
}

/**
 * @brief Check if resize is needed
 *
 * WHAT: Determines if table should be resized
 * WHY:  Maintain optimal load factor for performance
 * HOW:  Compare (size + tombstones) / capacity against threshold
 *
 * @param table Hash table
 * @return true if resize needed
 *
 * RESIZE TRIGGERS:
 * - Load factor > 0.75
 * - Tombstones > 25% of capacity
 */
static bool needs_resize(config_hash_table_t table) {
    double load = (double)(table->size + table->tombstones) / table->capacity;
    return load > CONFIG_HASH_MAX_LOAD_FACTOR;
}

/**
 * @brief Copy value (deep copy for strings)
 *
 * WHAT: Copies config value with proper memory management
 * WHY:  Ensure independent ownership of string values
 * HOW:  Shallow copy for primitives, strdup for strings
 *
 * @param dst Destination value
 * @param src Source value
 * @param type Value type
 * @return true on success, false on failure
 */
static bool copy_value(
    config_value_t* dst,
    const config_value_t* src,
    config_value_type_t type
) {
    // Primitive types: direct copy
    if (type != CONFIG_VALUE_STRING) {
        *dst = *src;
        return true;
    }

    // String type: duplicate
    if (src->s) {
        dst->s = nimcp_strdup(src->s);
        if (!dst->s) {
            LOG_ERROR("Failed to duplicate string value");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "copy_value: dst->s is NULL");
            return false;
        }
    } else {
        dst->s = NULL;
    }

    return true;
}

/**
 * @brief Free value resources
 *
 * WHAT: Releases memory for value (strings only)
 * WHY:  Prevent memory leaks
 * HOW:  Free string pointer for string types
 *
 * @param value Value to free
 * @param type Value type
 */
static void free_value(config_value_t* value, config_value_type_t type) {
    if (type == CONFIG_VALUE_STRING && value->s) {
        nimcp_free(value->s);
        value->s = NULL;
    }
}

/**
 * @brief Round up to next power of 2
 *
 * WHAT: Finds smallest power of 2 >= n
 * WHY:  Capacity must be power of 2 for efficient modulo
 * HOW:  Bit manipulation
 *
 * @param n Input value
 * @return Next power of 2
 */
static size_t next_power_of_2(size_t n) {
    if (n == 0) return 1;

    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    n++;

    return n;
}

//=============================================================================
// Public API Implementation
//=============================================================================

config_hash_table_t config_hash_create(size_t initial_capacity) {
    LOG_DEBUG("Creating config hash table (initial_capacity=%zu)",
              initial_capacity);

    // Use default capacity if not specified
    if (initial_capacity == 0) {
        initial_capacity = CONFIG_HASH_DEFAULT_CAPACITY;
    }

    // Ensure minimum capacity
    if (initial_capacity < CONFIG_HASH_MIN_CAPACITY) {
        initial_capacity = CONFIG_HASH_MIN_CAPACITY;
    }

    // Round up to power of 2 for efficient modulo
    size_t capacity = next_power_of_2(initial_capacity);

    // Cap at maximum
    if (capacity > CONFIG_HASH_MAX_CAPACITY) {
        capacity = CONFIG_HASH_MAX_CAPACITY;
    }

    // Create unified memory manager
    unified_mem_config_t mem_config = unified_mem_default_config();
    mem_config.enable_cow = false;  // Direct allocation for hash table
    unified_mem_manager_t mem_manager = unified_mem_create(&mem_config);
    if (!mem_manager) {
        LOG_ERROR("Failed to create unified memory manager");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mem_manager is NULL");

        return NULL;
    }

    // Allocate table structure
    unified_mem_request_t req = unified_mem_request_direct(
        sizeof(struct config_hash_table)
    );
    unified_mem_handle_t table_handle = unified_mem_alloc(mem_manager, &req);
    if (!table_handle) {
        LOG_ERROR("Failed to allocate config hash table structure");
        unified_mem_destroy(mem_manager);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "table_handle is NULL");

        return NULL;
    }

    config_hash_table_t table =
        (config_hash_table_t)unified_mem_write(table_handle);

    // Initialize table
    table->magic = CONFIG_HASH_MAGIC;
    table->capacity = capacity;
    table->size = 0;
    table->tombstones = 0;
    table->mem_manager = mem_manager;
    table->buckets_handle = NULL;
    table->security_registered = false;
    table->security_module_id = 0;

    // Allocate buckets array
    req = unified_mem_request_direct(capacity * sizeof(config_hash_entry_t));
    unified_mem_handle_t buckets_handle = unified_mem_alloc(mem_manager, &req);
    if (!buckets_handle) {
        LOG_ERROR("Failed to allocate buckets array");
        unified_mem_free(table_handle);
        unified_mem_destroy(mem_manager);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buckets_handle is NULL");

        return NULL;
    }

    table->buckets = (config_hash_entry_t*)unified_mem_write(buckets_handle);
    table->buckets_handle = buckets_handle;
    memset(table->buckets, 0, capacity * sizeof(config_hash_entry_t));

    // Initialize reader-writer lock
    int rc = nimcp_platform_rwlock_init(&table->lock);
    if (rc != 0) {
        LOG_ERROR("Failed to initialize rwlock: %d", rc);
        unified_mem_free(buckets_handle);
        unified_mem_free(table_handle);
        unified_mem_destroy(mem_manager);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_hash_create: validation failed");
        return NULL;
    }

    LOG_INFO("Created config hash table (capacity=%zu)", capacity);

    return table;
}

void config_hash_destroy(config_hash_table_t table) {
    if (!table) {
        return;
    }

    // Validate magic
    if (table->magic != CONFIG_HASH_MAGIC) {
        LOG_ERROR("Invalid config hash table (bad magic: 0x%08x)", table->magic);
        return;
    }

    LOG_DEBUG("Destroying config hash table (size=%zu, capacity=%zu)",
              table->size, table->capacity);

    // Free all entries
    for (size_t i = 0; i < table->capacity; i++) {
        config_hash_entry_t* entry = &table->buckets[i];
        if (entry->occupied && !entry->deleted) {
            nimcp_free(entry->key);
            free_value(&entry->value, entry->type);
        }
    }

    // Destroy lock
    nimcp_platform_rwlock_destroy(&table->lock);

    // Free buckets
    if (table->buckets_handle) {
        unified_mem_free(table->buckets_handle);
    }

    // Destroy memory manager
    unified_mem_destroy(table->mem_manager);

    // Clear magic
    table->magic = 0;

    LOG_INFO("Config hash table destroyed");
}

bool config_hash_set(
    config_hash_table_t table,
    const char* key,
    const config_value_t* value,
    config_value_type_t type
) {
    if (!table || !key || !value) {
        LOG_ERROR("Invalid parameters to config_hash_set");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_hash_set: required parameter is NULL (table, key, value)");
        return false;
    }

    if (table->magic != CONFIG_HASH_MAGIC) {
        LOG_ERROR("Invalid table magic");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_hash_set: validation failed");
        return false;
    }

    size_t key_len = strlen(key);
    if (key_len >= CONFIG_HASH_MAX_KEY_LEN) {
        LOG_ERROR("Key too long: %zu >= %d", key_len, CONFIG_HASH_MAX_KEY_LEN);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "config_hash_set: capacity exceeded");
        return false;
    }

    // Acquire exclusive lock
    nimcp_platform_rwlock_wrlock(&table->lock);

    // Check if resize needed
    if (needs_resize(table)) {
        size_t new_capacity = table->capacity * 2;
        if (new_capacity > CONFIG_HASH_MAX_CAPACITY) {
            LOG_ERROR("Cannot resize beyond max capacity");
            nimcp_platform_rwlock_unlock(&table->lock);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_hash_set: validation failed");
            return false;
        }

        if (!resize_table(table, new_capacity)) {
            LOG_ERROR("Failed to resize table");
            nimcp_platform_rwlock_unlock(&table->lock);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_hash_set: resize_table is NULL");
            return false;
        }
    }

    // Find insertion point
    ssize_t index = find_entry(table, key, true);
    if (index < 0) {
        LOG_ERROR("Failed to find insertion point for key: %s", key);
        nimcp_platform_rwlock_unlock(&table->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "config_hash_set: validation failed");
        return false;
    }

    config_hash_entry_t* entry = &table->buckets[index];

    // Update existing entry
    if (entry->occupied && !entry->deleted) {
        LOG_DEBUG("Updating existing entry: %s", key);

        // Free old value
        free_value(&entry->value, entry->type);

        // Copy new value
        if (!copy_value(&entry->value, value, type)) {
            nimcp_platform_rwlock_unlock(&table->lock);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_hash_set: copy_value is NULL");
            return false;
        }

        entry->type = type;

    } else {
        // Insert new entry
        LOG_DEBUG("Inserting new entry: %s", key);

        // Duplicate key
        entry->key = nimcp_strdup(key);
        if (!entry->key) {
            LOG_ERROR("Failed to duplicate key");
            nimcp_platform_rwlock_unlock(&table->lock);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "config_hash_set: entry->key is NULL");
            return false;
        }

        // Copy value
        if (!copy_value(&entry->value, value, type)) {
            nimcp_free(entry->key);
            nimcp_platform_rwlock_unlock(&table->lock);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_hash_set: copy_value is NULL");
            return false;
        }

        entry->type = type;
        entry->occupied = true;

        // Decrement tombstones if reusing deleted slot
        if (entry->deleted) {
            table->tombstones--;
        }
        entry->deleted = false;

        table->size++;
    }

    nimcp_platform_rwlock_unlock(&table->lock);

    LOG_DEBUG("Set config: %s = <value> (type=%s)",
              key, config_value_type_name(type));

    return true;
}

bool config_hash_get(
    config_hash_table_t table,
    const char* key,
    config_value_t* value,
    config_value_type_t* type
) {
    if (!table || !key || !value) {
        LOG_ERROR("Invalid parameters to config_hash_get");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_hash_get: required parameter is NULL (table, key, value)");
        return false;
    }

    if (table->magic != CONFIG_HASH_MAGIC) {
        LOG_ERROR("Invalid table magic");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_hash_get: validation failed");
        return false;
    }

    // Acquire shared lock
    nimcp_platform_rwlock_rdlock(&table->lock);

    // Find entry
    ssize_t index = find_entry(table, key, false);

    if (index < 0) {
        nimcp_platform_rwlock_unlock(&table->lock);
        LOG_DEBUG("Key not found: %s", key);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "config_hash_get: validation failed");
        return false;
    }

    config_hash_entry_t* entry = &table->buckets[index];

    // Copy value
    if (!copy_value(value, &entry->value, entry->type)) {
        nimcp_platform_rwlock_unlock(&table->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_hash_get: copy_value is NULL");
        return false;
    }

    if (type) {
        *type = entry->type;
    }

    nimcp_platform_rwlock_unlock(&table->lock);

    LOG_DEBUG("Get config: %s = <value> (type=%s)",
              key, config_value_type_name(entry->type));

    return true;
}

bool config_hash_remove(config_hash_table_t table, const char* key) {
    if (!table || !key) {
        LOG_ERROR("Invalid parameters to config_hash_remove");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_hash_remove: required parameter is NULL (table, key)");
        return false;
    }

    if (table->magic != CONFIG_HASH_MAGIC) {
        LOG_ERROR("Invalid table magic");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_hash_remove: validation failed");
        return false;
    }

    // Acquire exclusive lock
    nimcp_platform_rwlock_wrlock(&table->lock);

    // Find entry
    ssize_t index = find_entry(table, key, false);

    if (index < 0) {
        nimcp_platform_rwlock_unlock(&table->lock);
        LOG_DEBUG("Key not found for removal: %s", key);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "config_hash_remove: validation failed");
        return false;
    }

    config_hash_entry_t* entry = &table->buckets[index];

    // Free resources
    nimcp_free(entry->key);
    free_value(&entry->value, entry->type);

    // Mark as tombstone
    entry->key = NULL;
    entry->deleted = true;

    table->size--;
    table->tombstones++;

    nimcp_platform_rwlock_unlock(&table->lock);

    LOG_DEBUG("Removed config entry: %s", key);

    return true;
}

bool config_hash_contains(config_hash_table_t table, const char* key) {
    if (!table || !key) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_hash_contains: required parameter is NULL (table, key)");
        return false;
    }

    if (table->magic != CONFIG_HASH_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "config_hash_contains: validation failed");
        return false;
    }

    nimcp_platform_rwlock_rdlock(&table->lock);
    ssize_t index = find_entry(table, key, false);
    nimcp_platform_rwlock_unlock(&table->lock);

    return index >= 0;
}

void config_hash_clear(config_hash_table_t table) {
    if (!table) {
        return;
    }

    if (table->magic != CONFIG_HASH_MAGIC) {
        LOG_ERROR("Invalid table magic");
        return;
    }

    LOG_DEBUG("Clearing config hash table (size=%zu)", table->size);

    nimcp_platform_rwlock_wrlock(&table->lock);

    // Free all entries
    for (size_t i = 0; i < table->capacity; i++) {
        config_hash_entry_t* entry = &table->buckets[i];
        if (entry->occupied && !entry->deleted) {
            nimcp_free(entry->key);
            free_value(&entry->value, entry->type);
        }
    }

    // Reset all buckets
    memset(table->buckets, 0, table->capacity * sizeof(config_hash_entry_t));

    table->size = 0;
    table->tombstones = 0;

    nimcp_platform_rwlock_unlock(&table->lock);

    LOG_INFO("Config hash table cleared");
}

size_t config_hash_size(config_hash_table_t table) {
    if (!table) {
        return 0;
    }

    if (table->magic != CONFIG_HASH_MAGIC) {
        return 0;
    }

    nimcp_platform_rwlock_rdlock(&table->lock);
    size_t size = table->size;
    nimcp_platform_rwlock_unlock(&table->lock);

    return size;
}

void config_hash_iterate(
    config_hash_table_t table,
    config_hash_iterator_t iter,
    void* user_data
) {
    if (!table || !iter) {
        LOG_ERROR("Invalid parameters to config_hash_iterate");
        return;
    }

    if (table->magic != CONFIG_HASH_MAGIC) {
        LOG_ERROR("Invalid table magic");
        return;
    }

    nimcp_platform_rwlock_rdlock(&table->lock);

    for (size_t i = 0; i < table->capacity; i++) {
        config_hash_entry_t* entry = &table->buckets[i];

        if (entry->occupied && !entry->deleted) {
            iter(entry->key, &entry->value, entry->type, user_data);
        }
    }

    nimcp_platform_rwlock_unlock(&table->lock);
}

config_hash_table_t config_hash_snapshot(config_hash_table_t table) {
    if (!table) {
        LOG_ERROR("Invalid table for snapshot");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "table is NULL");

        return NULL;
    }

    if (table->magic != CONFIG_HASH_MAGIC) {
        LOG_ERROR("Invalid table magic");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config_hash_snapshot: validation failed");
        return NULL;
    }

    nimcp_platform_rwlock_rdlock(&table->lock);

    // Create new table with same capacity
    config_hash_table_t new_table = config_hash_create(table->capacity);
    if (!new_table) {
        nimcp_platform_rwlock_unlock(&table->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "new_table is NULL");

        return NULL;
    }

    // Copy all entries
    for (size_t i = 0; i < table->capacity; i++) {
        config_hash_entry_t* entry = &table->buckets[i];

        if (entry->occupied && !entry->deleted) {
            config_hash_set(new_table, entry->key, &entry->value, entry->type);
        }
    }

    nimcp_platform_rwlock_unlock(&table->lock);

    LOG_INFO("Created config hash table snapshot (size=%zu)", table->size);

    return new_table;
}

void config_hash_swap(
    config_hash_table_t* t1,
    config_hash_table_t* t2
) {
    if (!t1 || !t2) {
        LOG_ERROR("Invalid parameters to config_hash_swap");
        return;
    }

    config_hash_table_t tmp = *t1;
    *t1 = *t2;
    *t2 = tmp;

    LOG_DEBUG("Swapped config hash tables");
}

double config_hash_load_factor(config_hash_table_t table) {
    if (!table) {
        return 0.0;
    }

    if (table->magic != CONFIG_HASH_MAGIC) {
        return 0.0;
    }

    nimcp_platform_rwlock_rdlock(&table->lock);
    double load = (double)table->size / table->capacity;
    nimcp_platform_rwlock_unlock(&table->lock);

    return load;
}

size_t config_hash_capacity(config_hash_table_t table) {
    if (!table) {
        return 0;
    }

    if (table->magic != CONFIG_HASH_MAGIC) {
        return 0;
    }

    nimcp_platform_rwlock_rdlock(&table->lock);
    size_t capacity = table->capacity;
    nimcp_platform_rwlock_unlock(&table->lock);

    return capacity;
}

uint32_t config_hash_get_security_id(config_hash_table_t table) {
    if (!table || !table->security_registered) {
        return 0;
    }

    if (table->magic != CONFIG_HASH_MAGIC) {
        return 0;
    }

    return table->security_module_id;
}
